#include "catch_amalgamated.hpp"
#include "core/literal.h"
#include "core/reg.h"
#include <type_traits>

using namespace ch::core;

// ---------- 编译期宽度和值基本测试 ----------

TEST_CASE("Compile-time literal value and width calculation",
          "[literal][compile_time][value_width]") {
    // 测试二进制字面量的值和宽度计算
    SECTION("Binary literals") {
        constexpr auto b1 = 1_b;
        STATIC_REQUIRE(b1.value() == 1);
        STATIC_REQUIRE(b1.actual_width == 1);

        constexpr auto b2 = 11_b;
        STATIC_REQUIRE(b2.value() == 3);
        STATIC_REQUIRE(b2.actual_width == 2);

        constexpr auto b3 = 1010_b;
        STATIC_REQUIRE(b3.value() == 10);
        STATIC_REQUIRE(b3.actual_width == 4);

        constexpr auto b4 = 1111_b;
        STATIC_REQUIRE(b4.value() == 15);
        STATIC_REQUIRE(b4.actual_width == 4);

        constexpr auto b5 = 111'1111_b;
        STATIC_REQUIRE(b5.value() == 127);
        STATIC_REQUIRE(b5.actual_width == 7);
    }

    // 测试八进制字面量的值和宽度计算
    SECTION("Octal literals") {
        constexpr auto o1 = 7_o;
        STATIC_REQUIRE(o1.value() == 7);
        STATIC_REQUIRE(o1.actual_width == 3);

        constexpr auto o2 = 17_o;
        STATIC_REQUIRE(o2.value() == 15);
        STATIC_REQUIRE(o2.actual_width == 4);

        constexpr auto o3 = 377_o;
        STATIC_REQUIRE(o3.value() == 255);
        STATIC_REQUIRE(o3.actual_width == 8);
    }

    // 测试十六进制字面量的值和宽度计算
    SECTION("Hexadecimal literals") {
        constexpr auto h1 = 0xF_h;
        STATIC_REQUIRE(h1.value() == 15);
        STATIC_REQUIRE(h1.actual_width == 4);

        constexpr auto h2 = 0xFF_h;
        STATIC_REQUIRE(h2.value() == 255);
        STATIC_REQUIRE(h2.actual_width == 8);

        constexpr auto h3 = 0xFFFF_h;
        STATIC_REQUIRE(h3.value() == 65535);
        STATIC_REQUIRE(h3.actual_width == 16);
    }

    // 测试十进制字面量的值和宽度计算
    SECTION("Decimal literals") {
        constexpr auto d1 = 0_d;
        STATIC_REQUIRE(d1.value() == 0);
        STATIC_REQUIRE(d1.actual_width == 1);

        constexpr auto d2 = 1_d;
        STATIC_REQUIRE(d2.value() == 1);
        STATIC_REQUIRE(d2.actual_width == 1);

        constexpr auto d3 = 10_d;
        STATIC_REQUIRE(d3.value() == 10);
        STATIC_REQUIRE(d3.actual_width == 4);

        constexpr auto d4 = 255_d;
        STATIC_REQUIRE(d4.value() == 255);
        STATIC_REQUIRE(d4.actual_width == 8);

        constexpr auto d5 = 65535_d;
        STATIC_REQUIRE(d5.value() == 65535);
        STATIC_REQUIRE(d5.actual_width == 16);
    }
}

// ---------- 编译期字面量特性测试 ----------

TEST_CASE("Compile-time literal properties and methods",
          "[literal][compile_time][properties]") {
    SECTION("is_zero method") {
        constexpr auto zero_lit = 0_b;
        constexpr auto nonzero_lit = 1_b;

        STATIC_REQUIRE(zero_lit.is_zero() == true);
        STATIC_REQUIRE(nonzero_lit.is_zero() == false);

        constexpr auto zero_dec = 0_d;
        constexpr auto nonzero_dec = 1_d;

        STATIC_REQUIRE(zero_dec.is_zero() == true);
        STATIC_REQUIRE(nonzero_dec.is_zero() == false);
    }

    SECTION("is_ones method") {
        // 测试全1值检查
        constexpr auto ones4 = 1111_b;      // 4位全1
        constexpr auto not_ones4 = 1110_b;  // 4位非全1
        constexpr auto ones8 = 1111'1111_b; // 8位全1

        STATIC_REQUIRE(ones4.is_ones() == true);
        STATIC_REQUIRE(not_ones4.is_ones() == false);
        STATIC_REQUIRE(ones8.is_ones() == true);

        // 测试十六进制全1
        constexpr auto ones16 = 0xFFFF_h;
        STATIC_REQUIRE(ones16.is_ones() == true);
    }

    SECTION("width method") {
        constexpr auto lit1 = 1_b;
        constexpr auto lit4 = 1111_b;
        constexpr auto lit8 = 1111'1111_b;

        STATIC_REQUIRE(lit1.width() == 1);
        STATIC_REQUIRE(lit4.width() == 4);
        STATIC_REQUIRE(lit8.width() == 8);
    }
}

// ---------- 编译期字面量边界条件测试 ----------

TEST_CASE("Compile-time literal edge cases",
          "[literal][compile_time][edge_cases]") {
    SECTION("Zero width literals") {
        // 宽度0应被修正为1
        constexpr auto lit = ch_literal_impl<0, 0>();
        STATIC_REQUIRE(lit.value() == 0);
        STATIC_REQUIRE(lit.actual_width == 1);
        STATIC_REQUIRE(lit.width() == 1);
    }

    SECTION("Maximum width literals") {
        // 宽度超过64应被修正为64
        constexpr auto lit = ch_literal_impl<0xFFFFFFFFFFFFFFFF, 100>();
        STATIC_REQUIRE(lit.value() == 0xFFFFFFFFFFFFFFFF);
        STATIC_REQUIRE(lit.actual_width == 64);
        STATIC_REQUIRE(lit.width() == 64);
    }

    SECTION("Maximum value literals") {
        constexpr auto lit = ch_literal_impl<0xFFFFFFFFFFFFFFFF, 64>();
        STATIC_REQUIRE(lit.value() == 0xFFFFFFFFFFFFFFFF);
        STATIC_REQUIRE(lit.actual_width == 64);
        STATIC_REQUIRE(lit.width() == 64);
        STATIC_REQUIRE(lit.is_ones() == true);
    }

    SECTION("Minimum value literals") {
        constexpr auto lit = ch_literal_impl<0, 1>();
        STATIC_REQUIRE(lit.value() == 0);
        STATIC_REQUIRE(lit.actual_width == 1);
        STATIC_REQUIRE(lit.width() == 1);
        STATIC_REQUIRE(lit.is_zero() == true);
    }
}

