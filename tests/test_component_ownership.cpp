// tests/test_component_ownership.cpp
//
// 层次化 RTL 输出 · T3-①b（归属真源）的判据。
//
// 契约：capability `component-ownership-and-naming`
//   · 「节点归属 MUST 由构造作用域唯一决定」
//   · 「组件的 build context MUST 由显式来源确定」
//   · 「跨 context 再装配 MUST 被拒绝」
//   · 「`ch_module` 在『无 active parent』时 MUST 响亮失败；组件 MUST 登记进组件树」
//   · 「组件树登记 MUST 双向完整，且归属判定 MUST NOT 误杀成员子组件」
//
// ⚠️ 本文件钉的是**修复前**的三种可观测形态（ADR-059 §1.1）：
//   ① 成员的 `ch_reg` 归**父**组件（成员构造早于 `build()`）；
//   ② 成员 `ch_module` 挂**祖父**（`Component::current()` 在构造期指向上层）；
//   ③ 成员 `ch_module` **直接消失**（构造期 `ctx_ == nullptr` ⇒ `ch_module` 早退
//      ⇒ 后续 `io()` 解引用空指针 ⇒ SIGSEGV）。
//   ⇒ ⇒ ③ 的判别性最直接：`ch::ch_device<Rv32iPipeline>` 修复前必崩。

#include "catch_amalgamated.hpp"

#include "ch.hpp"
#include "component.h"
#include "device.h"
#include "module.h"
#include "simulator.h"

#include "cpu/pipeline/rv32i_pipeline.h"

#include <memory>
#include <stdexcept>
#include <string>

namespace {

// ── DUT：三层成员形态（Top --member ch_module--> Mid --member ch_module--> Leaf）
class Leaf : public ch::Component {
public:
    __io(ch_in<ch_uint<8>> din; ch_out<ch_uint<8>> dout;)

    // ⚠️ **成员**寄存器：修复前它的 `get_parent()` 是 Mid（或 nullptr）
    ch_reg<ch_uint<8>> r{0_d};

    Leaf(ch::Component* parent = nullptr, const std::string& name = "leaf")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        io().dout <<= r;
        r->next = io().din;
    }
};

class Mid : public ch::Component {
public:
    __io(ch_in<ch_uint<8>> din; ch_out<ch_uint<8>> dout;)

    ch::ch_module<Leaf> leaf{"leaf"};   // **成员**形态（修复前挂祖父 / 消失）

    Mid(ch::Component* parent = nullptr, const std::string& name = "mid")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        leaf.io().din <<= io().din;
        io().dout <<= leaf.io().dout;
    }
};

class Top : public ch::Component {
public:
    __io(ch_in<ch_uint<8>> din; ch_out<ch_uint<8>> dout;)

    ch::ch_module<Mid> mid{"mid"};      // **成员**形态

    Top(ch::Component* parent = nullptr, const std::string& name = "top")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        mid.io().din <<= io().din;
        io().dout <<= mid.io().dout;
    }
};

// ── DUT：io.h 四处方向守卫（"改前改后均无触发"的可执行判据）─────────────
class GuardChild : public ch::Component {
public:
    __io(ch_in<ch_uint<8>> din_a; ch_in<ch_uint<8>> din_b; ch_out<ch_uint<8>> dout;)

    GuardChild(ch::Component* parent = nullptr, const std::string& name = "gchild")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override { io().dout <<= io().din_a & io().din_b; }
};

class GuardTop : public ch::Component {
public:
    __io(ch_in<ch_uint<8>> i; ch_out<ch_uint<8>> o; ch_out<ch_uint<8>> o2;)

    ch::ch_module<GuardChild> sub{"sub"};

    // 四个守卫的**条件值**（在 `describe()` 里求值，事后断言 ⇒ 不在
    // 组件代码里抛 Catch2 异常）
    bool g1 = false;   // out <<= in      : receiver 的 parent == 当前模块
    bool g2 = false;   // in  <<= in      : driver   的 parent == 当前模块
    bool g3 = false;   // out <<= out     : receiver 的 parent == 当前模块
    bool g4 = false;   // in  <<= out     : receiver 的 parent != 当前模块

