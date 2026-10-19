/**
 * @file test_branch_predictor_v2.cpp
 * @brief 2-bit BHT 分支预测器仿真测试
 */

#include "catch_amalgamated.hpp"
#include "../examples/riscv-mini/src/branch_predictor_v2.h"
#include "component.h"
#include "core/context.h"
#include "device.h"
#include "dual_backend_fixture.h"
#include "simulator.h"
#include <memory>

using namespace riscv;
using namespace ch;
using namespace ch::core;

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

// dual-backend-test-coverage (Phase 1): BranchPredictorV2 timing tests
// driven through InterpreterBackend + VerilatorBackend. (The
// Type Check and BHT Config TEST_CASEs above are pure compile-time /
// static and stay interpreter-only per spec R6 #1.)
DUAL_BACKEND_TEST_PAIR(
    BranchPredictorV2,
    "BranchPredictorV2 - Initial Not Taken Prediction",
    "[branch][predict][chlib][dual]",
    [&]() {
        sim.set_input_value(dev.instance().io().pc, 0x100);
        sim.set_input_value(dev.instance().io().valid, 1);
        sim.set_input_value(dev.instance().io().branch_taken_actual, 0);
        sim.set_input_value(dev.instance().io().update, 0);
        sim.set_input_value(dev.instance().io().branch_target, 0x200);

        sim.tick();
        sim.tick();

        auto pt = static_cast<uint64_t>(
            sim.get_port_value(dev.instance().io().predict_taken));
        auto pnt = static_cast<uint64_t>(
            sim.get_port_value(dev.instance().io().predict_not_taken));
        REQUIRE(pt == 0);
        REQUIRE(pnt == 1);
    });

DUAL_BACKEND_TEST_PAIR(
    BranchPredictorV2,
    "BranchPredictorV2 - Update Changes Prediction",
    "[branch][update][chlib][dual]",
    [&]() {
        sim.set_input_value(dev.instance().io().pc, 0x100);
        sim.set_input_value(dev.instance().io().valid, 1);
        sim.set_input_value(dev.instance().io().update, 0);
        sim.set_input_value(dev.instance().io().branch_taken_actual, 0);
        sim.set_input_value(dev.instance().io().branch_target, 0x200);
        sim.tick();
        auto pt0 = static_cast<uint64_t>(
            sim.get_port_value(dev.instance().io().predict_taken));
        REQUIRE(pt0 == 0);

        for (int i = 0; i < 3; i++) {
            sim.set_input_value(dev.instance().io().pc, 0x100);
            sim.set_input_value(dev.instance().io().valid, 1);
            sim.set_input_value(dev.instance().io().update, 1);
            sim.set_input_value(dev.instance().io().branch_taken_actual, 1);
            sim.set_input_value(dev.instance().io().branch_target, 0x200);
            sim.tick();
        }

        sim.set_input_value(dev.instance().io().pc, 0x100);
        sim.set_input_value(dev.instance().io().branch_taken_actual, 0);
        sim.set_input_value(dev.instance().io().update, 0);
        sim.tick();
        sim.tick();
        auto pt3 = static_cast<uint64_t>(
            sim.get_port_value(dev.instance().io().predict_taken));
        REQUIRE(pt3 == 1);
    });

DUAL_BACKEND_TEST_PAIR(
    BranchPredictorV2,
    "BranchPredictorV2 - BTB Hit and Miss",
    "[branch][btb][chlib][dual]",
    [&]() {
        sim.set_input_value(dev.instance().io().pc, 0x100);
        sim.set_input_value(dev.instance().io().valid, 1);
        sim.set_input_value(dev.instance().io().update, 1);
        sim.set_input_value(dev.instance().io().branch_taken_actual, 1);
        sim.set_input_value(dev.instance().io().branch_target, 0xABCD);
        sim.tick();

        sim.set_input_value(dev.instance().io().pc, 0x100);
        sim.set_input_value(dev.instance().io().update, 0);
        sim.set_input_value(dev.instance().io().branch_taken_actual, 0);
        sim.tick();
        sim.tick();
        auto target_hit = static_cast<uint64_t>(
            sim.get_port_value(dev.instance().io().predicted_target));

        sim.set_input_value(dev.instance().io().pc, 0x200);
        sim.set_input_value(dev.instance().io().update, 0);
        sim.set_input_value(dev.instance().io().branch_taken_actual, 0);
        sim.tick();
        sim.tick();
        auto target_miss = static_cast<uint64_t>(
            sim.get_port_value(dev.instance().io().predicted_target));

        REQUIRE(target_hit == 0xABCD);
        REQUIRE(target_miss == 0x204);
    });
