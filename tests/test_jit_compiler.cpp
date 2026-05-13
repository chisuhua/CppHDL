#include "catch_amalgamated.hpp"
#include "ch.hpp"
#include "core/context.h"
#include "core/reg.h"
#include "core/uint.h"
#include "core/bool.h"
#include "simulator.h"

#if __has_include("jit/jit_compiler.h")
#include "jit/jit_compiler.h"
#endif

using namespace ch;
using namespace ch::core;
using namespace ch::core::literals;

TEST_CASE("JIT Compiler availability", "[jit][basic]") {
#if __has_include("jit/jit_compiler.h")
    jit::JitCompiler compiler;
    REQUIRE(compiler.is_available() == true);
#else
    SUCCEED("JIT not enabled - skipping");
#endif
}

TEST_CASE("JIT Compiler simple context", "[jit][basic]") {
#if __has_include("jit/jit_compiler.h")
    context ctx("jit_test_ctx");
    ctx_swap swap(&ctx);

    auto reg = ch_reg<ch_uint<8>>(0_b, "test_reg");
    reg <<= ch_uint<8>(42);

    Simulator sim(&ctx);
    REQUIRE(sim.is_jit_compiled() == true);

    sim.tick();

    REQUIRE(static_cast<uint64_t>(sim.get_value(reg)) == 42);
#else
    SUCCEED("JIT not enabled - skipping");
#endif
}

TEST_CASE("JIT Compiler addition", "[jit][arithmetic]") {
#if __has_include("jit/jit_compiler.h")
    context ctx("jit_add_ctx");
    ctx_swap swap(&ctx);

    ch_uint<8> a(10_d);
    ch_uint<8> b(5_d);
    auto result_reg = ch_reg<ch_uint<8>>(0_b, "result");

    result_reg <<= a + b;

    Simulator sim(&ctx);
    REQUIRE(sim.is_jit_compiled() == true);

    sim.tick();
    REQUIRE(static_cast<uint64_t>(sim.get_value(result_reg)) == 15);
#else
    SUCCEED("JIT not enabled - skipping");
#endif
}

TEST_CASE("JIT Compiler subtraction", "[jit][arithmetic]") {
#if __has_include("jit/jit_compiler.h")
    context ctx("jit_sub_ctx");
    ctx_swap swap(&ctx);

    ch_uint<8> a(20_d);
    ch_uint<8> b(8_d);
    auto result_reg = ch_reg<ch_uint<8>>(0_b, "result");

    result_reg <<= a - b;

    Simulator sim(&ctx);
    REQUIRE(sim.is_jit_compiled() == true);

    sim.tick();
    REQUIRE(static_cast<uint64_t>(sim.get_value(result_reg)) == 12);
#else
    SUCCEED("JIT not enabled - skipping");
#endif
}

TEST_CASE("JIT Compiler multiplication", "[jit][arithmetic]") {
#if __has_include("jit/jit_compiler.h")
    context ctx("jit_mul_ctx");
    ctx_swap swap(&ctx);

    ch_uint<8> a(7_d);
    ch_uint<8> b(6_d);
    auto result_reg = ch_reg<ch_uint<8>>(0_b, "result");

    result_reg <<= a * b;

    Simulator sim(&ctx);
    REQUIRE(sim.is_jit_compiled() == true);

    sim.tick();
    REQUIRE(static_cast<uint64_t>(sim.get_value(result_reg)) == 42);
#else
    SUCCEED("JIT not enabled - skipping");
#endif
}

TEST_CASE("JIT Compiler bitwise AND", "[jit][bitwise]") {
#if __has_include("jit/jit_compiler.h")
    context ctx("jit_and_ctx");
    ctx_swap swap(&ctx);

    ch_uint<8> a(12_d);
    ch_uint<8> b(10_d);
    auto result_reg = ch_reg<ch_uint<8>>(0_b, "result");

    result_reg <<= a & b;

    Simulator sim(&ctx);
    REQUIRE(sim.is_jit_compiled() == true);

    sim.tick();
    REQUIRE(static_cast<uint64_t>(sim.get_value(result_reg)) == 8);
#else
    SUCCEED("JIT not enabled - skipping");
#endif
}

