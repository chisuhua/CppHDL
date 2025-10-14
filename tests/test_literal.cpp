#include "catch_amalgamated.hpp"
#include "core/literal.h"
#include <type_traits>

using namespace ch::core;

// ---------- 基础类型测试 ----------

TEST_CASE("ch_literal: basic functionality", "[literal][ch_literal]") {
    // 默认构造
    ch_literal l1;
    REQUIRE(l1.value == 0);
    REQUIRE(l1.actual_width == 1);
    
    // 带参数构造
    ch_literal l2(0xFF, 8);
    REQUIRE(l2.value == 0xFF);
    REQUIRE(l2.actual_width == 8);
    
    // 边界条件测试
    ch_literal l3(0, 0);  // 宽度0应该被修正为1
    REQUIRE(l3.actual_width == 1);
    
    ch_literal l4(0x12345678, 100);  // 宽度超过64应该被修正为64
    REQUIRE(l4.actual_width == 64);
}

// ---------- 类型特征测试 ----------

TEST_CASE("is_ch_literal: type trait verification", "[literal][traits]") {
    STATIC_REQUIRE(is_ch_literal_v<ch_literal> == true);
    STATIC_REQUIRE(is_ch_literal_v<int> == false);
    STATIC_REQUIRE(is_ch_literal_v<double> == false);
    
    using ConstLit = const ch_literal;
    using VolatileLit = volatile ch_literal;
    using ConstVolatileLit = const volatile ch_literal;
    
    STATIC_REQUIRE(is_ch_literal_v<ConstLit> == true);
    STATIC_REQUIRE(is_ch_literal_v<VolatileLit> == true);
    STATIC_REQUIRE(is_ch_literal_v<ConstVolatileLit> == true);
}

// ---------- 辅助函数测试 ----------

TEST_CASE("bit_width: calculate minimum bit width", "[literal][bit_width]") {
    REQUIRE(bit_width(0) == 1);
    REQUIRE(bit_width(1) == 1);
    REQUIRE(bit_width(2) == 2);
    REQUIRE(bit_width(3) == 2);
    REQUIRE(bit_width(4) == 3);
    REQUIRE(bit_width(7) == 3);
    REQUIRE(bit_width(8) == 4);
    REQUIRE(bit_width(0xFF) == 8);
    REQUIRE(bit_width(0x100) == 9);
    REQUIRE(bit_width(0xFFFF) == 16);
    REQUIRE(bit_width(0x10000) == 17);
}

TEST_CASE("make_literal: create literals with specified width", "[literal][make_literal]") {
    auto l1 = make_literal(0xFF, 8);
    REQUIRE(l1.value == 0xFF);
    REQUIRE(l1.actual_width == 8);
    
    auto l2 = make_literal(0x1234, 16);
    REQUIRE(l2.value == 0x1234);
    REQUIRE(l2.actual_width == 16);
}

TEST_CASE("make_literal_auto: create literals with auto width", "[literal][make_literal_auto]") {
    auto l1 = make_literal_auto(0);
    REQUIRE(l1.value == 0);
    REQUIRE(l1.actual_width == 1);
    
    auto l2 = make_literal_auto(1);
    REQUIRE(l2.value == 1);
    REQUIRE(l2.actual_width == 1);
    
    auto l3 = make_literal_auto(0xFF);
    REQUIRE(l3.value == 0xFF);
    REQUIRE(l3.actual_width == 8);
    
    auto l4 = make_literal_auto(0x100);
    REQUIRE(l4.value == 0x100);
    REQUIRE(l4.actual_width == 9);
}

// ========== 硬件友好字面量测试 ==========

// ---------- 字面量解析器测试 ----------

TEST_CASE("lit_bin: binary literal parser", "[literal][lit_bin]") {
    REQUIRE(lit_bin::is_digit('0') == true);
    REQUIRE(lit_bin::is_digit('1') == true);
    REQUIRE(lit_bin::is_digit('2') == false);
    
    REQUIRE(lit_bin::is_escape('\'') == true);
    REQUIRE(lit_bin::is_escape('x') == false);
    
    REQUIRE(lit_bin::size('0', 0) == 1);
    REQUIRE(lit_bin::size('1', 1) == 2);
    REQUIRE(lit_bin::size('\'', 5) == 5);
    
    REQUIRE(lit_bin::chr2int('0') == 0);
    REQUIRE(lit_bin::chr2int('1') == 1);
}

