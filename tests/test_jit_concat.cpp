#include "catch_amalgamated.hpp"
#include "ch.hpp"
#include "component.h"
#include "core/operators.h"
#include "simulator.h"

using namespace ch;
using namespace ch::core;

// =============================================================================
// JIT CONCAT Tests
// =============================================================================
// These tests verify that concat() produces correct results when JIT compilation
// is enabled. Each test follows the canonical pattern from
// tests/test_operation_results.cpp:685-705 (ConcatTest) and the test plan in
// .omo/plans/jit-concat-lowering.md (Task 4).
//
// Reference expected values come from interpreter semantics
// (include/ast/instr_op.h:416-447 Concat::eval).
// =============================================================================

// -----------------------------------------------------------------------------
// Test 1: Basic 3+5 bit concat
// -----------------------------------------------------------------------------
TEST_CASE("JIT Concat: basic 3+5 bit", "[jit][concat][basic]") {
    struct ConcatDevice : public ch::Component {
        ch_in<ch_uint<3>> a;
        ch_in<ch_uint<5>> b;
        ch_out<ch_uint<8>> result;

        ConcatDevice(ch::Component *parent = nullptr,
                     const std::string &name = "top")
            : ch::Component(parent, name) {}

        void describe() override {
            result = concat(a, b);
        }
    };

    ch::ch_device<ConcatDevice> dev;
    ch::Simulator sim(dev.context());
    sim.set_jit_enabled(true);

    sim.set_input_value(dev.instance().a, 5);
    sim.set_input_value(dev.instance().b, 26);
    sim.tick();
    // Expected: (5 << 5) | 26 = 160 | 26 = 186
    auto out_val = sim.get_port_value(dev.instance().result);
    REQUIRE(static_cast<uint64_t>(out_val) == 186);
}

// -----------------------------------------------------------------------------
// Test 2: Asymmetric 1+7 bit concat
// -----------------------------------------------------------------------------
TEST_CASE("JIT Concat: asymmetric 1+7 bit", "[jit][concat][asymmetric]") {
    struct ConcatDevice : public ch::Component {
        ch_in<ch_uint<1>> a;
        ch_in<ch_uint<7>> b;
        ch_out<ch_uint<8>> result;

        ConcatDevice(ch::Component *parent = nullptr,
                     const std::string &name = "top")
            : ch::Component(parent, name) {}

        void describe() override {
            result = concat(a, b);
        }
    };

    ch::ch_device<ConcatDevice> dev;
    ch::Simulator sim(dev.context());
    sim.set_jit_enabled(true);

    sim.set_input_value(dev.instance().a, 1u);
    sim.set_input_value(dev.instance().b, 0x55u);
    sim.tick();
    // Expected: (1 << 7) | 0x55 = 0x80 | 0x55 = 0xD5 = 213
    auto out_val = sim.get_port_value(dev.instance().result);
    REQUIRE(static_cast<uint64_t>(out_val) == 0xD5);
}

// -----------------------------------------------------------------------------
// Test 3: Concat with a hardware literal on the LHS
// -----------------------------------------------------------------------------
TEST_CASE("JIT Concat: with literal", "[jit][concat][literal]") {
    struct ConcatDevice : public ch::Component {
        ch_in<ch_uint<4>> sig;
        ch_out<ch_uint<6>> result;

        ConcatDevice(ch::Component *parent = nullptr,
                     const std::string &name = "top")
            : ch::Component(parent, name) {}

        void describe() override {
            // 2-bit literal 0b10 on the LHS, then 4-bit signal
            result = concat(make_uint<2>(2), sig);
        }
    };

    ch::ch_device<ConcatDevice> dev;
    ch::Simulator sim(dev.context());
    sim.set_jit_enabled(true);

    sim.set_input_value(dev.instance().sig, 0xAu);
    sim.tick();
    // Expected: (2 << 4) | 0xA = 0x20 | 0x0A = 0x2A = 42
    auto out_val = sim.get_port_value(dev.instance().result);
    REQUIRE(static_cast<uint64_t>(out_val) == 0x2A);
}

