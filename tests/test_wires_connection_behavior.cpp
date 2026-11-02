// ⚠️⚠️ **A5 批次 B**（2026-11-02 · change `dual-backend-parity-batch-b-2026-11`）
//
// 目的：把 `tests/test_wires_connection.cpp` 的 **5** 个**可转换**用例转成
// **双后端 parity**（interpreter + verilator）⇒ ⇒ 「**wire 链 / 宽度扩展 /
// 寄存器捕获**在两条后端上逐位一致」这条契约第一次有可执行判据。
//
// ⚠️ **形态 = 仓内既有 pattern**：**新增 `_behavior` 文件** ⇒ ⇒
//    **原文件 MUST NOT 修改**（保留 interpreter-only baseline）⇒ ⇒ DUT 在本
//    文件里**原样复制**（逐字）⇒ ⇒ 只把「怎么驱动 / 怎么读」换成 dual 口径。
//
// ⚠️ **哪些用例被剔除（以及为什么）**：
//   · 「Basic ch_uint connection」—— **裸 context** 的**节点身份**断言
//     （`impl()` 指针相等）⇒ ⇒ 无 DUT ⇒ ⇒ 双后端宏**不适用**
//     （macro 的 R6 排除清单明列「bare-context tests」）⇒ ⇒ 不转；
//   · 「Empty constructed ch_uint behavior」—— **空节点语义**
//     （`impl() == nullptr`）⇒ ⇒ 是**解释器专属**的结构语义
//     ⇒ ⇒ 不转（plan §4 的剔除项 ⑤）。
//
// ⚠️ **期望值手推**（逐条，禁抄仿真输出）：
//   · wire 链（4 级）：输出 == 输入（纯连线）；
//   · 多级 wire + `+`：`ch_uint<4>` ⇒ 4 位 ⇒ **溢出截断** ⇒ 10+10=20 ⇒ 20&0xF=**4**；
//   · wire → `ch_reg`：每拍捕获当拍输入 ⇒ 10 / 20 / 30 / 40；
//   · 4 位 → 6 位 → 8 位：**零扩展** ⇒ 输出 == 输入。
//
// ⚠️ **用例表 MUST 放在文件作用域**：`DUAL_BACKEND_TEST_PAIR` 的 body 参数里
//    **花括号内的逗号**会被预处理器当成**参数分隔**（只有**圆括号**才保护）。
#include "catch_amalgamated.hpp"
#include "ch.hpp"
#include "component.h"
#include "core/context.h"
#include "core/reg.h"
#include "core/uint.h"
#include "dual_backend_fixture.h"
#include "simulator.h"

#include <tuple>
#include <vector>

using namespace ch;
using namespace ch::core;

namespace {

// ── DUT 1：wire → wire → 输出 ──
class WireConnectionModule : public Component {
public:
    __io(ch_in<ch_uint<8>> input_port; ch_out<ch_uint<8>> output_port;);

    WireConnectionModule(Component *parent = nullptr,
                         const std::string &name = "wire_conn")
        : Component(parent, name) {}

    void create_ports() override { new (this->io_storage_) io_type; }

    void describe() override {
        ch_uint<8> internal_wire;
        internal_wire <<= io().input_port;
        io().output_port <<= internal_wire;
    }
};

// ── DUT 2：多级 wire + `+`（4 位 ⇒ 溢出截断）──
class MultiWireConnectionModule : public Component {
public:
    __io(ch_in<ch_uint<4>> input_a; ch_in<ch_uint<4>> input_b;
         ch_out<ch_uint<4>> output_result;);

    MultiWireConnectionModule(Component *parent = nullptr,
                              const std::string &name = "multi_wire")
        : Component(parent, name) {}

    void create_ports() override { new (this->io_storage_) io_type; }

    void describe() override {
        ch_uint<4> wire1, wire2;
        ch_uint<4> wire3;
        wire1 <<= io().input_a;
        wire2 <<= io().input_b;
        wire3 <<= wire1 + wire2;
        io().output_result <<= wire3;
    }
};

// ── DUT 3：wire → `ch_reg` → 输出 ──
class WireToRegConnectionModule : public Component {
public:
    __io(ch_in<ch_uint<8>> input_data; ch_out<ch_uint<8>> output_data;);

    WireToRegConnectionModule(Component *parent = nullptr,
                              const std::string &name = "wire_to_reg")
        : Component(parent, name) {}

    void create_ports() override { new (this->io_storage_) io_type; }

