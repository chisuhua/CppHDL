#include "catch_amalgamated.hpp"
#include "core/reg.h"
#include "core/uint.h"
#include "core/bool.h"
#include "core/literal.h"
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

TEST_CASE("ch_width_impl: ch_bool type", "[reg][width][bool]") {
    STATIC_REQUIRE(ch_width_v<ch_bool> == 1);
    STATIC_REQUIRE(ch_width_impl<const ch_bool>::value == 1);
}

TEST_CASE("ch_width_impl: ch_reg basic types", "[reg][width][basic_reg]") {
    STATIC_REQUIRE(ch_width_v<ch_reg<ch_uint<1>>> == 1);
    STATIC_REQUIRE(ch_width_v<ch_reg<ch_uint<8>>> == 8);
    STATIC_REQUIRE(ch_width_v<ch_reg<ch_uint<16>>> == 16);
    STATIC_REQUIRE(ch_width_v<ch_reg<ch_uint<32>>> == 32);
    STATIC_REQUIRE(ch_width_v<ch_reg<ch_uint<64>>> == 64);
    STATIC_REQUIRE(ch_width_v<ch_reg<ch_bool>> == 1);
}

TEST_CASE("ch_width_impl: ch_reg boolean types", "[reg][width][bool_reg]") {
    STATIC_REQUIRE(ch_width_v<ch_reg<ch_bool>> == 1);
    STATIC_REQUIRE(ch_width_v<const ch_reg<ch_bool>> == 1);
    STATIC_REQUIRE(ch_width_v<volatile ch_reg<ch_bool>> == 1);
}

TEST_CASE("ch_width_impl: nested ch_reg types", "[reg][width][nested]") {
    using Reg8 = ch_reg<ch_uint<8>>;
    using NestedReg8 = ch_reg<Reg8>;
    using TripleNestedReg8 = ch_reg<NestedReg8>;
    using BoolReg = ch_reg<ch_bool>;
    using NestedBoolReg = ch_reg<BoolReg>;
    
    STATIC_REQUIRE(ch_width_v<Reg8> == 8);
    STATIC_REQUIRE(ch_width_v<NestedReg8> == 8);
    STATIC_REQUIRE(ch_width_v<TripleNestedReg8> == 8);
    STATIC_REQUIRE(ch_width_v<BoolReg> == 1);
    STATIC_REQUIRE(ch_width_v<NestedBoolReg> == 1);
}

TEST_CASE("ch_width_impl: const qualified ch_reg", "[reg][width][const]") {
    STATIC_REQUIRE(ch_width_v<const ch_reg<ch_uint<8>>> == 8);
    STATIC_REQUIRE(ch_width_v<volatile ch_reg<ch_uint<16>>> == 16);
    STATIC_REQUIRE(ch_width_v<const volatile ch_reg<ch_uint<32>>> == 32);
    STATIC_REQUIRE(ch_width_v<const ch_reg<ch_bool>> == 1);
}

// ---------- ch_reg Construction Tests ----------

TEST_CASE("ch_reg: default construction", "[reg][construction][default]") {
    ch::core::context test_ctx("test");
    ch::core::ctx_swap ctx_guard(&test_ctx);
    
    ch_reg<ch_uint<8>> reg8;
    REQUIRE(reg8.impl() != nullptr);
    
    ch_reg<ch_uint<16>> reg16;
    REQUIRE(reg16.impl() != nullptr);
    
    ch_reg<ch_uint<32>> reg32;
    REQUIRE(reg32.impl() != nullptr);
    
    ch_reg<ch_bool> bool_reg;
    REQUIRE(bool_reg.impl() != nullptr);
}

