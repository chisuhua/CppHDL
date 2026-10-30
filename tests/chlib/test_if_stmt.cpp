/**
 * @file test_if_stmt.cpp
 * @brief Unit tests for chlib::conditional_block (if_stmt.h) — **守卫贡献**语义
 *
 * ⚠️ **ADR-051**（`docs/adr/ADR-051-chlib-conditional-semantics.md`）落地后的
 *   重写版本。**重写原因**：原文件的用例**钉住了坏行为** ——
 *   「conditional_reg_assign inside if block forwards to next」用例的注释明写
 *   「end() sets finalized_=true BEFORE walking the bodies … This test pins
 *   down that current (deferred-select-tree) behavior so future changes are
 *   noticed」⇒ ⇒ 任何**正确**的实现都会让它变红 ⇒ ⇒ 它是「**假绿钉子**」。
 *   另两个用例只断言 **C++ 标志**（`body_executed`）⇒ **零硬件覆盖**。
 *
 * ⇒ 本文件现在：
 *   ① **移除**钉住坏行为的用例；
 *   ② 断言**硬件级**行为（条件为假 ⇒ 寄存器**不被写**）⇒ 而非 C++ 标志；
 *   ③ 补**编译期**断言（被移除 / 被改名的 API **不存在**）；
 *   ④ 补 ADR-051 §5 的其余判据（先匹配者胜 / 保持 / 输出缺默认 ⇒ 抛异常 /
 *      嵌套 / 条件来自 `ch_in<ch_bool>`）。
 *
 * ## 语义速览（ADR-051）
 *
 * - 条件块内的赋值 MUST 经 `conditional_assign(target).next() = value` 捕获；
 * - 各分支的候选值被合成为**一条** `select` 链 ⇒ **先匹配者胜**；
 * - **寄存器**无 `else_` ⇒ 未匹配时**保持**；**输出端口** MUST 有 `else_`；
 * - `end()` 遍历**所有**分支体（**捕获**语义）⇒ 分支体在**建图期**执行。
 */

#include "catch_amalgamated.hpp"
#include "chlib/if_stmt.h"
#include "core/bool.h"
#include "core/context.h"
#include "core/literal.h"
#include "core/reg.h"
#include "core/uint.h"
#include "device.h"
#include "simulator.h"
#include <memory>
#include <type_traits>

using namespace ch::core;
using namespace chlib;

namespace {

// RAII helper: build a context + ctx_swap for a test body. Mirrors the
// pattern used in test_logic.cpp / test_combinational.cpp.
struct test_ctx {
    std::unique_ptr<ch::core::context> ctx_;
    ch::core::ctx_swap swap_;

    explicit test_ctx(const char *name)
        : ctx_(std::make_unique<ch::core::context>(name)), swap_(ctx_.get()) {}
};

/// 读取一个顶层输出端口的当前值
uint64_t read_port(ch::Simulator &sim, ch::core::lnodeimpl *ln) {
    auto it = sim.data_map().find(ln->id());
    if (it == sim.data_map().end()) {
        return UINT64_MAX;
    }
    return static_cast<uint64_t>(it->second);
}

// ===========================================================================
// 硬件 DUT —— 把条件化的目标暴露成顶层 `ch_out`（verilator 也可观测）
// ===========================================================================

/// 单分支：条件为真 ⇒ 写 7；否则（`else_`）⇒ 写 9
class CondAssignDut : public ch::Component {
public:
    __io(ch_in<ch_bool> en; ch_out<ch_uint<8>> q;);

    CondAssignDut(ch::Component *parent = nullptr,
                  const std::string &name = "cond_assign_dut")
        : ch::Component(parent, name) {}
    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        ch_reg<ch_uint<8>> r(0_d, "r");
        _if(io().en)
            .then([&] { conditional_assign(r).next() = ch_uint<8>(7_d); })
            .else_()
            .then([&] { conditional_assign(r).next() = ch_uint<8>(9_d); })
            .end();
        io().q = r;
    }
};

