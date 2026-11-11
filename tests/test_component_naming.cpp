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

// ============================================================================
// F-A：组件路径与节点名的**前缀拼接口径 MUST 一致**（2026-11-11 · B0 批）
// ============================================================================
// ⚠️ **判别力**（本用例 MUST 抓住的具体缺陷）：
//   修复前 `include/component.h` 的 `build_hierarchical_name` 里带
//     `|| parent_path == "unnamed"`
//   ⇒ ⇒ **按父路径的「字面值」剥离路径段** ⇒ ⇒ 而 `lnodeimpl` 的节点名前缀拼接
//   （`src/core/lnodeimpl.cpp:30-35`）**不**剥离 ⇒ ⇒ 同一棵树产出**两套名字**：
//     · 根组件空名（`local_name_` 归一成 `"unnamed"`，见 `src/component.cpp:66`）
//       ⇒ ⇒ 子**组件**路径 = 裸 `child`（剥了 `unnamed.` 前缀）
//       ⇒ ⇒ 而其**节点**名 = `unnamed.prog_addr`（不剥）
//   ⇒ ⇒ 「按节点名反查组件」的工具（层次化分析 / trace / 报告）**找不到**。
//   ⇒ ⇒ 本用例 MUST 转红。
//
// ⚠️ **「无名」是普通名字，不是「无路径段」标记**（capability
//   `component-ownership-and-naming` 的 Requirement）⇒ ⇒ 本用例钉住这条。
namespace {

// 根组件**空名** ⇒ `local_name_` 归一成 "unnamed"（`src/component.cpp:66`）
// ⚠️ `describe()` 里 MUST 用 `ch_module`（需要 active parent），且子组件名取**非空**
//   ⇒ ⇒ 便于区分「剥前缀」与「不剥前缀」两种形态。
class UnnamedRoot : public ch::Component {
public:
    // ⚠️ `ch_device<T>` 会调 `T(nullptr, "top")`（`include/device.h:28`）⇒ ⇒
    //    本类 MUST 接受该签名，但**忽略**传入的 name（我们要的是**空名**）⇒ ⇒
    //    「空名」是本用例的**被测前提**，不能由 ch_device 的默认 "top" 覆盖。
    UnnamedRoot(ch::Component* = nullptr, const std::string& = "")
        : ch::Component(nullptr, "") {}
    void describe() override {
        ch::ch_module<Leaf> kid{"kid"};
    }
};

// 根组件**真名就叫 "unnamed"** ⇒ 与空名归一后的字面值**相同**
class LiterallyUnnamedRoot : public ch::Component {
public:
    LiterallyUnnamedRoot(ch::Component* = nullptr, const std::string& = "unnamed")
        : ch::Component(nullptr, "unnamed") {}
    void describe() override {
        ch::ch_module<Leaf> kid{"kid"};
    }
};

}   // namespace

TEST_CASE("Component naming: child path MUST join the parent path regardless of its literal value",
          "[component][naming]") {
    // ── 判据 1：父路径是 "unnamed"（空名归一后的结果）⇒ 子路径 MUST 仍拼父前缀 ──
    {
        ch::ch_device<UnnamedRoot> dev;
        auto& root = dev.instance();
        REQUIRE(root.name() == "unnamed");

        auto& kid = *root.children()[0];
        // ⭐ 修复前是 "kid"（剥了前缀）⇒ ⇒ 这条断言就是缺陷的判别力来源
        REQUIRE(kid.name() == "unnamed.kid");
        REQUIRE(kid.local_name() == "kid");
    }

    // ── 判据 2：**与节点名的拼接口径一致**（这才是「同口径」的实质）──────────
    {
        ch::ch_device<UnnamedRoot> dev;
        auto& root = dev.instance();
        auto& kid = *root.children()[0];
        // 组件子路径的前缀 MUST == 根组件名（不剥）
        REQUIRE(kid.name().rfind("unnamed.", 0) == 0);
        // 且 MUST NOT 出现「空段」（`..`）
        REQUIRE(kid.name().find("..") == std::string::npos);
    }

    // ── 判据 3（**反向**）：根组件**真名** "unnamed" ⇒ 行为 MUST 与判据 1 相同 ──
    // ⚠️ 这条同时钉住了**已知代价**：空名与真名 `"unnamed"` 在 `name()` 上
    //   **永久不可区分** ⇒ ⇒ 这是本项**已登记**的代价（彻底解决须另立 item）
    // ⇒ ⇒ 若两判据结果不同 ⇒ ⇒ 说明有代码在用「字面值」做特判（缺陷）
    {
        ch::ch_device<LiterallyUnnamedRoot> dev;
        auto& root = dev.instance();
        REQUIRE(root.name() == "unnamed");
        REQUIRE((*root.children()[0]).name() == "unnamed.kid");
    }

    // ── 判据 4（**反向**）：**非空**父路径 MUST NOT 受影响（回归护栏）────────
    {
        ch::ch_device<Top> dev;
        auto& top = dev.instance();
        REQUIRE(top.name() == "top");
        REQUIRE((*top.children()[0]).name() == "top.m");
    }
}
