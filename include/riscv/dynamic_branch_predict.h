/**
 * @file dynamic_branch_predict.h
 * @brief 动态分支预测器 - 2-bit 饱和计数器 (简化编译验证版)
 * 
 * 规格:
 * - BHT 条目：16
 * - 预测算法：2-bit 饱和计数器
 * - 目标准确率：>85%
 */
#pragma once

#include "ch.hpp"
#include "component.h"

using namespace ch::core;

namespace riscv {

/**
 * @brief 分支预测器配置
 */
struct BranchPredictConfig {
    static constexpr unsigned BHT_ENTRIES = 16;
    static constexpr unsigned BHT_INDEX_BITS = 4;
    static constexpr unsigned COUNTER_BITS = 2;
};

/**
 * @brief 2-bit 饱和计数器动态分支预测器 (简化编译验证版)
 * 
 * 注意：当前为简化实现，仅验证编译通过
 */
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
    
    void create_ports() override {
        new (io_storage_) io_type;
    }
    
    void describe() override {
        // 简化实现：固定预测 Not Taken
        io().predict_taken = ch_bool(false);
        io().predict_not_taken = ch_bool(true);
        
        // 更新逻辑暂不实现
        (void)io().update;
        (void)io().branch_taken_actual;
    }
};

} // namespace riscv
