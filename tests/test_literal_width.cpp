#include "catch_amalgamated.hpp"
#include "core/literal.h"
#include <type_traits>

using namespace ch::core;

// 测试二进制字面量的值和宽度
TEST_CASE("Binary literal value and width calculation", "[literal][binary]") {
    SECTION("Simple binary literals") {
        constexpr auto b1 = 1_b;
        STATIC_REQUIRE(b1.value == 1);
        STATIC_REQUIRE(b1.actual_width == 1);

        constexpr auto b11 = 11_b;
        STATIC_REQUIRE(b11.value == 3);
        STATIC_REQUIRE(b11.actual_width == 2);

        constexpr auto b1010 = 1010_b;
        STATIC_REQUIRE(b1010.value == 10);
        STATIC_REQUIRE(b1010.actual_width == 4);

        constexpr auto b1111 = 1111_b;
        STATIC_REQUIRE(b1111.value == 15);
        STATIC_REQUIRE(b1111.actual_width == 4);
    }

    SECTION("Binary literals with separators") {
        constexpr auto b1010_sep = 1'0'1'0_b;
        STATIC_REQUIRE(b1010_sep.value == 10);
        STATIC_REQUIRE(b1010_sep.actual_width == 4);

        constexpr auto b11110000 = 1111'0000_b;
        STATIC_REQUIRE(b11110000.value == 240);
        STATIC_REQUIRE(b11110000.actual_width == 8);
    }

    SECTION("Edge cases for binary literals") {
        constexpr auto b0 = 0_b;
        STATIC_REQUIRE(b0.value == 0);
        STATIC_REQUIRE(b0.actual_width == 1);

        constexpr auto b11111111 = 1111'1111_b;
        STATIC_REQUIRE(b11111111.value == 255);
        STATIC_REQUIRE(b11111111.actual_width == 8);
    }
}

// 测试八进制字面量的值和宽度
TEST_CASE("Octal literal value and width calculation", "[literal][octal]") {
    SECTION("Simple octal literals") {
        constexpr auto o7 = 7_o;
        STATIC_REQUIRE(o7.value == 7);
        STATIC_REQUIRE(o7.actual_width == 3);

        constexpr auto o17 = 17_o;
        STATIC_REQUIRE(o17.value == 15);
        STATIC_REQUIRE(o17.actual_width == 4);

        constexpr auto o377 = 377_o;
        STATIC_REQUIRE(o377.value == 255);
        STATIC_REQUIRE(o377.actual_width == 8);
    }

    SECTION("Octal literals with separators") {
        constexpr auto o377_sep = 3'7'7_o;
        STATIC_REQUIRE(o377_sep.value == 255);
        STATIC_REQUIRE(o377_sep.actual_width == 8);
    }

    SECTION("Edge cases for octal literals") {
        constexpr auto o0 = 0_o;
        STATIC_REQUIRE(o0.value == 0);
        STATIC_REQUIRE(o0.actual_width == 1);
    }
}

// 测试十六进制字面量的值和宽度
TEST_CASE("Hexadecimal literal value and width calculation", "[literal][hex]") {
    SECTION("Simple hex literals") {
        constexpr auto hF = 0xF_h;
        STATIC_REQUIRE(hF.value == 15);
        STATIC_REQUIRE(hF.actual_width == 4);

        constexpr auto hFF = 0xFF_h;
        STATIC_REQUIRE(hFF.value == 255);
        STATIC_REQUIRE(hFF.actual_width == 8);

        constexpr auto hFFFF = 0xFFFF_h;
        STATIC_REQUIRE(hFFFF.value == 65535);
        STATIC_REQUIRE(hFFFF.actual_width == 16);
    }

    SECTION("Hex literals with separators") {
        constexpr auto hDEADBEEF = 0xDEAD'BEEF_h;
        STATIC_REQUIRE(hDEADBEEF.value == 0xDEADBEEF);
        STATIC_REQUIRE(hDEADBEEF.actual_width == 32);
    }

    SECTION("Hex literals with x prefix") {
        constexpr auto hxFF = 0xFF_h;
        STATIC_REQUIRE(hxFF.value == 255);
        STATIC_REQUIRE(hxFF.actual_width == 8);
    }

    SECTION("Mixed case hex literals") {
        constexpr auto hABCD = 0xABCD_h;
        STATIC_REQUIRE(hABCD.value == 0xABCD);
        STATIC_REQUIRE(hABCD.actual_width == 16);
    }

    SECTION("Edge cases for hex literals") {
        constexpr auto h0 = 0x0_h;
        STATIC_REQUIRE(h0.value == 0);
        STATIC_REQUIRE(h0.actual_width == 1);
    }
}

