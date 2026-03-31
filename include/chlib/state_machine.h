#ifndef CHLIB_STATE_MACHINE_H
#define CHLIB_STATE_MACHINE_H

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
 */

#include "ch.hpp"
#include "core/reg.h"
#include "core/uint.h"
#include "core/bool.h"
#include "core/operators.h"
#include <array>
#include <functional>
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
    
public:
    /**
     * @brief Constructor
     */
    ch_state_machine() : state_reg(0_d), next_state(0_d), state_changed(false), built(false) {
        // Initialize state definitions
        for (size_t i = 0; i < N; i++) {
            states_[i].parent = this;
        }
    }
    
    /**
     * @brief Get state definition for a given state
     */
    state_def& state(StateEnum s) {
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
     */
    void transition_to(StateEnum s) {
        next_state = ch_uint<STATE_BITS>(static_cast<uint8_t>(s));
    }
    
    /**
     * @brief Get current state
     */
    StateEnum current_state() const {
        // For now, return entry state - full implementation needs simulator integration
        return entry_state;
    }
    
    /**
     * @brief Get current state as ch_uint
     */
    ch_uint<STATE_BITS> current_state_uint() const {
        return state_reg;
    }
    
    /**
     * @brief Check if in a specific state
     */
    ch_bool is_in(StateEnum s) const {
        return state_reg == ch_uint<STATE_BITS>(static_cast<uint8_t>(s));
    }
    
    /**
     * @brief Set entry state
     */
    void set_entry(StateEnum s) {
        entry_state = s;
        state_reg = ch_uint<STATE_BITS>(static_cast<uint8_t>(s));
    }
    
    /**
     * @brief Build the state machine (generate state register and logic)
     * 
     * This should be called after all states are defined.
     */
    void build() {
        if (built) return;
        
        // Set initial state
        set_entry(entry_state);
        
        // Generate state transition logic
        // This is a simplified implementation - full implementation would
        // need to generate proper combinational logic for state transitions
        
        // For now, we use a simple approach:
        // 1. Execute on_active for current state
        // 2. Update state register with next_state
        
        // State update (sequential)
        state_reg->next = next_state;
        
        built = true;
    }
    
    /**
     * @brief Execute state machine logic (called in describe())
     * 
     * This executes the on_active action for the current state.
     */
    void tick_state() {
        if (!built) {
            build();
        }
        
        // Execute on_active for current state
        StateEnum current = current_state();
        if (states_[static_cast<size_t>(current)].on_active) {
            states_[static_cast<size_t>(current)].on_active();
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

#endif // CHLIB_STATE_MACHINE_H
