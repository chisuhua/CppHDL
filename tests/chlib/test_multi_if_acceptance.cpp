/**
 * @file test_multi_if_acceptance.cpp
 * @brief `chlib::multi_if` 的**接受面**契约（**ADR-052**）—— 运行期部分
 *
 * ## 契约
 *
 * `multi_if<T>` 的接受面里**每一格** MUST 要么**语义正确**、要么**响亮拒绝**
 * ⇒ MUST NOT 存在第三态（「编译通过但语义错误」）。ADR-052 裁决了五种形态：
 *
 * | # | 形态 | 处置 | 覆盖 |
 * |---|---|---|---|
 * | **A** | 首分支窄、后续宽 | **编译期拒绝** | `scripts/verify_multi_if_width.sh` |
 * | **B** | **单分支**（无 `elif` / `else_`） | **建图期拒绝** | **本文件** |
 * | **C** | `T` 推导成 port | **归一化** | `scripts/verify_multi_if_width.sh` |
 * | **D** | `T` = 纯算术 | **编译期拒绝** | `scripts/verify_multi_if_width.sh` |
 * | **E** | 显式 `T` 过窄 | **编译期拒绝** | `scripts/verify_multi_if_width.sh` |
 *
 * ## 为什么**运行期**判据 MUST 存在
 *
 * ⚠️ **形态 B（单分支）无法用编译期判据捕获** —— 它**编译通过**，只是条件被
 * 静默忽略（`operator T()` 的循环从 `rbegin() + 1` 起 ⇒ 对单分支是**空循环**
 * ⇒ 生成的 Verilog 里**零 mux** ⇒ 条件彻底不参与）⇒ ⇒ 只能靠**转换点抛异常**
 * 来拒绝 ⇒ ⇒ 必须有运行期用例钉住它。
 *
 * ⚠️ **整数字面量的值域检查也只在运行期** —— 字面量的**值**是运行期信息
 * （`ch_width_v<int>` = 32 是**占位语义** ⇒ 类型宽度**不能**用来判）。
 *
 * ## 判别力
 *
 * - **M3** 去掉值域检查 ⇒ `T_range` 不再抛 ⇒ 红；
 * - **M4** 删掉 `branches_.size() < 2` 检查 ⇒ `T_B` 不再抛 ⇒ 红；
 * - **M5** `zext` 换 `sext` ⇒ 零扩展金标 `0x0A5` → `0x1A5` ⇒ 红。
 */

#include "catch_amalgamated.hpp"
#include "ch.hpp"
#include "chlib/if.h"
#include "component.h"
#include "core/context.h"
#include "device.h"
#include "simulator.h"
#include <cstdio>
#include <memory>

using namespace ch::core;
using namespace chlib;

namespace {

/// 承载结果输出的组件。
///
/// ⚠️ **条件用编译期常量**（`SelectFirst`）而不是顶层输入端口 ——
///    顶层 `ch_in` 在 `describe()` 内**节点为 null**（未驱动）⇒ 用它当值源会
///    **SIGSEGV**（ADR-052 Non-Goal #4 已登记该限制）⇒ ⇒ 用常量条件绕开。
///    ⚠️ 条件仍会参与合成（`select` 仍被构造）⇒ ⇒ 零扩展路径仍被**真实**覆盖。
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
        // ⚠️ 手推金标（**不**抄仿真输出）：
        //    结果宽度 **9 位**；分支一 = `ch_uint<8>(0xA5)` ⇒ **零扩展** ⇒ `0x0A5`；
        //    分支二 = `ch_uint<4>(0xF)`   ⇒ **零扩展** ⇒ `0x00F`。
        //    ⚠️ 取「高位为 1」的形态 ⇒ 若实现误用 **sext** ⇒ 会得到
        //    `0x1A5` / `0x01F` ⇒ **可判别**。
        ch_uint<9> v =
            if_then<ch_uint<9>>(c, ch_uint<8>(0xA5_h)).else_(ch_uint<4>(0xF_h));
        io().wide <<= v;
    }
};

/// 读一个输出端口的当前值
uint64_t read_port(ch::Simulator &sim, lnodeimpl *ln) {
    auto it = sim.data_map().find(ln->id());
    return it == sim.data_map().end() ? UINT64_MAX
                                       : static_cast<uint64_t>(it->second);
}

} // namespace

