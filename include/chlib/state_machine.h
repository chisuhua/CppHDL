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
#include <exception>
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
    
    // ------------------------------------------------------------------
    // ADR-050 D4：状态守卫贡献（state-guarded contribution）
    // ------------------------------------------------------------------
    //
    // 一条 contribution = 「在状态 S 下，target 的值是 value」。
    // resolve_* 把同一 target 的全部贡献合成**一条** select 链（守卫 = is_in(S)），
    // **写入动作仍由用户自己做**（`reg->next = sm.resolve_hold(reg);`）⇒
    // `ch_reg` 的单 next 源契约**零变化**。
    struct contribution {
        lnodeimpl *target;  // 目标节点（reg 的 proxy / output 的 impl）
        lnodeimpl *reg;     // 非空 ⇒ 目标是寄存器（用于查「已有 next 源」）
        StateEnum state;    // 归属状态（守卫 = is_in(state)）
        lnodeimpl *value;   // 贡献值（**eager** 捕获的节点）
    };
    std::vector<contribution> contributions_;
    std::vector<lnodeimpl *> resolved_targets_;
    
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
    
private:
    template <unsigned TargetW, unsigned ValueW, typename V>
    void add_contribution(lnodeimpl *target, lnodeimpl *reg, StateEnum s,
                          const V &value) {
        // (1) 状态枚举越界（`states_[…]` 越界是 UB）
        require_state_in_range(s, "ch_state_machine::contrib (state)");
        // (2) 宽度：value 不得**窄于** target（否则 select 的 max(width) 会
        //     **零扩展** ⇒ 语义被改变）⇒ 显式拒绝。
        //     value **宽于** target 是允许的（手写 select 链同样如此：链宽 =
        //     max(...) ⇒ 最终在寄存器赋值处截断）⇒ 此处**显式截断**到 target
        //     宽度 ⇒ 与手写链**逐位等价**。
        if (ValueW < TargetW) {
            CHFATAL_EXCEPTION(
                "ch_state_machine::contrib: value width %u is narrower than "
                "target width %u (the select tree would zero-extend => the "
                "value's semantics would change)",
                ValueW, TargetW);
        }
        // (3) 不得贡献给状态寄存器自身（会覆盖 build() 写的转移树 ⇒ 转移全死）
        if (target == state_reg.impl()) {
            CHFATAL_EXCEPTION(
                "ch_state_machine::contrib: the state register itself MUST NOT "
                "receive a contribution");
        }
        // (4) 同一 (target, state) 不得重复贡献
        for (const auto &c : contributions_) {
            if (c.target == target && c.state == s) {
                CHFATAL_EXCEPTION(
                    "ch_state_machine::contrib: duplicate contribution for the "
                    "same target and state");
            }
        }
        // (5) 目标已有 next 源 ⇒ 「直接写 + 贡献写」并存
        if (reg && reg->num_srcs() > 0) {
            CHFATAL_EXCEPTION(
                "ch_state_machine::contrib: the target already has a next "
                "source (direct write + contribution MUST NOT coexist)");
        }
        // ⚠️ value 在**调用点**捕获（eager）⇒ 之后重绑定该 C++ 对象会让本贡献
        //    持有**旧节点**（ADR-042 意义上的 operator=）⇒ 静默过期。
        lnodeimpl *v = to_operand(value).impl();
        if constexpr (ValueW > TargetW) {
            // 显式截断到 target 宽度（与手写 select 链同型 ⇒ 逐位等价）
            v = bits<TargetW - 1, 0>(ch_uint<ValueW>(v)).impl();
        }
        contributions_.push_back({target, reg, s, v});
    }
    
    bool is_resolved(lnodeimpl *target) const {
        for (auto *t : resolved_targets_) {
            if (t == target) return true;
        }
        return false;
    }
    
    void check_resolvable(lnodeimpl *target, lnodeimpl *reg) {
        if (is_resolved(target)) {
            CHFATAL_EXCEPTION(
                "ch_state_machine::resolve: the same target MUST NOT be "
                "resolved twice");
        }
        if (reg && reg->num_srcs() > 0) {
            CHFATAL_EXCEPTION(
                "ch_state_machine::resolve: the target already has a next "
                "source (direct write + contribution MUST NOT coexist)");
        }
    }
    
    void mark_resolved(lnodeimpl *target) {
        resolved_targets_.push_back(target);
    }
    
    template <unsigned W, typename V>
    static ch_uint<W> default_expr(const V &def) {
        return ch_uint<W>(to_operand(def).impl());
    }
    
    /// 把同一 target 的全部贡献合成**一条** select 链（守卫 = is_in(S)）。
    template <unsigned W>
    ch_uint<W> compose(lnodeimpl *target, const ch_uint<W> &acc0) {
        // 各 is_in(S) 只算一次（避免重复建比较节点）
        std::array<ch_bool, N> in_s{};
        for (size_t i = 0; i < N; ++i) {
            in_s[i] = is_in(static_cast<StateEnum>(i));
        }
        ch_uint<W> acc = acc0;
        for (size_t i = 0; i < N; ++i) {
            for (const auto &c : contributions_) {
                if (c.target != target) continue;
                if (static_cast<size_t>(c.state) != i) continue;
                // ⚠️ 必须是**一条**表达式链（每状态一个 mux 组件会重新引入多驱动）
                acc = select(in_s[i], ch_uint<W>(c.value), acc);
            }
        }
        return acc;
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
    
    /**
     * @brief Destructor: reject contributions that were never resolved
     *
     * ADR-050 D5：「contrib 了但忘了 resolve」是**静默**失效（贡献被丢弃 ⇒
     * 寄存器保持旧值）⇒ MUST 检出。
     *
     * ⚠️ 析构期抛异常 ⇒ 若正在**展开**中 ⇒ std::terminate ⇒ 故只在
     *    「无待处理异常」且「非静态析构期」时抛。
     */
    // ⚠️ `noexcept(false)`：析构函数默认是 noexcept ⇒ 析构期抛异常会
    //    `std::terminate` ⇒ 必须显式关闭 ⇒ 否则「未 resolve」的检出变成 abort
    //    （不可捕获 ⇒ 不可测）。⚠️ 展开期（`uncaught_exceptions() > 0`）**不抛**
    //    ⇒ 避免 terminate。
    ~ch_state_machine() noexcept(false) {
        for (const auto &c : contributions_) {
            if (is_resolved(c.target)) continue;
            if (std::uncaught_exceptions() == 0 &&
                !ch::detail::in_static_destruction()) {
                CHFATAL_EXCEPTION(
                    "ch_state_machine: contributions were registered but never "
                    "resolved (call resolve_hold / resolve_or for every target)");
            }
            return;
        }
    }
    
    /**
     * @brief Register a state-guarded contribution for a **register**
     *
     * @param target  目标寄存器
     * @param s       归属状态（守卫 = `is_in(s)`）
     * @param value   贡献值（**已建好的**表达式；MUST NOT 用 lambda 延迟求值）
     *
     * ⚠️ MUST 在 `on_active` 闭包**之外**调用（ADR-050 D4）⇒ 闭包内**只**
     *    注册转移。
     * ⚠️ value 的宽度 MUST 与 target 相同（否则 `select` 的 `max(width)` 会
     *    **静默**截断 / 扩展）⇒ 宽度不符即**显式拒绝**。
     */
    template <typename T, typename V>
    void contrib(const ch_reg<T> &target, StateEnum s, const V &value) {
        // `ch_reg` 的 impl() 是**代理**节点；其 src(0) 才是**寄存器本体**
        // （`build_register`：`proxyimpl(reg_node, ...)`）。
        add_contribution<ch_width_v<T>, ch_width_v<V>>(
            target.impl(), target.impl()->src(0), s, value);
    }
    
    /**
     * @brief Register a state-guarded contribution for an **output port**
     */
    template <typename T, typename V>
    void contrib(const port<T, output_direction> &target, StateEnum s,
                 const V &value) {
        add_contribution<ch_width_v<T>, ch_width_v<V>>(target.impl(),
                                                         nullptr, s, value);
    }
    
    // ⚠️ **故意不提供** `port<T, input_direction>` 的 `contrib` 重载 ——
    //    输入端口由**外部**驱动；给它接状态逻辑会让它**不再是顶层端口**
    //    （见根 AGENTS.md「Hierarchical input driver MUST be emitted by
    //    codegen」）⇒ **编译不过**（无匹配重载）。
    //
    //    ⚠️ 为何**不用** `= delete`：被删除的调用在 SFINAE 语境下是**硬错误**
    //    （实测）⇒ 无法写成**自动化**的编译期断言 ⇒ 「无匹配重载」形态才可被
    //    `std::void_t` 检测 ⇒ 才有**可执行**的验收判据（ADR-050 §5.2）。
    
    /**
     * @brief Compose a register's contributions; default tail = **hold**
     *
     * 用法：`reg->next = sm.resolve_hold(reg);` ⇒ **唯一**写入点仍由用户持有。
     */
    template <typename T>
    ch_uint<ch_width_v<T>> resolve_hold(ch_reg<T> &target) {
        lnodeimpl *tgt = target.impl();
        check_resolvable(tgt, tgt->src(0));
        // 起点 = 寄存器**当前值**（代理节点）⇒ 未覆盖的状态 ⇒ 保持。
        ch_uint<ch_width_v<T>> acc = ch_uint<ch_width_v<T>>(tgt);
        acc = compose(tgt, acc);
        mark_resolved(tgt);
        return acc;
    }
    
    /**
     * @brief Compose a register's contributions with an **explicit default**
     *
     * 「未覆盖的状态取某常量」时使用（默认尾部**不是**保持）。
     */
    template <typename T, typename V>
    ch_uint<ch_width_v<T>> resolve_or(ch_reg<T> &target, const V &def) {
        lnodeimpl *tgt = target.impl();
        check_resolvable(tgt, tgt->src(0));
        ch_uint<ch_width_v<T>> acc = default_expr<ch_width_v<T>>(def);
        acc = compose(tgt, acc);
        mark_resolved(tgt);
        return acc;
    }
    
    /**
     * @brief Compose an **output port**'s contributions with an explicit default
     *
     * ⚠️ 输出**必须**显式给默认：输出引用自身 = **组合环** ⇒ 拓扑排序 CHFATAL。
     */
    template <typename T, typename V>
    ch_uint<ch_width_v<T>> resolve_or(port<T, output_direction> &target,
                                      const V &def) {
        lnodeimpl *tgt = target.impl();
        check_resolvable(tgt, nullptr);
        ch_uint<ch_width_v<T>> acc = default_expr<ch_width_v<T>>(def);
        acc = compose(tgt, acc);
        mark_resolved(tgt);
        return acc;
    }
    
    // ⚠️ **故意不提供** `port<T, output_direction>` 的 `resolve_hold` 重载 ——
    //    输出的「保持」= 引用自身 = **组合环** ⇒ 拓扑排序 CHFATAL ⇒ 输出**必须**
    //    用 `resolve_or(out, default)` 显式给默认 ⇒ 误用即**编译不过**
    //    （无匹配重载；`= delete` 形态无法被 SFINAE 检测 ⇒ 同上）。
    
};
} // namespace chlib
