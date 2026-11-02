// ⚠️⚠️ **A5 批次 B**（2026-11-02 · change `dual-backend-parity-batch-b-2026-11`）
//
// 目的：把 `tests/test_operator_connection.cpp` 的 **4** 个用例转成**双后端
// parity**（interpreter + verilator）⇒ ⇒ 「**连接 + 组合逻辑**（`operator<<=`
// / `select` / `bits<>` / 寄存器推进）在两条后端上逐位一致」这条契约**第一次**
// 有可执行判据。
//
// ⚠️ **形态 = 仓内既有 pattern**：**新增 `_behavior` 文件** ⇒ ⇒
//    **原文件 MUST NOT 修改**（保留 interpreter-only baseline）⇒ ⇒ DUT 在本
//    文件里**原样复制**（逐字）⇒ ⇒ 只把「怎么驱动 / 怎么读」换成 dual 口径。
//
// ⚠️ **期望值手推**（逐条，禁抄仿真输出）：
//   · `TestModule`：纯连线 ⇒ 输出 == 输入（42）。
//   · `CounterModule`：`counter_reg <<= select(rst, 0, counter_reg + 1)`
//     ⇒ 复位拍（rst=1）后输出 0 ⇒ 释放复位后**每拍 +1** ⇒ 0 → 1 → 2。
//   · `ComplexConnectionModule`：`sum = a + b`（**5 位**）⇒ 输出取 `bits<3,0>`
//     ⇒ **低 4 位** ⇒ 15+15=30 ⇒ 30 & 0xF = **14**（溢出截断）；`max` 取
//     `select(a > b, a, b)` ⇒ 逐条手推见下表。
//   · `BoolConnectionModule`：bool → bool 与 bool → uint<1> ⇒ 输出 == 输入。
//
// ⚠️ **DUT 的 `clk` 端口是**死端口**（原用例如此）：`ch_reg` 用的是**隐式时钟**
//    ⇒ ⇒ 保留是为了与原 DUT **逐字一致**。
#include "catch_amalgamated.hpp"
#include "ch.hpp"
#include "component.h"
#include "core/bool.h"
#include "core/context.h"
#include "core/reg.h"
#include "core/uint.h"
#include "dual_backend_fixture.h"
#include "simulator.h"

#include <vector>

using namespace ch;
using namespace ch::core;

namespace {

// ── DUT 1：纯连线 ──
class TestModule : public Component {
public:
    __io(ch_in<ch_uint<8>> in_port; ch_out<ch_uint<8>> out_port;)

    TestModule(Component *parent = nullptr, const std::string &name = "test_mod")
        : Component(parent, name) {}

    void create_ports() override { new (this->io_storage_) io_type; }

    void describe() override {
        ch_uint<8> temp_sig = io().in_port;
        io().out_port <<= temp_sig;
    }
};

// ── DUT 2：寄存器 + `select` ──
class CounterModule : public Component {
public:
    __io(ch_in<ch_bool> clk; ch_in<ch_bool> rst;
         ch_out<ch_uint<8>> count_out;)

    CounterModule(Component *parent = nullptr, const std::string &name = "counter")
        : Component(parent, name) {}

    void create_ports() override { new (this->io_storage_) io_type; }

    void describe() override {
        ch_reg<ch_uint<8>> counter_reg(0_d, "counter");
        counter_reg <<= select(io().rst, 0_d, counter_reg + 1_d);
        io().count_out <<= counter_reg;
    }
};

// ── DUT 3：组合逻辑（`+` / `select` / `bits<>`）──
class ComplexConnectionModule : public Component {
public:
    __io(ch_in<ch_uint<4>> input_a; ch_in<ch_uint<4>> input_b;
         ch_out<ch_uint<4>> output_sum; ch_out<ch_uint<4>> output_max;)

    ComplexConnectionModule(Component *parent = nullptr,
                            const std::string &name = "complex_conn")
        : Component(parent, name) {}

    void create_ports() override { new (this->io_storage_) io_type; }