// ===========================================================================
// 形态 B：单分支 ⇒ MUST 拒绝（**建图期**）
// ===========================================================================

TEST_CASE("multi_if: a SINGLE branch MUST be rejected (its condition can never "
          "participate)",
          "[chlib][multi_if][adr052]") {
    auto ctx = std::make_unique<ch::core::context>("test_multi_if_single");
    ch::core::ctx_swap swap(ctx.get());

    ch_bool c(true, "c");
    ch_uint<8> a(0xA5_h);

    // ⚠️ 单分支 ⇒ 转换点 MUST 抛异常（**不是**静默返回该分支值）
    REQUIRE_THROWS_AS(
        [&] {
            ch_uint<8> v = if_then(c, a);
            (void)v;
        }(),
        std::runtime_error);
}

// ===========================================================================
// 整数字面量的**值域**检查（建图期）
// ===========================================================================

TEST_CASE("multi_if: an integer literal that does NOT fit the result width MUST "
          "be rejected",
          "[chlib][multi_if][adr052]") {
    auto ctx = std::make_unique<ch::core::context>("test_multi_if_range");
    ch::core::ctx_swap swap(ctx.get());

    ch_bool c(true, "c");
    ch_uint<8> a(0x11_h);

    // ⚠️ `256` **放不进** 8 位 ⇒ MUST 拒绝（今天：`ch_uint<8>(256)` 会
    //    **静默截断成 0** ⇒ 逻辑错误）
    REQUIRE_THROWS_AS(
        [&] {
            ch_uint<8> v = if_then(c, a).else_(256);
            (void)v;
        }(),
        std::runtime_error);
}

TEST_CASE("multi_if: a NEGATIVE integer literal MUST be rejected",
          "[chlib][multi_if][adr052]") {
    auto ctx = std::make_unique<ch::core::context>("test_multi_if_negative");
    ch::core::ctx_swap swap(ctx.get());

    ch_bool c(true, "c");
    ch_uint<8> a(0x11_h);

    REQUIRE_THROWS_AS(
        [&] {
            ch_uint<8> v = if_then(c, a).else_(-1);
            (void)v;
        }(),
        std::runtime_error);
}

TEST_CASE("multi_if: an integer literal that DOES fit MUST be accepted",
          "[chlib][multi_if][adr052]") {
    auto ctx = std::make_unique<ch::core::context>("test_multi_if_fit");
    ch::core::ctx_swap swap(ctx.get());

    ch_bool c(true, "c");
    ch_uint<8> a(0x11_h);

    // ⚠️ `1` 放得进 8 位 ⇒ MUST 通过（**不得**误拒 —— `ch_width_v<int>` = 32
    //    是**占位语义** ⇒ 用类型宽度判会误拒这个**最常见**的写法）
    REQUIRE_NOTHROW(
        [&] {
            ch_uint<8> v = if_then(c, a).else_(1);
            (void)v;
        }());
}

// ===========================================================================
// 零扩展的**运行期**正确性（手推金标，**不**抄仿真输出）
// ===========================================================================

TEST_CASE("multi_if: a NARROW branch MUST be ZERO-extended into the wider "
          "result",
          "[chlib][multi_if][adr052]") {
    {
        ch::ch_device<MultiIfDut<true>> dev;
        ch::Simulator sim(dev.context());
        // ⚠️ 手推：条件为真 ⇒ 取分支一 ⇒ `ch_uint<8>(0xA5)` 零扩展到 9 位
        //    ⇒ **0x0A5** = 165（若误用 sext ⇒ 0x1A5 = 421 ⇒ 可判别）
        sim.tick();
        REQUIRE(read_port(sim, dev.instance().io().wide.impl()) == 0x0A5u);
    }
    {
        ch::ch_device<MultiIfDut<false>> dev;
        ch::Simulator sim(dev.context());
        // ⚠️ 手推：条件为假 ⇒ 取 `else_` 分支 ⇒ `ch_uint<4>(0xF)` 零扩展
        //    ⇒ **0x00F** = 15（若误用 sext ⇒ 0x01F = 31 ⇒ 可判别）
        sim.tick();
        REQUIRE(read_port(sim, dev.instance().io().wide.impl()) == 0x00Fu);
    }
}
