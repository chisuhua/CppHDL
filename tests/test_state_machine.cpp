/**
 * @file test_state_machine.cpp
 * @brief Unit tests for ch_state_machine DSL
 */

#include "chlib/state_machine.h"
#include "component.h"
#include "simulator.h"
#include "catch_amalgamated.hpp"
#include <iostream>

using namespace ch;
using namespace ch::core;
using namespace chlib;

// Test state enum
enum class TestState : uint8_t {
    STATE_A,
    STATE_B,
    STATE_C
};

TEST_CASE("State Machine: Basic state definition", "[state_machine]") {
    // Note: ch_device requires a Component type, but for basic state machine tests
    // we can just test the state machine logic without a full device
    ch_state_machine<TestState, 3> sm;
    
    // Define states
    bool state_a_active = false;
    bool state_b_active = false;
    
    sm.state(TestState::STATE_A)
      .on_active([&state_a_active]() {
          state_a_active = true;
      });
    
    sm.state(TestState::STATE_B)
      .on_active([&state_b_active]() {
          state_b_active = true;
      });
    
    sm.set_entry(TestState::STATE_A);
    sm.build();
    
    // Verify initial state
    REQUIRE(sm.current_state() == TestState::STATE_A);
}

TEST_CASE("State Machine: is_in check", "[state_machine]") {
    ch_state_machine<TestState, 3> sm;
    
    sm.state(TestState::STATE_A).on_active([]() {});
    sm.state(TestState::STATE_B).on_active([]() {});
    sm.state(TestState::STATE_C).on_active([]() {});
    
    sm.set_entry(TestState::STATE_B);
    sm.build();
    
    REQUIRE(sm.current_state() == TestState::STATE_B);
}

TEST_CASE("State Machine: Entry/Exit actions", "[state_machine]") {
    ch_state_machine<TestState, 3> sm;
    
    bool entry_a_called = false;
    
    sm.state(TestState::STATE_A)
      .on_entry([&entry_a_called]() {
          entry_a_called = true;
      });
    
    sm.state(TestState::STATE_B)
      .on_active([]() {});
    
    sm.set_entry(TestState::STATE_A);
    sm.build();
    
    // Entry action should be called during build
    REQUIRE(entry_a_called == true);
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