TEST_CASE("ch_reg: construction with literal initial value", "[reg][construction][literal]") {
    ch::core::context test_ctx("test");
    ch::core::ctx_swap ctx_guard(&test_ctx);
    
    ch_reg<ch_uint<8>> reg8(42_d);
    REQUIRE(reg8.impl() != nullptr);
    
    ch_reg<ch_uint<16>> reg16(1000_d);
    REQUIRE(reg16.impl() != nullptr);
    
    // 布尔寄存器：使用 1_b / 0_b（或 true/false，如果你保留了 bool 构造）
    ch_reg<ch_bool> bool_reg_true(1_b);
    ch_reg<ch_bool> bool_reg_false(0_b);
    REQUIRE(bool_reg_true.impl() != nullptr);
    REQUIRE(bool_reg_false.impl() != nullptr);
}

TEST_CASE("ch_reg: construction with ch_bool initial value", "[reg][construction][ch_bool]") {
    ch::core::context test_ctx("test");
    ch::core::ctx_swap ctx_guard(&test_ctx);
    
    ch_bool init_true(1_b);
    ch_bool init_false(0_b);
    
    ch_reg<ch_bool> bool_reg1(init_true);
    ch_reg<ch_bool> bool_reg2(init_false);
    
    REQUIRE(bool_reg1.impl() != nullptr);
    REQUIRE(bool_reg2.impl() != nullptr);
}

// ---------- ch_reg Type Traits Tests ----------

TEST_CASE("ch_reg: type traits verification", "[reg][traits]") {
    using Reg8 = ch_reg<ch_uint<8>>;
    using Reg16 = ch_reg<ch_uint<16>>;
    using BoolReg = ch_reg<ch_bool>;
    
    STATIC_REQUIRE(std::is_class_v<Reg8>);
    STATIC_REQUIRE(std::is_class_v<Reg16>);
    STATIC_REQUIRE(std::is_class_v<BoolReg>);
    
    STATIC_REQUIRE(std::is_base_of_v<ch_uint<8>, Reg8>);
    STATIC_REQUIRE(std::is_base_of_v<logic_buffer<ch_uint<8>>, Reg8>);
    STATIC_REQUIRE(std::is_base_of_v<ch_bool, BoolReg>);
    STATIC_REQUIRE(std::is_base_of_v<logic_buffer<ch_bool>, BoolReg>);
}

TEST_CASE("ch_reg: nested type traits with bool", "[reg][traits][nested][bool]") {
    using BoolReg = ch_reg<ch_bool>;
    using NestedBoolReg = ch_reg<BoolReg>;
    
    STATIC_REQUIRE(std::is_class_v<NestedBoolReg>);
    STATIC_REQUIRE(std::is_base_of_v<BoolReg, NestedBoolReg>);
    STATIC_REQUIRE(std::is_base_of_v<ch_bool, NestedBoolReg>);
}

// ---------- ch_reg Width Consistency Tests ----------

TEST_CASE("ch_reg: width consistency across constructions", "[reg][width][consistency]") {
    ch::core::context test_ctx("test");
    ch::core::ctx_swap ctx_guard(&test_ctx);
    
    constexpr unsigned expected_width = 8;
    constexpr unsigned bool_width = 1;
    
    SECTION("Default construction") {
        ch_reg<ch_uint<expected_width>> reg;
        ch_reg<ch_bool> bool_reg;
        STATIC_REQUIRE(ch_width_v<decltype(reg)> == expected_width);
        STATIC_REQUIRE(ch_width_v<decltype(bool_reg)> == bool_width);
    }
    
    SECTION("Literal construction") {
        ch_reg<ch_uint<expected_width>> reg(42_d);
        ch_reg<ch_bool> bool_reg(1_b);
        STATIC_REQUIRE(ch_width_v<decltype(reg)> == expected_width);
        STATIC_REQUIRE(ch_width_v<decltype(bool_reg)> == bool_width);
    }
    
    SECTION("Nested construction") {
        using RegType = ch_reg<ch_uint<expected_width>>;
        using NestedRegType = ch_reg<RegType>;
        using BoolRegType = ch_reg<ch_bool>;
        using NestedBoolRegType = ch_reg<BoolRegType>;
        
        STATIC_REQUIRE(ch_width_v<NestedRegType> == expected_width);
        STATIC_REQUIRE(ch_width_v<NestedBoolRegType> == bool_width);
    }
}

