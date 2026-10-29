#pragma once

/**
 * @file state_machine.h
 * @brief CppHDL State Machine DSL
 * 
 * Provides SpinalHDL-like state machine support for CppHDL.
 * 
 * Usage Example (CORRECT FORM — see the discipline list below):
 * @code{.cpp}
 * enum class MyState : uint8_t { IDLE, RUNNING, DONE };
 *
 * class MyModule : public ch::Component {
 * public:
 *     void describe() override {
 *         ch_state_machine<MyState, 3> sm;
 *
 *         // Hardware conditions are NODES, not C++ booleans.
 *         auto start_sig = io().start;            // ch_bool
 *
 *         // The state closure registers TRANSITIONS ONLY.
 *         sm.state(MyState::IDLE).on_active([&] {
 *             sm.transition_when(start_sig, MyState::RUNNING);
 *         });
 *         sm.state(MyState::RUNNING).on_active([&] {
 *             sm.transition_when(done_sig, MyState::DONE);
 *         });
 *
 *         sm.set_entry(MyState::IDLE);
 *         sm.build();
 *
 *         // Registers / outputs are driven AFTER build(), exactly once each.
 *         counter->next = select(sm.is_in(MyState::IDLE) & start_sig,
 *                                ch_uint<8>(1_d), counter);
 *         io().busy = select(sm.is_in(MyState::RUNNING),
 *                            ch_bool(true), ch_bool(false));
 *     }
 * };
 * @endcode
 *
 * ===========================================================================
 * ⚠️ 写 FSM 的 5 条纪律（MUST；完整契约见 capability
 *    `chlib-state-machine-semantics`）
 * ===========================================================================
 *
 * ① **条件逻辑只用硬件原语**（`transition_when` / `select`）。MUST NOT 让硬件值
 *    参与 C++ 控制流 —— `if (硬件值)` / 三元 `? :` / `switch` / `while`-`for`
 *    的行程条件 / early `return` / 传给 `bool` 形参。
 *    ⚠️ `ch_bool` / `ch_uint` 的 `operator uint64_t()` **只对 const 节点返真值**
 *    （`src/lnode/bool.cpp:44-49`）⇒ 上述写法在**建图期**恒假 ⇒ 条件逻辑
 *    **不发射**（无 else 的 `if` 分支体永不发射、有 else 的只发射 else）。
 *
 * ② **每个寄存器只有一个 `next` 写入点**。各状态的候选值用 `select` 组合成
 *    **单一表达式**，写在 `sm.build()` **之后**。
 *    ⚠️ `ch_reg` 的 `next` 是 **last-wins**：`regimpl::set_next` 就是
 *    `set_src(0, next)`（`include/ast/ast_nodes.h:47-54`）⇒ 多次赋值时
 *    **最后一次覆盖** ⇒ 「每状态各写一次」**无法**表达多状态 FSM。
 *
 * ③ **每个输出只驱动一次**（按 `sm.is_in(...)` 多路选择）。
 *    ⚠️ `ch_out` 的 `operator=` 也是 `set_src(0, …)` ⇒ 多状态各驱动一次 ⇒
 *    **最后发射者获胜**。
 *
 * ④ **`transition_when` 的条件是电平敏感**（每拍求值、为真即走），**不是**
 *    「事件 / 完成」语义 ⇒ 「持续 N 拍后转移」MUST 与计数器条件**合取**
 *    （例：`bit_last & baud_done`）。
 *
 * ⑤ **不把 parity 当正确性证据**（两后端由**同一份** fixture 生成 ⇒ 缺陷
 *    **同构** ⇒ 两边**同时错** ⇒ parity **恒绿**）。
 *
 * ===========================================================================
 * ⚠️ 两条**框架级**陷阱（读这段再写 FSM）
 * ===========================================================================
 *
 * **(A) `build()` 遍历【所有】state 执行各自 `on_active`** ——
 *    `build()` 按 state index 依次执行每个状态的 `on_active` 闭包（**一次性**，
 *    在 describe 期），以便把 `transition_when` 注册进 select 树。
 *    ⇒ 因此 **`on_active` 闭包内 MUST NOT 写寄存器 `next`、MUST NOT 驱动
 *    输出**：那些写入会被**后执行的状态覆盖**（last-wins）⇒ 只有最后发射的
 *    那个状态生效 ⇒ 典型症状 = 「装载数据的分支**永不生效**」。
 *    ⇒ 正确形态：闭包内**只**写 `transition_when`；寄存器与输出写在
 *    `build()` **之后**（纪律 ② / ③）。
 *
 * **(B) `transition_when` 的 `target` MUST 是【编译期常量】** ——
 *    转移在 `build()` 注册时就把 target 固化了 ⇒ MUST NOT 用三元
 *    `cond ? A : B` 选 target（三元在**建图期**求值 ⇒ 恒取一个分支 ⇒
 *    另一个目标状态**永不可达** ⇒ 状态卡死）。
 *    ⇒ 正确形态：拆成**多条** `transition_when`（条件**互斥**）。
 *
 * ===========================================================================
 * 现状契约（把实现语义写死 ⇒ 将来若要改，MUST 走 OpenSpec MODIFIED delta）
 * ===========================================================================
 *
 * - `build()` 执行每个状态的 `on_active` 闭包一次（describe 期），并把
 *   期间注册的 `transition_when` 组合成 `state_reg->next` 的 select 树：
 *   `next = state_reg`（默认保持），再对每个转移套
 *   `select(is_in(S) & cond, target, next)` ⇒ **后注册者优先**。
 * - 该 select 树由 Simulator **每拍**求值 ⇒ 周期精确的多周期 FSM。
 */