    GuardTop(ch::Component* parent = nullptr, const std::string& name = "gtop")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        io().o <<= io().i;                  // ① out <<= in
        sub.io().din_a <<= io().i;          // ② in  <<= in（passthrough）
        io().o2 <<= sub.io().dout;          // ③ out <<= out
        sub.io().din_b <<= io().o;          // ④ in  <<= out

        const auto* owner = ch::Component::node_owner();
        g1 = (io().o.impl()->get_parent() == owner);
        g2 = (io().i.impl()->get_parent() == owner);
        g3 = (io().o2.impl()->get_parent() == owner);
        g4 = (sub.io().din_b.impl()->get_parent() != owner);
    }
};

// ── DUT：构造期不 build 的子组件（作用域 MUST NOT 被长期占用）──────────
class CreateChildNoBuild : public ch::Component {
public:
    std::shared_ptr<Leaf> kid;

    CreateChildNoBuild(ch::Component* parent = nullptr,
                       const std::string& name = "host")
        : ch::Component(parent, name) {
        kid = create_child<Leaf>("kid");   // ⚠️ 该组件**从不 build**
    }

    void describe() override {}
};

// ── DUT：构造期抛异常（作用域 MUST 被清理）──────────────────────────────
class ThrowsInCtor : public ch::Component {
public:
    ThrowsInCtor(ch::Component* parent = nullptr, const std::string& name = "boom")
        : ch::Component(parent, name) {
        throw std::runtime_error("ThrowsInCtor: intentional");
    }

    void describe() override {}
};

// ── DUT：在**组件作用域内**建一个 context（D10 判据）──────────────────
class CtxMaker : public ch::Component {
public:
    std::unique_ptr<ch::core::context> inner;

    CtxMaker(ch::Component* parent = nullptr, const std::string& name = "ctxmaker")
        : ch::Component(parent, name) {
        inner = std::make_unique<ch::core::context>("inner_ctx");
    }

    void describe() override {}
};

// ── DUT：单叶子组件（跨 ctx / defer 判据用）─────────────────────────────
class Plain : public ch::Component {
public:
    __io(ch_in<ch_uint<8>> din; ch_out<ch_uint<8>> dout;)

    Plain(ch::Component* parent = nullptr, const std::string& name = "plain")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override { io().dout <<= io().din; }
};

// ── DUT：含成员 ch_module，但宿主可能在「无环境 ctx」处装配 ─────────────
class DeferredTop : public ch::Component {
public:
    __io(ch_in<ch_uint<8>> din; ch_out<ch_uint<8>> dout;)

    ch::ch_module<Plain> leaf{"leaf"};
    bool leaf_port_exists = false;

    DeferredTop(ch::Component* parent = nullptr, const std::string& name = "deftop")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        leaf.io().din <<= io().din;
        io().dout <<= leaf.io().dout;
        // 子组件的端口 MUST 在宿主 `describe()` 之前就已建好
        // （⇒ 这正是「推迟到宿主 build 时补齐」的判据）
        leaf_port_exists = (leaf.io().din.impl() != nullptr) &&
                           (leaf.io().dout.impl() != nullptr);
    }
};

}   // namespace