// -----------------------------------------------------------------------------
// Test 4: ShiftRegister regression (the original failure case)
// Reproduces the structure from tests/test_reg_timing.cpp:183-208.
// -----------------------------------------------------------------------------
TEST_CASE("JIT Concat: shift register (regression)",
          "[jit][concat][shift_reg]") {
    class ShiftRegisterJit : public ch::Component {
      public:
        __io(ch_in<ch_uint<1>> in; ch_out<ch_uint<4>> out;)

        ShiftRegisterJit(ch::Component *parent = nullptr,
                         const std::string &name = "shift_reg_jit")
            : ch::Component(parent, name) {}

        void create_ports() override { new (io_storage_) io_type; }

        void describe() override {
            ch_reg<ch_uint<1>> bit1(0_b);
            ch_reg<ch_uint<1>> bit2(0_b);
            ch_reg<ch_uint<1>> bit3(0_b);
            ch_reg<ch_uint<1>> bit4(0_b);

            bit1->next = io().in;
            bit2->next = bit1;
            bit3->next = bit2;
            bit4->next = bit3;

            auto r1 = concat(bit4, bit3);
            auto r2 = concat(r1, bit2);
            io().out = concat(r2, bit1);
        }
    };

    ch::ch_device<ShiftRegisterJit> dev;
    ch::Simulator sim(dev.context());
    sim.set_jit_enabled(true);

    // Drive input bit to 1 throughout the test
    sim.set_input_value(dev.instance().io().in, 1u);

    // Tick 0: in=1 was latched on the first tick, so bit1=1 and out=1.
    sim.tick();
    REQUIRE(static_cast<uint64_t>(
                sim.get_port_value(dev.instance().io().out)) == 1);

    // Tick 1: bit2 latched bit1=1 -> out=3 (0b0011).
    sim.tick();
    REQUIRE(static_cast<uint64_t>(
                sim.get_port_value(dev.instance().io().out)) == 3);

    // Tick 2: bit3 latched bit2=1 -> out=7 (0b0111).
    sim.tick();
    REQUIRE(static_cast<uint64_t>(
                sim.get_port_value(dev.instance().io().out)) == 7);

    // Tick 3: bit4 latched bit3=1 -> out=15 (0b1111).
    sim.tick();
    REQUIRE(static_cast<uint64_t>(
                sim.get_port_value(dev.instance().io().out)) == 15);

    // Tick 4: pipeline full of 1s, remains at 15.
    sim.tick();
    REQUIRE(static_cast<uint64_t>(
                sim.get_port_value(dev.instance().io().out)) == 15);

    // Tick 5..8: pipeline remains full of 1s -> out = 0b1111 = 15
    for (int i = 0; i < 4; ++i) {
        sim.tick();
        REQUIRE(static_cast<uint64_t>(
                    sim.get_port_value(dev.instance().io().out)) == 15);
    }
}

// -----------------------------------------------------------------------------
// Test 5: Nested concat chain of four 1-bit inputs
// -----------------------------------------------------------------------------
TEST_CASE("JIT Concat: nested chain", "[jit][concat][nested]") {
    struct ConcatDevice : public ch::Component {
        ch_in<ch_uint<1>> a;
        ch_in<ch_uint<1>> b;
        ch_in<ch_uint<1>> c;
        ch_in<ch_uint<1>> d;
        ch_out<ch_uint<4>> result;

        ConcatDevice(ch::Component *parent = nullptr,
                     const std::string &name = "top")
            : ch::Component(parent, name) {}

        void describe() override {
            result = concat(concat(concat(a, b), c), d);
        }
    };

    ch::ch_device<ConcatDevice> dev;
    ch::Simulator sim(dev.context());
    sim.set_jit_enabled(true);

    sim.set_input_value(dev.instance().a, 1u);
    sim.set_input_value(dev.instance().b, 0u);
    sim.set_input_value(dev.instance().c, 1u);
    sim.set_input_value(dev.instance().d, 0u);
    sim.tick();
    // Expected: 0b1010 = 10
    auto out_val = sim.get_port_value(dev.instance().result);
    REQUIRE(static_cast<uint64_t>(out_val) == 10);
}

// -----------------------------------------------------------------------------
// Test 6: 32+32 bit concat crossing the 64-bit boundary
// -----------------------------------------------------------------------------
TEST_CASE("JIT Concat: 64-bit boundary", "[jit][concat][64bit]") {
    struct ConcatDevice : public ch::Component {
        ch_in<ch_uint<32>> a;
        ch_in<ch_uint<32>> b;
        ch_out<ch_uint<64>> result;

        ConcatDevice(ch::Component *parent = nullptr,
                     const std::string &name = "top")
            : ch::Component(parent, name) {}

        void describe() override {
            result = concat(a, b);
        }
    };

    ch::ch_device<ConcatDevice> dev;
    ch::Simulator sim(dev.context());
    sim.set_jit_enabled(true);

    sim.set_input_value(dev.instance().a,
                        static_cast<uint64_t>(0xFFFFFFFFULL));
    sim.set_input_value(dev.instance().b,
                        static_cast<uint64_t>(0x00000000ULL));
    sim.tick();
    // Expected: 0xFFFFFFFF00000000
    const uint64_t expected = (static_cast<uint64_t>(0xFFFFFFFFULL) << 32) |
                              0x00000000ULL;
    auto out_val = sim.get_port_value(dev.instance().result);
    REQUIRE(static_cast<uint64_t>(out_val) == expected);
}
