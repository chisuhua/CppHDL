/**
 * @file test_lnode_entry_set.cpp
 * @brief `get_lnode` 的**入口集合**契约（**ADR-053**）
 *
 * ## 契约
 *
 * `get_lnode` MUST 只有**两个**入口：
 *
 * | 入口 | 职责 |
 * |---|---|
 * | `get_lnode(const lnode<T> &)`（**恒等**） | 已是 `lnode` ⇒ 原样返回 |
 * | `get_lnode(const T &) requires { t.impl(); }` | **唯一**通用入口（**bundle 亦走它**） |
 *
 * ⇒ MUST NOT 存在**无约束**的第三个通用入口。
 *
 * ## 为什么是「可执行断言」而不是文档
 *
 * 一个**无约束**的 `get_lnode(const T &)` 有**两重**危害：
 *
 * 1. **对有 `impl()` 的类型「死」** —— C++20「约束更严者胜」偏序 ⇒ 受约束版恒胜
 *    ⇒ 无约束版**永不选中** ⇒ 纯死代码；
 * 2. **对无 `impl()` 的类型「有害」** —— 其返回类型**显式** ⇒
 *    `requires { get_lnode(x); }` 只检查重载决议、**不实例化函数体**
 *    ⇒ ⇒ 探测返回 **`true`**（**假阳性**的能力探测）；且报错会指向**框架内部**
 *    （实测：`bundle_base.h: 'multi_if<…>' has no member named 'impl'`）
 *    ⇒ ⇒ 用户误判为框架 bug。
 *
 * ⇒ ⇒ 本文件用**编译期断言**把该契约钉住：任何无约束入口的存在都会让
 *     `static_assert` **转红**。
 *
 * ## ⚠️ 为什么断言放在**本 TU** 而不是 `include/core/lnode.h`
 *
 * `lnode.h` 里的断言只能看到**该点之前**声明的重载 ⇒ 一个在 `lnode.h`
 * **之后**才声明的无约束入口会**逃过**它 —— 那**正是** `bundle_base.h` 的
 * 真实情形（`bundle_base.h` 在 `lnode.h` **之后**被包含）⇒ ⇒ 权威断言
 * MUST 放在**看得见完整头文件集合**的 TU 里 ⇒ ⇒ 即本文件。
 *
 * ## 判别力
 *
 * - **M1**（恢复无约束重载）⇒ 本文件的 `static_assert` **转红**；
 * - **V1**（误删受约束入口 ②）⇒ 本文件的编译期存活探针**编译失败**。
 */

#include "catch_amalgamated.hpp"
#include "ch.hpp"
#include "bundle/common_bundles.h"
#include "component.h"
#include "core/bundle/bundle_base.h"
#include "core/context.h"
#include "core/lnode.h"
#include "core/uint.h"
#include <memory>
#include <type_traits>

namespace {

/// 无 `impl()` 的类型 —— 用于探测 `requires { get_lnode(x); }`
struct no_impl_tag {};

/// 具体的 bundle 类型（ADR-053 的存活探针对象）
using TestBundle = ch::fifo_bundle<ch::core::ch_uint<8>>;

} // namespace

// ===========================================================================
// 权威护栏（编译期）—— 无约束的**通用** `get_lnode` 入口 MUST NOT 存在
// ===========================================================================

/// ⚠️ **必须是依赖上下文**：非依赖的 requires-expression 里，若表达式非法，
/// 编译器 MUST 报**硬错误**（而不是求值为 `false`）⇒ ⇒ 这里用**模板参数**
/// `T` 让探测成为**依赖**的 ⇒ 替换失败 ⇒ concept 求值为 `false`。
template <typename T>
concept can_get_lnode = requires(T x) { ch::core::get_lnode(x); };

static_assert(
    !can_get_lnode<no_impl_tag>,
    "ADR-053: 无约束的**通用** get_lnode 入口 MUST NOT 存在 —— 它对无 impl() 的"
    "类型产生**假阳性**的 requires 探测（返回类型显式 ⇒ requires 不实例化函数体），"
    "且把报错指向框架内部而非调用点。详见 docs/adr/ADR-053-get-lnode-entry-set.md");

/// **正向对照**：有 `impl()` 的类型 MUST 仍能被解析（否则断言恒假 ⇒ 失去判别力）
static_assert(can_get_lnode<ch::core::ch_uint<8>>,
              "ADR-053: 受约束的通用入口 MUST 仍能解析有 impl() 的类型");
static_assert(can_get_lnode<TestBundle>,
              "ADR-053: bundle MUST 仍能经受约束的通用入口解析");

// ===========================================================================
// 存活探针（编译期）—— 两个入口都 MUST 仍然可用
// ===========================================================================

/// **V1-a**：bundle MUST 仍能经**受约束**入口解析（bundle_base 继承
/// `logic_buffer::impl()` ⇒ 有 `impl()`）
static_assert(
    std::is_same_v<
        decltype(ch::core::get_lnode(std::declval<const TestBundle &>())),
        ch::core::lnode<TestBundle>>,
    "ADR-053: bundle MUST 仍经受约束入口解析（误删该入口 ⇒ 本断言编译失败）");

/// **V1-b**：标量硬件类型 MUST 仍能解析
static_assert(
    std::is_same_v<
        decltype(ch::core::get_lnode(std::declval<const ch::core::ch_uint<8> &>())),
        ch::core::lnode<ch::core::ch_uint<8>>>,
    "ADR-053: 标量硬件类型 MUST 仍经受约束入口解析");

/// **V1-c**：**恒等**入口 MUST 仍可用（已是 `lnode` ⇒ 原样返回）
static_assert(
    std::is_same_v<
        decltype(ch::core::get_lnode(
            std::declval<const ch::core::lnode<ch::core::ch_uint<8>> &>())),
        ch::core::lnode<ch::core::ch_uint<8>>>,
    "ADR-053: 恒等入口 MUST 仍可用");

// ===========================================================================
// 运行期
// ===========================================================================

/// 承载 bundle 的组件（bundle 作为成员 ⇒ 与仓内既有 bundle 测试同形态）
class BundleHolder : public ch::Component {
public:
    TestBundle io;

    BundleHolder(ch::Component *parent = nullptr,
                 const std::string &name = "bundle_holder")
        : ch::Component(parent, name) {
        io.as_master();
    }

    void create_ports() override {}
    void describe() override {}
};

TEST_CASE("get_lnode: bundle MUST resolve through the constrained entry",
          "[core][lnode][adr053]") {
    ch::ch_device<BundleHolder> dev;
    ch::core::context *ctx = dev.context();

    auto n = ch::core::get_lnode(dev.instance().io);
    REQUIRE(n.impl() != nullptr);
    REQUIRE(ctx != nullptr);
}

TEST_CASE("get_lnode: a bundle's node MUST be the SAME node the bundle holds",
          "[core][lnode][adr053]") {
    ch::ch_device<BundleHolder> dev;

    auto n = ch::core::get_lnode(dev.instance().io);
    // ⇒ ⇒ 证明「受约束入口」取的就是 bundle **自己**的节点（不是新建的）
    REQUIRE(n.impl() == dev.instance().io.impl());
}