/// 多分支：**先匹配者胜**（`a` 与 `b` 同时为真 ⇒ 取 `a` 的值）
class PriorityDut : public ch::Component {
public:
    __io(ch_in<ch_bool> a; ch_in<ch_bool> b; ch_out<ch_uint<8>> q;);

    PriorityDut(ch::Component *parent = nullptr,
                const std::string &name = "priority_dut")
        : ch::Component(parent, name) {}
    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        ch_reg<ch_uint<8>> r(0_d, "r");
        // ⚠️ **无** `else_` ⇒ 未匹配时**保持**
        _if(io().a)
            .then([&] { conditional_assign(r).next() = ch_uint<8>(1_d); })
            .elif(io().b)
            .then([&] { conditional_assign(r).next() = ch_uint<8>(2_d); })
            .end();
        io().q = r;
    }
};

/// 嵌套：内层 `_if` MUST 先合成**一条**表达式，再作为外层该分支的候选值
class NestedDut : public ch::Component {
public:
    __io(ch_in<ch_bool> a; ch_in<ch_bool> b; ch_out<ch_uint<8>> q;);

    NestedDut(ch::Component *parent = nullptr,
              const std::string &name = "nested_dut")
        : ch::Component(parent, name) {}
    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        ch_reg<ch_uint<8>> r(0_d, "r");
        _if(io().a)
            .then([&] {
                _if(io().b)
                    .then([&] { conditional_assign(r).next() = ch_uint<8>(11_d); })
                    .else_()
                    .then([&] { conditional_assign(r).next() = ch_uint<8>(12_d); })
                    .end();
            })
            .else_()
            .then([&] { conditional_assign(r).next() = ch_uint<8>(13_d); })
            .end();
        io().q = r;
    }
};

/// 输出端口的条件化（**必须**有 `else_` ⇒ 显式默认）
class CondOutDut : public ch::Component {
public:
    __io(ch_in<ch_bool> en; ch_out<ch_uint<8>> q;);

    CondOutDut(ch::Component *parent = nullptr,
               const std::string &name = "cond_out_dut")
        : ch::Component(parent, name) {}
    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        _if(io().en)
            .then([&] { conditional_assign(io().q).next() = ch_uint<8>(7_d); })
            .else_()
            .then([&] { conditional_assign(io().q).next() = ch_uint<8>(9_d); })
            .end();
    }
};

/// 输出端口**缺** `else_` ⇒ MUST 在**构建期**被拒绝（否则组合环）
class CondOutNoElseDut : public ch::Component {
public:
    __io(ch_in<ch_bool> en; ch_out<ch_uint<8>> q;);

    CondOutNoElseDut(ch::Component *parent = nullptr,
                     const std::string &name = "cond_out_no_else_dut")
        : ch::Component(parent, name) {}
    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        _if(io().en)
            .then([&] { conditional_assign(io().q).next() = ch_uint<8>(7_d); })
            .end();
    }
};

// ===========================================================================
// 编译期断言：被移除 / 被改名的 API MUST NOT 再存在
// ===========================================================================

/// `conditional_assign` MUST **存在**（SFINAE 可检测）
template <typename T, typename = void>
struct has_conditional_assign : std::false_type {};
template <typename T>
struct has_conditional_assign<T, std::void_t<decltype(
                      chlib::conditional_assign(
                          std::declval<ch_reg<ch_uint<8>> &>()))>>
    : std::true_type {};

/// ⚠️ **被移除**的自由函数（`conditional_reg_assign` / `priority_if` /
///    `priority_if_then`）**无法**用 SFINAE 断言「不存在」—— 名称查找失败
///    **不是** deduction failure ⇒ 是**硬错误** ⇒ 整个 TU 编译失败
///    （与 ADR-050 实测② 的 `= delete` 同型）⇒ ⇒ 改由**静态护栏**
///    （`scripts/verify_if_stmt_wiring.sh` 的 R2「被移除的符号 MUST NOT 出现」）
///    承担。

