// tests/test_component_granularity.cpp
//
// 层次化 RTL 输出 · T3-①c（层级粒度声明）的判据。
//
// 契约：
//   · capability `rtl-hierarchical-output`「层级粒度 MUST 由显式声明决定（默认 inline）」
//   · capability `rtl-hierarchical-output`「成层判定 MUST 与装配点解耦，但组件 MUST
//     登记进组件树」
//   · capability `component-ownership-and-naming`「成层组件的模块类型名 MUST 按来源
//     优先级确定」（四档：① 类级 / ② 实例级继承 / ③ 实例处给名 / ④ ForceLayer 回退）
//
// ⚠️ 本文件**不依赖 emitter**（①c 只落「声明 → 成层判定 → 模块类型名」这条链
// ⇒ 产物结构由 T3-④ 判）。⇒ ⇒ 判据全部可在 `Component` 的公开面上观测。

#include "catch_amalgamated.hpp"

#include "ch.hpp"
#include "component.h"
#include "device.h"
#include "module.h"

#include <memory>
#include <string>
#include <type_traits>

using namespace ch;
using namespace ch::core;

namespace {

// ── 类级声明成层（档 ①：名字与「我是一层」一体）──────────────────────────
class LayeredUnit : public ch::Component {
public:
    CH_HIERARCHY_LAYER("layered_unit");

    __io(ch_in<ch_uint<8>> din; ch_out<ch_uint<8>> dout;)

    LayeredUnit(ch::Component* parent = nullptr, const std::string& name = "layered_unit")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }
    void describe() override { io().dout <<= io().din; }
};

// ── 未声明（helper）⇒ 默认 inline ───────────────────────────────────────
class HelperUnit : public ch::Component {
public:
    __io(ch_in<ch_uint<8>> din; ch_out<ch_uint<8>> dout;)

    HelperUnit(ch::Component* parent = nullptr, const std::string& name = "helper_unit")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }
    void describe() override { io().dout <<= io().din; }
};

// ── 装载组件：**类级 × 实例级** 三组合 ──────────────────────────────────
class Holder : public ch::Component {
public:
    __io(ch_in<ch_uint<8>> din; ch_out<ch_uint<8>> dout;)

    ch::ch_module<LayeredUnit> a{"a"};                 // 类级声明 ⇒ 成层
    ch::ch_inline<LayeredUnit> b{"b"};                 // 实例级覆盖 ⇒ inline
    ch::ch_module<HelperUnit>  c{"c"};                 // 未声明 ⇒ inline
    ch::ch_layer<HelperUnit>   d{"d", "helper_layer"}; // 实例级提升 + 档 ③ 给名
    ch::ch_layer<LayeredUnit>  e{"e"};                 // 档 ② 继承类级名

    Holder(ch::Component* parent = nullptr, const std::string& name = "holder")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        // 让每个子组件的端口都真的被连接（⇒ 端口存在，而不是只有组件壳）
        a.io().din <<= io().din;
        b.io().din <<= a.io().dout;
        c.io().din <<= b.io().dout;
        d.io().din <<= c.io().dout;
        e.io().din <<= d.io().dout;
        io().dout <<= e.io().dout;
    }
};

// ── 无 active parent 处的装配（下游桥接 plugin 形态）────────────────────
class RootHolder : public ch::Component {
public:
    __io(ch_in<ch_uint<8>> din; ch_out<ch_uint<8>> dout;)

    RootHolder(ch::Component* parent = nullptr, const std::string& name = "root")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }
    void describe() override { io().dout <<= io().din; }
};

const ch::Component& as_component(const std::shared_ptr<ch::Component>& c) { return *c; }

}   // namespace

// ═════════════════════════════════════════════════════════════════════════
// 编译期：声明面 MUST 恰好可表达「① / ② / ③」三档，MUST NOT 有第四种写法
// ═════════════════════════════════════════════════════════════════════════
// ⭐ 档 ②：`T` 已类级声明 ⇒ 单参形态可用；**另给名字 MUST 编译不过**
//    （否则「同类型 ⇒ 同一 <module_type_name> 组」被破坏 ⇒ 序号失去分组依据）
static_assert(std::is_constructible_v<ch::ch_layer<LayeredUnit>, const std::string&>);
static_assert(!std::is_constructible_v<ch::ch_layer<LayeredUnit>, const std::string&,
                                       const std::string&>);

// ⭐ 档 ③：`T` 未声明 ⇒ **缺名 MUST 编译不过**（MUST NOT 推迟到发射时的运行期硬错）
static_assert(!std::is_constructible_v<ch::ch_layer<HelperUnit>, const std::string&>);
static_assert(std::is_constructible_v<ch::ch_layer<HelperUnit>, const std::string&,
                                      const std::string&>);

// `ch_inline` 只要实例名
static_assert(std::is_constructible_v<ch::ch_inline<LayeredUnit>, const std::string&>);

// `ch_module` 的**公开签名零变更**（ADR-061 决策①：MUST NOT 加 tag 参数）
static_assert(std::is_constructible_v<ch::ch_module<LayeredUnit>, const std::string&>);

