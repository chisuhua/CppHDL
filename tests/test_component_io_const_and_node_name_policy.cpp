// tests/test_component_io_const_and_node_name_policy.cpp
//
// `component-io-const-and-node-name-policy-2026-11` 的判据。
//
// 契约（两个 capability，各 ADDED ×1）：
//   · `component-io-const-access` —— `__io` 生成的 `io()` MUST 提供 **const** 版本
//     ⇒ ⇒ const 上下文 MUST 可读 `impl()` / `name()` / `is_valid()`
//   · `core-ctx-node-name-uniqueness` —— ctx MUST 提供 **opt-in** 的节点名唯一性策略
//     ⇒ ⇒ 默认 MUST 宽松（既有行为零变化）· 开启后同名 MUST 在**创建点** fail-fast
//     ⇒ ⇒ ctx MUST **不改名**
//
// ⚠️ **判据 MUST 用专门构造的重名 DUT**（`DupNameDut`）：仓内既有设计有 641 个 `ch_in`
//    声明**零**显式名字 ⇒ 拿它们当 DUT 会让「开启策略」直接全仓转红 ⇒ ⇒ 那是**规格
//    预期**，不是缺陷 ⇒ ⇒ 判据必须自带一个**显式重名**的最小 DUT。

#include "catch_amalgamated.hpp"

#include "ch.hpp"
#include "component.h"
#include "core/context.h"
#include "logger.h"

#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

using namespace ch;
using namespace ch::core;

namespace {

// ── U-2：`io() const` 的编译期探针 ─────────────────────────────────────────
class IoConstProbe : public ch::Component {
public:
    __io(ch_in<ch_uint<8>> a; ch_out<ch_uint<8>> y;)

    IoConstProbe(ch::Component *parent = nullptr,
                 const std::string &name = "io_const_probe")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override { io().y <<= io().a; }

    /// **const 上下文**读 IO —— 本函数的存在本身就是判据（去掉 const 重载即编译失败）
    bool io_readable_in_const() const {
        const auto &ports = io(); // ← 依赖 const 重载
        return ports.a.impl() != nullptr && !ports.a.name().empty() &&
               ports.a.is_valid() && !ports.y.name().empty();
    }
};

// 编译期判据 ①：const 对象的 `io()` MUST 返回 **const** 引用
static_assert(
    std::is_same_v<decltype(std::declval<const IoConstProbe &>().io()),
                   const IoConstProbe::io_type &>,
    "const io() MUST return const io_type&");

// 编译期判据 ②：非 const 对象的 `io()` MUST 仍返回**可变**引用（既有代码零变化）
static_assert(std::is_same_v<decltype(std::declval<IoConstProbe &>().io()),
                             IoConstProbe::io_type &>,
              "non-const io() MUST keep returning io_type&");

// ── U-1：静态析构期降级用的 RAII（异常安全 ⇒ REQUIRE 失败也不会污染后续用例）──
class StaticDestructionGuard {
public:
    StaticDestructionGuard() { ch::detail::in_static_destruction() = true; }
    ~StaticDestructionGuard() { ch::detail::in_static_destruction() = false; }
    StaticDestructionGuard(const StaticDestructionGuard &) = delete;
    StaticDestructionGuard &operator=(const StaticDestructionGuard &) = delete;
};

// ── U-1：**专门构造的重名 DUT**（判据目标：`describe()` 里两个显式同名节点）──
class DupNameDut : public ch::Component {
public:
    __io(ch_out<ch_uint<8>> y;)

    DupNameDut(ch::Component *parent = nullptr,
               const std::string &name = "dup_name_dut")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        auto *first = context()->create_input(8, "same_name");
        auto *second = context()->create_input(8, "same_name"); // ← 重名（判据目标）
        io().y <<= ch_uint<8>(first) ^ ch_uint<8>(second);
    }
};

} // namespace

// ===========================================================================
// U-2：`io() const`
// ===========================================================================

TEST_CASE("io() const：const 上下文 MUST 可读 IO", "[io_const][component]") {
    ch::ch_device<IoConstProbe> dev;
    const IoConstProbe &cref = dev.instance();

    REQUIRE(cref.io_readable_in_const());
    // const 引用上的 io() 也 MUST 可调用，且三个访问器全部可读
    REQUIRE(cref.io().a.impl() != nullptr);
    REQUIRE_FALSE(cref.io().a.name().empty());
    REQUIRE(cref.io().a.is_valid());
    REQUIRE(cref.io().y.impl() != nullptr);
}

