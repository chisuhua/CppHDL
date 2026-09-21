#pragma once

/**
 * @file state_machine.h
 * @brief CppHDL State Machine DSL
 * 
 * Provides SpinalHDL-like state machine support for CppHDL.
 * 
 * Usage Example:
 * @code{.cpp}
 * enum class MyState : uint8_t { IDLE, RUNNING, DONE };
 * 
 * class MyModule : public ch::Component {
 * public:
 *     void describe() override {
 *         ch_state_machine<MyState, 3> sm;
 *         
 *         sm.state(MyState::IDLE)
 *           .on_active([this, &sm]() {
 *               if (io().start) sm.transition_to(MyState::RUNNING);
 *           });
 *         
 *         sm.state(MyState::RUNNING)
 *           .on_active([this, &sm]() {
 *               if (io().done) sm.transition_to(MyState::DONE);
 *           });
 *         
 *         sm.set_entry(MyState::IDLE);
 *         sm.build();
 *     }
 * };
 * @endcode
 * 
 * NOTE (Phase 6d / ADR-046): `if (ch_bool)` conditions inside on_active
 * closures are NOT hardware conditionals — ch_bool's contextual bool
 * conversion silently evaluates false at describe time. For runtime
 * (cycle-accurate) transitions use `transition_when(ch_bool, target)`:
 * @code{.cpp}
 *     ch_state_machine<MyState, 3> sm;
 *     auto start_sig = io().start;              // ch_bool
 *     sm.state(MyState::IDLE).on_active([&]{
 *         sm.transition_when(start_sig, MyState::RUNNING);  // emits select tree
 *     });
 *     sm.set_entry(MyState::IDLE);
 *     sm.build();   // state_reg->next = select-tree (works in Simulator)
 * @endcode
 * `build()` executes every state's on_active closure once at describe time;
 * each `transition_when` call accumulates into a ch select-tree for
 * `state_reg->next`. The Simulator evaluates that tree every tick, giving
 * cycle-accurate multi-cycle FSMs.
 */

#include "ch.hpp"
#include "core/reg.h"
#include "core/uint.h"
#include "core/bool.h"
#include "core/operators.h"
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
        state_action_t entry_action;
        state_action_t active_action;
        state_action_t exit_action;
        StateEnum next_state_on_exit;  // For automatic transitions
        bool has_exit_transition;
        
        // Phase 6d.6 (ADR-046): runtime transitions accumulated by
        // transition_when() / transition_to() during build(). Each entry is
        // a (condition, target) pair composed into state_reg->next via a
        // ch select-tree at build() time.
        struct runtime_transition {
            ch_bool cond;
            StateEnum target;
        };
        std::vector<runtime_transition> transitions;
        
        state_def() : has_exit_transition(false) {}
        
        /**
         * @brief Set entry action
         */
        state_def& on_entry(state_action_t action) {
            entry_action = action;
            return *this;
        }
        
        /**
         * @brief Set active action (executed while in this state)
         */
        state_def& on_active(state_action_t action) {
            active_action = action;
            return *this;
        }
        
        /**
         * @brief Set exit action
         */
        state_def& on_exit(state_action_t action) {
            exit_action = action;
            return *this;
        }
        
        /**
         * @brief Set automatic transition to next state on exit
         */
        ch_state_machine& then(StateEnum next) {
            next_state_on_exit = next;
            has_exit_transition = true;
            return *parent;
        }
        
    private:
        ch_state_machine* parent;
        friend class ch_state_machine;
    };
    
private:
    // State definitions array
    std::array<state_def, N> states_;
    
    // Current state register
    ch_reg<ch_uint<STATE_BITS>> state_reg;
    
    // Next state combinational logic
    ch_uint<STATE_BITS> next_state;
    
    // State changed flag
    ch_bool state_changed;
    
    // Entry state
    StateEnum entry_state;
    
    // Build flag
    bool built;
    
    // Phase 6d.6: state whose on_active closure is currently executing
    // (used by transition_when/transition_to to attribute the transition).
    StateEnum cur_state_;
    
