// ⚠️⚠️ **A5 批次 B**（2026-11-02 · change `dual-backend-parity-batch-b-2026-11`）
//
// 目的：把 `tests/chlib/test_multi_if_acceptance.cpp` 里 **3** 个**运行期**用例
// 转成**双后端 parity**（interpreter + verilator）⇒ ⇒ 「`multi_if` 的**零扩展**
// 语义 + **直接赋给 `ch_out`** 的等价性」在两条后端上逐位一致。
//
// ⚠️ **形态 = 仓内既有 pattern**：**新增 `_behavior` 文件** ⇒ ⇒
//    **原文件 MUST NOT 修改**（保留 interpreter-only baseline）⇒ ⇒ DUT 在本
//    文件里**原样复制**（逐字）⇒ ⇒ 只把「怎么驱动 / 怎么读」换成 dual 口径。
//
// ⚠️ **哪些用例被剔除（以及为什么）**：
//   · 「a SINGLE branch MUST be rejected」「an integer literal that does NOT
//     fit MUST be rejected」「a NEGATIVE integer literal MUST be rejected」
//     —— 都是 **构图期**拒绝（`REQUIRE_THROWS_AS`）⇒ ⇒ 对两后端是**同一段
//     C++ 代码**（异常发生在建图期，与后端无关）⇒ ⇒ **dual 化零增量价值**
//     ⇒ ⇒ 不转（plan §4 的剔除项 ②）。
//
// ⚠️ **期望值手推**（禁抄仿真输出）：结果宽度 **9 位**；分支一 =
//    `ch_uint<8>(0xA5)` ⇒ **零扩展** ⇒ `0x0A5`（165）；分支二 =
//    `ch_uint<4>(0xF)` ⇒ **零扩展** ⇒ `0x00F`（15）。
//    ⚠️ 取「高位为 1」的形态 ⇒ 若实现误用 **sext** ⇒ 会得到 `0x1A5` / `0x01F`
//    ⇒ ⇒ **可判别**（这是 M5 变异钉住的那条）。
#include "catch_amalgamated.hpp"
#include "ch.hpp"
#include "chlib/if.h"
#include "component.h"
#include "core/context.h"
#include "device.h"
#include "dual_backend_fixture.h"
#include "simulator.h"

#include <cstdint>
#include <memory>

using namespace ch::core;
using namespace chlib;

namespace {

/// 条件用**编译期常量**（`SelectFirst`）—— 顶层 `ch_in` 在 `describe()` 内
/// **节点为 null**（未驱动）⇒ 用它当值源会 **SIGSEGV**（ADR-052 Non-Goal #4）
/// ⇒ ⇒ 用常量条件绕开；条件仍参与合成（`select` 仍被构造）⇒ 零扩展路径仍被
/// **真实**覆盖。
template <bool SelectFirst> class MultiIfDut : public ch::Component {
public:
    __io(ch_out<ch_uint<9>> wide;)

    MultiIfDut(ch::Component *parent = nullptr,
               const std::string &name = "multi_if_dut")
        : ch::Component(parent, name) {}

    // ⚠️ `__io` 宏只**声明**了存储（`io_storage_`）⇒ MUST 显式 placement-new
    //    出 `io_type` ⇒ 否则 `io()` 指向**未构造**的存储 ⇒ 端口节点为 null。
    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        ch_bool c(SelectFirst, "cond");
        ch_uint<9> v =
            if_then<ch_uint<9>>(c, ch_uint<8>(0xA5_h)).else_(ch_uint<4>(0xF_h));
        io().wide <<= v;
    }
};

/// 顶层端口的输出：**直接**由 `multi_if` 驱动（E1）与**先转 `ch_uint`**
/// 再驱动（对照）—— 两者 MUST **逐位相等**。
class MultiIfOutputDut : public ch::Component {
public:
    __io(ch_in<ch_bool> sel;
         ch_out<ch_uint<9>> direct;   // ← 直接赋 `multi_if`（E1）
         ch_out<ch_uint<9>> via_uint;) // ← 对照：先转 `ch_uint`

    MultiIfOutputDut(ch::Component *parent = nullptr,
                      const std::string &name = "multi_if_output_dut")
        : ch::Component(parent, name) {}
    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        io().direct = if_then<ch_uint<9>>(io().sel, ch_uint<8>(0xA5_h))
                          .else_(ch_uint<4>(0xF_h));
        ch_uint<9> v = if_then<ch_uint<9>>(io().sel, ch_uint<8>(0xA5_h))
                            .else_(ch_uint<4>(0xF_h));
        io().via_uint = v;
    }
};

}  // namespace

// ── 1 ──（条件为真 ⇒ 分支一 ⇒ 0x0A5）
DUAL_BACKEND_TEST_PAIR(
    MultiIfDut<true>,
    "multi_if - 条件为真 ⇒ 分支一零扩展到 9 位（0x0A5）",
    "[chlib][multi_if][adr052][dual]",
    [&]() {
        auto &io = dev.instance().io();
        sim.tick();
        REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.wide)) == 0x0A5u);
    });

// ── 2 ──（条件为假 ⇒ else 分支 ⇒ 0x00F）
DUAL_BACKEND_TEST_PAIR(
    MultiIfDut<false>,
    "multi_if - 条件为假 ⇒ else 分支零扩展到 9 位（0x00F）",
    "[chlib][multi_if][adr052][dual]",
    [&]() {
        auto &io = dev.instance().io();
        sim.tick();
        REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.wide)) == 0x00Fu);
    });

// ── 3 ──（E1：直接赋 `multi_if` ≡ 先转 `ch_uint`；E3：条件是端口）
DUAL_BACKEND_TEST_PAIR(
    MultiIfOutputDut,
    "multi_if - 直接赋给 ch_out 与「先转 ch_uint」逐位相等（sel 可为端口）",
    "[chlib][multi_if][adr052][e1][e3][dual]",
    [&]() {
        auto &io = dev.instance().io();
        sim.set_input_value(io.sel, 1u);
        sim.tick();
        REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.direct)) == 0x0A5u);
        REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.direct)) ==
                static_cast<uint64_t>(sim.get_port_value(io.via_uint)));

        sim.set_input_value(io.sel, 0u);
        sim.tick();
        REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.direct)) == 0x00Fu);
        REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.direct)) ==
                static_cast<uint64_t>(sim.get_port_value(io.via_uint)));
    });