/// **裸 literal** 的条件赋值（回归点：修复前 `0xA5_h` **编译不过**）
///
/// ⚠️ 既有 DUT（`CondAssignDut` 等）全都写成 `ch_uint<8>(7_d)` ——
///    **显式包裹** ⇒ ⇒ 那正是**绕过**缺陷的写法（缺陷期只有这种能编译）
///    ⇒ ⇒ 本 DUT **刻意**用**裸 literal** ⇒ 它是回归的**编译期**证据。
class ConditionalLiteralDut : public ch::Component {
public:
    __io(ch_in<ch_bool> en; ch_out<ch_uint<8>> q;);

    ConditionalLiteralDut(ch::Component *parent = nullptr,
                          const std::string &name = "conditional_literal_dut")
        : ch::Component(parent, name) {}
    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        ch_reg<ch_uint<8>> r(0_d, "r");
        _if(io().en)
            .then([&] { conditional_assign(r).next() = 0xA5_h; })  // ⚠️ 裸 literal
            .else_()
            .then([&] { conditional_assign(r).next() = 0x5A_h; })  // ⚠️ 裸 literal
            .end();
        io().q = r;
    }
};

/// **块外**的裸 literal 直写（走 `assign()` 的 else 分支 ⇒ 修复点所在）
class ConditionalLiteralOutsideDut : public ch::Component {
public:
    __io(ch_out<ch_uint<8>> q;);

    ConditionalLiteralOutsideDut(
        ch::Component *parent = nullptr,
        const std::string &name = "conditional_literal_outside_dut")
        : ch::Component(parent, name) {}
    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        ch_reg<ch_uint<8>> r(0_d, "r");
        // ⚠️ **不在**任何条件块内 ⇒ 走 `assign()` 的 else 分支 ⇒ **直写**
        //    ⇒ ⇒ 这是修复点**本身**（该分支此前对 literal 编译不过）。
        conditional_assign(r).next() = 0x5A_h;
        io().q = r;
    }
};

} // namespace

// ===========================================================================
// 条件化语义（硬件级）
// ===========================================================================

TEST_CASE("if_stmt: condition false => the register is NOT written",
          "[chlib][if_stmt][adr051]") {
    // ⚠️ **这条是本文件的核心判别用例**：原实现里 `end()` 在遍历分支体**之前**
    //    置 `finalized_` ⇒ 捕获期间 `is_active()` 恒 false ⇒ 赋值走「直接写入」
    //    ⇒ ⇒ **条件被彻底忽略** ⇒ 本用例在旧实现下**必然失败**。
    ch::ch_device<CondAssignDut> dev;
    ch::Simulator sim(dev.context());
    auto *q = dev.instance().io().q.impl();

    // 条件为**假** ⇒ MUST 取 `else_` 的值（9），而**不是** `then` 的值（7）
    sim.set_input_value(dev.instance().io().en, 0u);
    sim.tick(1);
    REQUIRE(read_port(sim, q) == 9u);

    // 条件为**真** ⇒ MUST 取 `then` 的值（7）
    sim.set_input_value(dev.instance().io().en, 1u);
    sim.tick(1);
    REQUIRE(read_port(sim, q) == 7u);

    // 再回到**假** ⇒ MUST 回到 9（⇒ 证明条件是**真的**被求值）
    sim.set_input_value(dev.instance().io().en, 0u);
    sim.tick(1);
    REQUIRE(read_port(sim, q) == 9u);
}