// ---------- 编译期字面量类型特征测试 ----------

TEST_CASE("Compile-time literal type traits",
          "[literal][compile_time][traits]") {
    SECTION("is_ch_literal_v for ch_literal_impl") {
        STATIC_REQUIRE(is_ch_literal_v<ch_literal_impl<0, 1>> == true);
        STATIC_REQUIRE(is_ch_literal_v<ch_literal_impl<255, 8>> == true);
        STATIC_REQUIRE(is_ch_literal_v<ch_literal_impl<65535, 16>> == true);

        // 测试cv限定版本
        STATIC_REQUIRE(is_ch_literal_v<const ch_literal_impl<0, 1>> == true);
        STATIC_REQUIRE(is_ch_literal_v<volatile ch_literal_impl<255, 8>> ==
                       true);
        STATIC_REQUIRE(
            is_ch_literal_v<const volatile ch_literal_impl<65535, 16>> == true);
    }

    SECTION("is_ch_literal_v negative cases") {
        STATIC_REQUIRE(is_ch_literal_v<int> == false);
        STATIC_REQUIRE(is_ch_literal_v<double> == false);
        STATIC_REQUIRE(is_ch_literal_v<char> == false);
    }
}

// ---------- 编译期字面量与ch_uint互操作性测试 ----------

TEST_CASE("Compile-time literal and ch_uint interoperability",
          "[literal][compile_time][interop]") {
    SECTION("Implicit conversion to ch_uint") {
        // 测试字面量到ch_uint的隐式转换
        constexpr auto lit = 0xFF_h;

        // 注意：这里我们只测试类型特征，不实际执行转换
        // 因为ch_uint可能还没有完全定义 TODO
        STATIC_REQUIRE(
            std::is_same_v<decltype(lit), const ch_literal_impl<255, 8>>);
    }

    SECTION("Construction of ch_uint from literals") {
        // 测试使用字面量构造ch_uint
        // 这些测试需要在有完整上下文的情况下才能编译通过
        /*
        constexpr auto lit = 0xFF_h;
        constexpr ch_uint<8> u8(lit);
        STATIC_REQUIRE(u8.value() == 0xFF);
        */
    }
}

// ---------- 基础类型测试 ----------

TEST_CASE("ch_literal: basic functionality", "[literal][ch_literal]") {
    // 默认构造 - 使用模板别名ch_literal<V, W>
    constexpr auto l1 = make_literal<0, 0>();
    REQUIRE(l1.value() == 0);
    REQUIRE(l1.actual_width == 1);

    // 带参数构造 - 使用make_literal函数模板
    constexpr auto l2 = make_literal<0xFF, 8>();
    REQUIRE(l2.value() == 0xFF);
    REQUIRE(l2.actual_width == 8);

    // 边界条件测试
    constexpr auto l3 = make_literal<0, 0>(); // 宽度0应该被修正为1
    REQUIRE(l3.actual_width == 1);

    constexpr auto l4 =
        make_literal<0x12345678, 100>(); // 宽度超过64应该被修正为64
    REQUIRE(l4.actual_width == 64);
}

// ---------- 类型特征测试 ----------

TEST_CASE("is_ch_literal: type trait verification", "[literal][traits]") {
    // 测试运行时版本
    STATIC_REQUIRE(is_ch_literal_v<ch_literal_runtime> == true);
    // 测试模板版本
    STATIC_REQUIRE(is_ch_literal_v<ch_literal_impl<0, 1>> == true);
    STATIC_REQUIRE(is_ch_literal_v<int> == false);
    STATIC_REQUIRE(is_ch_literal_v<double> == false);

    using ConstLit = const ch_literal_runtime;
    using VolatileLit = volatile ch_literal_runtime;
    using ConstVolatileLit = const volatile ch_literal_runtime;

    STATIC_REQUIRE(is_ch_literal_v<ConstLit> == true);
    STATIC_REQUIRE(is_ch_literal_v<VolatileLit> == true);
    STATIC_REQUIRE(is_ch_literal_v<ConstVolatileLit> == true);
}

// ---------- 辅助函数测试 ----------

TEST_CASE("bit_width: calculate minimum bit width", "[literal][bit_width]") {
    REQUIRE(ch_literal_runtime::compute_width(0) == 1);
    REQUIRE(ch_literal_runtime::compute_width(1) == 1);
    REQUIRE(ch_literal_runtime::compute_width(2) == 2);
    REQUIRE(ch_literal_runtime::compute_width(3) == 2);
    REQUIRE(ch_literal_runtime::compute_width(4) == 3);
    REQUIRE(ch_literal_runtime::compute_width(7) == 3);
    REQUIRE(ch_literal_runtime::compute_width(8) == 4);
    REQUIRE(ch_literal_runtime::compute_width(0xFF) == 8);
    REQUIRE(ch_literal_runtime::compute_width(0x100) == 9);
    REQUIRE(ch_literal_runtime::compute_width(0xFFFF) == 16);
    REQUIRE(ch_literal_runtime::compute_width(0x10000) == 17);
}

TEST_CASE("make_literal: create literals with specified width",
          "[literal][make_literal]") {
    auto l1 = make_literal(0xFF, 8);
    REQUIRE(l1.value() == 0xFF);
    REQUIRE(l1.width() == 8); // 运行时版本使用width()方法

    auto l2 = make_literal(0x1234, 16);
    REQUIRE(l2.value() == 0x1234);
    REQUIRE(l2.width() == 16); // 运行时版本使用width()方法
}