// ---------- ch_reg Alias Tests ----------

TEST_CASE("ch_reg: type identity verification with bool", "[reg][alias][bool]") {
    STATIC_REQUIRE(std::is_same_v<ch_reg<ch_uint<8>>, ch_reg<ch_uint<8>>>);
    STATIC_REQUIRE(std::is_same_v<ch_reg<ch_bool>, ch_reg<ch_bool>>);
    
    using Reg8 = ch_reg<ch_uint<8>>;
    using NestedReg8 = ch_reg<Reg8>;
    using BoolReg = ch_reg<ch_bool>;
    using NestedBoolReg = ch_reg<BoolReg>;
    
    STATIC_REQUIRE(std::is_same_v<NestedReg8, ch_reg<Reg8>>);
    STATIC_REQUIRE(std::is_same_v<NestedBoolReg, ch_reg<BoolReg>>);
}

// ---------- Edge Case Tests ----------

TEST_CASE("ch_reg: maximum width type with bool", "[reg][edge][max][bool]") {
    ch::core::context test_ctx("test");
    ch::core::ctx_swap ctx_guard(&test_ctx);
    
    ch_reg<ch_uint<32>> reg32;
    ch_reg<ch_bool> bool_reg;
    
    REQUIRE(reg32.impl() != nullptr);
    REQUIRE(bool_reg.impl() != nullptr);
}

// ---------- Template Instantiation Tests ----------

TEST_CASE("ch_reg: various template instantiations including bool", "[reg][template]") {
    ch::core::context test_ctx("test");
    ch::core::ctx_swap ctx_guard(&test_ctx);
    
    ch_reg<ch_uint<1>> reg1(1_b);
    ch_reg<ch_uint<2>> reg2(3_d);
    ch_reg<ch_uint<4>> reg4(15_d);
    ch_reg<ch_uint<8>> reg8(255_d);
    ch_reg<ch_uint<16>> reg16(65535_d);
    ch_reg<ch_uint<32>> reg32(0xFFFFFFFF_h);
    ch_reg<ch_bool> bool_reg(1_b);
    
    REQUIRE(reg1.impl() != nullptr);
    REQUIRE(reg2.impl() != nullptr);
    REQUIRE(reg4.impl() != nullptr);
    REQUIRE(reg8.impl() != nullptr);
    REQUIRE(reg16.impl() != nullptr);
    REQUIRE(reg32.impl() != nullptr);
    REQUIRE(bool_reg.impl() != nullptr);
    
    STATIC_REQUIRE(ch_width_v<decltype(reg1)> == 1);
    STATIC_REQUIRE(ch_width_v<decltype(reg2)> == 2);
    STATIC_REQUIRE(ch_width_v<decltype(reg4)> == 4);
    STATIC_REQUIRE(ch_width_v<decltype(reg8)> == 8);
    STATIC_REQUIRE(ch_width_v<decltype(reg16)> == 16);
    STATIC_REQUIRE(ch_width_v<decltype(reg32)> == 32);
    STATIC_REQUIRE(ch_width_v<decltype(bool_reg)> == 1);
}

// ---------- Compilation-only Tests ----------

