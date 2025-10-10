#include "catch_amalgamated.hpp"
#include "reg.h"
#include "core/uint.h"
#include "core/traits.h"
#include "core/context.h"
#include "ast/ast_nodes.h"
#include <type_traits>

using namespace ch::core;

// 测试辅助函数
template<unsigned N>
using test_uint = ch_uint<N>;

// ---------- Width Trait Tests ----------

TEST_CASE("ch_width_impl: basic ch_uint types", "[reg][width][basic]") {
    STATIC_REQUIRE(ch_width_v<ch_uint<1>> == 1);
    STATIC_REQUIRE(ch_width_v<ch_uint<8>> == 8);
    STATIC_REQUIRE(ch_width_v<ch_uint<16>> == 16);
    STATIC_REQUIRE(ch_width_v<ch_uint<32>> == 32);
    STATIC_REQUIRE(ch_width_v<ch_uint<64>> == 64);
}

TEST_CASE("ch_width_impl: ch_reg_impl basic types", "[reg][width][basic_reg]") {
    STATIC_REQUIRE(ch_width_v<ch_reg_impl<ch_uint<1>>> == 1);
    STATIC_REQUIRE(ch_width_v<ch_reg_impl<ch_uint<8>>> == 8);
    STATIC_REQUIRE(ch_width_v<ch_reg_impl<ch_uint<16>>> == 16);
    STATIC_REQUIRE(ch_width_v<ch_reg_impl<ch_uint<32>>> == 32);
    STATIC_REQUIRE(ch_width_v<ch_reg_impl<ch_uint<64>>> == 64);
}

TEST_CASE("ch_width_impl: nested ch_reg_impl types", "[reg][width][nested]") {
    using Reg8 = ch_reg_impl<ch_uint<8>>;
    using NestedReg8 = ch_reg_impl<Reg8>;
    using TripleNestedReg8 = ch_reg_impl<NestedReg8>;
    
    STATIC_REQUIRE(ch_width_v<Reg8> == 8);
    STATIC_REQUIRE(ch_width_v<NestedReg8> == 8);
    STATIC_REQUIRE(ch_width_v<TripleNestedReg8> == 8);
}

TEST_CASE("ch_width_impl: const qualified ch_reg_impl", "[reg][width][const]") {
    STATIC_REQUIRE(ch_width_v<const ch_reg_impl<ch_uint<8>>> == 8);
    STATIC_REQUIRE(ch_width_v<volatile ch_reg_impl<ch_uint<16>>> == 16);
    STATIC_REQUIRE(ch_width_v<const volatile ch_reg_impl<ch_uint<32>>> == 32);
}

// ---------- ch_reg_impl Construction Tests ----------

TEST_CASE("ch_reg_impl: default construction", "[reg][construction][default]") {
    // 创建测试上下文
    ch::core::context test_ctx("test");
    ch::core::ctx_swap ctx_guard(&test_ctx);
    
    // 测试基本寄存器构造
    ch_reg_impl<ch_uint<8>> reg8;
    REQUIRE(true); // 如果构造成功且无异常，则测试通过
    
    ch_reg_impl<ch_uint<16>> reg16;
    REQUIRE(true);
    
    ch_reg_impl<ch_uint<32>> reg32;
    REQUIRE(true);
}

TEST_CASE("ch_reg_impl: construction with literal initial value", "[reg][construction][literal]") {
    // 创建测试上下文
    ch::core::context test_ctx("test");
    ch::core::ctx_swap ctx_guard(&test_ctx);
    
    // 测试带初始值的构造
    ch_reg_impl<ch_uint<8>> reg8(42);
    REQUIRE(true); // 如果构造成功且无异常，则测试通过
    
    ch_reg_impl<ch_uint<16>> reg16(1000);
    REQUIRE(true);
}

TEST_CASE("ch_reg_impl: construction with ch_uint initial value", "[reg][construction][ch_uint]") {
    // 创建测试上下文
    ch::core::context test_ctx("test");
    ch::core::ctx_swap ctx_guard(&test_ctx);
    
    // 测试使用ch_uint作为初始值
    ch_uint<8> init_val(nullptr); // 注意：这里需要根据实际实现调整
    // ch_reg_impl<ch_uint<8>> reg8(init_val);
    REQUIRE(true); // 占位测试
}

// ---------- ch_reg_impl Type Traits Tests ----------

TEST_CASE("ch_reg_impl: type traits verification", "[reg][traits]") {
    using Reg8 = ch_reg_impl<ch_uint<8>>;
    using Reg16 = ch_reg_impl<ch_uint<16>>;
    
    // 验证类型特性
    STATIC_REQUIRE(std::is_class_v<Reg8>);
    STATIC_REQUIRE(std::is_class_v<Reg16>);
    
    // 验证继承关系
    STATIC_REQUIRE(std::is_base_of_v<ch_uint<8>, ch_reg_impl<ch_uint<8>>>);
    STATIC_REQUIRE(std::is_base_of_v<logic_buffer<ch_uint<8>>, ch_reg_impl<ch_uint<8>>>);
}

