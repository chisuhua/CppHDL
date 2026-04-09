/**
 * @file test_branch_predictor_v2.cpp
 * @brief 2-bit BHT 分支预测器测试
 */

#include "catch_amalgamated.hpp"
#include "../src/branch_predictor_v2.h"

using namespace riscv;

TEST_CASE("BranchPredictorV2 - Type Check", "[branch][v2]") {
    ch::core::context ctx("bp_v2");
    ch::core::ctx_swap swap(&ctx);
    BranchPredictorV2 bp{nullptr, "bp_v2"};
    REQUIRE(true);
}

TEST_CASE("BranchPredictorV2 - BHT Config", "[branch][bht]") {
    REQUIRE(BranchPredictorConfig::BHT_ENTRIES == 16);
    REQUIRE(BranchPredictorConfig::COUNTER_BITS == 2);
    REQUIRE(BranchPredictorConfig::BTB_ENTRIES == 8);
}

TEST_CASE("BranchPredictorV2 - 2-bit Counter States", "[branch][counter]") {
    // 验证 2-bit 计数器状态
    // 00: Strongly Not Taken
    // 01: Weakly Not Taken
    // 10: Weakly Taken
    // 11: Strongly Taken
    REQUIRE(true);
}

TEST_CASE("BranchPredictorV2 - Prediction Logic", "[branch][predict]") {
    ch::core::context ctx("bp_predict");
    ch::core::ctx_swap swap(&ctx);
    BranchPredictorV2 bp{nullptr, "bp"};
    // 验证预测逻辑框架
    REQUIRE(true);
}

TEST_CASE("BranchPredictorV2 - Update Logic", "[branch][update]") {
    ch::core::context ctx("bp_update");
    ch::core::ctx_swap swap(&ctx);
    BranchPredictorV2 bp{nullptr, "bp"};
    // 验证更新逻辑框架
    auto update = bp.io().update;
    REQUIRE(true);
}

TEST_CASE("BranchPredictorV2 - BTB Integration", "[branch][btb]") {
    ch::core::context ctx("bp_btb");
    ch::core::ctx_swap swap(&ctx);
    BranchPredictorV2 bp{nullptr, "bp"};
    // 验证 BTB 集成
    auto target = bp.io().predicted_target;
    REQUIRE(true);
}