#include "ch.hpp"
#include "core/reg.h"
#include "core/uint.h"
#include "core/bool.h"
#include "core/operators.h"
#include "utils/logger.h"
#include <array>
#include <functional>
#include <vector>
#include <cstdint>

namespace chlib {

using namespace ch::core;

/**
 * @brief Compute number of bits needed to represent N states
 */
constexpr unsigned compute_state_bits(unsigned n) {
    if (n <= 1) return 1;
    unsigned bits = 0;
    unsigned v = n - 1;
    while (v > 0) {
        v >>= 1;
        bits++;
    }
    return bits;
}

/**
 * @brief State Machine DSL for CppHDL
 * 
 * @tparam StateEnum Enum type defining the states
 * @tparam N Number of states
 */
template <typename StateEnum, size_t N>
class ch_state_machine {
public:
    using state_type = StateEnum;
    using state_action_t = std::function<void()>;
    using transition_check_t = std::function<bool()>;
    
    static constexpr unsigned STATE_BITS = compute_state_bits(N);
    static constexpr size_t NUM_STATES = N;
    
    /**
     * @brief State definition
     */
    struct state_def {
        state_action_t active_action;
        
        // Phase 6d.6 (ADR-046): runtime transitions accumulated by
        // transition_when() / transition_to() during build(). Each entry is
        // a (condition, target) pair composed into state_reg->next via a
        // ch select-tree at build() time.
        struct runtime_transition {
            ch_bool cond;
            StateEnum target;
        };
        std::vector<runtime_transition> transitions;
        
        /**
         * @brief Set active action (executed while in this state)
         *
         * NOTE (ADR-050): the closure MUST register transitions ONLY
         * (`transition_when` / `transition_to`). Writing a register's `next`
         * or driving an IO port inside it is a **silent** breakage — see
         * the discipline list in this file's header.
         */
        state_def& on_active(state_action_t action) {
            active_action = action;
            return *this;
        }
    };
    
private:
    // State definitions array
    std::array<state_def, N> states_;
    
    // Current state register
    ch_reg<ch_uint<STATE_BITS>> state_reg;
    
    // Next state combinational logic
    ch_uint<STATE_BITS> next_state;
    
    // Entry state（**编译期**描述值；运行时状态请用 current_state_uint() /
    // is_in()）。⚠️ MUST 在 ctor 初始化列表中初始化（ADR-050 硬化：此前未初始化
    // ⇒ 读取它是 UB）。
    StateEnum entry_state_;
    
    // Build flag
    bool built;
    
    // Phase 6d.6: state whose on_active closure is currently executing
    // (used by transition_when/transition_to to attribute the transition).
    StateEnum cur_state_;
    
    /**
     * @brief Reject an out-of-range state enum value
     *
     * ADR-050 硬化：此前 `states_[static_cast<size_t>(s)]` **无边界检查** ⇒
     * 越界的枚举值是 **UB**（越界读写）。这里改为**显式拒绝**。
     */
    static void require_state_in_range(StateEnum s, const char *where) {
        if (static_cast<size_t>(s) >= N) {
            CHFATAL_EXCEPTION(
                "%s: state enum value %u is out of range (N=%zu)",
                where, static_cast<unsigned>(s), static_cast<size_t>(N));
        }
    }
    
public:
    /**
     * @brief Constructor
     */
    ch_state_machine() : state_reg(0_d), next_state(0_d),
                         entry_state_(static_cast<StateEnum>(0)), built(false),
                         cur_state_(static_cast<StateEnum>(0)) {
        // Initialize state definitions
        for (size_t i = 0; i < N; i++) {
            states_[i].active_action = nullptr;
        }
    }
    
    /**
     * @brief Get state definition for a given state
     */
    state_def& state(StateEnum s) {
        require_state_in_range(s, "ch_state_machine::state");
        cur_state_ = s;
        return states_[static_cast<size_t>(s)];
    }
    
    /**
     * @brief Get state definition for a given state (const version)
     */
    const state_def& state(StateEnum s) const {
        require_state_in_range(s, "ch_state_machine::state");
        return states_[static_cast<size_t>(s)];
    }
    
