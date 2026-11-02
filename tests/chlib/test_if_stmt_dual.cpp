// ⚠️⚠️ **路线 B（A5 的 H 批）第 2 批**（2026-11-02 · change
// `dual-backend-parity-route-b-conditional-2026-11`）
//
// ## 为什么这个文件【值得】存在（立项理由，不是凑数）
//
// `chlib::conditional_block`（ADR-051 的 `_if` 语句块 DSL）是仓内**唯一**的
// 「条件赋值」入口，而它的**运行期语义**在 **verilator codegen 路径上从未被
// 验证过**。
//
// ⚠️ **为什么这条语义特别值得双后端覆盖**（不是泛泛的「新组件加测试」）：
//
// 1. **它是本仓「假绿」病的原型病例**。ADR-051 记录的实现缺陷是：
//    `end()` **先**置 `finalized_` **再**遍历分支体 ⇒ 遍历期间 `is_active()`
//    恒 false ⇒ 赋值走「直接写入」分支 ⇒ ⇒ **条件被彻底忽略** ⇒ 而**既有
//    测试竟然钉住了这个坏行为**（原注释明写「This test pins down that current
//    … behavior so future changes are noticed」）⇒ ⇒ **绿灯承载的是缺陷**。
//    ⇒ ⇒ 一条「条件真的参与了求值」的判据是**该病种的疫苗**。
//
// 2. **它的语义是「守卫贡献」模型**（与 `ch_state_machine::contrib` 同型）
//    ⇒ 合成方式是**一条 `select` 链** ⇒ ⇒ codegen 侧要正确表达这条链的
//    **嵌套结构**与**优先级方向**（先匹配者胜）⇒ ⇒ 这正是 interpreter 与
//    codegen 容易分叉的地方（一边是嵌套 mux、一边是链式 select）。
//
// 3. **ADR-051 明确要求「默���尾部」的语义在两条后端上一致**（寄存器 = 保持
//    · 输出端口 = 显式默认）⇒ ⇒ 「保持」在 codegen 侧是**寄存器自反馈**，
//    是最容易写错的一类。
//
// ## 为什么**不是**转换 `tests/chlib/test_if_stmt.cpp`（如实说明）
//
// 那个文件 10 个用例里 **4** 个是**构图期**判据（输出无 `else_` MUST 被拒绝 ·
// `validate` 抛异常 ×2 · `end()` 遍历所有分支体的捕获语义）⇒ ⇒ 对两后端是
// **同一段 C++ 代码** ⇒ ⇒ **dual 化零增量价值**。
// 其余 6 个是**运行期**用例，但它们**只跑 interpreter** ⇒ ⇒ 「条件赋值在
// codegen 路径上是否等价」**从未被问过**。
//
// ⇒ ⇒ 本 change 是**新增**（路线 B）：原文件 **MUST NOT 修改**。
//
// ## 期望值（**手推**，禁抄仿真输出）
//
// 语义前提：
//   · `_if(cond).then(v1).else_().then(v2)` ⇒ 结果 =
//     `select(cond, v1, v2)` ⇒ 条件**真** ⇒ v1，**假** ⇒ v2；
//   · `.then(v1).elif(c2).then(v2)`（**无** `else_`）⇒ 结果 =
//     `select(c1, v1, select(c2, v2, <保持>))` ⇒ 两条都假 ⇒ **保持**
//     （寄存器 = 当前值 ⇒ 读回上一拍的值，**不是** 0）；
//   · 「先匹配者胜」⇒ `c1` 与 `c2` **同时**为真 ⇒ 取 **v1**（`c1` 的分支
//     在链的**外层**）；
//   · 嵌套 ⇒ 内层先合成**一条**表达式，再作为外层该分支的候选值 ⇒
//     外层命中 ⇒ 内层结果；外层未命中 ⇒ 外层 `else_` 的值（**内层不生效**）。
//
// ⚠️ **判据（N2′）**：断言数(with verilator) == **2 ×** 断言数(interpreter-only)
//    ⇒ 证明 verilator 半边**真的跑了**（缺席时只有 1 倍）。
#include "catch_amalgamated.hpp"

#include "chlib/if_stmt.h"
#include "component.h"
#include "core/bool.h"
#include "core/context.h"
#include "core/reg.h"
#include "core/uint.h"
#include "device.h"
#include "dual_backend_fixture.h"
#include "simulator.h"

using namespace ch;
using namespace ch::core;
using namespace chlib;

namespace {

/// DUT 1：`then` + `else_`（**有**显式默认）⇒ 条件真/假两路都有确定值。
class CondAssignDut : public ch::Component {
public:
    __io(ch_in<ch_bool> en; ch_out<ch_uint<8>> q;)

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

/// DUT 2：`then` + `elif`（**无** `else_`）⇒ 未匹配时**保持**。
///
/// ⚠️ 「保持」在 codegen 侧是**寄存器自反馈** ⇒ 是最容易写错的一类 ⇒
///    故必须有「全部为假 ⇒ 读回**上一拍**的值（不是 0）」这条断言。
class PriorityDut : public ch::Component {
public:
    __io(ch_in<ch_bool> a; ch_in<ch_bool> b; ch_out<ch_uint<8>> q;)