TEST_CASE("io() const：非 const 对象 MUST 仍可写（纯新增 · 零破坏）",
          "[io_const][component]") {
    ch::ch_device<IoConstProbe> dev;
    auto &ports = dev.instance().io();
    // 非 const ⇒ 解析到非 const 重载（赋值得以成立）
    static_assert(std::is_same_v<decltype(dev.instance().io()),
                                 IoConstProbe::io_type &>);
    ports.a <<= ch_uint<8>(1_d);
    REQUIRE(ports.a.impl() != nullptr);
}

// ===========================================================================
// U-1：opt-in 节点名唯一性
// ===========================================================================

TEST_CASE("节点名策略：默认宽松 ⇒ 同名可共存且 MUST NOT 改名",
          "[node_name_policy][ctx]") {
    ch::core::context ctx("dup_default");
    ch::core::ctx_swap swap(&ctx);

    REQUIRE(ctx.node_name_policy() == ch::core::NodeNamePolicy::kAllowDuplicates);

    auto *first = ctx.create_input(8, "dup");
    auto *second = ctx.create_input(8, "dup"); // MUST NOT 抛
    REQUIRE(first != nullptr);
    REQUIRE(second != nullptr);

    // ctx MUST NOT 自动改名（既不自动加后缀，也不静默替换）
    REQUIRE(first->name() == "dup");
    REQUIRE(second->name() == "dup");
    REQUIRE(ctx.duplicate_name_violations().empty());
}

TEST_CASE("节点名策略：严格模式下**唯一**名字 MUST 不报错（无假阳性）",
          "[node_name_policy][ctx]") {
    ch::core::context ctx("unique_strict");
    ch::core::ctx_swap swap(&ctx);
    ctx.set_node_name_policy(ch::core::NodeNamePolicy::kRejectDuplicates);

    REQUIRE_NOTHROW(ctx.create_input(8, "alpha"));
    REQUIRE_NOTHROW(ctx.create_input(8, "beta"));
    REQUIRE(ctx.duplicate_name_violations().empty());
}

TEST_CASE("节点名策略：严格模式 ⇒ 创建点抛异常且错误信息含冲突名",
          "[node_name_policy][ctx]") {
    ch::core::context ctx("dup_strict");
    ch::core::ctx_swap swap(&ctx);
    ctx.set_node_name_policy(ch::core::NodeNamePolicy::kRejectDuplicates);

    auto *first = ctx.create_input(8, "same_name");
    REQUIRE(first != nullptr);

    std::string caught_text;
    bool threw = false;
    try {
        (void)ctx.create_input(8, "same_name");
    } catch (const std::runtime_error &e) {
        threw = true;
        caught_text = e.what();
    }

    REQUIRE(threw); // MUST 抛（MUST NOT 只打日志 / MUST NOT 静默返回空）
    REQUIRE(caught_text.find("same_name") != std::string::npos);
    REQUIRE(caught_text.find("kRejectDuplicates") != std::string::npos);
}

TEST_CASE("节点名策略：严格性 MUST 是**每 ctx** 的（跨 ctx 不互相影响）",
          "[node_name_policy][ctx]") {
    ch::core::context outer("outer_ctx");
    ch::core::context inner("inner_ctx", &outer);

    {
        ch::core::ctx_swap swap(&outer);
        outer.set_node_name_policy(ch::core::NodeNamePolicy::kRejectDuplicates);
        REQUIRE_NOTHROW(outer.create_input(8, "same_name"));
    }
    {
        ch::core::ctx_swap swap(&inner);
        inner.set_node_name_policy(ch::core::NodeNamePolicy::kRejectDuplicates);
        // inner 自己的名字空间里没有同名者 ⇒ MUST 不抛
        REQUIRE_NOTHROW(inner.create_input(8, "same_name"));
    }
}

TEST_CASE("节点名策略：可切回宽松（策略是状态，不是一次性开关）",
          "[node_name_policy][ctx]") {
    ch::core::context ctx("dup_toggle");
    ch::core::ctx_swap swap(&ctx);

    ctx.set_node_name_policy(ch::core::NodeNamePolicy::kRejectDuplicates);
    REQUIRE_NOTHROW(ctx.create_input(8, "toggle"));
    REQUIRE_THROWS_AS(ctx.create_input(8, "toggle"), std::runtime_error);

    ctx.set_node_name_policy(ch::core::NodeNamePolicy::kAllowDuplicates);
    REQUIRE_NOTHROW(ctx.create_input(8, "toggle"));
}