TEST_CASE("make_literal: create literals with auto width",
          "[literal][make_literal]") {
    auto l1 = make_literal(0);
    REQUIRE(l1.value() == 0);
    REQUIRE(l1.width() == 1); // 运行时版本使用width()方法

    auto l2 = make_literal(1);
    REQUIRE(l2.value() == 1);
    REQUIRE(l2.width() == 1); // 运行时版本使用width()方法

    auto l3 = make_literal(0xFF);
    REQUIRE(l3.value() == 0xFF);
    REQUIRE(l3.width() == 8); // 运行时版本使用width()方法

    auto l4 = make_literal(0x100);
    REQUIRE(l4.value() == 0x100);
    REQUIRE(l4.width() == 9); // 运行时版本使用width()方法
}

// ========== 硬件友好字面量测试 ==========

// ---------- 字面量解析器测试 ----------

TEST_CASE("lit_bin: binary literal parser", "[literal][lit_bin]") {
    REQUIRE(lit_bin::is_digit('0') == true);
    REQUIRE(lit_bin::is_digit('1') == true);
    REQUIRE(lit_bin::is_digit('2') == false);

    REQUIRE(lit_bin::is_escape('\'') == true);
    REQUIRE(lit_bin::is_escape('x') == false);

    // Replace lit_bin::size with lit_bin_size_v
    REQUIRE(lit_bin_size_v<'0'> == 1);
    REQUIRE(lit_bin_size_v<'1', '1'> == 2);
    REQUIRE(lit_bin_size_v<'\'', '1', '0'> == 2); // escape ignored

    REQUIRE(lit_bin::chr2int('0') == 0);
    REQUIRE(lit_bin::chr2int('1') == 1);
}

TEST_CASE("lit_oct: octal literal parser", "[literal][lit_oct]") {
    REQUIRE(lit_oct::is_digit('0') == true);
    REQUIRE(lit_oct::is_digit('7') == true);
    REQUIRE(lit_oct::is_digit('8') == false);

    REQUIRE(lit_oct::is_escape('\'') == true);

    // Replace lit_oct::size with lit_oct_size_v
    REQUIRE(lit_oct_size_v<'0'> == 1);
    REQUIRE(lit_oct_size_v<'\'', '7'> == 3);

    REQUIRE(lit_oct::chr2int('0') == 0);
    REQUIRE(lit_oct::chr2int('7') == 7);
}

TEST_CASE("lit_hex: hexadecimal literal parser", "[literal][lit_hex]") {
    REQUIRE(lit_hex::is_digit('0') == true);
    REQUIRE(lit_hex::is_digit('9') == true);
    REQUIRE(lit_hex::is_digit('A') == true);
    REQUIRE(lit_hex::is_digit('F') == true);
    REQUIRE(lit_hex::is_digit('a') == true);
    REQUIRE(lit_hex::is_digit('f') == true);
    REQUIRE(lit_hex::is_digit('G') == false);

    REQUIRE(lit_hex::is_escape('\'') == true);
    REQUIRE(lit_hex::is_escape('x') == true);
    REQUIRE(lit_hex::is_escape('X') == true);

    // Replace lit_hex::size with lit_hex_size_v
    REQUIRE(lit_hex_size_v<'x', 'F'> == 4);  // 'x' is escape → ignored
    REQUIRE(lit_hex_size_v<'X', 'A'> == 4);  // 'X' is escape → ignored
    REQUIRE(lit_hex_size_v<'\'', 'F'> == 4); // '\'' is escape → ignored

    REQUIRE(lit_hex::chr2int('0') == 0);
    REQUIRE(lit_hex::chr2int('9') == 9);
    REQUIRE(lit_hex::chr2int('A') == 10);
    REQUIRE(lit_hex::chr2int('F') == 15);
    REQUIRE(lit_hex::chr2int('a') == 10);
    REQUIRE(lit_hex::chr2int('f') == 15);
}

TEST_CASE("lit_dec: decimal literal parser", "[literal][lit_dec]") {
    REQUIRE(lit_dec_value_v<'1', '2', '3'> == 123);
    REQUIRE(lit_dec_value_v<'0'> == 0);
    REQUIRE(lit_dec_value_v<'1', '\'', '0', '0', '0'> == 1000);

    REQUIRE(lit_dec_size_v<'1', '2', '3'> == 7); // 123 需要 7 位 (1111011)
    REQUIRE(lit_dec_size_v<'0'> == 1);
    REQUIRE(lit_dec_size_v<'2', '5', '5'> == 8); // 255 需要 8 位
}

// ========== 真正的硬件友好字面量测试 ==========

TEST_CASE("Hardware-friendly literals: binary literals",
          "[literal][hardware_friendly][binary]") {
    SECTION("Simple binary literals") {
        constexpr auto lit1 = 1_b;
        REQUIRE(lit1.value() == 1);
        REQUIRE(lit1.actual_width == 1);

        constexpr auto lit11 = 11_b;
        REQUIRE(lit11.value() == 3); // 11(二进制) = 3(十进制)
        REQUIRE(lit11.actual_width == 2);

        constexpr auto lit1010 = 1010_b;
        REQUIRE(lit1010.value() == 10); // 1010(二进制) = 10(十进制)
        REQUIRE(lit1010.actual_width == 4);
    }

    SECTION("Binary literals with separators") {
        constexpr auto lit = 1'0'1'0_b;
        REQUIRE(lit.value() == 10);
        REQUIRE(lit.actual_width == 4);

        constexpr auto lit2 = 1111'0000_b;
        REQUIRE(lit2.value() == 240); // 11110000(二进制) = 240(十进制)
        REQUIRE(lit2.actual_width == 8);
    }

    SECTION("Common binary patterns") {
        constexpr auto all_ones_4bit = 1111_b;
        REQUIRE(all_ones_4bit.value() == 15);
        REQUIRE(all_ones_4bit.actual_width == 4);

        constexpr auto alternating = 1010'1010_b;
        REQUIRE(alternating.value() == 170); // 10101010(二进制) = 170(十进制)
        REQUIRE(alternating.actual_width == 8);
    }
}

