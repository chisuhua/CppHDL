// ⚠️⚠️ **A5 批次 B**（2026-11-02 · change `dual-backend-parity-batch-b-2026-11`）
//
// 目的：把 `tests/test_bool_connection.cpp` 的 **4** 个用例转成**双后端 parity**
// （interpreter + verilator）⇒ ⇒ 「**连接语义**（`operator<<=`）在两条后端上
// 逐位一致」这条契约**第一次**有可执行判据。
//
// ⚠️ **形态 = 仓内既有 pattern**（`test_blt_debug_behavior` /
//    `test_axi_spi_behavior` / `test_rv32i_pipeline_behavior`）：
//    **新增 `_behavior` 文件** ⇒ ⇒ **原文件 MUST NOT 修改**
//    （保留 interpreter-only baseline ⇒ ⇒ 零回归风险）⇒ ⇒ DUT 在本文件里
//    **原样复制**（逐字）⇒ ⇒ 只把「怎么驱动 / 怎么读」换成 dual 口径。
//
// ⚠️ **期望值手推**：三个组合子的 `describe()` 都是**纯连线**（无运算）⇒ ⇒
//    输出 MUST **逐位等于**输入 ⇒ ⇒ 期望值 = 输入值（0 / 1）⇒ ⇒ 不引入
//    任何「抄仿真输出」的期望。
//
// ⚠️ **DUT 的 `clk_in` / `rst_in` 是**死端口**（原用例如此）：`ch_reg` 用的是
//    **隐式时钟** ⇒ ⇒ 保留它们是为了与原 DUT **逐字一致**（不是误用）。
//
// ⚠️ **判据（N2′）**：本文件的判据是
//    「**断言数(with verilator) == 2 × 断言数(interpreter-only)**」
//    ⇒ ⇒ 证明 verilator 半边**真的跑了**（缺席时只有 1 倍）。
#include "catch_amalgamated.hpp"
#include "ch.hpp"
#include "component.h"
#include "core/bool.h"
#include "core/reg.h"
#include "core/uint.h"
#include "dual_backend_fixture.h"
#include "simulator.h"

using namespace ch;
using namespace ch::core;

namespace {

// ── DUT 1：ch_bool ← ch_bool（原 `tests/test_bool_connection.cpp`）──
class BoolConnectionModule : public Component {
public:
    __io(ch_in<ch_bool> input_bool; ch_out<ch_bool> output_bool;)

    BoolConnectionModule(Component *parent = nullptr,
                          const std::string &name = "bool_conn")
        : Component(parent, name) {}

    void create_ports() override { new (this->io_storage_) io_type; }

    void describe() override {
        ch_bool temp_signal;
        temp_signal <<= io().input_bool;
        io().output_bool <<= temp_signal;
    }
};

// ── DUT 2：ch_bool ← ch_uint<1> ──
class BoolUintConnectionModule : public Component {
public:
    __io(ch_in<ch_uint<1>> input_uint1; ch_out<ch_bool> output_bool;)

    BoolUintConnectionModule(Component *parent = nullptr,
                              const std::string &name = "bool_uint_conn")
        : Component(parent, name) {}

    void create_ports() override { new (this->io_storage_) io_type; }

    void describe() override {
        ch_bool bool_signal;
        bool_signal <<= io().input_uint1;
        io().output_bool <<= bool_signal;
    }
};

// ── DUT 3：ch_bool ← ch_reg<ch_bool> ──
class BoolRegConnectionModule : public Component {
public:
    __io(ch_in<ch_bool> input_bool; ch_in<ch_bool> clk_in;
         ch_in<ch_bool> rst_in; ch_out<ch_bool> output_bool;)

    BoolRegConnectionModule(Component *parent = nullptr,
                             const std::string &name = "bool_reg_conn")
        : Component(parent, name) {}

    void create_ports() override { new (this->io_storage_) io_type; }

    void describe() override {
        ch_reg<ch_bool> bool_reg(0_b, "bool_reg");
        bool_reg <<= io().input_bool;
        io().output_bool <<= bool_reg;
    }
};

// ── DUT 4：ch_bool ← 字面量（**无输入** ⇒ 恒为 1）──
class BoolLiteralConnectionModule : public Component {
public:
    __io(ch_out<ch_bool> output_bool;)

    BoolLiteralConnectionModule(Component *parent = nullptr,
                                const std::string &name = "bool_lit_conn")
        : Component(parent, name) {}

    void create_ports() override { new (this->io_storage_) io_type; }

    void describe() override {
        ch_bool bool_signal(1_b);
        io().output_bool <<= bool_signal;
    }
};

}  // namespace

// ── 1 ──
DUAL_BACKEND_TEST_PAIR(
    BoolConnectionModule,
    "ch_bool operator<<= with ch_bool - 两后端逐位一致",
    "[bool][connection][operator][dual]",
    [&]() {
        auto &io = dev.instance().io();
        sim.set_input_value(io.input_bool, 0);
        sim.tick();
        REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.output_bool)) == 0u);

        sim.set_input_value(io.input_bool, 1);
        sim.tick();
        REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.output_bool)) == 1u);
    });

// ── 2 ──
DUAL_BACKEND_TEST_PAIR(
    BoolUintConnectionModule,
    "ch_bool operator<<= with ch_uint<1> - 两后端逐位一致",
    "[bool][connection][operator][ch_uint][dual]",
    [&]() {
        auto &io = dev.instance().io();
        sim.set_input_value(io.input_uint1, 0);
        sim.tick();
        REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.output_bool)) == 0u);

        sim.set_input_value(io.input_uint1, 1);
        sim.tick();
        REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.output_bool)) == 1u);
    });

// ── 3 ──
DUAL_BACKEND_TEST_PAIR(
    BoolRegConnectionModule,
    "ch_bool operator<<= with ch_reg<ch_bool> - 两后端逐位一致",
    "[bool][connection][operator][ch_reg][dual]",
    [&]() {
        auto &io = dev.instance().io();
        sim.set_input_value(io.input_bool, 0);
        sim.set_input_value(io.clk_in, 1);
        sim.set_input_value(io.rst_in, 0);
        sim.tick();
        REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.output_bool)) == 0u);

        sim.set_input_value(io.input_bool, 1);
        sim.tick();
        REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.output_bool)) == 1u);
    });

// ── 4 ──
DUAL_BACKEND_TEST_PAIR(
    BoolLiteralConnectionModule,
    "ch_bool operator<<= with literal - 两后端逐位一致（恒为 1）",
    "[bool][connection][operator][literal][dual]",
    [&]() {
        auto &io = dev.instance().io();
        sim.tick();
        REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.output_bool)) == 1u);
    });