TEST_CASE("节点名策略：静态析构期降级为「记录 + 失败结果」（MUST NOT 抛）",
          "[node_name_policy][ctx]") {
    ch::core::context ctx("dup_static_destruction");
    ch::core::ctx_swap swap(&ctx);
    ctx.set_node_name_policy(ch::core::NodeNamePolicy::kRejectDuplicates);
    REQUIRE(ctx.create_input(8, "dup") != nullptr);

    ch::core::inputimpl *second = nullptr;
    bool threw = false;
    {
        StaticDestructionGuard guard; // 该期内抛异常会 std::terminate
        try {
            second = ctx.create_input(8, "dup");
        } catch (...) {
            threw = true;
        }
    }

    REQUIRE_FALSE(threw);        // 降级：不抛
    REQUIRE(second == nullptr);  // 失败结果：创建失败（不放进 ctx）
    REQUIRE(ctx.duplicate_name_violations().size() == 1);
    REQUIRE(ctx.duplicate_name_violations()[0] == "dup");
}

TEST_CASE("节点名策略：**先宽松建、后开严格** ⇒ 既有名字 MUST 同样受保护",
          "[node_name_policy][ctx]") {
    // ⚠️ 本用例是 critic BLOCKER 的闭合探针：若切严格时**不**播种既有节点
    //    ⇒ 下面两次创建都会**不抛** ⇒ 保护是幻觉（假绿）。
    ch::core::context ctx("seed_existing");
    ch::core::ctx_swap swap(&ctx);

    // ① 宽松期建一个用户节点 + ② 宽松期已由 `init()` 建好 default_clock
    REQUIRE_NOTHROW(ctx.create_input(8, "foo"));
    REQUIRE(ctx.has_default_clock());

    ctx.set_node_name_policy(ch::core::NodeNamePolicy::kRejectDuplicates);

    // ③ 与宽松期建的 "foo" 同名 ⇒ MUST 抛
    REQUIRE_THROWS_AS(ctx.create_input(8, "foo"), std::runtime_error);
    // ④ 与 `init()` 期建的 "default_clock" 同名 ⇒ MUST 抛（证明播种覆盖构造期节点）
    REQUIRE_THROWS_AS(ctx.create_input(8, "default_clock"), std::runtime_error);
}

TEST_CASE("节点名策略：切回宽松会清空登记，再开严格会重新播种",
          "[node_name_policy][ctx]") {
    ch::core::context ctx("seed_reset");
    ch::core::ctx_swap swap(&ctx);

    REQUIRE_NOTHROW(ctx.create_input(8, "bar"));
    ctx.set_node_name_policy(ch::core::NodeNamePolicy::kRejectDuplicates);
    REQUIRE_THROWS_AS(ctx.create_input(8, "bar"), std::runtime_error);

    // 切回宽松 ⇒ 放行
    ctx.set_node_name_policy(ch::core::NodeNamePolicy::kAllowDuplicates);
    REQUIRE_NOTHROW(ctx.create_input(8, "bar"));

    // 再开严格 ⇒ 重新播种（此时 "bar" 已经有多份 ⇒ 新创建 MUST 仍抛）
    ctx.set_node_name_policy(ch::core::NodeNamePolicy::kRejectDuplicates);
    REQUIRE_THROWS_AS(ctx.create_input(8, "bar"), std::runtime_error);
}

TEST_CASE("重名 DUT：严格模式 ⇒ build MUST 抛；宽松模式 ⇒ build MUST 成功",
          "[node_name_policy][component]") {
    // ── 宽松（默认）⇒ 两个同名节点共存 ⇒ 构建成功 ──
    {
        ch::core::context ctx("dup_dut_lax");
        ch::core::ctx_swap swap(&ctx);
        auto dut = std::make_unique<DupNameDut>(nullptr, "dup_dut_lax");
        REQUIRE(ctx.node_name_policy() ==
                ch::core::NodeNamePolicy::kAllowDuplicates);
        REQUIRE_NOTHROW(dut->build(&ctx));
    }
    // ── 严格 ⇒ 第二个同名节点在创建点抛 ⇒ 异常从 build() 冒出 ──
    {
        ch::core::context ctx("dup_dut_strict");
        ch::core::ctx_swap swap(&ctx);
        ctx.set_node_name_policy(ch::core::NodeNamePolicy::kRejectDuplicates);
        auto dut = std::make_unique<DupNameDut>(nullptr, "dup_dut_strict");

        bool threw = false;
        std::string caught_text;
        try {
            dut->build(&ctx);
        } catch (const std::runtime_error &e) {
            threw = true;
            caught_text = e.what();
        }
        REQUIRE(threw);
        REQUIRE(caught_text.find("same_name") != std::string::npos);
    }
}