TEST_CASE("lit_oct: octal literal parser", "[literal][lit_oct]") {
    REQUIRE(lit_oct::is_digit('0') == true);
    REQUIRE(lit_oct::is_digit('7') == true);
    REQUIRE(lit_oct::is_digit('8') == false);
    
    REQUIRE(lit_oct::is_escape('\'') == true);
    
    REQUIRE(lit_oct::size('0', 0) == 3);
    REQUIRE(lit_oct::size('\'', 5) == 5);
    
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
    
    REQUIRE(lit_hex::size('0', 0) == 4);
    REQUIRE(lit_hex::size('x', 0) == 0);
    REQUIRE(lit_hex::size('X', 0) == 0);
    REQUIRE(lit_hex::size('\'', 5) == 5);
    
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
    
    REQUIRE(lit_dec_size_v<'1', '2', '3'> == 7);  // 123 需要 7 位 (1111011)
    REQUIRE(lit_dec_size_v<'0'> == 1);
    REQUIRE(lit_dec_size_v<'2', '5', '5'> == 8);  // 255 需要 8 位
}

// ---------- 编译时字面量大小计算测试 ----------

TEST_CASE("lit_size: compile-time size calculation", "[literal][lit_size]") {
    // 二进制字面量大小计算
    STATIC_REQUIRE(lit_bin_size_v<> == 0);
    STATIC_REQUIRE(lit_bin_size_v<'1'> == 1);
    STATIC_REQUIRE(lit_bin_size_v<'1', '0'> == 2);
    STATIC_REQUIRE(lit_bin_size_v<'1', '\'', '0', '1'> == 3);
    STATIC_REQUIRE(lit_bin_size_v<'1', '1', '1', '1'> == 4);
    
    // 八进制字面量大小计算
    STATIC_REQUIRE(lit_oct_size_v<> == 0);
    STATIC_REQUIRE(lit_oct_size_v<'7'> == 3);
    STATIC_REQUIRE(lit_oct_size_v<'1', '7'> == 6);
    STATIC_REQUIRE(lit_oct_size_v<'1', '\'', '7'> == 6);
    
    // 十六进制字面量大小计算
    STATIC_REQUIRE(lit_hex_size_v<> == 0);
    STATIC_REQUIRE(lit_hex_size_v<'F'> == 4);
    STATIC_REQUIRE(lit_hex_size_v<'1', 'F'> == 8);
    STATIC_REQUIRE(lit_hex_size_v<'1', '\'', 'F'> == 8);
    STATIC_REQUIRE(lit_hex_size_v<'x', 'F', 'F'> == 8);
}

// ---------- 编译时字面量值计算测试 ----------

TEST_CASE("lit_value: compile-time value calculation", "[literal][lit_value]") {
    // 二进制字面量值计算
    STATIC_REQUIRE(lit_bin_value_v<> == 0);
    STATIC_REQUIRE(lit_bin_value_v<'1'> == 1);
    STATIC_REQUIRE(lit_bin_value_v<'1', '0'> == 2);
    STATIC_REQUIRE(lit_bin_value_v<'1', '1'> == 3);
    STATIC_REQUIRE(lit_bin_value_v<'1', '0', '1', '0'> == 10);
    
    // 八进制字面量值计算
    STATIC_REQUIRE(lit_oct_value_v<> == 0);
    STATIC_REQUIRE(lit_oct_value_v<'7'> == 7);
    STATIC_REQUIRE(lit_oct_value_v<'1', '7'> == 15); // 1*8 + 7 = 15
    
    // 十六进制字面量值计算
    STATIC_REQUIRE(lit_hex_value_v<> == 0);
    STATIC_REQUIRE(lit_hex_value_v<'F'> == 15);
    STATIC_REQUIRE(lit_hex_value_v<'1', 'F'> == 31); // 1*16 + 15 = 31
    STATIC_REQUIRE(lit_hex_value_v<'F', 'F'> == 255); // 15*16 + 15 = 255
    
    // 十进制字面量值计算
    STATIC_REQUIRE(lit_dec_value_v<'0'> == 0);
    STATIC_REQUIRE(lit_dec_value_v<'1'> == 1);
    STATIC_REQUIRE(lit_dec_value_v<'1', '0'> == 10);
    STATIC_REQUIRE(lit_dec_value_v<'2', '5', '5'> == 255);
    STATIC_REQUIRE(lit_dec_value_v<'1', '\'', '0', '0', '0'> == 1000);
}

