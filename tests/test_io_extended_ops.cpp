#include "catch_amalgamated.hpp"
#include "core/context.h"
#include "core/io.h"
#include "core/operators.h"
#include "core/uint.h"

using namespace ch::core;

TEST_CASE("IOExtendedOps - BitSelectOperation",
          "[io][extended_ops][bit_select]") {
    auto ctx = context("test_ctx");

    SECTION("Compile-time bit selection") {
        ch_out<ch_uint<8>> port("test_port");
        auto bit0 = bit_select<0>(port);
        auto bit7 = bit_select<7>(port);

        REQUIRE(ch_width_v<decltype(bit0)> == 1);
        REQUIRE(ch_width_v<decltype(bit7)> == 1);
    }
}

TEST_CASE("IOExtendedOps - BitsOperation", "[io][extended_ops][bits]") {
    auto ctx = context("test_ctx");

    SECTION("Static bits extraction") {
        ch_out<ch_uint<8>> port("test_port");
        auto bits_3_0 = bits<ch_uint<8>, 3, 0>(port);
        auto bits_7_4 = bits<ch_uint<8>, 7, 4>(port);

        REQUIRE(ch_width_v<decltype(bits_3_0)> == 4);
        REQUIRE(ch_width_v<decltype(bits_7_4)> == 4);
    }
}

TEST_CASE("IOExtendedOps - ConcatOperation", "[io][extended_ops][concat]") {
    auto ctx = context("test_ctx");

    SECTION("Port to port concatenation") {
        ch_out<ch_uint<4>> port_a("port_a");
        ch_out<ch_uint<4>> port_b("port_b");
        auto result = concat(port_a, port_b);

        REQUIRE(ch_width_v<decltype(result)> == 8);
    }

    SECTION("Port to literal concatenation") {
        ch_out<ch_uint<4>> port_a("port_a");
        auto result = concat(port_a, 1111_b);

        REQUIRE(ch_width_v<decltype(result)> == 8);
    }

    SECTION("Literal to port concatenation") {
        ch_out<ch_uint<4>> port_b("port_b");
        auto result = concat(1111_b, port_b);

        REQUIRE(ch_width_v<decltype(result)> == 8);
    }
}

// TEST_CASE("IOExtendedOps - SelectOperation", "[io][extended_ops][select]") {

//     SECTION("Conditional selection between ports") {
//         auto ctx = context("test_ctx");
//         ch_out<ch_bool> condition("condition");
//         ch_out<ch_uint<8>> true_port("true_port");
//         ch_out<ch_uint<8>> false_port("false_port");
//         auto result = select(condition, true_port, false_port);

//         REQUIRE(ch_width_v<decltype(result)> == 8);
//     }
// }

TEST_CASE("IOExtendedOps - ReductionOperations",
          "[io][extended_ops][reduction]") {
    auto ctx = context("test_ctx");

    SECTION("And reduction") {
        ch_out<ch_uint<8>> port("test_port");
        auto result = and_reduce(port);

        REQUIRE(ch_width_v<decltype(result)> == 1);
        REQUIRE(std::is_same_v<std::remove_cvref_t<decltype(result)>, ch_bool>);
    }

    SECTION("Or reduction") {
        ch_out<ch_uint<8>> port("test_port");
        auto result = or_reduce(port);

        REQUIRE(ch_width_v<decltype(result)> == 1);
        REQUIRE(std::is_same_v<std::remove_cvref_t<decltype(result)>, ch_bool>);
    }

    SECTION("Xor reduction") {
        ch_out<ch_uint<8>> port("test_port");
        auto result = xor_reduce(port);

        REQUIRE(ch_width_v<decltype(result)> == 1);
        REQUIRE(std::is_same_v<std::remove_cvref_t<decltype(result)>, ch_bool>);
    }
}

TEST_CASE("IOExtendedOps - ExtensionOperations",
          "[io][extended_ops][extension]") {
    auto ctx = context("test_ctx");

    SECTION("Sign extension") {
        ch_out<ch_uint<8>> port("test_port");
        auto result = sext<ch_uint<8>, 16>(port);

        REQUIRE(ch_width_v<decltype(result)> == 16);
    }

    SECTION("Zero extension") {
        ch_out<ch_uint<8>> port("test_port");
        auto result = zext<ch_uint<8>, 16>(port);

        REQUIRE(ch_width_v<decltype(result)> == 16);
    }
}

TEST_CASE("IOExtendedOps - RotateOperations", "[io][extended_ops][rotate]") {
    auto ctx = context("test_ctx");

    SECTION("Rotate left") {
        ch_out<ch_uint<8>> data_port("data");
        ch_out<ch_uint<3>> shift_port("shift");
        auto result = rotate_left(data_port, shift_port);

        REQUIRE(ch_width_v<decltype(result)> == 8);
    }

    SECTION("Rotate right") {
        ch_out<ch_uint<8>> data_port("data");
        ch_out<ch_uint<3>> shift_port("shift");
        auto result = rotate_right(data_port, shift_port);

        REQUIRE(ch_width_v<decltype(result)> == 8);
    }
}

TEST_CASE("IOExtendedOps - PopcountOperation", "[io][extended_ops][popcount]") {
    auto ctx = context("test_ctx");

    SECTION("Popcount operation") {
        ch_out<ch_uint<8>> port("test_port");
        auto result = popcount(port);

        // 8位输入最多有8个1，需要4位表示(0-8)
        REQUIRE(ch_width_v<decltype(result)> == 4);
    }
}