TEST_CASE("JIT Compiler bitwise OR", "[jit][bitwise]") {
#if __has_include("jit/jit_compiler.h")
    context ctx("jit_or_ctx");
    ctx_swap swap(&ctx);

    ch_uint<8> a(12_d);
    ch_uint<8> b(10_d);
    auto result_reg = ch_reg<ch_uint<8>>(0_b, "result");

    result_reg <<= a | b;

    Simulator sim(&ctx);
    REQUIRE(sim.is_jit_compiled() == true);

    sim.tick();
    REQUIRE(static_cast<uint64_t>(sim.get_value(result_reg)) == 14);
#else
    SUCCEED("JIT not enabled - skipping");
#endif
}

TEST_CASE("JIT Compiler bitwise XOR", "[jit][bitwise]") {
#if __has_include("jit/jit_compiler.h")
    context ctx("jit_xor_ctx");
    ctx_swap swap(&ctx);

    ch_uint<8> a(12_d);
    ch_uint<8> b(10_d);
    auto result_reg = ch_reg<ch_uint<8>>(0_b, "result");

    result_reg <<= a ^ b;

    Simulator sim(&ctx);
    REQUIRE(sim.is_jit_compiled() == true);

    sim.tick();
    REQUIRE(static_cast<uint64_t>(sim.get_value(result_reg)) == 6);
#else
    SUCCEED("JIT not enabled - skipping");
#endif
}

TEST_CASE("JIT Compiler comparison LT", "[jit][comparison]") {
#if __has_include("jit/jit_compiler.h")
    context ctx("jit_lt_ctx");
    ctx_swap swap(&ctx);

    ch_uint<8> a(10_d);
    ch_uint<8> b(20_d);
    auto result_reg = ch_reg<ch_bool>(false, "result");

    result_reg <<= a < b;

    Simulator sim(&ctx);
    REQUIRE(sim.is_jit_compiled() == true);

    sim.tick();
    REQUIRE(static_cast<uint64_t>(sim.get_value(result_reg)) == 1);
#else
    SUCCEED("JIT not enabled - skipping");
#endif
}

TEST_CASE("JIT Compiler comparison GT", "[jit][comparison]") {
#if __has_include("jit/jit_compiler.h")
    context ctx("jit_gt_ctx");
    ctx_swap swap(&ctx);

    ch_uint<8> a(10_d);
    ch_uint<8> b(20_d);
    auto result_reg = ch_reg<ch_bool>(false, "result");

    result_reg <<= a > b;

    Simulator sim(&ctx);
    REQUIRE(sim.is_jit_compiled() == true);

    sim.tick();
    REQUIRE(static_cast<uint64_t>(sim.get_value(result_reg)) == 0);
#else
    SUCCEED("JIT not enabled - skipping");
#endif
}

TEST_CASE("JIT Compiler comparison EQ", "[jit][comparison]") {
#if __has_include("jit/jit_compiler.h")
    context ctx("jit_eq_ctx");
    ctx_swap swap(&ctx);

    ch_uint<8> a(10_d);
    auto result_reg = ch_reg<ch_bool>(false, "result");

    result_reg <<= (a == 10_d);

    Simulator sim(&ctx);
    REQUIRE(sim.is_jit_compiled() == true);

    sim.tick();
    REQUIRE(static_cast<uint64_t>(sim.get_value(result_reg)) == 1);
#else
    SUCCEED("JIT not enabled - skipping");
#endif
}

TEST_CASE("JIT Compiler select true", "[jit][select]") {
#if __has_include("jit/jit_compiler.h")
    context ctx("jit_sel_ctx");
    ctx_swap swap(&ctx);

    ch_uint<8> a(100_d);
    ch_uint<8> b(200_d);
    ch_bool condition(true);
    auto result_reg = ch_reg<ch_uint<8>>(0_b, "result");

    result_reg <<= ch::core::select(condition, a, b);

    Simulator sim(&ctx);
    REQUIRE(sim.is_jit_compiled() == true);

    sim.tick();
    REQUIRE(static_cast<uint64_t>(sim.get_value(result_reg)) == 100);
#else
    SUCCEED("JIT not enabled - skipping");
#endif
}