    void describe() override {
        ch_uint<5> sum = io().input_a + io().input_b;
        ch_uint<4> max_val =
            select(io().input_a > io().input_b, io().input_a, io().input_b);
        io().output_sum <<= bits<3, 0>(sum);
        io().output_max <<= max_val;
    }
};

// ── DUT 4：bool → bool / uint<1> ──
class BoolConnectionModule : public Component {
public:
    __io(ch_in<ch_bool> input_flag; ch_out<ch_bool> output_flag;
         ch_out<ch_uint<1>> output_uint1;)

    BoolConnectionModule(Component *parent = nullptr,
                          const std::string &name = "bool_conn")
        : Component(parent, name) {}

    void create_ports() override { new (this->io_storage_) io_type; }

    void describe() override {
        io().output_flag <<= io().input_flag;
        io().output_uint1 <<= io().input_flag;
    }
};

// ⚠️ **用例表 MUST 放在文件作用域**：`DUAL_BACKEND_TEST_PAIR` 的 body 参数里
//    **花括号内的逗号**会被预处理器当成**参数分隔**（只有**圆括号**才保护）
//    ⇒ ⇒ 宏会收到 22 个参数 ⇒ 编译失败。
struct ComplexCase {
    uint64_t a, b;
    uint64_t expected_sum, expected_max;
};

// ⚠️ **手推金标**（禁抄仿真输出）：sum = (a + b) & 0xF（5 位和取低 4 位）
//    · 2 + 3 = 5      ⇒ sum 5   ；2 < 3   ⇒ max 3
//    · 10 + 5 = 15    ⇒ sum 15  ；10 > 5  ⇒ max 10
//    · 0 + 7 = 7      ⇒ sum 7   ；0 < 7   ⇒ max 7
//    · 15 + 15 = 30   ⇒ 30 & 0xF = 14（**溢出截断**）；相等 ⇒ max 15
const std::vector<ComplexCase> kComplexCases = {
    {2, 3, 5, 3}, {10, 5, 15, 10}, {0, 7, 7, 7}, {15, 15, 14, 15}};

}  // namespace

// ── 1 ──
DUAL_BACKEND_TEST_PAIR(
    TestModule,
    "operator<<= - ch_uint 纯连线（输出 == 输入）",
    "[connection][operator][dual]",
    [&]() {
        auto &io = dev.instance().io();
        sim.set_input_value(io.in_port, 42);
        sim.tick();
        REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.out_port)) == 42u);
    });

// ── 2 ──
DUAL_BACKEND_TEST_PAIR(
    CounterModule,
    "operator<<= - 寄存器 + select（复位 ⇒ 0，释放后每拍 +1）",
    "[connection][register][operator][dual]",
    [&]() {
        auto &io = dev.instance().io();
        sim.set_input_value(io.rst, 1);
        sim.set_input_value(io.clk, 1);
        sim.tick();
        REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.count_out)) == 0u);

        sim.set_input_value(io.rst, 0);
        sim.tick();
        REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.count_out)) == 1u);

        sim.tick();
        REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.count_out)) == 2u);
    });

// ── 3 ──（手推金标表见文件头；`15+15` ⇒ 5 位和 = 30 ⇒ 低 4 位 = 14）
DUAL_BACKEND_TEST_PAIR(
    ComplexConnectionModule,
    "operator<<= - 组合逻辑（低 4 位和 / max）两后端逐位一致",
    "[connection][complex][operator][dual]",
    [&]() {
        auto &io = dev.instance().io();
        for (const auto &tc : kComplexCases) {
            sim.set_input_value(io.input_a, tc.a);
            sim.set_input_value(io.input_b, tc.b);
            sim.tick();
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.output_sum)) ==
                    tc.expected_sum);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.output_max)) ==
                    tc.expected_max);
        }
    });

// ── 4 ──
DUAL_BACKEND_TEST_PAIR(
    BoolConnectionModule,
    "operator<<= - ch_bool → bool / uint<1> 两后端逐位一致",
    "[connection][bool][operator][dual]",
    [&]() {
        auto &io = dev.instance().io();
        sim.set_input_value(io.input_flag, 0);
        sim.tick();
        REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.output_flag)) == 0u);
        REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.output_uint1)) == 0u);

        sim.set_input_value(io.input_flag, 1);
        sim.tick();
        REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.output_flag)) == 1u);
        REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.output_uint1)) == 1u);
    });