// ═════════════════════════════════════════════════════════════════════════
// 归属：成员的 ch_reg / 成员 ch_module
// ═════════════════════════════════════════════════════════════════════════
TEST_CASE("ownership: member ch_reg belongs to the component under construction",
          "[ownership][component]") {
    ch::ch_device<Top> dev;
    auto& top = dev.instance();

    REQUIRE(top.child_count() == 1);
    auto& mid = *top.children()[0];
    REQUIRE(mid.child_count() == 1);
    auto& leaf = *mid.children()[0];

    // ② 成员 ch_module 挂**直接父**（修复前挂祖父 / 消失）
    REQUIRE(mid.name() == "top.mid");
    REQUIRE(leaf.name() == "top.mid.leaf");
    REQUIRE(mid.parent().get() == &top);
    REQUIRE(leaf.parent().get() == &mid);

    // ① 成员的 ch_reg 归属**它所在的组件**（修复前归父 / 无归属）
    auto& leaf_typed = static_cast<Leaf&>(leaf);
    REQUIRE(leaf_typed.r.impl()->get_parent() == &leaf);

    // 归属与命名同源：成员节点名 = 「组件全路径 + 局部节点名」
    REQUIRE(leaf_typed.r.impl()->name() == "top.mid.leaf._reg");
}

TEST_CASE("ownership: ch_device<Rv32iPipeline> instantiates without SIGSEGV",
          "[ownership][component][pipeline]") {
    // ⭐ 这条是 `examples/removed_examples.txt` 里 `test_rv32i_pipeline_device`
    //    的**归位条件**：`Rv32iPipeline` 把 `ch_module<IfStage>` 等声明为**成员**
    //    ⇒ 修复前 `ch_module` 在构造期拿不到 parent / ctx ⇒ 早退 ⇒ `io()` SIGSEGV。
    ch::ch_device<riscv::Rv32iPipeline> dev;
    auto& pipeline = dev.instance();

    // 8 个成员 `ch_module`（if/id/ex/mem/**load_lane**/wb/hazard/fwd_mux）
    // ⚠️ **2026-11-13 · ADR-069 A0**：新增第 8 个成员 `load_lane`
    //    （`riscv::LoadLaneSelector` —— load 的车道选择从 `MemStage` 迁出，
    //    因为 `sread` 让 load 数据晚 1 拍可用 ⇒ ⇒ 车道选择 MUST 在 M2 的下游做）
    //    ⇒ ⇒ 原值 7 → **8**
    REQUIRE(pipeline.child_count() == 8);

    for (auto& child : pipeline.children()) {
        INFO("child: " << child->name());
        // 挂**直接父**，且组件树登记**双向完整**（D9）
        REQUIRE(child->parent().get() == &pipeline);
        REQUIRE(child->name().rfind("top.", 0) == 0);
        // 子组件的端口真的建出来了（修复前这里根本没有子组件）
        REQUIRE(child->context() == dev.context());
    }

    // 每个成员阶段 MUST **真的拥有节点**（端口 / 流水线寄存器）
    // ⇒ 这是「归属 = 那个组件」的直接判据：修复前这些节点挂在祖父/无归属上
    for (auto& child : pipeline.children()) {
        std::size_t owned = 0;
        for (const auto& node : dev.context()->get_nodes()) {
            if (node->get_parent() == child.get()) {
                ++owned;
            }
        }
        INFO("child: " << child->name() << " owned nodes: " << owned);
        REQUIRE(owned > 0);
    }
}

// ═════════════════════════════════════════════════════════════════════════
// 作用域纪律（D6）
// ═════════════════════════════════════════════════════════════════════════
TEST_CASE("ownership: construction scope stack is balanced on every path",
          "[ownership][component][scope]") {
    REQUIRE(ch::Component::scope_depth() == 0);

    SECTION("wrapped constructions return to depth 0") {
        {
            ch::ch_device<Top> dev;
            // 组件**存活期间**作用域 MUST 为空（否则后续节点的归属被污染）
            REQUIRE(ch::Component::scope_depth() == 0);
        }
        REQUIRE(ch::Component::scope_depth() == 0);
    }

    SECTION("create_child that never builds MUST NOT hold a scope frame") {
        {
            // ⚠️ 直接构造 ⇒ 用 `ConstructionFrame` 截断（与 ch_device / ch_module /
            //    create_child 的做法一致 —— 这是「直接构造」的契约形态）
            ch::Component::ConstructionFrame frame;
            CreateChildNoBuild host;
            REQUIRE(host.kid != nullptr);
            // 子组件**从不 build** ⇒ 它 MUST NOT 继续占据作用域栈顶
            // （⇒ 帧已被 create_child 截断）
            REQUIRE(ch::Component::scope_depth() == 1);   // 只剩宿主的帧
            REQUIRE(ch::Component::node_owner() == &host);
        }
        REQUIRE(ch::Component::scope_depth() == 0);
    }

    SECTION("a throwing constructor MUST NOT leak a scope frame") {
        // ⚠️ 这里**故意不**用 `ConstructionFrame` —— 考的是析构兜底
        // （派生类构造抛出时基类子对象仍会被析构）
        REQUIRE_THROWS_AS(ThrowsInCtor(nullptr, "boom"), std::runtime_error);
        REQUIRE(ch::Component::scope_depth() == 0);
    }
}

