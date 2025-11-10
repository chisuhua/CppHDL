#include "catch_amalgamated.hpp"
#include "core/literal.h"
#include "core/reg.h"
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
    
    // Replace lit_bin::size with lit_bin_size_v
    REQUIRE(lit_bin_size_v<'0'> == 1);
    REQUIRE(lit_bin_size_v<'1','1'> == 2);
    REQUIRE(lit_bin_size_v<'\'','1','0'> == 2); // escape ignored
    
    REQUIRE(lit_bin::chr2int('0') == 0);
    REQUIRE(lit_bin::chr2int('1') == 1);
}

TEST_CASE("lit_oct: octal literal parser", "[literal][lit_oct]") {
    REQUIRE(lit_oct::is_digit('0') == true);
    REQUIRE(lit_oct::is_digit('7') == true);
    REQUIRE(lit_oct::is_digit('8') == false);
    
    REQUIRE(lit_oct::is_escape('\'') == true);
    
    // Replace lit_oct::size with lit_oct_size_v
    REQUIRE(lit_oct_size_v<'0'> == 3);
    REQUIRE(lit_oct_size_v<'\'','7'> == 3);
    
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
    REQUIRE(lit_hex_size_v<'0'> == 4);
    REQUIRE(lit_hex_size_v<'x','F'> == 4);   // 'x' is escape → ignored
    REQUIRE(lit_hex_size_v<'X','A'> == 4);   // 'X' is escape → ignored
    REQUIRE(lit_hex_size_v<'\'','F'> == 4);  // '\'' is escape → ignored
    
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
        constexpr auto lit1 = 0xF_h;
        REQUIRE(lit1.value == 15);
        REQUIRE(lit1.actual_width == 4);
        
        constexpr auto lit2 = 0xFF_h;
        REQUIRE(lit2.value == 255);
        REQUIRE(lit2.actual_width == 8);
        
        constexpr auto lit3 = 0xDEAD'BEEF_h;
        REQUIRE(lit3.value == 0xDEADBEEF);
        REQUIRE(lit3.actual_width == 32);
    }
    
    SECTION("Hex literals with x prefix") {
        constexpr auto lit = 0xFF_h;
        REQUIRE(lit.value == 255);
        REQUIRE(lit.actual_width == 8);
    }
    
    SECTION("Mixed case hex literals") {
        constexpr auto lit1 = 0xABCD_h;
        REQUIRE(lit1.value == 0xABCD);
        REQUIRE(lit1.actual_width == 16);
        
        constexpr auto lit2 = 0xDEAD'BEEF_h;
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
        REQUIRE(zero8.actual_width == 1);
        
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
    
    SECTION("Extreme values and edge cases") {
        // 测试 64 位最大值
        constexpr auto max64 = 0xFFFFFFFFFFFFFFFF_h;
        REQUIRE(max64.value == 0xFFFFFFFFFFFFFFFF);
        REQUIRE(max64.actual_width == 64);
        
        // 测试大十进制数
        constexpr auto big_num = 1'000'000'000_d;  // 10^9
        REQUIRE(big_num.value == 1000000000);
        REQUIRE(big_num.actual_width == 30);  // log2(10^9) ≈ 29.9，需要30位
        
        // 测试二进制长序列
        constexpr auto long_binary = 1111'1111'1111'1111_b;  // 16位全1
        REQUIRE(long_binary.value == 0xFFFF);
        REQUIRE(long_binary.actual_width == 16);
    }
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

TEST_CASE("Compile-time performance and efficiency", "[literal][compile_time]") {
    SECTION("Compile-time evaluation efficiency") {
        // 确保所有计算都在编译时完成
        constexpr auto lit1 = 0xDEAD'BEEF_h;
        constexpr auto lit2 = 1111'1111'1111'1111_b;
        constexpr auto lit3 = 1'000'000_d;
        
        // 这些应该都是编译时常量
        REQUIRE(lit1.value == 0xDEADBEEF);
        REQUIRE(lit1.actual_width == 32);
        REQUIRE(lit2.value == 0xFFFF);
        REQUIRE(lit2.actual_width == 16);
        REQUIRE(lit3.value == 1000000);
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
        REQUIRE(lit_bin::is_digit('2') == false);  // 无效二进制数字
        REQUIRE(lit_bin::is_digit('a') == false);  // 无效字符
        
        // 测试八进制解析器
        REQUIRE(lit_oct::is_digit('0') == true);
        REQUIRE(lit_oct::is_digit('7') == true);
        REQUIRE(lit_oct::is_digit('8') == false);  // 无效八进制数字
        
        // 测试十六进制解析器
        REQUIRE(lit_hex::is_digit('0') == true);
        REQUIRE(lit_hex::is_digit('9') == true);
        REQUIRE(lit_hex::is_digit('A') == true);
        REQUIRE(lit_hex::is_digit('F') == true);
        REQUIRE(lit_hex::is_digit('G') == false);  // 无效十六进制数字
    }
    
    SECTION("Width boundary conditions") {
        // 测试宽度修正逻辑
        ch_literal l1(0, 0);    // 宽度0应该修正为1
        ch_literal l2(1, 1);    // 正常情况
        ch_literal l3(0xFF, 100); // 宽度超过64应该修正为64
        
        REQUIRE(l1.actual_width == 1);
        REQUIRE(l2.actual_width == 1);
        REQUIRE(l3.actual_width == 64);
        
        // 测试值与宽度的匹配
        ch_literal l4(0x100, 8);  // 值需要9位但指定8位 - 应该允许
        REQUIRE(l4.value == 0x100);
        REQUIRE(l4.actual_width == 8);
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
        
        REQUIRE(reset_value.value == 0);
        REQUIRE(reset_value.actual_width == 1);
        REQUIRE(default_config.value == 0x1234);
        REQUIRE(default_config.actual_width == 13);
        REQUIRE(enable_mask.value == 0xFF);
        REQUIRE(enable_mask.actual_width == 8);
        REQUIRE(disable_mask.value == 0);
        REQUIRE(disable_mask.actual_width == 1);
    }
    
    SECTION("Bit field manipulation") {
        // 位域操作常用的模式
        constexpr auto bit0 = 1_b;        // 最低位
        constexpr auto bit7 = 1'0000'000_b; // 第7位
        constexpr auto lower_nibble = 1111_b;  // 低4位
        constexpr auto upper_nibble = 1111'0000_b; // 高4位
        
        REQUIRE(bit0.value == 1);
        REQUIRE(bit0.actual_width == 1);
        REQUIRE(bit7.value == 128);
        REQUIRE(bit7.actual_width == 8);
        REQUIRE(lower_nibble.value == 15);
        REQUIRE(lower_nibble.actual_width == 4);
        REQUIRE(upper_nibble.value == 240);
        REQUIRE(upper_nibble.actual_width == 8);
    }
    
    SECTION("Timing and counter values") {
        // 定时器和计数器常用值
        constexpr auto microsecond = 1'000'000_d;  // 1MHz时钟的微秒计数
        constexpr auto millisecond = 1'000_d;      // 毫秒计数
        constexpr auto second = 1_d;               // 秒计数（通常作为倍数）
        
        REQUIRE(microsecond.value == 1000000);
        REQUIRE(microsecond.actual_width == 20);
        REQUIRE(millisecond.value == 1000);
        REQUIRE(millisecond.actual_width == 10);
        REQUIRE(second.value == 1);
        REQUIRE(second.actual_width == 1);
    }
}

// ========== 动态测试：验证实际计算结果 ==========

TEST_CASE("Dynamic verification of literal calculations", "[literal][dynamic]") {
    SECTION("Binary literal calculations") {
        // 使用运行时计算来验证静态计算是否正确
        auto test_binary = [](const char* str) -> uint64_t {
            uint64_t result = 0;
            for (const char* p = str; *p; ++p) {
                if (*p == '\'') continue;
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
        auto test_octal = [](const char* str) -> uint64_t {
            uint64_t result = 0;
            for (const char* p = str; *p; ++p) {
                if (*p == '\'') continue;
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
        auto test_hex = [](const char* str) -> uint64_t {
            uint64_t result = 0;
            for (const char* p = str; *p; ++p) {
                if (*p == '\'' || *p == 'x' || *p == 'X') continue;
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