    void describe() override {
        ch_reg<ch_uint<8>> data_reg(0_d, "data_reg");
        ch_uint<8> internal_wire;
        internal_wire <<= io().input_data;
        data_reg <<= internal_wire;
        io().output_data <<= data_reg;
    }
};

// ── DUT 4：4 级 wire 链 ──
class WireChainModule : public Component {
public:
    __io(ch_in<ch_uint<8>> start_signal; ch_out<ch_uint<8>> end_signal;);

    WireChainModule(Component *parent = nullptr,
                    const std::string &name = "wire_chain")
        : Component(parent, name) {}

    void create_ports() override { new (this->io_storage_) io_type; }

    void describe() override {
        ch_uint<8> w1, w2, w3, w4;
        w1 <<= io().start_signal;
        w2 <<= w1;
        w3 <<= w2;
        w4 <<= w3;
        io().end_signal <<= w4;
    }
};

// ── DUT 5：4 位 → 6 位 → 8 位（零扩展）──
class WidthConversionModule : public Component {
public:
    __io(ch_in<ch_uint<4>> input_4bit; ch_out<ch_uint<8>> output_8bit;);

    WidthConversionModule(Component *parent = nullptr,
                          const std::string &name = "width_conv")
        : Component(parent, name) {}

    void create_ports() override { new (this->io_storage_) io_type; }

    void describe() override {
        ch_uint<6> internal_6bit;
        ch_uint<8> internal_8bit;
        internal_6bit <<= io().input_4bit;
        internal_8bit <<= internal_6bit;
        io().output_8bit <<= internal_8bit;
    }
};

// ⚠️ **手推金标**（禁抄仿真输出）：4 位加法 ⇒ **溢出截断**（mod 16）
const std::vector<std::tuple<uint64_t, uint64_t, uint64_t>> kAddCases = {
    {1, 2, 3},      // 1+2 = 3
    {5, 3, 8},      // 5+3 = 8
    {7, 8, 15},     // 7+8 = 15（4 位最大值）
    {10, 10, 4},    // 10+10 = 20 ⇒ 超出 4 位 ⇒ 20 % 16 = 4
};

const std::vector<uint64_t> kByteValues = {0, 1, 42, 100, 255};
const std::vector<uint64_t> kRegValues = {10, 20, 30, 40};
const std::vector<uint64_t> kChainValues = {0, 1, 55, 128, 255};

}  // namespace

// ── 1 ──
DUAL_BACKEND_TEST_PAIR(
    WireConnectionModule,
    "wires - wire → wire → 输出（输出 == 输入）两后端逐位一致",
    "[wires][connection][module][dual]",
    [&]() {
        auto &io = dev.instance().io();
        for (uint64_t v : kByteValues) {
            sim.set_input_value(io.input_port, v);
            sim.tick();
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.output_port)) ==
                    v);
        }
    });

// ── 2 ──
DUAL_BACKEND_TEST_PAIR(
    MultiWireConnectionModule,
    "wires - 多级 wire + 加法（4 位溢出截断）两后端逐位一致",
    "[wires][connection][multi][dual]",
    [&]() {
        auto &io = dev.instance().io();
        for (const auto &tc : kAddCases) {
            sim.set_input_value(io.input_a, std::get<0>(tc));
            sim.set_input_value(io.input_b, std::get<1>(tc));
            sim.tick();
            REQUIRE(static_cast<uint64_t>(
                        sim.get_port_value(io.output_result)) ==
                    std::get<2>(tc));
        }
    });

// ── 3 ──
DUAL_BACKEND_TEST_PAIR(
    WireToRegConnectionModule,
    "wires - wire → 寄存器（每拍捕获当拍输入）两后端逐位一致",
    "[wires][connection][reg][dual]",
    [&]() {
        auto &io = dev.instance().io();
        for (uint64_t v : kRegValues) {
            sim.set_input_value(io.input_data, v);
            sim.tick();
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.output_data)) ==
                    v);
        }
    });

// ── 4 ──
DUAL_BACKEND_TEST_PAIR(
    WireChainModule,
    "wires - 4 级 wire 链（输出 == 输入）两后端逐位一致",
    "[wires][connection][chain][dual]",
    [&]() {
        auto &io = dev.instance().io();
        for (uint64_t v : kChainValues) {
            sim.set_input_value(io.start_signal, v);
            sim.tick();
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.end_signal)) ==
                    v);
        }
    });

// ── 5 ──
DUAL_BACKEND_TEST_PAIR(
    WidthConversionModule,
    "wires - 4 位 → 8 位零扩展（输出 == 输入）两后端逐位一致",
    "[wires][connection][width][dual]",
    [&]() {
        auto &io = dev.instance().io();
        for (uint64_t i = 0; i <= 15; ++i) {
            sim.set_input_value(io.input_4bit, i);
            sim.tick();
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.output_8bit)) ==
                    i);
        }
    });