// ═════════════════════════════════════════════════════════════════════════
// context 级节点 MUST 无归属（D10）
// ═════════════════════════════════════════════════════════════════════════
TEST_CASE("ownership: context-level nodes (clock/reset) have no owner",
          "[ownership][component][ctx]") {
    std::unique_ptr<CtxMaker> maker;
    {
        // 组件**作用域内**建 ctx ⇒ 若不抑制归属，新 ctx 的 clock/reset 会拿到
        // `owner == &maker` ⇒ 「context 级节点不归属任何组件」被违反
        // ⇒ 层次化 emitter 的 clock/reset 特判随之失效。
        ch::Component::ConstructionFrame frame;
        maker = std::make_unique<CtxMaker>(nullptr, "ctxmaker");
    }

    REQUIRE(maker->inner != nullptr);
    REQUIRE(maker->inner->get_default_clock() != nullptr);
    REQUIRE(maker->inner->get_default_reset() != nullptr);
    REQUIRE(maker->inner->get_default_clock()->get_parent() == nullptr);
    REQUIRE(maker->inner->get_default_reset()->get_parent() == nullptr);
}

// ═════════════════════════════════════════════════════════════════════════
// io.h 四处方向守卫：判据 = **「改前改后均无触发」**
// ═════════════════════════════════════════════════════════════════════════
TEST_CASE("ownership: io.h direction guards do not fire", "[ownership][component][guards]") {
    ch::ch_device<GuardTop> dev;
    auto& top = dev.instance();

    // ⚠️ 判据 MUST 是「无触发」，**不是**「触发面逐项相同」：
    //    守卫用的是 `CHREQUIRE`（**只打日志不抛**）⇒ 「触发面」无法用 diff 收敛
    //    （新误报会让 diff 永远不等）。⇒ ⇒ 直接断言**条件本身成立**。
    REQUIRE(top.g1);   // out <<= in
    REQUIRE(top.g2);   // in  <<= in (passthrough)
    REQUIRE(top.g3);   // out <<= out
    REQUIRE(top.g4);   // in  <<= out
}

// ═════════════════════════════════════════════════════════════════════════
// 归属 ↔ 语义分流（D7）：`type_input` 的分类 MUST NOT 变
// ═════════════════════════════════════════════════════════════════════════
TEST_CASE("ownership: no ownerless type_input node is introduced",
          "[ownership][component][simulator]") {
    ch::ch_device<Top> dev;

    // `src/simulator_init.cpp` 用 `!node->get_parent()` 把 `type_input` 分到
    // `input_instr_list_`（否则进 `combinational_instr_list_`）
    // ⇒ ⇒ 归属真源变更 MUST NOT 让任何**组件内的**输入端口掉进「无归属」桶，
    //      也 MUST NOT 让新建的输入节点从「无归属」变成「有归属」。
    std::size_t inputs_total = 0;
    std::size_t inputs_ownerless = 0;
    for (const auto& owner : dev.context()->get_nodes()) {
        if (owner->type() != ch::core::lnodetype::type_input) {
            continue;
        }
        ++inputs_total;
        if (owner->get_parent() == nullptr) {
            ++inputs_ownerless;
        }
    }

    // 本设计的所有 `ch_in` 都在 `create_ports()`（build 期）创建 ⇒ 有归属
    REQUIRE(inputs_total > 0);
    REQUIRE(inputs_ownerless == 0);
}

