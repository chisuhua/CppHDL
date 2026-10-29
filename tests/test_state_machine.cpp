/**
 * @file test_state_machine.cpp
 * @brief Unit tests for ch_state_machine DSL
 *
 * ⚠️ ADR-050（2026-10-29）随「死 / 误导 API 的物理移除」更新 ——
 *   ① `current_state()` → `entry_state()`（它**从未**返回运行时状态）；
 *   ② `on_entry` / `on_exit` / `then()` / `tick_state()` / `is_built()` /
 *      `CH_STATE_MACHINE*` **已移除** ⇒ **编译期**断言它们**不存在**；
 *   ③ `set_entry(非 0 枚举)` 现在**显式抛异常**（此前是**静默**的错误复位态
 *      —— `state_reg` 的初值在构造期定死为 `0_d`）；
 *   ④ 「is_in check」用例改为断言**运行时**状态（此前只断言编译期字段 ⇒
 *      **真空断言**：它把「陷阱存在」这件事伪装成「功能正常」）。
 *
 * ⚠️ 另外：本文件原先的 3 个用例**都没有 context** ⇒ `ch_reg` 构造期拿不到
 *    节点（`build_register` 在无 context 时返回 `{nullptr, nullptr}`）⇒ 那几个
 *    用例断言的全是**编译期字段** ⇒ **零硬件覆盖**。现在硬件相关的断言一律
 *    走 `ch_device` + `Simulator`。
 */

#include "chlib/state_machine.h"
#include "component.h"
#include "core/context.h"
#include "device.h"
#include "simulator.h"
#include "catch_amalgamated.hpp"
#include <iostream>
#include <memory>
#include <type_traits>

using namespace ch;
using namespace ch::core;
using namespace chlib;

// Test state enum
enum class TestState : uint8_t {
    STATE_A = 0,
    STATE_B = 1,
    STATE_C = 2
};

namespace {

// ---------------------------------------------------------------------------
// 检测成员函数是否仍存在（ADR-050：被移除的 API MUST NOT 再存在 ⇒ 编译期断言）
//
// ⚠️ 「不存在」这件事**只能编译期**断言：成员函数不存在 ⇒ 无法在运行时引用它
//   ⇒ 用 SFINAE 检测惯用法（`void_t` + `decltype`）。
// ---------------------------------------------------------------------------
template <typename T, typename = void>
struct has_on_entry : std::false_type {};
template <typename T>
struct has_on_entry<T, std::void_t<decltype(std::declval<T &>().on_entry(
                     std::declval<typename T::state_action_t>()))>>
    : std::true_type {};

template <typename T, typename = void>
struct has_on_exit : std::false_type {};
template <typename T>
struct has_on_exit<T, std::void_t<decltype(std::declval<T &>().on_exit(
                     std::declval<typename T::state_action_t>()))>>
    : std::true_type {};

template <typename T, typename = void>
struct has_then : std::false_type {};
template <typename T>
struct has_then<T, std::void_t<decltype(std::declval<T &>().then(
                     std::declval<typename T::state_type>()))>>
    : std::true_type {};

template <typename T, typename = void>
struct has_tick_state : std::false_type {};
template <typename T>
struct has_tick_state<T,
                      std::void_t<decltype(std::declval<T &>().tick_state())>>
    : std::true_type {};

template <typename T, typename = void>
struct has_is_built : std::false_type {};
template <typename T>
struct has_is_built<T,
                    std::void_t<decltype(std::declval<const T &>().is_built())>>
    : std::true_type {};

template <typename T, typename = void>
struct has_current_state : std::false_type {};
template <typename T>
struct has_current_state<
    T, std::void_t<decltype(std::declval<const T &>().current_state())>>
    : std::true_type {};

// ---------------------------------------------------------------------------
// 硬件 DUT：把**运行时**状态寄存器暴露成顶层 `ch_out`（verilator 可观测）
// ---------------------------------------------------------------------------
class StateMachineDut : public ch::Component {
public:
    __io(ch_out<ch_uint<2>> dbg_state;);

    StateMachineDut(ch::Component *parent = nullptr,
                    const std::string &name = "sm_dut")
        : ch::Component(parent, name) {}
    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        chlib::ch_state_machine<TestState, 3> sm;
        sm.state(TestState::STATE_A).on_active([]() {});
        sm.state(TestState::STATE_B).on_active([]() {});
        sm.state(TestState::STATE_C).on_active([]() {});
        sm.set_entry(TestState::STATE_A);
        sm.build();
        // ⚠️ 观测点必须是**运行时**状态寄存器（`current_state_uint()`），
        //    **不是**编译期的 `entry_state()`（后者恒为枚举值 0 ⇒ 真空断言）。
        io().dbg_state = sm.current_state_uint();
    }
};

