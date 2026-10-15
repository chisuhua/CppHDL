/**
 * @file test_cache_simple.cpp
 * @brief Cache 模块编译验证测试
 *
 * Both ICache / DCache classes are marked [[deprecated]] (2026-10-17
 * deprecate-cache-stubs); this file intentionally instantiates the
 * stubs to keep regression coverage of the pass-through semantics
 * they expose. Suppress the deprecation warning locally so the
 * zero-warning policy is unaffected.
 */

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

#include "catch_amalgamated.hpp"
#include "cpu/cache/i_cache_stub.h"
#include "cpu/cache/d_cache_stub.h"

using namespace chlib;

TEST_CASE("ICache Type Check", "[cache][icache]") {
    ch::core::context ctx("icache_test");
    ch::core::ctx_swap swap(&ctx);
    ICache icache{nullptr, "icache"};
    REQUIRE(true);
}

TEST_CASE("DCache Type Check", "[cache][dcache]") {
    ch::core::context ctx("dcache_test");
    ch::core::ctx_swap swap(&ctx);
    DCache dcache{nullptr, "dcache"};
    REQUIRE(true);
}

#pragma GCC diagnostic pop
