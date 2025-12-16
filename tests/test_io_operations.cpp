// tests/test_io_operations.cpp
#define CATCH_CONFIG_MAIN
#include "catch_amalgamated.hpp"
#include "core/bool.h"
#include "core/context.h"
#include "core/io.h"
#include "core/literal.h"
#include "core/uint.h"
#include "core/operators.h"
#include <memory>

using namespace ch;
using namespace ch::core;

TEST_CASE("IOOperations - BasicPortCreation", "[io][operations][basic]") {
    auto ctx = std::make_unique<ch::core::context>("test_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());

    ch_out<ch_uint<8>> output_port("test_output");
    ch_in<ch_uint<8>> input_port("test_input");

    REQUIRE(output_port.impl() != nullptr);
    REQUIRE(input_port.impl() != nullptr);
    REQUIRE(output_port.name() == "test_output");
    REQUIRE(input_port.name() == "test_input");
}

TEST_CASE("IOOperations - BitwiseOperations", "[io][operations][bitwise]") {
    auto ctx = std::make_unique<ch::core::context>("test_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());

    ch_out<ch_uint<8>> port_a("port_a");
    ch_out<ch_uint<8>> port_b("port_b");
    ch_in<ch_uint<8>> port_c("port_c");

    SECTION("Bitwise AND") {
        auto result = port_a & port_b;
        REQUIRE(result.impl() != nullptr);
    }

    SECTION("Bitwise OR") {
        auto result = port_a | port_b;
        REQUIRE(result.impl() != nullptr);
    }

    SECTION("Bitwise XOR") {
        auto result = port_a ^ port_b;
        REQUIRE(result.impl() != nullptr);
    }

    SECTION("Bitwise NOT") {
        auto result = ~port_a;
        REQUIRE(result.impl() != nullptr);
    }
}

TEST_CASE("IOOperations - ArithmeticOperations", "[io][operations][arithmetic]") {
    auto ctx = std::make_unique<ch::core::context>("test_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());

    ch_out<ch_uint<8>> port_a("port_a");
    ch_out<ch_uint<8>> port_b("port_b");

    SECTION("Addition") {
        auto result = port_a + port_b;
        REQUIRE(result.impl() != nullptr);
    }

    SECTION("Subtraction") {
        auto result = port_a - port_b;
        REQUIRE(result.impl() != nullptr);
    }

    SECTION("Multiplication") {
        auto result = port_a * port_b;
        REQUIRE(result.impl() != nullptr);
    }

    SECTION("Division") {
        auto result = port_a / port_b;
        REQUIRE(result.impl() != nullptr);
    }

    SECTION("Modulo") {
        auto result = port_a % port_b;
        REQUIRE(result.impl() != nullptr);
    }

    SECTION("Unary minus") {
        auto result = -port_a;
        REQUIRE(result.impl() != nullptr);
    }
}

TEST_CASE("IOOperations - ShiftOperations", "[io][operations][shift]") {
    auto ctx = std::make_unique<ch::core::context>("test_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());

    ch_out<ch_uint<8>> port_a("port_a");
    ch_out<ch_uint<4>> port_b("port_b"); // Shift amount

    SECTION("Left shift") {
        auto result = port_a << port_b;
        REQUIRE(result.impl() != nullptr);
    }

    SECTION("Right shift") {
        auto result = port_a >> port_b;
        REQUIRE(result.impl() != nullptr);
    }
}

TEST_CASE("IOOperations - ComparisonOperations", "[io][operations][comparison]") {
    auto ctx = std::make_unique<ch::core::context>("test_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());

    ch_out<ch_uint<8>> port_a("port_a");
    ch_out<ch_uint<8>> port_b("port_b");

    SECTION("Equality") {
        auto result = port_a == port_b;
        REQUIRE(result.impl() != nullptr);
    }

    SECTION("Inequality") {
        auto result = port_a != port_b;
        REQUIRE(result.impl() != nullptr);
    }

    SECTION("Less than") {
        auto result = port_a < port_b;
        REQUIRE(result.impl() != nullptr);
    }

    SECTION("Less or equal") {
        auto result = port_a <= port_b;
        REQUIRE(result.impl() != nullptr);
    }

    SECTION("Greater than") {
        auto result = port_a > port_b;
        REQUIRE(result.impl() != nullptr);
    }

    SECTION("Greater or equal") {
        auto result = port_a >= port_b;
        REQUIRE(result.impl() != nullptr);
    }
}

TEST_CASE("IOOperations - MixedOperations", "[io][operations][mixed]") {
    auto ctx = std::make_unique<ch::core::context>("test_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());

    ch_out<ch_uint<8>> port_a("port_a");

    SECTION("Port and literal addition") {
        auto result = port_a + 10_d;
        REQUIRE(result.impl() != nullptr);
    }

    SECTION("Literal and port addition") {
        auto result = 10_d + port_a;
        REQUIRE(result.impl() != nullptr);
    }
}

TEST_CASE("IOOperations - ComplexExpressions", "[io][operations][complex]") {
    auto ctx = std::make_unique<ch::core::context>("test_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());

    ch_out<ch_uint<8>> port_a("port_a");
    ch_out<ch_uint<8>> port_b("port_b");
    ch_out<ch_uint<8>> port_c("port_c");

    SECTION("Complex arithmetic expression") {
        auto result = (port_a + port_b) * port_c;
        REQUIRE(result.impl() != nullptr);
    }

    SECTION("Complex bitwise expression") {
        auto result = (port_a & port_b) | port_c;
        REQUIRE(result.impl() != nullptr);
    }

    SECTION("Mixed expression with literals") {
        auto result = (port_a + 5_d);
        REQUIRE(result.impl() != nullptr);
    }
}