// ═════════════════════════════════════════════════════════════════════════
// 跨 ctx 再装配 MUST 被拒绝（D2″，基准分两支）
// ═════════════════════════════════════════════════════════════════════════
TEST_CASE("ownership: cross-context re-assembly is rejected", "[ownership][component][ctx]") {
    SECTION("branch 1: constructed inside a ctx, built with another") {
        ch::core::context c1("c1");
        ch::core::context c2("c2");

        std::unique_ptr<Plain> comp;
        {
            ch::core::ctx_swap swap(&c1);
            ch::Component::ConstructionFrame frame;
            comp = std::make_unique<Plain>(nullptr, "p1");
        }
        REQUIRE(comp->context() == nullptr);   // 构造期 ctx_ 恒为 nullptr
        REQUIRE_THROWS_AS(comp->build(&c2), std::runtime_error);
        // 同一 ctx ⇒ 正常
        REQUIRE_NOTHROW(comp->build(&c1));
    }

    SECTION("branch 2: no env ctx at construction, then two different ctxs") {
        ch::core::context ca("ca");
        ch::core::context cb("cb");

        std::unique_ptr<Plain> comp;
        {
            ch::Component::ConstructionFrame frame;
            comp = std::make_unique<Plain>(nullptr, "p2");
        }
        REQUIRE_NOTHROW(comp->build(&ca));
        REQUIRE(comp->context() == &ca);
        // 重复 build 同一 ctx ⇒ 早退（不抛）
        REQUIRE_NOTHROW(comp->build(&ca));
        // 换 ctx ⇒ MUST 抛（⚠️ 守卫在 `built_` 早退**之前**，否则永不生效）
        REQUIRE_THROWS_AS(comp->build(&cb), std::runtime_error);
    }
}

// ═════════════════════════════════════════════════════════════════════════
// 环境 ctx 为空（D2′）：MUST NOT 硬失败，子组件 build 推迟到宿主 build
// ═════════════════════════════════════════════════════════════════════════
TEST_CASE("ownership: env ctx == nullptr defers the child build", "[ownership][component][ctx]") {
    std::unique_ptr<DeferredTop> host;
    {
        // ⚠️ 无环境 ctx（下游 `make_unique<Unit>()` 然后 `build(ctx)` 的常态）
        ch::Component::ConstructionFrame frame;
        REQUIRE_NOTHROW(host = std::make_unique<DeferredTop>(nullptr, "deftop"));
    }

    // 子组件已登记进组件树（D8：组件 MUST 登记），但其 build 还在排队
    REQUIRE(host->child_count() == 1);
    REQUIRE(host->children()[0]->context() == nullptr);

    ch::core::context ctx("late_ctx");
    REQUIRE_NOTHROW(host->build(&ctx));

    // 推迟的 build 已执行 ⇒ 子组件进了**同一个** ctx，端口也建好了
    REQUIRE(host->children()[0]->context() == &ctx);
    REQUIRE(host->leaf_port_exists);
}

// ═════════════════════════════════════════════════════════════════════════
// `ch_module` 无 active parent ⇒ 响亮失败（D8）
// ═════════════════════════════════════════════════════════════════════════
TEST_CASE("ownership: ch_module without an active parent fails loudly",
          "[ownership][component][module]") {
    REQUIRE(ch::Component::scope_depth() == 0);
    REQUIRE(ch::Component::node_owner() == nullptr);

    // ⚠️ 修复前是 `CHERROR` + `return`（只打日志）⇒ 后续 `io()` 解引用空指针
    //    ⇒ SIGSEGV（本仓 ANTI-PATTERNS 已登记该形态）⇒ ⇒ MUST 改为抛异常
    REQUIRE_THROWS_AS((ch::ch_module<Leaf>{"orphan"}), std::runtime_error);
}