TEST_CASE("if_stmt: multiple branches => the first match wins",
          "[chlib][if_stmt][adr051]") {
    ch::ch_device<PriorityDut> dev;
    ch::Simulator sim(dev.context());
    auto *q = dev.instance().io().q.impl();

    // `a` 与 `b` **同时**为真 ⇒ MUST 取 `a` 的值（1）⇒「先匹配者胜」
    sim.set_input_value(dev.instance().io().a, 1u);
    sim.set_input_value(dev.instance().io().b, 1u);
    sim.tick(1);
    REQUIRE(read_port(sim, q) == 1u);

    // 只有 `b` 为真 ⇒ 取 2
    sim.set_input_value(dev.instance().io().a, 0u);
    sim.tick(1);
    REQUIRE(read_port(sim, q) == 2u);
}

TEST_CASE("if_stmt: a register without else_ keeps its value when unmatched",
          "[chlib][if_stmt][adr051]") {
    ch::ch_device<PriorityDut> dev;
    ch::Simulator sim(dev.context());
    auto *q = dev.instance().io().q.impl();

    // 两个条件都为**假** ⇒ 无 `else_` ⇒ MUST **保持**（初值 0）
    sim.set_input_value(dev.instance().io().a, 0u);
    sim.set_input_value(dev.instance().io().b, 0u);
    sim.tick(1);
    REQUIRE(read_port(sim, q) == 0u);

    // 命中 `a` ⇒ 变成 1
    sim.set_input_value(dev.instance().io().a, 1u);
    sim.tick(1);
    REQUIRE(read_port(sim, q) == 1u);

    // 再次全部为**假** ⇒ MUST **保持** 1（而不是回到 0）⇒ 证明「保持」而非「清零」
    sim.set_input_value(dev.instance().io().a, 0u);
    sim.tick(1);
    REQUIRE(read_port(sim, q) == 1u);
}

TEST_CASE("if_stmt: nested blocks compose inner-first",
          "[chlib][if_stmt][adr051]") {
    ch::ch_device<NestedDut> dev;
    ch::Simulator sim(dev.context());
    auto *q = dev.instance().io().q.impl();

    // a=1, b=1 ⇒ 内层命中 ⇒ 11
    sim.set_input_value(dev.instance().io().a, 1u);
    sim.set_input_value(dev.instance().io().b, 1u);
    sim.tick(1);
    REQUIRE(read_port(sim, q) == 11u);

    // a=1, b=0 ⇒ 内层走 else ⇒ 12
    sim.set_input_value(dev.instance().io().b, 0u);
    sim.tick(1);
    REQUIRE(read_port(sim, q) == 12u);

    // a=0 ⇒ 外层走 else ⇒ 13（⇒ 内层**不**生效 ⇒ 嵌套隔离正确）
    sim.set_input_value(dev.instance().io().a, 0u);
    sim.tick(1);
    REQUIRE(read_port(sim, q) == 13u);
}

TEST_CASE("if_stmt: an output port can be conditionally assigned",
          "[chlib][if_stmt][adr051]") {
    ch::ch_device<CondOutDut> dev;
    ch::Simulator sim(dev.context());
    auto *q = dev.instance().io().q.impl();

    sim.set_input_value(dev.instance().io().en, 1u);
    sim.tick(1);
    REQUIRE(read_port(sim, q) == 7u);

    sim.set_input_value(dev.instance().io().en, 0u);
    sim.tick(1);
    REQUIRE(read_port(sim, q) == 9u);
}

TEST_CASE("if_stmt: an output without else_ MUST be rejected at build time",
          "[chlib][if_stmt][adr051][negative]") {
    // ⚠️ 输出的「保持」= 引用自身 = **组合环** ⇒ 拓扑排序 CHFATAL（**难**诊断）
    //    ⇒ ⇒ MUST 在**构建期**给出可行动的错误。
    REQUIRE_THROWS_AS(
        [] {
            ch::ch_device<CondOutNoElseDut> dev;
            ch::Simulator sim(dev.context());
            sim.tick(1);
        }(),
        std::runtime_error);
}

// ===========================================================================
// 捕获语义（`end()` 遍历**所有**分支体）
// ===========================================================================