// ========== 真正的硬件友好字面量测试 ==========

TEST_CASE("Hardware-friendly literals: binary literals", "[literal][hardware_friendly][binary]") {
    SECTION("Simple binary literals") {
        constexpr auto lit1 = 1_b;
        REQUIRE(lit1.value == 1);
        REQUIRE(lit1.actual_width == 1);
        
        constexpr auto lit11 = 11_b;
        REQUIRE(lit11.value == 3);  // 11(二进制) = 3(十进制)
        REQUIRE(lit11.actual_width == 2);
        
        constexpr auto lit1010 = 1010_b;
        REQUIRE(lit1010.value == 10);  // 1010(二进制) = 10(十进制)
        REQUIRE(lit1010.actual_width == 4);
    }
    
    SECTION("Binary literals with separators") {
        constexpr auto lit = 1'0'1'0_b;
        REQUIRE(lit.value == 10);
        REQUIRE(lit.actual_width == 4);
        
        constexpr auto lit2 = 1111'0000_b;
        REQUIRE(lit2.value == 240);  // 11110000(二进制) = 240(十进制)
        REQUIRE(lit2.actual_width == 8);
    }
    
    SECTION("Common binary patterns") {
        constexpr auto all_ones_4bit = 1111_b;
        REQUIRE(all_ones_4bit.value == 15);
        REQUIRE(all_ones_4bit.actual_width == 4);
        
        constexpr auto alternating = 1010'1010_b;
        REQUIRE(alternating.value == 170);  // 10101010(二进制) = 170(十进制)
        REQUIRE(alternating.actual_width == 8);
    }
}

TEST_CASE("Hardware-friendly literals: octal literals", "[literal][hardware_friendly][octal]") {
    SECTION("Simple octal literals") {
        constexpr auto lit1 = 7_o;
        REQUIRE(lit1.value == 7);
        REQUIRE(lit1.actual_width == 3);  // 7需要3位二进制表示
        
        constexpr auto lit2 = 17_o;
        REQUIRE(lit2.value == 15);  // 17(八进制) = 15(十进制)
        REQUIRE(lit2.actual_width == 4);  // 15需要4位二进制表示
    }
    
    SECTION("Octal literals with separators") {
        constexpr auto lit = 3'7'7_o;
        REQUIRE(lit.value == 255);  // 377(八进制) = 255(十进制)
        REQUIRE(lit.actual_width == 8);
    }
}

TEST_CASE("Hardware-friendly literals: hexadecimal literals", "[literal][hardware_friendly][hex]") {
    SECTION("Simple hex literals") {
        constexpr auto lit1 = F_h;
        REQUIRE(lit1.value == 15);
        REQUIRE(lit1.actual_width == 4);
        
        constexpr auto lit2 = FF_h;
        REQUIRE(lit2.value == 255);
        REQUIRE(lit2.actual_width == 8);
        
        constexpr auto lit3 = DEAD'BEEF_h;
        REQUIRE(lit3.value == 0xDEADBEEF);
        REQUIRE(lit3.actual_width == 32);
    }
    
    SECTION("Hex literals with x prefix") {
        constexpr auto lit = xFF_h;
        REQUIRE(lit.value == 255);
        REQUIRE(lit.actual_width == 8);
    }
    
    SECTION("Mixed case hex literals") {
        constexpr auto lit1 = AbCd_h;
        REQUIRE(lit1.value == 0xABCD);
        REQUIRE(lit1.actual_width == 16);
        
        constexpr auto lit2 = dead'beef_h;
        REQUIRE(lit2.value == 0xDEADBEEF);
        REQUIRE(lit2.actual_width == 32);
    }
}

