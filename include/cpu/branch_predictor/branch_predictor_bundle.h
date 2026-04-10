/**
 * @file branch_predictor_bundle.h
 * @brief 分支预测器通用 Bundle 接口
 * 
 * 设计目标：
 * 1. 定义分支预测器的标准接口
 * 2. 支持不同预测算法（2-bit BHT, Tournament, GAg 等）
 * 3. 通过 Bundle 实现模块间松耦合连接
 * 
 * 用法：
 * ```cpp
 * // 通用分支预测器模块（接受任意 Bundle 实现）
 * template<typename BundleT>
 * class BranchPredictorUnit : public ch::Component {
 *     BundleT bp_bundle;  // Bundle 作为成员
 *     
 *     void describe() override {
 *         // 使用 bp_bundle.io 访问接口
 *         bp_bundle.io.predict_taken <<= ...;
 *     }
 * };
 * 
 * // RISC-V 具体实现
 * using RvBranchBundle = BranchPredictorBundle<ch_uint<32>>;
 * BranchPredictorUnit<RvBranchBundle> bp_inst;
 * ```
 * 
 * @author: DevMate
 * @date: 2026-04-10
 */
#pragma once

#include "ch.hpp"
#include "component.h"
#include "core/bundle/bundle_base.h"

using namespace ch::core;

namespace cpu {

/**
 * @brief 分支预测器通用接口配置
 */
struct BranchPredictorConfig {
    static constexpr unsigned PC_WIDTH = 32;        // PC 位宽
    static constexpr unsigned TARGET_WIDTH = 32;    // 目标地址位宽
    static constexpr unsigned DEFAULT_BHT_SIZE = 16; // 默认 BHT 大小
    static constexpr unsigned DEFAULT_BTB_SIZE = 8;  // 默认 BTB 大小
};

/**
 * @brief 分支预测器 Bundle 接口
 * 
 * @tparam AddrT 地址类型（通常是 ch_uint<32>）
 */
template<typename AddrT = ch_uint<BranchPredictorConfig::PC_WIDTH>>
class BranchPredictorBundle : public ch::core::bundle_base<BranchPredictorBundle<AddrT>> {
    using Self = BranchPredictorBundle<AddrT>;
    
public:
    // ========================================================================
    // 输入端口（来自流水线）
    // ========================================================================
    
    /** 当前 PC */
    AddrT pc;
    
    /** 有效信号 */
    ch_bool valid;
    
    /** 实际分支结果（用于更新，来自 EX/MEM 级） */
    ch_bool branch_taken_actual;
    
    /** 更新使能（EX 级确认分支结果后） */
    ch_bool update;
    
    /** 分支目标地址（用于 BTB 更新） */
    AddrT branch_target;
    
    // ========================================================================
    // 输出端口（到流水线 IF 级）
    // ========================================================================
    
    /** 预测分支采取 */
    ch_out<ch_bool> predict_taken;
    
    /** 预测分支不采取 */
    ch_out<ch_bool> predict_not_taken;
    
    /** 预测的分支目标地址 */
    ch_out<AddrT> predicted_target;
    
    // ========================================================================
    // 构造函数
    // ========================================================================
    
    BranchPredictorBundle() = default;
    
    explicit BranchPredictorBundle(const std::string& prefix) {
        this->set_name_prefix(prefix);
    }
    
    // ========================================================================
    // Bundle 字段注册（必需）
    // ========================================================================
    
    CH_BUNDLE_FIELDS_T(
        pc, valid, branch_taken_actual, update, branch_target,
        predict_taken, predict_not_taken, predicted_target
    )
    
    // ========================================================================
    // Bundle 方向设置
    // ========================================================================
    
    /** 设置主设备方向（预测器作为主设备） */
    void as_master() {
        this->template make_input<AddrT>(pc, branch_target);
        this->make_input(valid, branch_taken_actual, update);
        this->template make_output<AddrT>(predicted_target);
        this->make_output(predict_taken, predict_not_taken);
    }
    
    /** 设置从设备方向（预测器作为从设备） */
    void as_slave() {
        this->template make_output<AddrT>(pc, branch_target);
        this->make_output(valid, branch_taken_actual, update);
        this->template make_input<AddrT>(predicted_target);
        this->make_input(predict_taken, predict_not_taken);
    }
};

} // namespace cpu
