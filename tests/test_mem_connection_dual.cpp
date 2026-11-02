// ⚠️⚠️ **路线 A（D 批）—— `ch_mem` 读端口「连接」语义进双后端 parity**
// （2026-11-02 · D 批第 1 个文件）
//
// ## 为什么这个文件【值得】存在（立项理由，不是凑数）
//
// `tests/test_mem_connection.cpp` 覆盖 **4 种 `ch_mem` 读端口的连接形态**
// （`sread` → `ch_out` · `aread` → `ch_out` · `sread` → `ch_uint` 中转 →
// `ch_out` · `sread` → `ch_reg` → `ch_out`），**全部是裸 `ch_device` 单后端**
// ⇒ ⇒ 这 4 条连接路径在 **verilator codegen 上从未被验证过**。
//
// ⚠️ **为什么连接形态值得单独钉**：`ch_mem` 的读端口是**虚构节点**
// （它自己没有 Verilog 信号），它的数据靠 `data_proxy` 承载
// （`read_port → data_proxy` 的边）⇒ ⇒ codegen 必须：
//   ① 跳过「`src(0)` 是 mem read port」的 proxy 赋值（否则双驱动）；
//   ② 在 `print_output` 的代理链遍历里**不越过** mem read port
//      （否则 `assign out = <read_port>` 引用**未声明**信号）；
//   ③ 不把 `type_mem_read_port` 放进 wire 声明列表。
// ⇒ ⇒ 这三处任一错了，**单后端测试完全看不见**（interpreter 走的是 data_map，
// 不经过这些 codegen 分支）⇒ ⇒ 只有双后端能抓。
//
// ⚠️ **与 `test_mem_sread_disable_dual.cpp` 的分工**：那个文件钉**禁读拍**
// （`enable == 0`）的语义；本文件钉**连接形态**（`enable` 恒 1，或 `aread`
// 无 enable）⇒ ⇒ 两者正交，不重复。
//
// ## 期望值（**手推**，禁抄仿真输出）
//
// 语义前提：
//   ① `sread` 是 **1-cycle**（`always_ff`）⇒ `tick()` 一次即完成沿上采样；
//   ② `aread` 是 **0-cycle**（`assign`）⇒ 同拍可见；
//   ③ `ch_reg` 中转会**多一拍**（寄存器在下一个沿采到 proxy 的沿前值）
//      ⇒ ⇒ 该 pair 用 **2 次 tick**（沿用原文件的节奏，已由原单后端用例验证）；
//   ④ 各 DUT 的初始化内容见其 `describe()`。
//
// ⚠️ **已省略**原文件里 DUT 的 `clk_in` / `rst_in` 端口：它们**从未被驱动、
//   也从未被使用**（死端口）⇒ ⇒ 在双后端下「未驱动端口」读到的是**未定义值**
//   （仓内 ANTI-PATTERNS 已登记）⇒ ⇒ 保留只会引入无谓的风险，故不复制。
//
// ⚠️ **判据（N2′）**：断言数(with verilator) == **2 ×** 断言数(interpreter-only)。
#include "catch_amalgamated.hpp"

#include "component.h"
#include "core/bool.h"
#include "core/context.h"
#include "core/mem.h"
#include "core/reg.h"
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

struct AddrExpect {
    uint64_t addr;
    uint64_t value;
};

// ── DUT 1：`sread` → `ch_out`（enable 恒 1）──
class SReadToOutDut : public ch::Component {
public:
    __io(ch_in<ch_uint<4>> addr_in; ch_out<ch_uint<8>> data_out;)

    SReadToOutDut(ch::Component *parent = nullptr,
                  const std::string &name = "sread_to_out")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        const std::vector<uint64_t> init = {10,  20,  30,  40,  50,  60,
                                           70,  80,  90,  100, 110, 120,
                                           130, 140, 150, 160};
        ch_mem<ch_uint<8>, 16> mem(init, "sread_mem");
        auto read_port = mem.sread(io().addr_in, ch_bool(1_b), "sread_port");
        io().data_out <<= read_port;
    }
};
// 每步 2 次 tick（沿用原文件节奏）
const AddrExpect kSReadToOut[] = {{0, 10}, {1, 20}, {2, 30}, {7, 80}, {15, 160}};

// ── DUT 2：`aread` → `ch_out`（0-cycle）──
class AReadToOutDut : public ch::Component {
public:
    __io(ch_in<ch_uint<4>> addr_in; ch_out<ch_uint<8>> data_out;)