TEST_CASE("ch_reg: compilation-only tests with bool", "[reg][compile][bool]") {
    using Reg8 = ch_reg<ch_uint<8>>;
    using NestedReg8 = ch_reg<Reg8>;
    using ConstNestedReg8 = const ch_reg<NestedReg8>;
    using BoolReg = ch_reg<ch_bool>;
    using NestedBoolReg = ch_reg<BoolReg>;
    
    using RegAlias = ch_reg<ch_uint<8>>;
    using BoolRegAlias = ch_reg<ch_bool>;
    
    STATIC_REQUIRE(true);
    STATIC_REQUIRE(ch_width_v<Reg8> == 8);
    STATIC_REQUIRE(ch_width_v<NestedReg8> == 8);
    STATIC_REQUIRE(ch_width_v<ConstNestedReg8> == 8);
    STATIC_REQUIRE(ch_width_v<RegAlias> == 8);
    STATIC_REQUIRE(ch_width_v<BoolReg> == 1);
    STATIC_REQUIRE(ch_width_v<NestedBoolReg> == 1);
    STATIC_REQUIRE(ch_width_v<BoolRegAlias> == 1);
}

// ---------- ch_reg next Assignment Tests ----------

TEST_CASE("ch_reg: next assignment functionality with bool", "[reg][next][bool]") {
    ch::core::context test_ctx("test");
    ch::core::ctx_swap ctx_guard(&test_ctx);
    
    ch_reg<ch_uint<8>> reg(0_d);
    ch_uint<8> src_value(42_d);
    reg->next = src_value;
    REQUIRE(reg.impl() != nullptr);
    
    ch_reg<ch_bool> bool_reg(0_b);
    ch_bool bool_src(1_b);
    bool_reg->next = bool_src;
    REQUIRE(bool_reg.impl() != nullptr);
}

TEST_CASE("ch_reg: next assignment with boolean expressions", "[reg][next][bool_expr]") {
    ch::core::context test_ctx("test");
    ch::core::ctx_swap ctx_guard(&test_ctx);
    
    ch_reg<ch_bool> bool_reg1(0_b);
    ch_reg<ch_bool> bool_reg2(1_b);
    ch_reg<ch_uint<8>> uint_reg(0_d);
    
    bool_reg1->next = bool_reg2 && ch_bool(1_b);
    bool_reg2->next = !(uint_reg == ch_uint<8>(0_d));
    
    REQUIRE(bool_reg1.impl() != nullptr);
    REQUIRE(bool_reg2.impl() != nullptr);
}

// ---------- ch_reg as_ln() Tests ----------

TEST_CASE("ch_reg: as_ln() conversion with bool", "[reg][conversion][bool]") {
    ch::core::context test_ctx("test");
    ch::core::ctx_swap ctx_guard(&test_ctx);
    
    ch_reg<ch_uint<8>> reg(42_d);
    ch_reg<ch_bool> bool_reg(1_b);
    
    lnode<ch_uint<8>> ln = reg;
    lnode<ch_bool> bool_ln = bool_reg;
    REQUIRE(ln.impl() != nullptr);
    REQUIRE(bool_ln.impl() != nullptr);
    
    lnode<ch_uint<8>> ln2 = reg.as_ln();
    lnode<ch_bool> bool_ln2 = bool_reg.as_ln();
    REQUIRE(ln2.impl() != nullptr);
    REQUIRE(bool_ln2.impl() != nullptr);
    
    REQUIRE(ln.impl() == ln2.impl());
    REQUIRE(bool_ln.impl() == bool_ln2.impl());
}