TEST_CASE("if_stmt: end() walks ALL branch bodies (capture semantics)",
          "[chlib][if_stmt][adr051]") {
    // ⚠️ `end()` MUST 遍历**所有**分支体以**收集候选值** ⇒ ⇒ 分支体在
    //    **建图期**执行 ⇒ ⇒ 这是**捕获**语义，不是「条件成立才执行」。
    test_ctx ctx("test_if_stmt_capture");

    int if_hits = 0;
    int elif_hits = 0;
    int else_hits = 0;

    _if(ch_bool(true))
        .then([&if_hits] { ++if_hits; })
        .elif(ch_bool(false))
        .then([&elif_hits] { ++elif_hits; })
        .else_()
        .then([&else_hits] { ++else_hits; })
        .end();

    REQUIRE(if_hits == 1);
    REQUIRE(elif_hits == 1);
    REQUIRE(else_hits == 1);
}

// ===========================================================================
// validate() 错误
// ===========================================================================

TEST_CASE("if_stmt: validate throws when then body is missing",
          "[chlib][if_stmt][validate]") {
    test_ctx ctx("test_if_stmt_no_body");

    bool caught = false;
    try {
        _if(ch_bool(true)).end();
    } catch (const std::runtime_error &) {
        caught = true;
    }
    REQUIRE(caught);
}

TEST_CASE("if_stmt: validate throws when an elif branch has no body",
          "[chlib][if_stmt][validate]") {
    test_ctx ctx("test_if_stmt_elif_no_body");

    bool caught = false;
    try {
        _if(ch_bool(true))
            .then([] {})
            .elif(ch_bool(false))
            .end();
    } catch (const std::runtime_error &) {
        caught = true;
    }
    REQUIRE(caught);
}

TEST_CASE("if_stmt: elif after else_ throws", "[chlib][if_stmt][validate]") {
    test_ctx ctx("test_if_stmt_elif_after_else");

    bool caught = false;
    try {
        _if(ch_bool(true))
            .then([] {})
            .else_()
            .then([] {})
            .elif(ch_bool(false));
    } catch (const std::runtime_error &) {
        caught = true;
    }
    REQUIRE(caught);
}

TEST_CASE("if_stmt: duplicate else_ throws", "[chlib][if_stmt][validate]") {
    test_ctx ctx("test_if_stmt_double_else");

    bool caught = false;
    try {
        _if(ch_bool(true))
            .then([] {})
            .else_()
            .then([] {})
            .else_();
    } catch (const std::runtime_error &) {
        caught = true;
    }
    REQUIRE(caught);
}

TEST_CASE("if_stmt: end() is idempotent", "[chlib][if_stmt]") {
    test_ctx ctx("test_if_stmt_idempotent_end");

    int hits = 0;

    auto block = _if(ch_bool(true));
    block.then([&hits] { ++hits; });
    block.end();
    REQUIRE_NOTHROW(block.end());

    REQUIRE(hits == 1);
}

TEST_CASE("if_stmt: then() chained after _if() always works",
          "[chlib][if_stmt]") {
    test_ctx ctx("test_if_stmt_then_chained");

    REQUIRE_NOTHROW(_if(ch_bool(true)).then([] {}).end());
}

// ===========================================================================
// 条件来自输入端口（**实施期实测补充**的用例）
// ===========================================================================

TEST_CASE("if_stmt: the condition MAY come from a ch_in<ch_bool> port",
          "[chlib][if_stmt][adr051]") {
    // ⚠️ **实施期实测**：原签名 `const ch_bool &` ⇒ **`ch_in<ch_bool>` 不能绑到
    //    `const ch_bool&`** ⇒ ⇒ DSL 对「条件来自输入端口」这个**最自然**的用例
    //    **不可用** ⇒ ⇒ 条件参数已改为模板（接受任意硬件操作数）。
    //    本用例**钉住**该能力 ⇒ 防止签名被改回 `const ch_bool &`。
    ch::ch_device<CondAssignDut> dev;   // 该 DUT 的条件就是 `io().en`
    ch::Simulator sim(dev.context());
    auto *q = dev.instance().io().q.impl();

    sim.set_input_value(dev.instance().io().en, 1u);
    sim.tick(1);
    REQUIRE(read_port(sim, q) == 7u);
}