TEST_CASE("Hardware-friendly literals: decimal literals", "[literal][hardware_friendly][decimal]") {
    SECTION("Simple decimal literals") {
        constexpr auto lit1 = 0_d;
        REQUIRE(lit1.value == 0);
        REQUIRE(lit1.actual_width == 1);
        
        constexpr auto lit2 = 1_d;
        REQUIRE(lit2.value == 1);
        REQUIRE(lit2.actual_width == 1);
        
        constexpr auto lit3 = 10_d;
        REQUIRE(lit3.value == 10);
        REQUIRE(lit3.actual_width == 4);  // 10 需要 4 位 (1010)
        
        constexpr auto lit4 = 255_d;
        REQUIRE(lit4.value == 255);
        REQUIRE(lit4.actual_width == 8);
    }
    
    SECTION("Decimal literals with separators") {
        constexpr auto lit = 1'000'000_d;
        REQUIRE(lit.value == 1000000);
        REQUIRE(lit.actual_width == 20);  // 1000000 需要 20 位
    }
    
    SECTION("Common decimal values") {
        constexpr auto count = 1024_d;
        REQUIRE(count.value == 1024);
        REQUIRE(count.actual_width == 11);  // 2^10 = 1024, 需要 11 位
        
        constexpr auto max_uint8 = 255_d;
        REQUIRE(max_uint8.value == 255);
        REQUIRE(max_uint8.actual_width == 8);
    }
}

TEST_CASE("Hardware-friendly literals: practical usage examples", "[literal][hardware_friendly][practical]") {
    SECTION("Common register initialization values") {
        // 全0
        constexpr auto zero8 = 0000'0000_b;
        REQUIRE(zero8.value == 0);
        REQUIRE(zero8.actual_width == 8);
        
        // 全1
        constexpr auto ones8 = 1111'1111_b;
        REQUIRE(ones8.value == 255);
        REQUIRE(ones8.actual_width == 8);
        
        // 交替模式
        constexpr auto pattern8 = 1010'1010_b;
        REQUIRE(pattern8.value == 170);
        REQUIRE(pattern8.actual_width == 8);
    }
    
    SECTION("Memory address patterns") {
        constexpr auto addr = 0xDEAD'BEEF_h;
        REQUIRE(addr.value == 0xDEADBEEF);
        REQUIRE(addr.actual_width == 32);
        
        constexpr auto low_addr = 0x1000_h;
        REQUIRE(low_addr.value == 0x1000);
        REQUIRE(low_addr.actual_width == 13);  // 0x1000 = 4096，需要13位
    }
    
    SECTION("Decimal counters and delays") {
        constexpr auto counter_max = 1000_d;
        REQUIRE(counter_max.value == 1000);
        REQUIRE(counter_max.actual_width == 10);  // 1000 需要 10 位
        
        constexpr auto delay_cycles = 100_d;
        REQUIRE(delay_cycles.value == 100);
        REQUIRE(delay_cycles.actual_width == 7);  // 100 需要 7 位
    }
    
    SECTION("Bit mask patterns") {
        constexpr auto mask4 = 1111_b;      // 4位全1掩码
        constexpr auto mask8 = 1111'1111_b; // 8位全1掩码
        constexpr auto mask16 = 0xFFFF_h;   // 16位全1掩码
        
        REQUIRE(mask4.value == 15);
        REQUIRE(mask4.actual_width == 4);
        REQUIRE(mask8.value == 255);
        REQUIRE(mask8.actual_width == 8);
        REQUIRE(mask16.value == 0xFFFF);
        REQUIRE(mask16.actual_width == 16);
    }
}

// ========== 边界条件和错误处理测试 ==========

TEST_CASE("Edge cases and boundary conditions", "[literal][edge]") {
    SECTION("Zero values") {
        auto l1 = make_literal_auto(0);
        REQUIRE(l1.value == 0);
        REQUIRE(l1.actual_width == 1);
        
        ch_literal l2(0, 0);
        REQUIRE(l2.value == 0);
        REQUIRE(l2.actual_width == 1);  // 被修正
    }
    
    SECTION("Maximum values") {
        auto l1 = make_literal_auto(0xFF);
        REQUIRE(l1.value == 0xFF);
        REQUIRE(l1.actual_width == 8);
        
        auto l2 = make_literal_auto(0xFFFFFFFFFFFFFFFF);
        REQUIRE(l2.value == 0xFFFFFFFFFFFFFFFF);
        REQUIRE(l2.actual_width == 64);  // 最大宽度限制
    }
    
    SECTION("Width constraints") {
        auto l1 = make_literal(0x123, 16);
        REQUIRE(l1.value == 0x123);
        REQUIRE(l1.actual_width == 16);
        
        // 超过64位的宽度应该被限制
        auto l2 = make_literal(0x1FFFF, 100);
        REQUIRE(l2.actual_width == 64);  // 被修正为64
    }
}

