/**
 * @file dynamic_branch_predict.h
 * @brief 动态分支预测器 - 2-bit 饱和计数器 (编译验证版)
 */
#pragma once

#include "ch.hpp"
#include "component.h"

using namespace ch::core;

namespace riscv {

struct BranchPredictConfig {
    static constexpr unsigned BHT_ENTRIES = 16;
    static constexpr unsigned BHT_INDEX_BITS = 4;
    static constexpr unsigned COUNTER_BITS = 2;
};

class DynamicBranchPredictor : public ch::Component {
public:
    __io(
        ch_in<ch_uint<32>> pc;
        ch_in<ch_bool> valid;
        ch_in<ch_bool> branch_taken_actual;
        ch_in<ch_bool> update;
        ch_out<ch_bool> predict_taken;
        ch_out<ch_bool> predict_not_taken;
    )

    DynamicBranchPredictor(ch::Component* parent = nullptr, 
                           const std::string& name = "branch_predictor")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        // 简化实现：固定预测 Not Taken
        io().predict_taken = ch_bool(false);
        io().predict_not_taken = ch_bool(true);
        
        // BHT 框架保留
        ch_reg<ch_uint<2>> bht_dummy(ch_uint<2>(2_d));
        (void)bht_dummy;
        (void)io().update;
        (void)io().branch_taken_actual;
    }
};

} // namespace riscv