uint64_t read_port(ch::Simulator &sim, ch::core::lnodeimpl *ln) {
    auto it = sim.data_map().find(ln->id());
    if (it == sim.data_map().end()) return UINT64_MAX;
    return static_cast<uint64_t>(it->second);
}

} // namespace

TEST_CASE("State Machine: removed APIs MUST NOT exist",
          "[state_machine][adr050][compile]") {
    // ⚠️ ADR-050 D2：这些 API 是**死 / 误导**的（`on_exit` 从不执行 ·
    //    `on_entry` 只在 build 期对 entry 态执行一次 · `then()` 是 legacy 自动
    //    退出转移 · `tick_state()` **说谎**（执行编译期 entry 态闭包 + 二次执行
    //    重复发射节点）· `is_built()` 零消费者 · `current_state()` 返回编译期
    //    entry 而非运行时状态）⇒ 物理移除 ⇒ 这里**编译期**钉住。
    using Sm = chlib::ch_state_machine<TestState, 3>;
    STATIC_REQUIRE(!has_on_entry<Sm>::value);
    STATIC_REQUIRE(!has_on_exit<Sm>::value);
    STATIC_REQUIRE(!has_then<Sm>::value);
    STATIC_REQUIRE(!has_tick_state<Sm>::value);
    STATIC_REQUIRE(!has_is_built<Sm>::value);
    STATIC_REQUIRE(!has_current_state<Sm>::value);
}

TEST_CASE("State Machine: entry_state() reports the compile-time entry state",
          "[state_machine][adr050]") {
    // ⚠️ 需要 context：`ch_state_machine` 的 ctor 会建 `ch_reg` 节点。
    auto ctx = std::make_unique<ch::core::context>("sm_ctx");
    ch::core::ctx_swap swap(ctx.get());

    chlib::ch_state_machine<TestState, 3> sm;
    sm.state(TestState::STATE_A).on_active([]() {});
    sm.state(TestState::STATE_B).on_active([]() {});
    sm.state(TestState::STATE_C).on_active([]() {});
    sm.set_entry(TestState::STATE_A);
    sm.build();

    // ⚠️ `entry_state()` 是**编译期**描述值（不是运行时状态）⇒ 运行时状态
    //    MUST 用 `current_state_uint()` / `is_in()`（见下一个用例）。
    REQUIRE(sm.entry_state() == TestState::STATE_A);
}

TEST_CASE("State Machine: set_entry with a non-zero enum MUST throw",
          "[state_machine][adr050][negative]") {
    // ⚠️ ADR-050 硬化：`state_reg` 的初值在**构造期**定死为 `0_d` ⇒ 入口状态
    //    MUST 是**枚举值 0**。此前 `set_entry(STATE_B)` 只是**记录**了一个
    //    非 0 的编译期值 ⇒ 硬件仍复位到 `STATE_A` ⇒ **静默**的错误复位态。
    auto ctx = std::make_unique<ch::core::context>("sm_ctx");
    ch::core::ctx_swap swap(ctx.get());

    chlib::ch_state_machine<TestState, 3> sm;
    REQUIRE_THROWS_AS(sm.set_entry(TestState::STATE_B), std::runtime_error);
    // 枚举值 0 是**合法**的入口 ⇒ MUST NOT 抛。
    REQUIRE_NOTHROW(sm.set_entry(TestState::STATE_A));
}

TEST_CASE("State Machine: runtime state after reset", "[state_machine][adr050]") {
    // ⚠️ 本用例断言**运行时**状态寄存器（此前该文件**没有任何**硬件覆盖 ⇒
    //    「is_in check」用例断言的是编译期字段 ⇒ **真空断言**）。
    ch::ch_device<StateMachineDut> dev;
    ch::Simulator sim(dev.context());
    sim.tick(1);

    auto *state = dev.instance().io().dbg_state.impl();
    REQUIRE(read_port(sim, state) == 0u); // STATE_A（枚举值 0 == 复位值）
}

TEST_CASE("State Machine: compute_state_bits", "[state_machine][utility]") {
    REQUIRE(compute_state_bits(1) == 1);
    REQUIRE(compute_state_bits(2) == 1);
    REQUIRE(compute_state_bits(3) == 2);
    REQUIRE(compute_state_bits(4) == 2);
    REQUIRE(compute_state_bits(5) == 3);
    REQUIRE(compute_state_bits(8) == 3);
    REQUIRE(compute_state_bits(9) == 4);
}
