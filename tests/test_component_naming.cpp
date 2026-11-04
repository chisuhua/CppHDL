// tests/test_component_naming.cpp
//
// 层次化 RTL 输出 · T3-①a（纯命名）的判据。
//
// 契约：capability `component-ownership-and-naming`
//   · 「组件实例路径 MUST 无重复前缀」
//   · 「组件实例 MUST 提供局部名与模块类型名两个命名维度」
// + capability `rtl-hierarchical-output`「命名权威 MUST 是 ctx 级全局表」
//
// ⚠️ **判别力**：本文件 MUST 抓住「深度 ≥3 的重复前缀」这个具体缺陷
//   （修复前 Leaf 的路径是 `top.top.m.lf`）⇒ ⇒ 若实现改回「把已是全路径的
//   `name_` 再拼一次父路径」，本文件 MUST 转红。

#include "catch_amalgamated.hpp"

#include "ch.hpp"

#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace {

// ── 编译期：`hierarchical_name()` MUST NOT 仍存在 ────────────────────────
// （它是「第二个全路径拼接点」= 两个真源 ⇒ 前缀重复会复发 ⇒ ADR-059 D3）
template <typename T, typename = void>
struct has_hierarchical_name : std::false_type {};

template <typename T>
struct has_hierarchical_name<T, std::void_t<decltype(std::declval<const T&>().hierarchical_name())>>
    : std::true_type {};

static_assert(!has_hierarchical_name<ch::Component>::value,
              "Component::hierarchical_name() MUST NOT exist —— 全路径的唯一真源是 name()");

// ── DUT：三层组件树 Top → Mid → Leaf ─────────────────────────────────────
class Leaf : public ch::Component {
public:
    Leaf(ch::Component* parent = nullptr, const std::string& name = "leaf")
        : ch::Component(parent, name) {}
    void describe() override {}
};

class Mid : public ch::Component {
public:
    Mid(ch::Component* parent = nullptr, const std::string& name = "mid")
        : ch::Component(parent, name) {}
    void describe() override {
        // ⚠️ `ch_module` MUST 建在 `describe()` 内（需要 active parent）
        ch::ch_module<Leaf> leaf{"lf"};
    }
};

class Top : public ch::Component {
public:
    Top(ch::Component* parent = nullptr, const std::string& name = "top")
        : ch::Component(parent, name) {}
    void describe() override {
        ch::ch_module<Mid> mid{"m"};
    }
};

// ── 成层组件（声明了模块类型名）──────────────────────────────────────────
class Layered : public ch::Component {
public:
    CH_HIERARCHY_LAYER("my_layer");

    Layered(ch::Component* parent = nullptr, const std::string& name = "layered")
        : ch::Component(parent, name) {}
    void describe() override {}
};

// ── 非成层组件（回落 `typeid`）──────────────────────────────────────────
class Plain : public ch::Component {
public:
    Plain(ch::Component* parent = nullptr, const std::string& name = "plain")
        : ch::Component(parent, name) {}
    void describe() override {}
};

} // namespace

TEST_CASE("Component naming: depth>=3 MUST have no duplicated prefix", "[component][naming]") {
    ch::ch_device<Top> dev;
    auto& top = dev.instance();

    REQUIRE(top.name() == "top");
    REQUIRE(top.local_name() == "top");
    REQUIRE(top.children().size() == 1);

    auto& mid = *top.children()[0];
    REQUIRE(mid.name() == "top.m");
    REQUIRE(mid.local_name() == "m");
    REQUIRE(mid.children().size() == 1);

    auto& leaf = *mid.children()[0];
    // ⭐ 修复前是 "top.top.m.lf" ⇒ ⇒ 这条断言就是缺陷的判别力来源
    REQUIRE(leaf.name() == "top.m.lf");
    REQUIRE(leaf.local_name() == "lf");

    // 路径的每一段都只出现一次（比逐字相等更强：任何重复段都会被抓住）
    const std::string path = leaf.name();
    REQUIRE(path.find("top.top") == std::string::npos);
    REQUIRE(path.find(".m.m") == std::string::npos);
}

TEST_CASE("Component naming: local_name and module_type_name are two separate dimensions",
          "[component][naming]") {
    ch::ch_device<Top> dev;
    auto& top = dev.instance();
    auto& mid = *top.children()[0];

    // 局部名不含父路径
    REQUIRE(mid.local_name() == "m");
    REQUIRE(mid.local_name().find('.') == std::string::npos);
    // 全路径含父路径
    REQUIRE(mid.name() == "top.m");

    // 模块类型名：同一类型 ⇒ 相同值
    Plain p1;
    Plain p2;
    REQUIRE(p1.module_type_name() == p2.module_type_name());
    REQUIRE_FALSE(p1.module_type_name().empty());

    // 成层组件 ⇒ 用类级声明的名字（不是 typeid）
    Layered layered;
    REQUIRE(layered.module_type_name() == "my_layer");
}

TEST_CASE("Component naming: a layer MUST declare its module type name at compile time",
          "[component][naming]") {
    // ⚠️ 「成层却忘了命名」**编译期即不可表达** ⇒ ⇒ 合并宏**要求一个名字参数**
    // ⇒ ⇒ 无参形态（CH_HIERARCHY_LAYER;）MUST 编译失败
    // ⇒ ⇒ 该判据由**编译期** fixture + 护栏承载：
    //    tests/fixtures/component_naming/neg_layer_macro_missing_name.cpp
    //    + scripts/verify_component_naming.sh 的 R2
    // ⇒ ⇒ 这里断言的是**正向**那一半：类级名字 MUST **编译期可见**
    // （static constexpr）⇒ ⇒ 否则 ch_layer<T> 无法在
    // 「② 继承」与「③ 必须另给」之间做**编译期**分派（ADR-059 §2.2.1）。
    static_assert(std::string_view(Layered::kChHierarchyLayerName) == "my_layer",
                  "类级模块类型名 MUST 编译期可见（static constexpr）");
    static_assert(Layered::kChIsLayer,
                  "CH_HIERARCHY_LAYER MUST 同时标记「我是一个层」");

    // 运行期：同一份字面量既是编译期可见的，也是运行期返回的
    Layered layered;
    REQUIRE(layered.module_type_name() == "my_layer");
    REQUIRE(layered.module_type_name() == Layered::kChHierarchyLayerName);
}