    /**
     * @brief Transition to a new state
     * 
     * Unconditional runtime transition from the currently-being-defined
     * state. Equivalent to transition_when(ch_bool(true), s).
     */
    void transition_to(StateEnum s) {
        transition_when(ch_bool(true), s);
    }
    
    /**
     * @brief Conditioned runtime transition (Phase 6d.6 / ADR-046)
     * 
     * Records a hardware transition from the currently-being-defined state:
     *   when state_reg == cur_state_ & cond  ->  next = target
     * Composed into the next-state select tree by build().
     * 
     * NOTE: unlike `if (ch_bool)` (which silently evaluates false at describe
     * time), `cond` is a real ch_bool hardware signal evaluated by the
     * Simulator every tick — this enables cycle-accurate multi-cycle FSMs.
     */
    void transition_when(const ch_bool& cond, StateEnum target) {
        require_state_in_range(cur_state_,
                               "ch_state_machine::transition_when (owner)");
        require_state_in_range(target,
                               "ch_state_machine::transition_when (target)");
        states_[static_cast<size_t>(cur_state_)].transitions.push_back(
            {cond, target});
    }
    
    /**
     * @brief Get the **entry** state (compile-time describe value)
     *
     * NOTE (ADR-050): renamed from `current_state()` — it never returned the
     * runtime state. For the runtime state register value use
     * `current_state_uint()` / `is_in()`.
     */
    StateEnum entry_state() const {
        return entry_state_;
    }
    
    /**
     * @brief Get current state as ch_uint
     */
    ch_uint<STATE_BITS> current_state_uint() const {
        return state_reg;
    }
    
    /**
     * @brief Check if in a specific state (hardware comparison, runtime)
     */
    ch_bool is_in(StateEnum s) const {
        require_state_in_range(s, "ch_state_machine::is_in");
        // ⚠️ ADR-050 硬化：此前用 `static_cast<uint8_t>(s)` ⇒ STATE_BITS > 8 时
        // 会**静默截断** ⇒ 指向错误状态。改用整型字面量构造（宽度 = STATE_BITS）。
        return state_reg == ch_uint<STATE_BITS>(static_cast<uint32_t>(s));
    }
    
    /**
     * @brief Set entry state
     */
    void set_entry(StateEnum s) {
        // ADR-050 硬化：`state_reg` 的初值在**构造期**定死为 `0_d` ⇒ 入口状态
        // MUST 是**枚举值 0**；此前非 0 的入口会**静默**产生错误的复位态
        // （`entry_state` 被记录成非 0，但硬件仍复位到 0）。
        if (static_cast<uint32_t>(s) != 0) {
            CHFATAL_EXCEPTION(
                "ch_state_machine::set_entry: the entry state MUST be the enum "
                "value 0 (the state register's reset value is fixed at 0_d); "
                "got %u",
                static_cast<unsigned>(s));
        }
        entry_state_ = s;
        // NOTE (Phase 6d.6): do NOT clobber state_reg's node with a literal.
    }
    
    /**
     * @brief Build the state machine (generate state register and logic)
     * 
     * Executes each state's on_active closure once at describe time to emit
     * combinational logic and collect transitions, then composes a ch
     * select-tree for state_reg->next:
     *   next = state_reg                          (hold by default)
     *   for each state S, transition (cond,target):
     *     next = select(is_in(S) & cond, target, next)
     * 
     * The Simulator evaluates this tree every tick, giving cycle-accurate
     * multi-cycle FSM behavior (Phase 6d.6 / ADR-046).
     */
    void build() {
        if (built) return;
        
        // Set initial state
        // state_reg init value fixed at construction; entry_state_ recorded
        // for entry_state() reporting.
        
        // Phase 6d.6: run every state's active action at describe time so
        // its transition_when()/transition_to() calls (and any combinational
        // output assignments) are emitted into the DAG.
        for (size_t i = 0; i < N; i++) {
            cur_state_ = static_cast<StateEnum>(i);
            if (states_[i].active_action) {
                states_[i].active_action();
            }
        }
        
        // Compose next-state select tree. Default: hold current state.
        // Later-registered transitions have higher priority (last select
        // wins) — register exclusive conditions to avoid ambiguity.
        ch_uint<STATE_BITS> hold = state_reg;
        next_state = hold;
        for (size_t i = 0; i < N; i++) {
            const auto& sd = states_[i];
            auto in_s = is_in(static_cast<StateEnum>(i));
            for (const auto& tr : sd.transitions) {
                // ⚠️ ADR-050 硬化：此前 `static_cast<uint8_t>(tr.target)` 在
                // STATE_BITS > 8 时会**静默截断** ⇒ 转移指向错误状态。
                auto target_lit =
                    ch_uint<STATE_BITS>(static_cast<uint32_t>(tr.target));
                next_state = select(in_s & tr.cond, target_lit, next_state);
            }
        }
        
        // State update (sequential)
        state_reg->next = next_state;
        
        built = true;
    }
    
};
} // namespace chlib
