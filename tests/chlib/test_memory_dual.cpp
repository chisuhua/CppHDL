// ⚠️⚠️ **路线 B（A5 的 H 批）第 6 批**（2026-11-02 · change
// `dual-backend-parity-route-b-pipeline-2026-11`）
//
// ## 为什么这个文件【值得】存在（立项理由，不是凑数）
//
// `chlib::memory` 的 **single-port RAM / dual-port RAM** 是仓内的基础存储组件，
// 而盘点实测它们的 dual 覆盖率是 **0** ⇒ ⇒ 它们的 Verilog codegen 路径
// **从未被验证过**。
//
// ⚠️ **为什么这类语义特别危险**：RAM 的读写端口在 Verilog 侧要展开成
// `always_ff @(posedge clk) if (we) mem[addr] <= din;` + `assign dout = mem[addr];`
// ⇒ ⇒ **写拍禁读**（实现用 `mem.sread(addr, !we)`）在 codegen 侧要靠
// **enable 信号**表达 ⇒ ⇒ 而「**写的那一拍读端口返回什么**」是典型的
// **同拍写读竞态**（仓内 AGENTS.md 专门记录过 `mem-read-timing` 契约）⇒ ⇒
// 这正是两后端最容易分叉的地方。
//
// ## 期望值（**逐拍手推**，禁抄仿真输出）
//
// **语义前提**（逐条来自 `include/chlib/memory.h` 的实现）：
//   ① `single_port_ram`：`mem.write(addr, din, we)` + `mem.sread(addr, !we)`
//      ⇒ ⇒ **写拍禁读**（`!we` 作为读使能）；
//   ② `sread` 是 **1-cycle**（沿上采样）⇒ ⇒ 读到的值是**上一拍**的地址/内容；
//   ③ `dual_port_ram`：两个端口**各自独立**（各自的 `we` 决定自己那一拍读不读）。
//
// ⚠️ **互证基准**：`tests/chlib/test_memory.cpp` 的既有用例（裸 context）给出
//   「**写 1 拍 → 读 1 拍 ⇒ 读到写入值**」这一序列 ⇒ ⇒ 即：**写拍**读到的
//   **不是**新内容（`!we` ⇒ 读端口被禁），**下一拍**（we=0）读到**新**内容。
//
// 🔴🔴 **`sread` 的禁读拍曾是一次真 parity 分歧（已由
// `sread-disable-cycle-divergence-2026-11` 修复）**：
// **「禁读的那一拍，输出是【保持上一拍的值】还是【清零】？」**
// **修复前两条后端给出不同答案**（实测）：
//
// | 后端 | 禁读拍（`enable=0`） | 代码 |
// |---|---|---|
// | **interpreter** | **直接 return** ⇒ 输出**保持上一拍的值** | `src/ast/instr_mem.cpp:258-259`（`if (!enabled) return;`）|
// | **Verilog** | `else proxy <= 1'b0;` ⇒ 输出**清零** | `src/codegen_verilog.cpp:1261` |
//
// ⇒ ⇒ **精确范围**：只影响**带 `enable` 的 `sread`** ⇒ ⇒ `aread` **不受影响**
//   （`ch_mem::aread` 恒传 `nullptr` 作 enable）⇒ ⇒ 既有 dual 文件
//   （`test_mem_read_timing_dual.cpp`）只用 `sread(addr, ch_bool(true))`（**恒使能**）
//   ⇒ ⇒ **这条路径从未被双后端验证过**。
// ⇒ ⇒ **裁决 = 对齐 Verilog（清零）**：只改 Simulator 一侧
//   （`src/ast/instr_mem.cpp`）⇒ 既有用户生成的 Verilog **零变化**。
// ⇒ ⇒ ⇒ 本文件的写拍期望值因此是 **`0`**（不是「保持上一拍读值」）⇒ ⇒
//   **它同时是该分歧的回归探针**：任一侧被改回「保持」⇒ 立刻变红。
//
// ⚠️ **判据（N2′）**：断言数(with verilator) == **2 ×** 断言数(interpreter-only)。
#include "catch_amalgamated.hpp"

#include "chlib/memory.h"
#include "component.h"
#include "core/bool.h"
#include "core/context.h"
#include "core/uint.h"
#include "device.h"
#include "dual_backend_fixture.h"
#include "simulator.h"

using namespace ch;
using namespace ch::core;
using namespace chlib;

namespace {

// ⚠️ **用例表 MUST 放在文件作用域**：`DUAL_BACKEND_TEST_PAIR` 的 body 参数里
//    **花括号内的逗号**会被预处理器当成**参数分隔**（只有**圆括号**才保护）。

/// DUT 1：`single_port_ram<8, 4>`（深度 16）。
class SingleRamDut : public ch::Component {
public:
    __io(ch_in<ch_uint<4>> addr; ch_in<ch_uint<8>> din; ch_in<ch_uint<1>> we;
         ch_out<ch_uint<8>> dout;)

    SingleRamDut(ch::Component *parent = nullptr,
                 const std::string &name = "single_ram_dut")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        ch_bool we_b = ch_bool(io().we == ch_uint<1>(1_d));
        io().dout = single_port_ram<8, 4>(io().addr, io().din, we_b, "spram");
    }
};