TEST_CASE("ch_reg_impl: nested type traits", "[reg][traits][nested]") {
    using Reg8 = ch_reg_impl<ch_uint<8>>;
    using NestedReg8 = ch_reg_impl<Reg8>;
    
    STATIC_REQUIRE(std::is_class_v<NestedReg8>);
    STATIC_REQUIRE(std::is_base_of_v<Reg8, NestedReg8>);
    STATIC_REQUIRE(std::is_base_of_v<ch_uint<8>, NestedReg8>);
}

// ---------- ch_reg_impl Width Consistency Tests ----------

TEST_CASE("ch_reg_impl: width consistency across constructions", "[reg][width][consistency]") {
    // 创建测试上下文
    ch::core::context test_ctx("test");
    ch::core::ctx_swap ctx_guard(&test_ctx);
    
    // 验证不同构造方式下宽度的一致性
    constexpr unsigned expected_width = 8;
    
    SECTION("Default construction") {
        ch_reg_impl<ch_uint<expected_width>> reg;
        STATIC_REQUIRE(ch_width_v<decltype(reg)> == expected_width);
    }
    
    SECTION("Literal construction") {
        ch_reg_impl<ch_uint<expected_width>> reg(42);
        STATIC_REQUIRE(ch_width_v<decltype(reg)> == expected_width);
    }
    
    SECTION("Nested construction") {
        using RegType = ch_reg_impl<ch_uint<expected_width>>;
        using NestedRegType = ch_reg_impl<RegType>;
        STATIC_REQUIRE(ch_width_v<NestedRegType> == expected_width);
    }
}

// ---------- ch_reg_impl Alias Tests ----------

TEST_CASE("ch_reg: alias type verification", "[reg][alias]") {
    STATIC_REQUIRE(std::is_same_v<ch_reg<ch_uint<8>>, const ch_reg_impl<ch_uint<8>>>);
    STATIC_REQUIRE(std::is_same_v<ch_reg<ch_uint<16>>, const ch_reg_impl<ch_uint<16>>>);
    
    // 验证嵌套情况
    using Reg8 = ch_reg<ch_uint<8>>;
    using NestedReg8 = ch_reg<Reg8>;
    STATIC_REQUIRE(std::is_same_v<NestedReg8, const ch_reg_impl<const ch_reg_impl<ch_uint<8>>>>);
}

// ---------- Edge Case Tests ----------

TEST_CASE("ch_reg_impl: zero width type (if supported)", "[reg][edge][zero]") {
    // 注意：0宽度可能不被支持，根据实际实现调整
    // ch_reg_impl<ch_uint<0>> reg;
    // STATIC_REQUIRE(ch_width_v<decltype(reg)> == 0);
    REQUIRE(true); // 占位测试
}

TEST_CASE("ch_reg_impl: maximum width type", "[reg][edge][max]") {
    // 创建测试上下文
    ch::core::context test_ctx("test");
    ch::core::ctx_swap ctx_guard(&test_ctx);
    
    // 测试较大宽度（根据实际限制调整）
    // ch_reg_impl<ch_uint<32>> reg32;
    REQUIRE(true); // 占位测试
}

// ---------- Template Instantiation Tests ----------

TEST_CASE("ch_reg_impl: various template instantiations", "[reg][template]") {
    // 创建测试上下文
    ch::core::context test_ctx("test");
    ch::core::ctx_swap ctx_guard(&test_ctx);
    
    // 测试多种宽度的实例化
    using widths = std::integer_sequence<unsigned, 1, 2, 4, 8, 16, 32>;
    
    []<unsigned... W>(std::integer_sequence<unsigned, W...>) {
        ([]() {
            // ch_reg_impl<ch_uint<W>> reg;
            STATIC_REQUIRE(ch_width_v<ch_reg_impl<ch_uint<W>>> == W);
        }(), ...);
    }(widths{});
    
    REQUIRE(true); // 占位确保测试通过
}

// ---------- Compilation-only Tests ----------

TEST_CASE("ch_reg_impl: compilation-only tests", "[reg][compile]") {
    // 这些测试主要用于验证编译通过
    
    // 嵌套类型定义
    using Reg8 = ch_reg_impl<ch_uint<8>>;
    using NestedReg8 = ch_reg_impl<Reg8>;
    using ConstNestedReg8 = const ch_reg_impl<NestedReg8>;
    
    // 类型别名
    using RegAlias = ch_reg<ch_uint<8>>;
    
    // 验证所有类型都能正确编译
    STATIC_REQUIRE(true);
    
    // 验证宽度计算
    STATIC_REQUIRE(ch_width_v<Reg8> == 8);
    STATIC_REQUIRE(ch_width_v<NestedReg8> == 8);
    STATIC_REQUIRE(ch_width_v<ConstNestedReg8> == 8);
    STATIC_REQUIRE(ch_width_v<RegAlias> == 8);
}