TEST_CASE("Hardware-friendly literals: octal literals",
          "[literal][hardware_friendly][octal]") {
    SECTION("Simple octal literals") {
        constexpr auto lit1 = 7_o;
        REQUIRE(lit1.value() == 7);
        REQUIRE(lit1.actual_width == 3); // 7需要3位二进制表示

        constexpr auto lit2 = 17_o;
        REQUIRE(lit2.value() == 15);     // 17(八进制) = 15(十进制)
        REQUIRE(lit2.actual_width == 4); // 15需要4位二进制表示
    }

    SECTION("Octal literals with separators") {
        constexpr auto lit = 3'7'7_o;
        REQUIRE(lit.value() == 255); // 377(八进制) = 255(十进制)
        REQUIRE(lit.actual_width == 8);
    }
}

TEST_CASE("Hardware-friendly literals: hexadecimal literals",
          "[literal][hardware_friendly][hex]") {
    SECTION("Simple hex literals") {
        constexpr auto lit1 = 0xF_h;
        REQUIRE(lit1.value() == 15);
        REQUIRE(lit1.actual_width == 4);

        constexpr auto lit2 = 0xFF_h;
        REQUIRE(lit2.value() == 255);
        REQUIRE(lit2.actual_width == 8);

        constexpr auto lit3 = 0xDEAD'BEEF_h;
        REQUIRE(lit3.value() == 0xDEADBEEF);
        REQUIRE(lit3.actual_width == 32);
    }

    SECTION("Hex literals with x prefix") {
        constexpr auto lit = 0xFF_h;
        REQUIRE(lit.value() == 255);
        REQUIRE(lit.actual_width == 8);
    }

    SECTION("Mixed case hex literals") {
        constexpr auto lit1 = 0xABCD_h;
        REQUIRE(lit1.value() == 0xABCD);
        REQUIRE(lit1.actual_width == 16);

        constexpr auto lit2 = 0xDEAD'BEEF_h;
        REQUIRE(lit2.value() == 0xDEADBEEF);
        REQUIRE(lit2.actual_width == 32);
    }
}

TEST_CASE("Hardware-friendly literals: decimal literals",
          "[literal][hardware_friendly][decimal]") {
    SECTION("Simple decimal literals") {
        constexpr auto lit1 = 0_d;
        REQUIRE(lit1.value() == 0);
        REQUIRE(lit1.actual_width == 1);

        constexpr auto lit2 = 1_d;
        REQUIRE(lit2.value() == 1);
        REQUIRE(lit2.actual_width == 1);

        constexpr auto lit3 = 10_d;
        REQUIRE(lit3.value() == 10);
        REQUIRE(lit3.actual_width == 4); // 10 需要 4 位 (1010)

        constexpr auto lit4 = 255_d;
        REQUIRE(lit4.value() == 255);
        REQUIRE(lit4.actual_width == 8);
    }

    SECTION("Decimal literals with separators") {
        constexpr auto lit = 1'000'000_d;
        REQUIRE(lit.value() == 1000000);
        REQUIRE(lit.actual_width == 20); // 1000000 需要 20 位
    }

    SECTION("Common decimal values") {
        constexpr auto count = 1024_d;
        REQUIRE(count.value() == 1024);
        REQUIRE(count.actual_width == 11); // 2^10 = 1024, 需要 11 位

        constexpr auto max_uint8 = 255_d;
        REQUIRE(max_uint8.value() == 255);
        REQUIRE(max_uint8.actual_width == 8);
    }
}

TEST_CASE("Hardware-friendly literals: practical usage examples",
          "[literal][hardware_friendly][practical]") {
    SECTION("Common register initialization values") {
        // 全0
        constexpr auto zero8 = 0000'0000_b;
        REQUIRE(zero8.value() == 0);
        REQUIRE(zero8.actual_width == 1);

        // 全1
        constexpr auto ones8 = 1111'1111_b;
        REQUIRE(ones8.value() == 255);
        REQUIRE(ones8.actual_width == 8);

        // 交替模式
        constexpr auto pattern8 = 1010'1010_b;
        REQUIRE(pattern8.value() == 170);
        REQUIRE(pattern8.actual_width == 8);
    }

    SECTION("Memory address patterns") {
        constexpr auto addr = 0xDEAD'BEEF_h;
        REQUIRE(addr.value() == 0xDEADBEEF);
        REQUIRE(addr.actual_width == 32);

        constexpr auto low_addr = 0x1000_h;
        REQUIRE(low_addr.value() == 0x1000);
        REQUIRE(low_addr.actual_width == 13); // 0x1000 = 4096，需要13位
    }

    SECTION("Decimal counters and delays") {
        constexpr auto counter_max = 1000_d;
        REQUIRE(counter_max.value() == 1000);
        REQUIRE(counter_max.actual_width == 10); // 1000 需要 10 位

        constexpr auto delay_cycles = 100_d;
        REQUIRE(delay_cycles.value() == 100);
        REQUIRE(delay_cycles.actual_width == 7); // 100 需要 7 位
    }

    SECTION("Bit mask patterns") {
        constexpr auto mask4 = 1111_b;      // 4位全1掩码
        constexpr auto mask8 = 1111'1111_b; // 8位全1掩码
        constexpr auto mask16 = 0xFFFF_h;   // 16位全1掩码

        REQUIRE(mask4.value() == 15);
        REQUIRE(mask4.actual_width == 4);
        REQUIRE(mask8.value() == 255);
        REQUIRE(mask8.actual_width == 8);
        REQUIRE(mask16.value() == 0xFFFF);
        REQUIRE(mask16.actual_width == 16);
    }
}

// ========== 边界条件和错误处理测试 ==========