TEST_CASE("JIT Compiler select false", "[jit][select]") {
#if __has_include("jit/jit_compiler.h")
    context ctx("jit_sel2_ctx");
    ctx_swap swap(&ctx);

    ch_uint<8> a(100_d);
    ch_uint<8> b(200_d);
    ch_bool condition(false);
    auto result_reg = ch_reg<ch_uint<8>>(0_b, "result");

    result_reg <<= ch::core::select(condition, a, b);

    Simulator sim(&ctx);
    REQUIRE(sim.is_jit_compiled() == true);

    sim.tick();
    REQUIRE(static_cast<uint64_t>(sim.get_value(result_reg)) == 200);
#else
    SUCCEED("JIT not enabled - skipping");
#endif
}

TEST_CASE("JIT Compiler chained operations", "[jit][arithmetic]") {
#if __has_include("jit/jit_compiler.h")
    context ctx("jit_chain_ctx");
    ctx_swap swap(&ctx);

    ch_uint<8> a(2_d);
    ch_uint<8> b(3_d);
    ch_uint<8> c(4_d);
    auto result_reg = ch_reg<ch_uint<8>>(0_b, "result");

    result_reg <<= (a + b) * c;

    Simulator sim(&ctx);
    REQUIRE(sim.is_jit_compiled() == true);

    sim.tick();
    REQUIRE(static_cast<uint64_t>(sim.get_value(result_reg)) == 20);
#else
    SUCCEED("JIT not enabled - skipping");
#endif
}

TEST_CASE("JIT Compiler register set and get", "[jit][reg]") {
#if __has_include("jit/jit_compiler.h")
    context ctx("jit_reg_ctx");
    ctx_swap swap(&ctx);

    auto counter = ch_reg<ch_uint<8>>(0_b, "counter");
    counter <<= ch_uint<8>(42);

    Simulator sim(&ctx);
    REQUIRE(sim.is_jit_compiled() == true);

    sim.tick();
    REQUIRE(static_cast<uint64_t>(sim.get_value(counter)) == 42);
#else
    SUCCEED("JIT not enabled - skipping");
#endif
}

TEST_CASE("JIT Compiler sext", "[jit][arithmetic]") {
#if __has_include("jit/jit_compiler.h")
    context ctx("jit_sext_ctx");
    ctx_swap swap(&ctx);

    ch_uint<4> a(5_d);
    auto result_reg = ch_reg<ch_uint<8>>(0_b, "result");

    result_reg <<= sext<8>(a);

    Simulator sim(&ctx);
    REQUIRE(sim.is_jit_compiled() == true);

    sim.tick();
    REQUIRE(static_cast<uint64_t>(sim.get_value(result_reg)) == 5);
#else
    SUCCEED("JIT not enabled - skipping");
#endif
}

TEST_CASE("JIT Compiler zext", "[jit][arithmetic]") {
#if __has_include("jit/jit_compiler.h")
    context ctx("jit_zext_ctx");
    ctx_swap swap(&ctx);

    ch_uint<4> a(5_d);
    auto result_reg = ch_reg<ch_uint<8>>(0_b, "result");

    result_reg <<= zext<8>(a);

    Simulator sim(&ctx);
    REQUIRE(sim.is_jit_compiled() == true);

    sim.tick();
    REQUIRE(static_cast<uint64_t>(sim.get_value(result_reg)) == 5);
#else
    SUCCEED("JIT not enabled - skipping");
#endif
}

TEST_CASE("JIT Compiler neg", "[jit][arithmetic]") {
#if __has_include("jit/jit_compiler.h")
    context ctx("jit_neg_ctx");
    ctx_swap swap(&ctx);

    ch_uint<8> a(5_d);
    auto result_reg = ch_reg<ch_uint<8>>(0_b, "result");

    result_reg <<= -a;

    Simulator sim(&ctx);
    REQUIRE(sim.is_jit_compiled() == true);

    sim.tick();
    REQUIRE(static_cast<uint64_t>(sim.get_value(result_reg)) == 251);
#else
    SUCCEED("JIT not enabled - skipping");
#endif
}