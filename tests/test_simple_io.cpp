// tests/test_simple_io.cpp
#define CATCH_CONFIG_MAIN
#include "catch_amalgamated.hpp"
#include "core/bool.h"
#include "core/context.h"
#include "core/io.h"
#include "core/literal.h"
#include "core/uint.h"
#include <memory>

using namespace ch;
using namespace ch::core;

TEST_CASE("SimpleIO - BasicPortCreation", "[io][basic]") {
    auto ctx = std::make_unique<ch::core::context>("test_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());

    ch_out<ch_uint<8>> output_port("test_output");
    ch_in<ch_uint<8>> input_port("test_input");

    REQUIRE(output_port.impl() != nullptr);
    REQUIRE(input_port.impl() != nullptr);
    REQUIRE(output_port.name() == "test_output");
    REQUIRE(input_port.name() == "test_input");
}

TEST_CASE("SimpleIO - BooleanPorts", "[io][bool]") {
    auto ctx = std::make_unique<ch::core::context>("test_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());

    ch_out<ch_bool> bool_out("bool_out");
    ch_in<ch_bool> bool_in("bool_in");

    REQUIRE(bool_out.impl() != nullptr);
    REQUIRE(bool_in.impl() != nullptr);
}

TEST_CASE("SimpleIO - PortAssignment", "[io][assignment]") {
    auto ctx = std::make_unique<ch::core::context>("test_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());

    ch_out<ch_uint<8>> src("src");
    ch_in<ch_uint<8>> dst("dst");

    ch_uint<8> value(123_d);
    // 简单测试 - 不实际连接，只是验证创建
    REQUIRE(src.impl() != nullptr);
    REQUIRE(dst.impl() != nullptr);
}

TEST_CASE("SimpleIO - ContextIsolation", "[io][context]") {
    auto ctx1 = std::make_unique<ch::core::context>("ctx1");
    auto ctx2 = std::make_unique<ch::core::context>("ctx2");

    {
        ch::core::ctx_swap ctx_guard(ctx1.get());
        ch_out<ch_uint<8>> port1("port1");
        REQUIRE(port1.impl() != nullptr);
    }

    {
        ch::core::ctx_swap ctx_guard(ctx2.get());
        ch_out<ch_uint<8>> port2("port2");
        REQUIRE(port2.impl() != nullptr);
    }
}