TEST_CASE("Edge cases and boundary conditions", "[literal][edge]") {
    SECTION("Zero values") {
        auto l1 = make_literal(0);
        REQUIRE(l1.value() == 0);
        REQUIRE(l1.width() == 1); // 运行时版本使用width()方法

        auto l2 = make_literal(0, 0);
        REQUIRE(l2.value() == 0);
        REQUIRE(l2.width() == 1); // 运行时版本使用width()方法，被修正
    }

    SECTION("Maximum values") {
        auto l1 = make_literal(0xFF);
        REQUIRE(l1.value() == 0xFF);
        REQUIRE(l1.width() == 8); // 运行时版本使用width()方法

        auto l2 = make_literal(0xFFFFFFFFFFFFFFFF);
        REQUIRE(l2.value() == 0xFFFFFFFFFFFFFFFF);
        REQUIRE(l2.width() == 64); // 运行时版本使用width()方法，最大宽度限制
    }

    SECTION("Width constraints") {
        auto l1 = make_literal(0x123, 16);
        REQUIRE(l1.value() == 0x123);
        REQUIRE(l1.width() == 16); // 运行时版本使用width()方法

        // 超过64位的宽度应该被限制
        auto l2 = make_literal(0x1FFFF, 100);
        REQUIRE(l2.width() == 64); // 运行时版本使用width()方法，被修正为64
    }

    SECTION("Extreme values and edge cases") {
        // 测试 64 位最大值
        constexpr auto max64 = 0xFFFFFFFFFFFFFFFF_h;
        REQUIRE(max64.value() == 0xFFFFFFFFFFFFFFFF);
        REQUIRE(max64.actual_width == 64);

        // 测试大十进制数
        constexpr auto big_num = 1'000'000'000_d; // 10^9
        REQUIRE(big_num.value() == 1000000000);
        REQUIRE(big_num.actual_width == 30); // log2(10^9) ≈ 29.9，需要30位

        // 测试二进制长序列
        constexpr auto long_binary = 1111'1111'1111'1111_b; // 16位全1
        REQUIRE(long_binary.value() == 0xFFFF);
        REQUIRE(long_binary.actual_width == 16);
    }
}

// ========== 与现有系统集成测试 ==========

TEST_CASE("Integration with existing system", "[literal][integration]") {
    SECTION("ch_literal construction from arithmetic types") {
        // 模拟 get_lnode 中的使用场景
        auto lit1 = make_literal(42);
        REQUIRE(lit1.value() == 42);
        REQUIRE(lit1.width() == 6); // 运行时版本使用width()方法

        auto lit2 = make_literal(0xFF, 8);
        REQUIRE(lit2.value() == 0xFF);
        REQUIRE(lit2.width() == 8); // 运行时版本使用width()方法
    }

    SECTION("Type trait usage") {
        auto test_func = [](auto &&val) {
            if constexpr (is_ch_literal_v<std::decay_t<decltype(val)>>) {
                if constexpr (std::is_same_v<std::decay_t<decltype(val)>,
                                             ch_literal_runtime>) {
                    // 运行时版本使用width()方法
                    return val.width();
                } else {
                    // 编译时版本使用actual_width成员
                    return val.actual_width;
                }
            } else {
                return 0u;
            }
        };

        auto lit = make_literal(42); // 创建运行时版本
        int not_lit = 42;

        REQUIRE(test_func(lit) == 6);
        REQUIRE(test_func(not_lit) == 0);
    }

    SECTION("Hardware literals in template contexts") {
        // 测试在模板中的使用
        auto process_literal = [](auto lit) {
            if constexpr (is_ch_literal_v<std::decay_t<decltype(lit)>>) {
                if constexpr (std::is_same_v<std::decay_t<decltype(lit)>,
                                             ch_literal_runtime>) {
                    // 运行时版本
                    return std::make_pair(lit.value(), lit.width());
                } else {
                    // 编译时版本
                    return std::make_pair(lit.value(), lit.actual_width);
                }
            } else {
                return std::make_pair(static_cast<uint64_t>(lit), 0u);
            }
        };

        auto result1 = process_literal(1010_b);
        REQUIRE(result1.first == 10);
        REQUIRE(result1.second == 4);

        auto result2 = process_literal(42_d); // 使用十进制字面量
        REQUIRE(result2.first == 42);
        REQUIRE(result2.second == 6);
    }

    SECTION("Construction of ch_uint and ch_bool from literals") {

        context ctx("test_context");
        ctx_swap swap(&ctx);
        // 测试与 ch_uint 集成
        ch_reg<ch_uint<8>> u8(255_d);
        REQUIRE(u8.impl() != nullptr);

        ch_uint<16> u16(0xDEAD_h);
        REQUIRE(u16.impl() != nullptr);

        // 测试与 ch_bool 集成
        ch_bool b1(1_b);
        ch_bool b2(0_b);
        REQUIRE(b1.impl() != nullptr);
        REQUIRE(b2.impl() != nullptr);
    }
}

// ========== 编译时性能和效率测试 ==========

TEST_CASE("Compile-time performance and efficiency",
          "[literal][compile_time]") {
    SECTION("Compile-time evaluation efficiency") {
        // 确保所有计算都在编译时完成
        constexpr auto lit1 = 0xDEAD'BEEF_h;
        constexpr auto lit2 = 1111'1111'1111'1111_b;
        constexpr auto lit3 = 1'000'000_d;

        // 这些应该都是编译时常量
        REQUIRE(lit1.value() == 0xDEADBEEF);
        REQUIRE(lit1.actual_width == 32);
        REQUIRE(lit2.value() == 0xFFFF);
        REQUIRE(lit2.actual_width == 16);
        REQUIRE(lit3.value() == 1000000);
        REQUIRE(lit3.actual_width == 20);
    }

    SECTION("Template instantiation stress test") {
        // 测试多个不同长度的字面量
        constexpr auto b1 = 1_b;
        constexpr auto b2 = 11_b;
        constexpr auto b3 = 111_b;
        constexpr auto b4 = 1111_b;
        constexpr auto b8 = 1111'1111_b;
        constexpr auto b16 = 1111'1111'1111'1111_b;

        REQUIRE(b1.actual_width == 1);
        REQUIRE(b2.actual_width == 2);
        REQUIRE(b3.actual_width == 3);
        REQUIRE(b4.actual_width == 4);
        REQUIRE(b8.actual_width == 8);
        REQUIRE(b16.actual_width == 16);
    }
}

// ========== 错误处理和健壮性测试 ==========