// 测试十进制字面量的值和宽度
TEST_CASE("Decimal literal value and width calculation", "[literal][decimal]") {
    SECTION("Simple decimal literals") {
        constexpr auto d0 = 0_d;
        STATIC_REQUIRE(d0.value == 0);
        STATIC_REQUIRE(d0.actual_width == 1);

        constexpr auto d1 = 1_d;
        STATIC_REQUIRE(d1.value == 1);
        STATIC_REQUIRE(d1.actual_width == 1);

        constexpr auto d10 = 10_d;
        STATIC_REQUIRE(d10.value == 10);
        STATIC_REQUIRE(d10.actual_width == 4);

        constexpr auto d255 = 255_d;
        STATIC_REQUIRE(d255.value == 255);
        STATIC_REQUIRE(d255.actual_width == 8);

        constexpr auto d65535 = 65535_d;
        STATIC_REQUIRE(d65535.value == 65535);
        STATIC_REQUIRE(d65535.actual_width == 16);
    }

    SECTION("Decimal literals with separators") {
        constexpr auto d1000000 = 1'000'000_d;
        STATIC_REQUIRE(d1000000.value == 1000000);
        STATIC_REQUIRE(d1000000.actual_width == 20);
    }

    SECTION("Common decimal values") {
        constexpr auto d1024 = 1024_d;
        STATIC_REQUIRE(d1024.value == 1024);
        STATIC_REQUIRE(d1024.actual_width == 11);

        constexpr auto d1000 = 1'000_d;
        STATIC_REQUIRE(d1000.value == 1000);
        STATIC_REQUIRE(d1000.actual_width == 10);
    }
}

// 测试不同进制间的一致性
TEST_CASE("Consistency between different literal bases",
          "[literal][consistency]") {
    SECTION("Same value in different bases") {
        constexpr auto b11111111 = 1111'1111_b; // 255 in binary
        constexpr auto o377 = 377_o;            // 255 in octal
        constexpr auto hFF = 0xFF_h;            // 255 in hex
        constexpr auto d255 = 255_d;            // 255 in decimal

        STATIC_REQUIRE(b11111111.value == o377.value);
        STATIC_REQUIRE(o377.value == hFF.value);
        STATIC_REQUIRE(hFF.value == d255.value);

        STATIC_REQUIRE(b11111111.actual_width == o377.actual_width);
        STATIC_REQUIRE(o377.actual_width == hFF.actual_width);
        STATIC_REQUIRE(hFF.actual_width == d255.actual_width);
    }

    SECTION("Powers of 2") {
        constexpr auto b10000000 = 1'0000'0000_b; // 256 in binary
        constexpr auto o400 = 400_o;              // 256 in octal
        constexpr auto h100 = 0x100_h;            // 256 in hex
        constexpr auto d256 = 256_d;              // 256 in decimal

        STATIC_REQUIRE(b10000000.value == o400.value);
        STATIC_REQUIRE(o400.value == h100.value);
        STATIC_REQUIRE(h100.value == d256.value);

        STATIC_REQUIRE(b10000000.actual_width == o400.actual_width);
        STATIC_REQUIRE(o400.actual_width == h100.actual_width);
        STATIC_REQUIRE(h100.actual_width == d256.actual_width);
    }
}

// 测试边界情况
TEST_CASE("Boundary conditions for literal widths", "[literal][boundary]") {
    SECTION("Zero value") {
        constexpr auto b0 = 0_b;
        constexpr auto o0 = 0_o;
        constexpr auto h0 = 0x0_h;
        constexpr auto d0 = 0_d;

        STATIC_REQUIRE(b0.actual_width == 1);
        STATIC_REQUIRE(o0.actual_width == 1);
        STATIC_REQUIRE(h0.actual_width == 1);
        STATIC_REQUIRE(d0.actual_width == 1);
    }

    SECTION("Maximum values") {
        constexpr auto
            b1111111111111111111111111111111111111111111111111111111111111111 =
                1111'1111'1111'1111'1111'1111'1111'1111'1111'1111'1111'1111'1111'1111'1111'1111_b;
        STATIC_REQUIRE(
            b1111111111111111111111111111111111111111111111111111111111111111
                .value == UINT64_MAX);
        STATIC_REQUIRE(
            b1111111111111111111111111111111111111111111111111111111111111111
                .actual_width == 64);

        constexpr auto o1777777777777777777777 =
            1'777'777'777'777'777'777'777_o;
        STATIC_REQUIRE(o1777777777777777777777.value == UINT64_MAX);
        STATIC_REQUIRE(o1777777777777777777777.actual_width == 64);

        constexpr auto hFFFFFFFFFFFFFFFF = 0xFFFF'FFFF'FFFF'FFFF_h;
        STATIC_REQUIRE(hFFFFFFFFFFFFFFFF.value == UINT64_MAX);
        STATIC_REQUIRE(hFFFFFFFFFFFFFFFF.actual_width == 64);
    }
}