// ========== 编译时特性和类型特征测试 ==========

TEST_CASE("Type traits and compile-time properties", "[literal][traits]") {
    STATIC_REQUIRE(std::is_standard_layout_v<ch_literal>);
    STATIC_REQUIRE(std::is_trivially_constructible_v<ch_literal>);
    
    // 测试 constexpr 函数
    STATIC_REQUIRE(bit_width(0) == 1);
    STATIC_REQUIRE(bit_width(1) == 1);
    STATIC_REQUIRE(bit_width(255) == 8);
    
    constexpr auto lit1 = make_literal_auto(42);
    STATIC_REQUIRE(lit1.value == 42);
    STATIC_REQUIRE(lit1.actual_width == 6);
    
    constexpr auto lit2 = make_literal(0xFF, 8);
    STATIC_REQUIRE(lit2.value == 0xFF);
    STATIC_REQUIRE(lit2.actual_width == 8);
    
    // 测试真正的硬件字面量编译时计算
    STATIC_REQUIRE(1_b.value == 1);
    STATIC_REQUIRE(1_b.actual_width == 1);
    
    STATIC_REQUIRE(11_b.value == 3);
    STATIC_REQUIRE(11_b.actual_width == 2);
    
    STATIC_REQUIRE(F_h.value == 15);
    STATIC_REQUIRE(F_h.actual_width == 4);
    
    STATIC_REQUIRE(FF_h.value == 255);
    STATIC_REQUIRE(FF_h.actual_width == 8);
    
    STATIC_REQUIRE(0_d.value == 0);
    STATIC_REQUIRE(0_d.actual_width == 1);
    
    STATIC_REQUIRE(255_d.value == 255);
    STATIC_REQUIRE(255_d.actual_width == 8);
    
    STATIC_REQUIRE(1000_d.value == 1000);
    STATIC_REQUIRE(1000_d.actual_width == 10);
}

// ========== 与现有系统集成测试 ==========

TEST_CASE("Integration with existing system", "[literal][integration]") {
    SECTION("ch_literal construction from arithmetic types") {
        // 模拟 get_lnode 中的使用场景
        ch_literal lit1 = make_literal_auto(42);
        REQUIRE(lit1.value == 42);
        REQUIRE(lit1.actual_width == 6);
        
        ch_literal lit2 = make_literal(0xFF, 8);
        REQUIRE(lit2.value == 0xFF);
        REQUIRE(lit2.actual_width == 8);
    }
    
    SECTION("Type trait usage") {
        auto test_func = [](auto&& val) {
            if constexpr (is_ch_literal_v<decltype(val)>) {
                return val.actual_width;
            } else {
                return 0u;
            }
        };
        
        ch_literal lit(42, 8);
        int not_lit = 42;
        
        REQUIRE(test_func(lit) == 8);
        REQUIRE(test_func(not_lit) == 0);
    }
    
    SECTION("Hardware literals in template contexts") {
        // 测试在模板中的使用
        auto process_literal = [](auto lit) {
            if constexpr (is_ch_literal_v<decltype(lit)>) {
                return std::make_pair(lit.value, lit.actual_width);
            } else {
                return std::make_pair(static_cast<uint64_t>(lit), 0u);
            }
        };
        
        auto result1 = process_literal(1010_b);
        REQUIRE(result1.first == 10);
        REQUIRE(result1.second == 4);
        
        auto result2 = process_literal(42_d);  // 使用十进制字面量
        REQUIRE(result2.first == 42);
        REQUIRE(result2.second == 6);
    }
    
    SECTION("Construction of ch_uint and ch_bool from literals") {
        // 测试与 ch_uint 集成
        ch_uint<8> u8(255_d);
        REQUIRE(static_cast<uint64_t>(u8) == 255);
        
        ch_uint<16> u16(0xDEAD_h);
        REQUIRE(static_cast<uint64_t>(u16) == 0xDEAD);
        
        // 测试与 ch_bool 集成
        ch_bool b1(1_b);
        ch_bool b2(0_b);
        REQUIRE(static_cast<bool>(b1) == true);
        REQUIRE(static_cast<bool>(b2) == false);
    }
}