    AReadToOutDut(ch::Component *parent = nullptr,
                  const std::string &name = "aread_to_out")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        const std::vector<uint64_t> init = {5,   15,  25,  35,  45,  55,
                                           65,  75,  85,  95,  105, 115,
                                           125, 135, 145, 155};
        ch_mem<ch_uint<8>, 16> mem(init, "aread_mem");
        auto read_port = mem.aread(io().addr_in, "aread_port");
        io().data_out <<= read_port;
    }
};
// 每步 1 次 tick（aread 同拍可见）
const AddrExpect kAReadToOut[] = {{0, 5}, {1, 15}, {3, 35}, {10, 105}, {15, 155}};

// ── DUT 3：`sread` → `ch_uint` 中转 → `ch_out`（16 位数据，8 深度）──
class SReadViaUIntDut : public ch::Component {
public:
    __io(ch_in<ch_uint<3>> addr_in; ch_out<ch_uint<16>> data_out;)

    SReadViaUIntDut(ch::Component *parent = nullptr,
                    const std::string &name = "sread_via_uint")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        const std::vector<uint64_t> init = {100, 200, 300, 400,
                                           500, 600, 700, 800};
        ch_mem<ch_uint<16>, 8> mem(init, "sread16_mem");
        auto read_port = mem.sread(io().addr_in, ch_bool(1_b), "sread_port");
        ch_uint<16> read_data;
        read_data <<= read_port;
        io().data_out <<= read_data;
    }
};
const AddrExpect kSReadViaUInt[] = {{0, 100}, {1, 200}, {2, 300},
                                    {4, 500}, {7, 800}};

// ── DUT 4：`sread` → `ch_reg` → `ch_out`（寄存器多一拍）──
class SReadToRegDut : public ch::Component {
public:
    __io(ch_in<ch_uint<4>> addr_in; ch_out<ch_uint<8>> data_out;)

    SReadToRegDut(ch::Component *parent = nullptr,
                  const std::string &name = "sread_to_reg")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        const std::vector<uint64_t> init = {11,  22,  33,  44,  55,  66,
                                           77,  88,  99,  111, 122, 133,
                                           144, 155, 166, 177};
        ch_mem<ch_uint<8>, 16> mem(init, "sread_reg_mem");
        auto read_port = mem.sread(io().addr_in, ch_bool(1_b), "sread_port");
        ch_reg<ch_uint<8>> read_reg(0_d, "read_reg");
        read_reg <<= read_port;
        io().data_out <<= read_reg;
    }
};
// 每步 2 次 tick（sread 1 拍 + 寄存器 1 拍）
const AddrExpect kSReadToReg[] = {{0, 11}, {1, 22}, {5, 66}, {10, 122}, {15, 177}};

}  // namespace

// ───────────── pair 1：sread → ch_out ─────────────
DUAL_BACKEND_TEST_PAIR(
    SReadToOutDut,
    "ch_mem sread 端口连接 ch_out（两后端逐地址一致）",
    "[mem][connection][sread][dual]",
    [&]() {
        auto &io = dev.instance().io();
        for (const auto &tc : kSReadToOut) {
            sim.set_input_value(io.addr_in, tc.addr);
            sim.tick();
            sim.tick();
            INFO("addr=" << tc.addr);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.data_out)) ==
                    tc.value);
        }
    });

// ───────────── pair 2：aread → ch_out ─────────────
DUAL_BACKEND_TEST_PAIR(
    AReadToOutDut,
    "ch_mem aread 端口连接 ch_out（0-cycle，两后端逐地址一致）",
    "[mem][connection][aread][dual]",
    [&]() {
        auto &io = dev.instance().io();
        for (const auto &tc : kAReadToOut) {
            sim.set_input_value(io.addr_in, tc.addr);
            sim.tick();
            INFO("addr=" << tc.addr);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.data_out)) ==
                    tc.value);
        }
    });

// ───────────── pair 3：sread → ch_uint 中转 → ch_out ─────────────
DUAL_BACKEND_TEST_PAIR(
    SReadViaUIntDut,
    "ch_mem sread 端口经 ch_uint 中转连接 ch_out（16 位数据，两后端一致）",
    "[mem][connection][read_port][ch_uint][dual]",
    [&]() {
        auto &io = dev.instance().io();
        for (const auto &tc : kSReadViaUInt) {
            sim.set_input_value(io.addr_in, tc.addr);
            sim.tick();
            INFO("addr=" << tc.addr);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.data_out)) ==
                    tc.value);
        }
    });

// ───────────── pair 4：sread → ch_reg → ch_out ─────────────
DUAL_BACKEND_TEST_PAIR(
    SReadToRegDut,
    "ch_mem sread 端口经 ch_reg 中转连接 ch_out（多一拍，两后端一致）",
    "[mem][connection][sread][ch_reg][dual]",
    [&]() {
        auto &io = dev.instance().io();
        for (const auto &tc : kSReadToReg) {
            sim.set_input_value(io.addr_in, tc.addr);
            sim.tick();
            sim.tick();
            INFO("addr=" << tc.addr);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.data_out)) ==
                    tc.value);
        }
    });
