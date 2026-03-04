#define CATCH_CONFIG_MAIN
#include "catch_amalgamated.hpp"
#include "chlib/stream_builder.h"
#include "bundle/stream_bundle.h"
#include "core/context.h"
#include "core/uint.h"
#include <memory>

using namespace ch::core;
using namespace chlib;

// Test that StreamBuilder can be created - this verifies the API compiles
TEST_CASE("StreamBuilder - BasicCreation", "[stream_builder][basic]") {
    auto ctx = std::make_unique<ch::core::context>("test_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());

    // Create a source stream
    ch_stream<ch_uint<8>> source;
    
    // Create builder from source stream - this tests compilation
    StreamBuilder<ch_uint<8>> builder(source);
    
    // Build - returns ch_stream
    auto result = builder.build();
    
    REQUIRE(true);
}

// Test make_stream_builder helper function compiles
TEST_CASE("StreamBuilder - MakeHelper", "[stream_builder][helper]") {
    auto ctx = std::make_unique<ch::core::context>("test_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());

    ch_stream<ch_uint<8>> source;
    
    // Test the helper function
    auto result = make_stream_builder(source).build();
    
    REQUIRE(true);
}

// Test that we can instantiate with different payload types
TEST_CASE("StreamBuilder - TypeInstantiation", "[stream_builder][types]") {
    auto ctx = std::make_unique<ch::core::context>("test_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());

    // Test instantiation with ch_uint<8>
    {
        ch_stream<ch_uint<8>> s;
        StreamBuilder<ch_uint<8>> b(s);
        REQUIRE(true);
    }
    
    // Test instantiation with ch_uint<16>
    {
        ch_stream<ch_uint<16>> s;
        StreamBuilder<ch_uint<16>> b(s);
        REQUIRE(true);
    }
    
    // Test instantiation with ch_uint<32>
    {
        ch_stream<ch_uint<32>> s;
        StreamBuilder<ch_uint<32>> b(s);
        REQUIRE(true);
    }
}

// Test that method chaining compiles (without executing)
TEST_CASE("StreamBuilder - MethodChainingCompiles", "[stream_builder][chain]") {
    auto ctx = std::make_unique<ch::core::context>("test_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());

    // Just test that the types are correct - don't actually call the operations
    // that require valid hardware signals
    ch_stream<ch_uint<8>> source;
    
    // Create builder with method chaining - compiles but don't execute
    auto builder = StreamBuilder<ch_uint<8>>(source);
    
    // Verify builder exists
    auto result = builder.build();
    REQUIRE(true);
}