// ===========================================================================
// 编译期断言：被移除 / 被改名的 API MUST NOT 再存在
// ===========================================================================

TEST_CASE("if_stmt: removed / renamed APIs MUST NOT exist",
          "[chlib][if_stmt][adr051][compile]") {
    // ✅ `conditional_assign` **存在**（ADR-051 D4 的统一入口）
    STATIC_REQUIRE(has_conditional_assign<void>::value);
    // ⚠️ `conditional_reg_assign` → `conditional_assign`（ADR-051 D4：**不留**
    //    deprecated 别名 ⇒ Zero-Debt + ADR-043 先例）
    // ⚠️ `priority_if` / `priority_if_then` ⇒ **物理移除**（D8：与 `multi_if`
    //    逐行等价 ⇒ 名字承诺「优先级 / 互斥优化」但**未兑现**）
    // ⇒ 这三个**自由函数**的「不存在」**无法**用 SFINAE 断言（见上方注释）
    // ⇒ ⇒ 由**静态护栏**的「被移除的符号 MUST NOT 出现」规则承担。
}

// ===========================================================================
// `conditional_assign` 接受**裸 literal**（`0xA5_h` / `0x5A_h`）
//
// ⚠️ 修复前的缺陷：`assign()` 的 else 分支写 `(*out_) <<= value;` ⇒ 它走
//   `operator<<=` ⇒ 用 `get_lnode(driver)` ⇒ `ch_literal_impl` **没有**
//   `get_lnode` 重载 ⇒ ⇒ **裸 literal 编译不过** ⇒ ⇒ 连 `if_stmt.h` 的
//   **头注释示例**都编译不过。
//
// ⚠️ 该分支**总被实例化** ⇒ ⇒ 即使运行期走不到 ⇒ 也会编译失败 ⇒ ⇒
//   「能不能编译」与「走哪条分支」**无关**。
// ===========================================================================

TEST_CASE("if_stmt: a BARE literal is a valid conditional_assign value",
          "[chlib][if_stmt][literal]") {
    ch::ch_device<ConditionalLiteralDut> dev;
    ch::Simulator sim(dev.context());
    auto *q = dev.instance().io().q.impl();

    // ⚠️ 手推：条件为真 ⇒ 取 `then` 分支的 **0xA5**；为假 ⇒ 取 `else_`
    //    分支的 **0x5A**（两个值都是**裸 literal** ⇒ 编译期就证明修复生效）。
    sim.set_input_value(dev.instance().io().en, 1u);
    sim.tick(1);
    REQUIRE(read_port(sim, q) == 0xA5u);

    sim.set_input_value(dev.instance().io().en, 0u);
    sim.tick(1);
    REQUIRE(read_port(sim, q) == 0x5Au);
}

TEST_CASE("if_stmt: a bare literal OUTSIDE a conditional block is a direct write",
          "[chlib][if_stmt][literal]") {
    ch::ch_device<ConditionalLiteralOutsideDut> dev;
    ch::Simulator sim(dev.context());
    auto *q = dev.instance().io().q.impl();

    // ⚠️ 手推：块外的 `conditional_assign(reg).next() = 0x5A_h;` ⇒ 走
    //    `assign()` 的 else 分支 ⇒ **直写**（不捕获）⇒ ⇒ 读回 **0x5A**。
    //    ⚠️ 该分支在修复前对 literal **编译不过** ⇒ ⇒ 这是**编译期**证据。
    sim.tick(1);
    REQUIRE(read_port(sim, q) == 0x5Au);
}
