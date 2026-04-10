/**
 * @file test_cache_simple.cpp
 * @brief Cache 模块编译验证测试
 */

#include "catch_amalgamated.hpp"
#include "cpu/cache/i_cache.h"
#include "cpu/cache/d_cache.h"
#include "../examples/riscv-mini/src/dynamic_branch_predict.h"

using namespace chlib;
using namespace riscv;

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

TEST_CASE("BranchPredictor Type Check", "[branch][predict]") {
    ch::core::context ctx("branch_pred_test");
    ch::core::ctx_swap swap(&ctx);
    DynamicBranchPredictor predictor{nullptr, "predictor"};
    REQUIRE(true);
}