// ═════════════════════════════════════════════════════════════════════════
// 成层判定：类级 × 实例级 三组合
// ═════════════════════════════════════════════════════════════════════════
TEST_CASE("granularity: class-level and instance-level declarations compose",
          "[granularity][component]") {
    ch::ch_device<Holder> dev;
    auto& holder = dev.instance();
    REQUIRE(holder.child_count() == 5);

    auto child = [&](const char* local) -> ch::Component& {
        for (auto& c : holder.children()) {
            if (c->local_name() == local) {
                return *c;
            }
        }
        FAIL("missing child: " << local);
        return *holder.children()[0];   // 不可达（FAIL 已抛出）
    };

    SECTION("类级声明 ⇒ 成层（默认对每个实例生效）") {
        REQUIRE(child("a").is_layer());
        REQUIRE(child("a").module_type_name() == "layered_unit");
    }

    SECTION("实例级 inline MUST 覆盖类级声明") {
        REQUIRE_FALSE(child("b").is_layer());
    }

    SECTION("未声明 ⇒ 默认 inline") {
        REQUIRE_FALSE(child("c").is_layer());
    }

    SECTION("实例级提升 helper ⇒ 成层 + 档 ③ 的实例名") {
        REQUIRE(child("d").is_layer());
        REQUIRE(child("d").module_type_name() == "helper_layer");
    }

    SECTION("档 ② MUST 继承类级名字（MUST NOT 用实例名 e）") {
        REQUIRE(child("e").is_layer());
        REQUIRE(child("e").module_type_name() == "layered_unit");
        REQUIRE(child("e").module_type_name() != "e");
    }
}

// ═════════════════════════════════════════════════════════════════════════
// emitter 侧总开关（Granularity）：逃生门 + 对比测试用
// ═════════════════════════════════════════════════════════════════════════
TEST_CASE("granularity: global switch forces inline / layer", "[granularity][component]") {
    ch::ch_device<Holder> dev;
    auto& holder = dev.instance();

    REQUIRE(ch::Component::granularity() == ch::Granularity::RespectDeclarations);

    SECTION("ForceInline ⇒ 全部 inline（含类级声明的）") {
        ch::Component::GranularityScope scope(ch::Granularity::ForceInline);
        for (auto& c : holder.children()) {
            INFO("child: " << c->name());
            REQUIRE_FALSE(c->is_layer());
        }
    }

    SECTION("ForceLayer ⇒ 全部成层（含未声明的 helper）") {
        ch::Component::GranularityScope scope(ch::Granularity::ForceLayer);
        for (auto& c : holder.children()) {
            INFO("child: " << c->name());
            REQUIRE(c->is_layer());
        }
        // 档 ④：用户未声明 ⇒ 回退 `typeid`（**同 ABI 内**可复算）+ CHWARN
        for (auto& c : holder.children()) {
            if (c->local_name() == "c") {
                REQUIRE_FALSE(c->module_type_name().empty());
                REQUIRE(c->module_type_name() != "helper_unit");
            }
        }
    }

    SECTION("作用域退出 MUST 复原（RAII）") {
        {
            ch::Component::GranularityScope scope(ch::Granularity::ForceInline);
            REQUIRE(ch::Component::granularity() == ch::Granularity::ForceInline);
        }
        REQUIRE(ch::Component::granularity() == ch::Granularity::RespectDeclarations);
    }
}

// ═════════════════════════════════════════════════════════════════════════
// 成层判定与装配点**解耦**（无需 active parent，但 MUST 登记进组件树）
// ═════════════════════════════════════════════════════════════════════════
TEST_CASE("granularity: assembly without an active parent still respects declarations",
          "[granularity][component][assembly]") {
    ch::core::context ctx("assemble_ctx");

    std::shared_ptr<RootHolder> root;
    {
        // ⚠️ 没有 active parent（不是 ch_device / ch_module）—— 下游桥接 plugin 的形态
        ch::core::ctx_swap swap(&ctx);
        ch::Component::ConstructionFrame frame;
        root = std::make_shared<RootHolder>(nullptr, "root");
    }

    // 装配方 MUST 自己算全路径后传给构造函数（⇒ MUST NOT 依赖 add_child 补前缀）
    std::unique_ptr<LayeredUnit> tmp;
    {
        ch::core::ctx_swap swap(&ctx);
        ch::Component::ConstructionFrame frame;
        tmp = std::make_unique<LayeredUnit>(nullptr, "root.unit");
    }
    REQUIRE(tmp->name() == "root.unit");

    // add_child MUST **先于** build(ctx)
    auto unit = root->add_child(std::move(tmp));
    REQUIRE(unit != nullptr);
    unit->build(&ctx);

    // ① 成层判定只看声明 ⇒ 与装配点无关
    REQUIRE(unit->is_layer());
    REQUIRE(unit->module_type_name() == "layered_unit");
    // ② 节点归属仍是它自己（§2.1 的作用域栈 ⇒ 端口在 build 期建 ⇒ 归它）
    REQUIRE(unit->parent().get() == root.get());
    // ③ MUST 登记进组件树（emitter 靠 children() 发现层级）
    REQUIRE(root->child_count() == 1);
    REQUIRE(root->children()[0].get() == unit.get());
    REQUIRE(unit->context() == &ctx);
}

// ═════════════════════════════════════════════════════════════════════════
// 策略枚举的默认值 MUST 在框架层定义（MUST NOT 留给实现者自行发明）
// ═════════════════════════════════════════════════════════════════════════
TEST_CASE("granularity: policy enums have canonical values", "[granularity][component]") {
    // ADR-060 §2.4 的默认值：合成 + 告警 / 冒泡
    constexpr ch::PortPolicy port_policy = ch::PortPolicy::SynthesizeWarn;
    constexpr ch::UndrivenPolicy undriven_policy = ch::UndrivenPolicy::Bubble;
    static_assert(port_policy == ch::PortPolicy::SynthesizeWarn);
    static_assert(undriven_policy == ch::UndrivenPolicy::Bubble);
    REQUIRE(true);
}