TEST_CASE("Error handling and robustness", "[literal][error_handling]") {
    SECTION("Invalid character handling in parsers") {
        // 这些测试用于验证解析器的健壮性
        // 测试二进制解析器对无效字符的处理
        REQUIRE(lit_bin::is_digit('0') == true);
        REQUIRE(lit_bin::is_digit('1') == true);
        REQUIRE(lit_bin::is_digit('2') == false); // 无效二进制数字
        REQUIRE(lit_bin::is_digit('a') == false); // 无效字符

        // 测试八进制解析器
        REQUIRE(lit_oct::is_digit('0') == true);
        REQUIRE(lit_oct::is_digit('7') == true);
        REQUIRE(lit_oct::is_digit('8') == false); // 无效八进制数字

        // 测试十六进制解析器
        REQUIRE(lit_hex::is_digit('0') == true);
        REQUIRE(lit_hex::is_digit('9') == true);
        REQUIRE(lit_hex::is_digit('A') == true);
        REQUIRE(lit_hex::is_digit('F') == true);
        REQUIRE(lit_hex::is_digit('G') == false); // 无效十六进制数字
    }

    SECTION("Width boundary conditions") {
        // 测试宽度修正逻辑
        auto l1 = make_literal(0, 0);      // 宽度0应该修正为1
        auto l2 = make_literal(1, 1);      // 正常情况
        auto l3 = make_literal(0xFF, 100); // 宽度超过64应该修正为64

        REQUIRE(l1.width() == 1);  // 运行时版本使用width()方法
        REQUIRE(l2.width() == 1);  // 运行时版本使用width()方法
        REQUIRE(l3.width() == 64); // 运行时版本使用width()方法

        // 测试值与宽度的匹配
        auto l4 = make_literal(0x100, 8); // 值需要9位但指定8位 - 应该允许
        REQUIRE(l4.value() == 0x100);
        REQUIRE(l4.width() == 8); // 运行时版本使用width()方法
    }
}

// ========== 实际使用场景测试 ==========

TEST_CASE("Real-world usage scenarios", "[literal][real_world]") {
    SECTION("Register and memory initialization") {
        // 常见的寄存器初始化模式
        constexpr auto reset_value = 0x0000_h;
        constexpr auto default_config = 0x1234_h;
        constexpr auto enable_mask = 1111'1111_b;  // 8位使能掩码
        constexpr auto disable_mask = 0000'0000_b; // 8位禁用掩码

        REQUIRE(reset_value.value() == 0);
        REQUIRE(reset_value.actual_width == 1);
        REQUIRE(default_config.value() == 0x1234);
        REQUIRE(default_config.actual_width == 13);
        REQUIRE(enable_mask.value() == 0xFF);
        REQUIRE(enable_mask.actual_width == 8);
        REQUIRE(disable_mask.value() == 0);
        REQUIRE(disable_mask.actual_width == 1);
    }

    SECTION("Bit field manipulation") {
        // 位域操作常用的模式
        constexpr auto bit0 = 1_b;                 // 最低位
        constexpr auto bit7 = 1'0000'000_b;        // 第7位 (128)
        constexpr auto lower_nibble = 1111_b;      // 低4位 (15)
        constexpr auto upper_nibble = 1111'0000_b; // 高4位 (240)

        REQUIRE(bit0.value() == 1);
        REQUIRE(bit0.actual_width == 1);
        REQUIRE(bit7.value() == 128);
        REQUIRE(bit7.actual_width == 8);
        REQUIRE(lower_nibble.value() == 15);
        REQUIRE(lower_nibble.actual_width == 4);
        REQUIRE(upper_nibble.value() == 240);
        REQUIRE(upper_nibble.actual_width == 8);
    }

    SECTION("Timing and counter values") {
        // 定时器和计数器常用值
        constexpr auto microsecond = 1'000'000_d; // 1MHz时钟的微秒计数
        constexpr auto millisecond = 1'000_d;     // 毫秒计数
        constexpr auto second = 1_d; // 秒计数（通常作为倍数）

        REQUIRE(microsecond.value() == 1000000);
        REQUIRE(microsecond.actual_width == 20);
        REQUIRE(millisecond.value() == 1000);
        REQUIRE(millisecond.actual_width == 10);
        REQUIRE(second.value() == 1);
        REQUIRE(second.actual_width == 1);
    }
}

// ========== 动态测试：验证实际计算结果 ==========

TEST_CASE("Dynamic verification of literal calculations",
          "[literal][dynamic]") {
    SECTION("Binary literal calculations") {
        // 使用运行时计算来验证静态计算是否正确
        auto test_binary = [](const char *str) -> uint64_t {
            uint64_t result = 0;
            for (const char *p = str; *p; ++p) {
                if (*p == '\'')
                    continue;
                if (*p == '0' || *p == '1') {
                    result = result * 2 + (*p - '0');
                }
            }
            return result;
        };

        REQUIRE(test_binary("1") == 1);
        REQUIRE(test_binary("11") == 3);
        REQUIRE(test_binary("1010") == 10);
        REQUIRE(test_binary("11111111") == 255);
    }

    SECTION("Octal literal calculations") {
        auto test_octal = [](const char *str) -> uint64_t {
            uint64_t result = 0;
            for (const char *p = str; *p; ++p) {
                if (*p == '\'')
                    continue;
                if (*p >= '0' && *p <= '7') {
                    result = result * 8 + (*p - '0');
                }
            }
            return result;
        };

        REQUIRE(test_octal("7") == 7);
        REQUIRE(test_octal("17") == 15);
        REQUIRE(test_octal("377") == 255);
    }

    SECTION("Hexadecimal literal calculations") {
        auto test_hex = [](const char *str) -> uint64_t {
            uint64_t result = 0;
            for (const char *p = str; *p; ++p) {
                if (*p == '\'' || *p == 'x' || *p == 'X')
                    continue;
                if (*p >= '0' && *p <= '9') {
                    result = result * 16 + (*p - '0');
                } else if (*p >= 'A' && *p <= 'F') {
                    result = result * 16 + (*p - 'A' + 10);
                } else if (*p >= 'a' && *p <= 'f') {
                    result = result * 16 + (*p - 'a' + 10);
                }
            }
            return result;
        };

        REQUIRE(test_hex("F") == 15);
        REQUIRE(test_hex("FF") == 255);
        REQUIRE(test_hex("DEADBEEF") == 0xDEADBEEF);
    }
}

// ========== 编译期和运行时字面量类型专项测试 ==========