/// DUT 2：`dual_port_ram<8, 4>`（深度 16，两个独立端口）。
class DualRamDut : public ch::Component {
public:
    __io(ch_in<ch_uint<4>> addr_a; ch_in<ch_uint<8>> din_a;
         ch_in<ch_uint<1>> we_a; ch_in<ch_uint<4>> addr_b;
         ch_in<ch_uint<8>> din_b; ch_in<ch_uint<1>> we_b;
         ch_out<ch_uint<8>> dout_a; ch_out<ch_uint<8>> dout_b;)

    DualRamDut(ch::Component *parent = nullptr,
               const std::string &name = "dual_ram_dut")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        ch_bool we_a_b = ch_bool(io().we_a == ch_uint<1>(1_d));
        ch_bool we_b_b = ch_bool(io().we_b == ch_uint<1>(1_d));
        DualPortRAMResult<8, 4> r = dual_port_ram<8, 4>(
            io().addr_a, io().din_a, we_a_b, io().addr_b, io().din_b, we_b_b,
            "dpram");
        io().dout_a = r.dout_a;
        io().dout_b = r.dout_b;
    }
};

// ── 手推金标 ────────────────────────────────────────────────────────────
// single_port_ram：写拍禁读（`!we`）⇒ 写那一拍 dout 保持**上一拍**的值；
//                  下一拍（we=0）读到**刚写入**的值。
struct RamStep {
    uint64_t addr, din, we;
    uint64_t dout;
};
const RamStep kRamSteps[] = {
    // tick1: 写 addr0 = 0x55 ⇒ 写拍禁读 ⇒ **清零** ⇒ 0
    {0, 0x55, 1, 0x00},
    // tick2: we=0, addr=0 ⇒ 读到 0x55
    {0, 0x00, 0, 0x55},
    // tick3: ★ 写 addr5 = 0xAA ⇒ 写拍禁读 ⇒ **清零 0**（不是保持 0x55）
    //        （若某侧回归「保持」⇒ 这里会是 0x55 ⇒ 红）
    {5, 0xAA, 1, 0x00},
    // tick4: we=0, addr=5 ⇒ 读到 0xAA
    {5, 0x00, 0, 0xAA},
    // tick5: we=0, addr=0 ⇒ 读到 0x55（addr0 没被覆盖）
    {0, 0x00, 0, 0x55},
};

// dual_port_ram：两端口**各自独立**；端口 A 写 addr2 = 0x33，端口 B 读 addr2
struct DualStep {
    uint64_t addr_a, din_a, we_a, addr_b, din_b, we_b;
    uint64_t dout_a, dout_b;
};
const DualStep kDualSteps[] = {
    // tick1: A 写 addr2=0x33（B 读 addr2，we_b=0 ⇒ 读被允许）
    //        ⇒ B 读到的是**上一拍**的内容（0）⇒ A 写拍禁读 ⇒ A 读 0
    {2, 0x33, 1, 2, 0x00, 0, 0x00, 0x00},
    // tick2: A 停写、B 读 addr2 ⇒ 读到 0x33；A 也读到 0x33
    {2, 0x00, 0, 2, 0x00, 0, 0x33, 0x33},
    // tick3: ★ B 写 addr4=0x77 ⇒ B 禁读 ⇒ **清零 0**（不是保持 0x33）·
    //        A 读 addr4 ⇒ 上一拍 0
    //        （若某侧回归「保持」⇒ B 会是 0x33 ⇒ 红）
    {4, 0x00, 0, 4, 0x77, 1, 0x00, 0x00},
    // tick4: 两端口都读 ⇒ A 读 addr4 = 0x77 · B 读 addr2 = 0x33
    {4, 0x00, 0, 2, 0x00, 0, 0x77, 0x33},
};

}  // namespace

// ───────────────────── pair 1：single-port RAM ─────────────────────
DUAL_BACKEND_TEST_PAIR(
    SingleRamDut,
    "single_port_ram - 写拍禁读 / 次拍读到新值（两后端逐拍一致）",
    "[chlib][memory][single_port_ram][dual][route_b]",
    [&]() {
        auto &io = dev.instance().io();
        for (const auto &tc : kRamSteps) {
            sim.set_input_value(io.addr, tc.addr);
            sim.set_input_value(io.din, tc.din);
            sim.set_input_value(io.we, tc.we);
            sim.tick();
            INFO("addr=" << tc.addr << " din=0x" << std::hex << tc.din
                        << " we=" << tc.we);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.dout)) ==
                    tc.dout);
        }
    });

// ───────────────────── pair 2：dual-port RAM ─────────────────────
DUAL_BACKEND_TEST_PAIR(
    DualRamDut,
    "dual_port_ram - 两端口独立 / 各自写拍禁读（两后端逐拍一致）",
    "[chlib][memory][dual_port_ram][dual][route_b]",
    [&]() {
        auto &io = dev.instance().io();
        for (const auto &tc : kDualSteps) {
            sim.set_input_value(io.addr_a, tc.addr_a);
            sim.set_input_value(io.din_a, tc.din_a);
            sim.set_input_value(io.we_a, tc.we_a);
            sim.set_input_value(io.addr_b, tc.addr_b);
            sim.set_input_value(io.din_b, tc.din_b);
            sim.set_input_value(io.we_b, tc.we_b);
            sim.tick();
            INFO("A: addr=" << tc.addr_a << " we=" << tc.we_a
                        << " | B: addr=" << tc.addr_b << " we=" << tc.we_b);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.dout_a)) ==
                    tc.dout_a);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.dout_b)) ==
                    tc.dout_b);
        }
    });