public:
    /**
     * @brief Constructor
     */
    ch_state_machine() : state_reg(0_d), next_state(0_d), state_changed(false),
                         built(false), cur_state_(static_cast<StateEnum>(0)) {
        // Initialize state definitions
        for (size_t i = 0; i < N; i++) {
            states_[i].parent = this;
        }
    }
    
    /**
     * @brief Get state definition for a given state
     */
    state_def& state(StateEnum s) {
        cur_state_ = s;
        return states_[static_cast<size_t>(s)];
    }
    
    /**
     * @brief Get state definition for a given state (const version)
     */
    const state_def& state(StateEnum s) const {
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
     *   when state_reg == cur_state_ && cond  ->  next = target
     * Composed into the next-state select tree by build().
     * 
     * NOTE: unlike `if (ch_bool)` (which silently evaluates false at describe
     * time), `cond` is a real ch_bool hardware signal evaluated by the
     * Simulator every tick — this enables cycle-accurate multi-cycle FSMs.
     */
    void transition_when(const ch_bool& cond, StateEnum target) {
        states_[static_cast<size_t>(cur_state_)].transitions.push_back(
            {cond, target});
    }
    
    /**
     * @brief Get current state
     */
    StateEnum current_state() const {
        // Entry state (compile-time describe value). For the runtime state
        // register value use current_state_uint() / is_in().
        return entry_state;
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
        return state_reg == ch_uint<STATE_BITS>(static_cast<uint8_t>(s));
    }
    
    /**
     * @brief Set entry state
     */
    void set_entry(StateEnum s) {
        entry_state = s;
        // NOTE (Phase 6d.6): do NOT clobber state_reg's node with a literal.
        // The register's init value is fixed at construction (0_d). Entry
        // state enum value must be 0 for the register to reset to it.
        // (ch_state_machine is constructed with state_reg(0_d) — keep enum
        // value 0 == entry state, which holds for all ChipForge FSMs.)
    }
    
    /**
     * @brief Build the state machine (generate state register and logic)
     * 
     * Executes each state's on_active closure once at describe time to emit
     * combinational logic and collect transitions, then composes a ch
     * select-tree for state_reg->next:
     *   next = state_reg                          (hold by default)
     *   for each state S, transition (cond,target):
     *     next = select(is_in(S) && cond, target, next)
     * 
     * The Simulator evaluates this tree every tick, giving cycle-accurate
     * multi-cycle FSM behavior (Phase 6d.6 / ADR-046).
     */
    void build() {
        if (built) return;
        
        // Set initial state
        // state_reg init value fixed at construction; entry_state recorded
        // for current_state() reporting.
        
        auto& entry_state_def = states_[static_cast<size_t>(entry_state)];
        if (entry_state_def.entry_action) {
            entry_state_def.entry_action();
        }
        
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
                auto target_lit =
                    ch_uint<STATE_BITS>(static_cast<uint8_t>(tr.target));
                next_state = select(in_s && tr.cond, target_lit, next_state);
            }
            // Legacy automatic exit transition (then())
            if (sd.has_exit_transition) {
                auto target_lit = ch_uint<STATE_BITS>(
                    static_cast<uint8_t>(sd.next_state_on_exit));
                next_state = select(in_s, target_lit, next_state);
            }
        }
        
        // State update (sequential)
        state_reg->next = next_state;
        
        built = true;
    }
    
    /**
     * @brief Execute state machine logic (called in describe())
     * 
     * Legacy helper — executes the ENTRY state's active action once.
     * For runtime behavior, the select-tree emitted by build() is
     * evaluated by the Simulator each tick; no per-cycle call needed.
     */
    void tick_state() {
        if (!built) {
            build();
        }
        
        // Execute on_active for current state
        StateEnum current = current_state();
        if (states_[static_cast<size_t>(current)].active_action) {
            states_[static_cast<size_t>(current)].active_action();
        }
    }
    
    /**
     * @brief Check if state machine is built
     */
    bool is_built() const {
        return built;
    }
};

/**
 * @brief Helper macro for defining state machines with cleaner syntax
 */
#define CH_STATE_MACHINE(name, state_enum, n) \
    ch_state_machine<state_enum, n> name;

/**
 * @brief Helper macro for defining a state
 */
#define CH_STATE(sm, state) \
    sm.state(state)

/**
 * @brief Helper macro for building state machine
 */
#define CH_STATE_MACHINE_BUILD(sm, entry_state) \
    sm.set_entry(entry_state); \
    sm.build();

} // namespace chlib