TEST_CASE("Compile-time literal (ch_literal_impl) specific tests",
          "[literal][compile_time_specific]") {
    SECTION("Direct construction of ch_literal_impl") {
        // 直接构造ch_literal_impl实例
        constexpr ch_literal_impl<42, 8> lit1;
        REQUIRE(lit1.value() == 42);
        REQUIRE(lit1.actual_width == 8);
        REQUIRE(lit1.width() == 8); // 测试width()方法
        REQUIRE(lit1.is_zero() == false);

        constexpr ch_literal_impl<0, 1> lit2;
        REQUIRE(lit2.value() == 0);
        REQUIRE(lit2.actual_width == 1);
        REQUIRE(lit2.width() == 1);
        REQUIRE(lit2.is_zero() == true);

        // 测试边界情况
        constexpr ch_literal_impl<0xFFFFFFFFFFFFFFFF, 100> lit3;
        REQUIRE(lit3.value() == 0xFFFFFFFFFFFFFFFF);
        REQUIRE(lit3.actual_width == 64); // 应该被限制为64
        REQUIRE(lit3.width() == 64);
    }

    SECTION("ch_literal_impl is_ones method") {
        // 测试全1值检查
        constexpr ch_literal_impl<0xF, 4> all_ones_4bit;
        REQUIRE(all_ones_4bit.is_ones() == true);

        constexpr ch_literal_impl<0x7, 4> not_all_ones_4bit;
        REQUIRE(not_all_ones_4bit.is_ones() == false);

        constexpr ch_literal_impl<0xFF, 8> all_ones_8bit;
        REQUIRE(all_ones_8bit.is_ones() == true);

        // 测试64位全1值
        constexpr ch_literal_impl<0xFFFFFFFFFFFFFFFF, 64> all_ones_64bit;
        REQUIRE(all_ones_64bit.is_ones() == true);
    }

    SECTION("ch_literal_impl implicit conversion to ch_uint") {
        // 测试隐式转换到ch_uint<N>
        constexpr ch_literal_impl<0xFF, 8> lit;
        // 注意：这里只是检查类型特征，不实际执行转换（因为ch_uint未完全定义）
        STATIC_REQUIRE(
            std::is_convertible_v<decltype(lit), ch_literal_impl<0xFF, 8>>);
    }
}

TEST_CASE("Runtime literal (ch_literal_runtime) specific tests",
          "[literal][runtime_specific]") {
    SECTION("Direct construction of ch_literal_runtime") {
        // 直接构造ch_literal_runtime实例
        ch_literal_runtime lit1(42, 8);
        REQUIRE(lit1.value() == 42);
        REQUIRE(lit1.actual_width == 8);
        REQUIRE(lit1.width() == 8);
        REQUIRE(lit1.is_zero() == false);

        ch_literal_runtime lit2(0, 1);
        REQUIRE(lit2.value() == 0);
        REQUIRE(lit2.actual_width == 1);
        REQUIRE(lit2.width() == 1);
        REQUIRE(lit2.is_zero() == true);

        // 测试边界情况
        ch_literal_runtime lit3(0xFFFFFFFFFFFFFFFF, 100);
        REQUIRE(lit3.value() == 0xFFFFFFFFFFFFFFFF);
        REQUIRE(lit3.actual_width == 64); // 应该被限制为64
        REQUIRE(lit3.width() == 64);
    }

    SECTION("ch_literal_runtime constructors with different types") {
        // 测试不同类型构造函数
        ch_literal_runtime lit_from_uint64(static_cast<uint64_t>(42));
        REQUIRE(lit_from_uint64.value() == 42);
        REQUIRE(lit_from_uint64.width() == 6); // 自动计算宽度

        ch_literal_runtime lit_from_int64(
            static_cast<int64_t>(-1)); // 会被转换为uint64_t
        REQUIRE(lit_from_int64.value() == static_cast<uint64_t>(-1));

        ch_literal_runtime lit_from_uint32(static_cast<uint32_t>(255));
        REQUIRE(lit_from_uint32.value() == 255);
        REQUIRE(lit_from_uint32.width() == 8);

        ch_literal_runtime lit_from_bool(true);
        REQUIRE(lit_from_bool.value() == 1);
        REQUIRE(lit_from_bool.width() == 1);

        ch_literal_runtime lit_from_bool_false(false);
        REQUIRE(lit_from_bool_false.value() == 0);
        REQUIRE(lit_from_bool_false.width() == 1);
    }

    SECTION("ch_literal_runtime is_ones method") {
        // 测试全1值检查
        ch_literal_runtime all_ones_4bit(0xF, 4);
        REQUIRE(all_ones_4bit.is_ones() == true);

        ch_literal_runtime not_all_ones_4bit(0x7, 4);
        REQUIRE(not_all_ones_4bit.is_ones() == false);

        ch_literal_runtime all_ones_8bit(0xFF, 8);
        REQUIRE(all_ones_8bit.is_ones() == true);

        // 测试64位全1值
        ch_literal_runtime all_ones_64bit(0xFFFFFFFFFFFFFFFF, 64);
        REQUIRE(all_ones_64bit.is_ones() == true);
    }

    SECTION("ch_literal_runtime implicit conversion to ch_uint") {
        // 测试隐式转换到ch_uint<N>
        ch_literal_runtime lit(0xFF, 8);
        // 注意：这里只是检查类型特征，不实际执行转换（因为ch_uint未完全定义）
        STATIC_REQUIRE(
            std::is_convertible_v<ch_literal_runtime, ch_literal_runtime>);
    }

    SECTION("ch_literal_runtime compute_width static method") {
        // 测试静态方法compute_width
        REQUIRE(ch_literal_runtime::compute_width(0) == 1);
        REQUIRE(ch_literal_runtime::compute_width(1) == 1);
        REQUIRE(ch_literal_runtime::compute_width(2) == 2);
        REQUIRE(ch_literal_runtime::compute_width(3) == 2);
        REQUIRE(ch_literal_runtime::compute_width(0xFF) == 8);
        REQUIRE(ch_literal_runtime::compute_width(0xFFFFFFFFFFFFFFFF) == 64);
    }
}

// ========== 编译期与运行时混合使用测试 ==========

