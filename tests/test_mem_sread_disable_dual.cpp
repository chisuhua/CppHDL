// ⚠️⚠️ **`sread` 禁读拍语义的双后端判据**（2026-11-02 · change
// `sread-disable-cycle-divergence-2026-11`）
//
// ## 这个文件存在的理由
//
// `ch_mem::sread(addr, enable)` 在 **`enable == 0`（禁读拍）** 时，两条后端
// 曾经给出不同答案：
//
// | 后端 | 修复前 | 修复后 |
// |---|---|---|
// | Simulator | `if (!enabled) return;` ⇒ **保持上一拍的值** | **清零** |
// | 生成的 Verilog | `else proxy <= 1'b0;` ⇒ **清零** | 清零（未动） |
//
// ⚠️ **为什么这条路径此前零覆盖**：既有用到「可变 `enable`」的只有
// `tests/test_mem_connection.cpp:43`，而它的 `enable_in` **恒为 1**
// ⇒ ⇒ 禁读拍从未被任何测试触及（`tests/test_mem.cpp` 里 sread 相关的 4 个
// SECTION 全是**结构断言**，不跑仿真）。
//
// ## 期望值（**逐拍手推**，禁抄仿真输出）
//
// **语义前提**：
//   ① `sread` 是 **1-cycle**（Verilog `always_ff`）⇒ tick 后读到的是
//      **本拍沿上采样**的结果（`tick N` 读到第 N 拍采样的值）；
//   ② `ch_mem::sread` 的 `enable` 在**沿上**判断 ⇒ 禁读拍**不更新**输出代理，
//      且按裁决**清零**（不是保持）；
//   ③ 初始化内容 `init_data = {10, 20, …, 160}`（16 项，索引 0..15）。
//
// ⚠️ **判据（N2′）**：断言数(with verilator) == **2 ×** 断言数(interpreter-only)。
#include "catch_amalgamated.hpp"

#include "component.h"
#include "core/bool.h"
#include "core/context.h"
#include "core/mem.h"
#include "core/uint.h"
#include "device.h"
#include "dual_backend_fixture.h"
#include "simulator.h"

#include <vector>

using namespace ch;
using namespace ch::core;

namespace {

// ⚠️ **用例表 MUST 放在文件作用域**：`DUAL_BACKEND_TEST_PAIR` 的 body 参数里
//    **花括号内的逗号**会被预处理器当成**参数分隔**（只有**圆括号**才保护）。

const std::vector<uint64_t> kInit = {10,  20,  30,  40,  50,  60,  70,  80,
                                     90,  100, 110, 120, 130, 140, 150, 160};

/// DUT：`ch_mem` + 带 `enable` 的 `sread` ⇒ 唯一会踩到禁读拍的形态。
class SreadDisableDut : public ch::Component {
public:
    __io(ch_in<ch_uint<4>> addr_in; ch_in<ch_uint<1>> enable_in;
         ch_out<ch_uint<8>> data_out;)

    SreadDisableDut(ch::Component *parent = nullptr,
                    const std::string &name = "sread_disable_dut")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        ch_mem<ch_uint<8>, 16> mem(kInit, "sread_mem");
        // ⚠️ `ch_in<ch_bool>` 不能绑到 `const ch_bool &`（框架约束）
        //    ⇒ 用 `port<ch_uint<1>> == ch_uint<1>(1_d)` 做组合直读。
        ch_bool en(io().enable_in == ch_uint<1>(1_d));
        auto read_port = mem.sread(io().addr_in, en, "sread_port");
        io().data_out <<= read_port;
    }
};

// ── pair 1：同一地址上翻转 enable ⇒ 直接钉「清零 vs 保持」──
struct DisableStep {
    uint64_t addr, en;
    uint64_t dout;
};
const DisableStep kToggleSteps[] = {
    // tick1: en=1, addr=0 ⇒ 读到 10
    {0, 1, 10},
    // tick2: ★ en=0 ⇒ 禁读拍**清零**（若仍是「保持」⇒ 会是 10 ⇒ 红）
    {0, 0, 0},
    // tick3: en=1, addr=1 ⇒ 读到 20
    {1, 1, 20},
    // tick4: ★ en=0 ⇒ 0（保持的话是 20）
    {1, 0, 0},
    // tick5: en=1, addr=15 ⇒ 读到 160
    {15, 1, 160},
    // tick6: ★ en=0 ⇒ 0（保持的话是 160）
    {15, 0, 0},
};

// ── pair 2：禁读拍里**换地址** ⇒ 钉「禁读拍不采样新地址」──
const DisableStep kAddrSteps[] = {
    // tick1: en=0（初值 0）⇒ 0
    {0, 0, 0},
    // tick2: en=1, addr=5 ⇒ 读到 60
    {5, 1, 60},
    // tick3: ★ en=0 且**地址换成 6** ⇒ 0（既不是 60 的保持、也不是 70）
    {6, 0, 0},
    // tick4: en=1, addr=6 ⇒ 读到 70（证明 tick3 没有污染）
    {6, 1, 70},
    // tick5: ★ en=0, addr=7 ⇒ 0（不是 80）
    {7, 0, 0},
    // tick6: en=1, addr=7 ⇒ 读到 80
    {7, 1, 80},
};

}  // namespace

// ───────────── pair 1：enable 翻转时的禁读拍输出 ─────────────
DUAL_BACKEND_TEST_PAIR(
    SreadDisableDut,
    "sread 禁读拍（enable=0）输出清零，不是保持上一拍值（两后端逐拍一致）",
    "[mem][sread][disable][dual]",
    [&]() {
        auto &io = dev.instance().io();
        for (const auto &tc : kToggleSteps) {
            sim.set_input_value(io.addr_in, tc.addr);
            sim.set_input_value(io.enable_in, tc.en);
            sim.tick();
            INFO("addr=" << tc.addr << " en=" << tc.en);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.data_out)) ==
                    tc.dout);
        }
    });

// ───────────── pair 2：禁读拍换地址时不采样 ─────────────
DUAL_BACKEND_TEST_PAIR(
    SreadDisableDut,
    "sread 禁读拍（enable=0）不采样新地址（两后端逐拍一致）",
    "[mem][sread][disable][addr][dual]",
    [&]() {
        auto &io = dev.instance().io();
        for (const auto &tc : kAddrSteps) {
            sim.set_input_value(io.addr_in, tc.addr);
            sim.set_input_value(io.enable_in, tc.en);
            sim.tick();
            INFO("addr=" << tc.addr << " en=" << tc.en);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.data_out)) ==
                    tc.dout);
        }
    });
