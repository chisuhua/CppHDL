/**
 * @file branch_predictor_v2.h
 * @brief 动态分支预测器 v2 - 2-bit BHT + BTB
 * 
 * 规格:
 * - BHT: 16 条目 × 2-bit 饱和计数器
 * - BTB: 8 条目 (存储分支目标地址)
 * - 预测算法：2-bit 饱和计数器
 * - 目标准确率：>85%
 * 
 * 2-bit 计数器状态:
 * - 00: Strongly Not Taken (0)
 * - 01: Weakly Not Taken (1)
 * - 10: Weakly Taken (2)
 * - 11: Strongly Taken (3)
 * 
 * 更新规则:
 * - 实际 Taken: counter++ (max 3)
 * - 实际 Not Taken: counter-- (min 0)
 * 
 * 作者：DevMate
 * 日期：2026-04-10
 */
#pragma once

#include "ch.hpp"
#include "component.h"

using namespace ch::core;

namespace riscv {

/**
 * @brief 分支预测器配置
 */
struct BranchPredictorConfig {
    static constexpr unsigned BHT_ENTRIES = 16;       // BHT 条目数
    static constexpr unsigned BHT_INDEX_BITS = 4;     // log2(16)
    static constexpr unsigned COUNTER_BITS = 2;       // 2-bit 计数器
    static constexpr unsigned BTB_ENTRIES = 8;        // BTB 条目数
    static constexpr unsigned BTB_INDEX_BITS = 3;     // log2(8)
};

/**
 * @brief 2-bit 饱和计数器动态分支预测器
 */
class BranchPredictorV2 : public ch::Component {
public:
    __io(
        // 输入
        ch_in<ch_uint<32>> pc;                  // 当前 PC
        ch_in<ch_bool> valid;                   // 有效信号
        ch_in<ch_bool> branch_taken_actual;     // 实际分支结果 (用于更新)
        ch_in<ch_bool> update;                  // 更新使能 (EX 级确认分支结果)
        ch_in<ch_uint<32>> branch_target;       // 分支目标地址 (用于 BTB 更新)
        
        // 输出
        ch_out<ch_bool> predict_taken;          // 预测分支采取
        ch_out<ch_bool> predict_not_taken;      // 预测分支不采取
        ch_out<ch_uint<32>> predicted_target;   // 预测的分支目标地址
    )

    BranchPredictorV2(ch::Component* parent = nullptr, 
                      const std::string& name = "branch_predictor_v2")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        // =====================================================================
        // BHT: 16 条目 × 2-bit 饱和计数器
        // =====================================================================
        ch_reg<ch_uint<2>> bht[BranchPredictorConfig::BHT_ENTRIES];
        
        // 初始化：全部设为 Weakly Taken (10 = 2)
        for (unsigned i = 0; i < BranchPredictorConfig::BHT_ENTRIES; i++) {
            bht[i] = ch_reg<ch_uint<2>>(ch_uint<2>(2_d));
        }
        
        // =====================================================================
        // BTB: 8 条目 (Tag + Target)
        // =====================================================================
        ch_reg<ch_uint<20>> bht_tag[BranchPredictorConfig::BTB_ENTRIES];  // PC[23:4] 作为 tag
        ch_reg<ch_uint<32>> btb_target[BranchPredictorConfig::BTB_ENTRIES];
        ch_reg<ch_bool> btb_valid[BranchPredictorConfig::BTB_ENTRIES];
        
        // 初始化 BTB
        for (unsigned i = 0; i < BranchPredictorConfig::BTB_ENTRIES; i++) {
            btb_valid[i] = ch_reg<ch_bool>(ch_bool(false));
        }
        
        // =====================================================================
        // BHT 索引：PC[7:4] (使用地址低位)
        // =====================================================================
        auto bht_index = io().pc(BranchPredictorConfig::BHT_INDEX_BITS + 1, 2);
        
        // 读取预测计数器
        auto counter = bht[bht_index];
        
        // =====================================================================
        // 预测逻辑：counter >= 2 则预测 Taken
        // =====================================================================
        ch_bool predict = (counter >= ch_uint<2>(2_d));
        io().predict_taken <<= predict;
        io().predict_not_taken <<= !predict;
        
        // =====================================================================
        // BTB 查找 (简化：使用 PC 低位索引)
        // =====================================================================
        auto btb_index = io().pc(BranchPredictorConfig::BTB_INDEX_BITS + 2, 3);
        auto btb_tag = io().pc(23_d, 4);
        
        // BTB Hit 检测
        ch_bool btb_hit = btb_valid[btb_index] && (bht_tag[btb_index] == btb_tag);
        
        // 输出预测目标地址
        io().predicted_target <<= select(btb_hit, btb_target[btb_index], 
                                         io().pc + ch_uint<32>(4_d));  // 默认 PC+4
        
        // =====================================================================
        // 更新逻辑 (EX 级确认后)
        // =====================================================================
        if (io().update == ch_bool(true)) {
            // 更新 BHT 计数器
            auto old_counter = bht[bht_index];
            
            // 实际 Taken: counter++; Not Taken: counter--
            auto new_counter = select(io().branch_taken_actual == ch_bool(true),
                                      select(old_counter < ch_uint<2>(3_d), 
                                             old_counter + ch_uint<2>(1_d),
                                             old_counter),
                                      select(old_counter > ch_uint<2>(0_d),
                                             old_counter - ch_uint<2>(1_d),
                                             old_counter));
            
            bht[bht_index] = ch_reg<ch_uint<2>>(new_counter);
            
            // 更新 BTB (分支实际采取时)
            if (io().branch_taken_actual == ch_bool(true)) {
                bht_tag[btb_index] = ch_reg<ch_uint<20>>(btb_tag);
                btb_target[btb_index] = ch_reg<ch_uint<32>>(io().branch_target);
                btb_valid[btb_index] = ch_reg<ch_bool>(ch_bool(true));
            }
        }
    }
};

} // namespace riscv