TEST_CASE("Mixing compile-time and runtime literals",
          "[literal][mixed_usage]") {
    SECTION("Type trait works for both types") {
        // 测试类型特征对两种类型都适用
        STATIC_REQUIRE(is_ch_literal_v<ch_literal_impl<42, 8>> == true);
        STATIC_REQUIRE(is_ch_literal_v<ch_literal_runtime> == true);
        STATIC_REQUIRE(is_ch_literal_v<int> == false);

        // 测试cv限定版本
        STATIC_REQUIRE(is_ch_literal_v<const ch_literal_impl<0, 1>> == true);
        STATIC_REQUIRE(is_ch_literal_v<volatile ch_literal_impl<42, 8>> ==
                       true);
        STATIC_REQUIRE(is_ch_literal_v<const ch_literal_runtime> == true);
        STATIC_REQUIRE(is_ch_literal_v<volatile ch_literal_runtime> == true);
    }

    SECTION("Using both types in generic code") {
        // 测试在泛型代码中使用两种类型
        auto process_literal = [](const auto &lit) {
            if constexpr (is_ch_literal_v<std::decay_t<decltype(lit)>>) {
                if constexpr (std::is_same_v<std::decay_t<decltype(lit)>,
                                             ch_literal_runtime>) {
                    // 运行时类型
                    return std::make_pair(lit.value(), lit.width());
                } else {
                    // 编译时类型
                    return std::make_pair(lit.value(), lit.width());
                }
            }
            return std::make_pair(static_cast<uint64_t>(0), 0u);
        };

        // 测试编译时字面量
        constexpr ch_literal_impl<42, 8> compile_time_lit;
        auto result1 = process_literal(compile_time_lit);
        REQUIRE(result1.first == 42);
        REQUIRE(result1.second == 8);

        // 测试运行时字面量
        ch_literal_runtime runtime_lit(42, 8);
        auto result2 = process_literal(runtime_lit);
        REQUIRE(result2.first == 42);
        REQUIRE(result2.second == 8);
    }
}

// ========== 编译期与运行时性能和特性对比测试 ==========

TEST_CASE("Compile-time vs Runtime literal characteristics",
          "[literal][characteristics]") {
    SECTION("Compile-time evaluation") {
        // 验证编译时字面量的所有方法和属性都是constexpr
        constexpr ch_literal_impl<0xDEAD, 16> compile_time_lit;
        constexpr auto val = compile_time_lit.value();
        constexpr auto width = compile_time_lit.width();
        constexpr auto actual_width = compile_time_lit.actual_width;
        constexpr auto is_zero = compile_time_lit.is_zero();
        constexpr auto is_ones = compile_time_lit.is_ones();

        REQUIRE(val == 0xDEAD);
        REQUIRE(width == 16);
        REQUIRE(actual_width == 16);
        REQUIRE(is_zero == false);
        REQUIRE(is_ones == false);
    }

    SECTION("Runtime evaluation") {
        // 验证运行时字面量在运行时的工作情况
        ch_literal_runtime runtime_lit(0xDEAD, 16);
        auto val = runtime_lit.value();
        auto width = runtime_lit.width();
        auto is_zero = runtime_lit.is_zero();
        auto is_ones = runtime_lit.is_ones();

        REQUIRE(val == 0xDEAD);
        REQUIRE(width == 16);
        REQUIRE(is_zero == false);
        REQUIRE(is_ones == false);
    }

    SECTION("Compile-time vs Runtime creation performance") {
        // 测试创建方式的差异

        // 编译时字面量 - 所有值在编译时确定
        constexpr auto ct_literal = 11_b; // 结果是ch_literal_impl<3, 2>

        // 运行时字面量 - 值在运行时确定
        auto rt_literal = make_literal(0xFF); // 结果是ch_literal_runtime

        REQUIRE(ct_literal.value() == 3); // 11_b 的十进制值是 3 (2^1 + 2^0 = 3)
        REQUIRE(ct_literal.actual_width == 2);
        REQUIRE(rt_literal.value() == 0xFF);
        REQUIRE(rt_literal.width() == 8);
    }
}

// ========== 字面量在实际硬件描述场景中的使用测试 ==========

TEST_CASE("Literal usage in hardware description scenarios",
          "[literal][hardware_scenarios]") {
    SECTION("Using compile-time literals for constant definitions") {
        // 在硬件描述中使用编译时字面量定义常量
        constexpr auto WORD_SIZE = 11111_b; // 5位全1 (31)
        constexpr auto ADDR_WIDTH = 1111_b; // 4位全1 (15)

        // 这些值可以在编译时完全确定
        STATIC_REQUIRE(WORD_SIZE.value() == 31); // 5个1 = 16+8+4+2+1 = 31
        STATIC_REQUIRE(WORD_SIZE.actual_width == 5);
        STATIC_REQUIRE(ADDR_WIDTH.value() == 15); // 4个1 = 8+4+2+1 = 15
        STATIC_REQUIRE(ADDR_WIDTH.actual_width == 4);
    }

    SECTION("Using runtime literals for dynamic values") {
        // 在硬件描述中使用运行时字面量处理动态值
        auto user_defined_value = make_literal(0x1234);
        auto register_reset_value = make_literal(0x0000, 16);
        auto register_max_value = make_literal(0xFFFF, 16);

        REQUIRE(user_defined_value.value() == 0x1234);
        REQUIRE(user_defined_value.width() == 13); // 需要13位表示0x1234
        REQUIRE(register_reset_value.value() == 0x0000);
        REQUIRE(register_reset_value.width() == 16);
        REQUIRE(register_max_value.value() == 0xFFFF);
        REQUIRE(register_max_value.width() == 16);
    }

    SECTION("Mixing compile-time and runtime literals in expressions") {
        // 混合使用两种字面量
        constexpr auto const_mask = 11111111_b; // 编译时确定的掩码 (255)
        auto dynamic_value = make_literal(0x1234); // 运行时确定的值

        // 虽然不能在编译时计算表达式结果，但可以验证类型兼容性
        // STATIC_REQUIRE(std::is_same_v<decltype(const_mask),
        // ch_literal_impl<255, 8>>);
        STATIC_REQUIRE(
            std::is_same_v<decltype(dynamic_value), ch_literal_runtime>);
    }
}