    PriorityDut(ch::Component *parent = nullptr,
                const std::string &name = "priority_dut")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        ch_reg<ch_uint<8>> r(0_d, "r");
        _if(io().a)
            .then([&] { conditional_assign(r).next() = ch_uint<8>(1_d); })
            .elif(io().b)
            .then([&] { conditional_assign(r).next() = ch_uint<8>(2_d); })
            .end();
        io().q = r;
    }
};

/// DUT 3：**嵌套** ⇒ 内层先合成一条表达式，再作为外层该分支的候选值。
class NestedDut : public ch::Component {
public:
    __io(ch_in<ch_bool> a; ch_in<ch_bool> b; ch_out<ch_uint<8>> q;)

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

/// DUT 4：**输出端口**的条件化（输出**不能**「保持」⇒ MUST 有 `else_`）
/// ⇒ 默认尾部是**显式默认**（不是寄存器自反馈）。
class CondOutDut : public ch::Component {
public:
    __io(ch_in<ch_bool> en; ch_out<ch_uint<8>> q;)

    CondOutDut(ch::Component *parent = nullptr,
               const std::string &name = "cond_out_dut")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        _if(io().en)
            .then([&] { conditional_assign(io().q).next() = ch_uint<8>(3_d); })
            .else_()
            .then([&] { conditional_assign(io().q).next() = ch_uint<8>(4_d); })
            .end();
    }
};

// ⚠️ 用例表 MUST 放在**文件作用域**：`DUAL_BACKEND_TEST_PAIR` 的 body 参数里
//    花括号内的逗号会被预处理器当**参数分隔**（只有圆括号才保护）。

}  // namespace

// ───────────────────────── DUT 1 ─────────────────────────
DUAL_BACKEND_TEST_PAIR(
    CondAssignDut,
    "条件赋值 - 条件真取 then 值 / 假取 else 值（两后端逐拍一致）",
    "[chlib][if_stmt][adr051][dual][route_b]",
    [&]() {
        auto &io = dev.instance().io();
        // 条件**假** ⇒ else 分支 = 9
        sim.set_input_value(io.en, 0u);
        sim.tick();
        REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.q)) == 9u);
        // 条件**真** ⇒ then 分支 = 7
        sim.set_input_value(io.en, 1u);
        sim.tick();
        REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.q)) == 7u);
        // 再回**假** ⇒ 回到 9（⇒ 证明条件**真的**被求值）
        sim.set_input_value(io.en, 0u);
        sim.tick();
        REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.q)) == 9u);
    });

// ───────────────────────── DUT 2 ─────────────────────────
DUAL_BACKEND_TEST_PAIR(
    PriorityDut,
    "条件赋值 - 先匹配者胜 + 无 else_ 时**保持**（两后端逐拍一致）",
    "[chlib][if_stmt][adr051][dual][priority][route_b]",
    [&]() {
        auto &io = dev.instance().io();
        // 两条都**假** ⇒ 无 else_ ⇒ **保持**初值 0
        sim.set_input_value(io.a, 0u);
        sim.set_input_value(io.b, 0u);
        sim.tick();
        REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.q)) == 0u);
        // 命中 a ⇒ 1
        sim.set_input_value(io.a, 1u);
        sim.tick();
        REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.q)) == 1u);
        // 再次全**假** ⇒ MUST **保持 1**（不是回到 0）⇒ 证明「保持」而非「清零」
        sim.set_input_value(io.a, 0u);
        sim.tick();
        REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.q)) == 1u);
        // 命中 b ⇒ 2
        sim.set_input_value(io.b, 1u);
        sim.tick();
        REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.q)) == 2u);
        // a 与 b **同时**为真 ⇒ MUST 取 a 的值（1）⇒「先匹配者胜」
        sim.set_input_value(io.a, 1u);
        sim.tick();
        REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.q)) == 1u);
    });

// ───────────────────────── DUT 3 ─────────────────────────
DUAL_BACKEND_TEST_PAIR(
    NestedDut,
    "条件赋值 - 嵌套块内层优先合成（两后端逐拍一致）",
    "[chlib][if_stmt][adr051][dual][nested][route_b]",
    [&]() {
        auto &io = dev.instance().io();
        // a=1, b=1 ⇒ 内层命中 ⇒ 11
        sim.set_input_value(io.a, 1u);
        sim.set_input_value(io.b, 1u);
        sim.tick();
        REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.q)) == 11u);
        // a=1, b=0 ⇒ 内层走 else ⇒ 12
        sim.set_input_value(io.b, 0u);
        sim.tick();
        REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.q)) == 12u);
        // a=0 ⇒ 外层走 else ⇒ 13（⇒ 内层**不**生效 ⇒ 嵌套隔离正确）
        sim.set_input_value(io.a, 0u);
        sim.tick();
        REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.q)) == 13u);
    });

// ───────────────────────── DUT 4 ─────────────────────────
DUAL_BACKEND_TEST_PAIR(
    CondOutDut,
    "条件赋值 - 输出端口的显式默认尾部（两后端逐拍一致）",
    "[chlib][if_stmt][adr051][dual][output][route_b]",
    [&]() {
        auto &io = dev.instance().io();
        sim.set_input_value(io.en, 0u);
        sim.tick();
        REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.q)) == 4u);
        sim.set_input_value(io.en, 1u);
        sim.tick();
        REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.q)) == 3u);
    });