TEST_CASE("ch_reg: construction with hardware literals including bool", "[reg][construction][literals][bool]") {
    ch::core::context test_ctx("test");
    ch::core::ctx_swap ctx_guard(&test_ctx);
    
    SECTION("Binary literal construction") {
        ch_reg<ch_uint<8>> reg(1111'1111_b);  // 255
        REQUIRE(reg.impl() != nullptr);
    }
    
    SECTION("Hexadecimal literal construction") {
        ch_reg<ch_uint<16>> reg(0xDEAD_h);    // 57005
        REQUIRE(reg.impl() != nullptr);
    }
    
    SECTION("Boolean literal construction") {
        ch_reg<ch_bool> bool_reg1(1_b);       // true
        ch_reg<ch_bool> bool_reg2(0_b);       // false
        REQUIRE(bool_reg1.impl() != nullptr);
        REQUIRE(bool_reg2.impl() != nullptr);
    }
    
    SECTION("Mixed literal construction") {
        ch_reg<ch_uint<32>> reg(0xDEAD'BEEF_h); // 3735928559
        REQUIRE(reg.impl() != nullptr);
    }
    
    SECTION("Literal overflow warning") {
        ch_reg<ch_uint<4>> reg(0xFF_h);  // 255 > 15, 应该触发警告
        REQUIRE(reg.impl() != nullptr);
    }
}

TEST_CASE("ch_reg: next assignment with hardware literals including bool", "[reg][next][literals][bool]") {
    ch::core::context test_ctx("test");
    ch::core::ctx_swap ctx_guard(&test_ctx);
    
    SECTION("Next assignment with binary literals") {
        ch_reg<ch_uint<8>> reg(0_d);
        reg->next = 1111'1111_b;  // 255
        REQUIRE(reg.impl() != nullptr);
    }
    
    SECTION("Next assignment with hex literals") {
        ch_reg<ch_uint<16>> reg(0_d);
        reg->next = 0xDEAD_h;     // 57005
        REQUIRE(reg.impl() != nullptr);
    }
    
    SECTION("Next assignment with boolean literals") {
        ch_reg<ch_bool> bool_reg(0_b);
        bool_reg->next = 1_b;     // true
        REQUIRE(bool_reg.impl() != nullptr);
    }
    
    SECTION("Complex next expressions with literals") {
        ch_reg<ch_uint<8>> reg1(0_d);
        ch_reg<ch_uint<8>> reg2(10_d);
        ch_reg<ch_bool> bool_reg(0_b);
        
        reg1->next = reg2 + 05_h;  // 10 + 5 = 15
        bool_reg->next = (reg1 > 0x0A_h); // 15 > 10 = true
        REQUIRE(reg1.impl() != nullptr);
        REQUIRE(bool_reg.impl() != nullptr);
    }
}

TEST_CASE("ch_reg: boolean register operations", "[reg][bool][operations]") {
    ch::core::context test_ctx("test");
    ch::core::ctx_swap ctx_guard(&test_ctx);
    
    SECTION("Boolean register logical operations") {
        ch_reg<ch_bool> bool_reg1(1_b);
        ch_reg<ch_bool> bool_reg2(0_b);
        ch_reg<ch_bool> result_reg(0_b);
        
        result_reg->next = bool_reg1 && bool_reg2;  // true && false = false
        REQUIRE(result_reg.impl() != nullptr);
        
        result_reg->next = bool_reg1 || bool_reg2;  // true || false = true
        REQUIRE(result_reg.impl() != nullptr);
        
        result_reg->next = !bool_reg1;              // !true = false
        REQUIRE(result_reg.impl() != nullptr);
    }
    
    SECTION("Boolean register comparison operations") {
        ch_reg<ch_uint<8>> uint_reg1(5_d);
        ch_reg<ch_uint<8>> uint_reg2(10_d);
        ch_reg<ch_bool> comp_result(0_b);
        
        comp_result->next = (uint_reg1 == uint_reg2);  // false
        comp_result->next = (uint_reg1 < uint_reg2);   // true
        comp_result->next = (uint_reg1 != uint_reg2);  // true
        
        REQUIRE(comp_result.impl() != nullptr);
    }
    
    SECTION("Mixed boolean and integer register operations") {
        ch_reg<ch_bool> bool_reg(1_b);
        ch_reg<ch_uint<8>> uint_reg(5_d);
        ch_reg<ch_bool> result_reg(0_b);
        
        // 使用字面量确保无歧义
        result_reg->next = bool_reg && (uint_reg > ch_uint<8>(0_d));  // true && true = true
        result_reg->next = (uint_reg == ch_uint<8>(5_d)) || !bool_reg; // true || false = true
        
        REQUIRE(result_reg.impl() != nullptr);
    }
}
