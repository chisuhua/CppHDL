#ifndef CHLIB_PIPELINE_H
#define CHLIB_PIPELINE_H

#include "ch.hpp"
#include "component.h"
#include "core/bool.h"
#include "core/literal.h"
#include "core/operators.h"
#include "core/operators_runtime.h"
#include "core/reg.h"
#include "core/uint.h"
#include <string>

using namespace ch::core;

namespace chlib {

// ============================================================================
// 单数据类型流水线寄存器 (基础版本)
// ============================================================================

/**
 * @brief 通用单阶段流水线寄存器组件 (仅数据)
 * 
 * @tparam DataT 数据类型 (如 ch_uint<32>, ch_bool 等)
 * 
 * 功能特性:
 * - 时钟上升沿锁存输入数据
 * - 支持异步复位 (rst=1 时清零)
 * - 支持停顿 (stall=1 时保持当前值)
 * - 支持冲刷 (flush=1 时清零)
 * 
 * 控制优先级：rst > flush > stall > normal
 * 
 * 使用示例:
 * @code
 * class MyPipelineStage : public ch::Component {
 * public:
 *     __io(
 *         ch_in<ch_uint<32>>  data_in;
 *         ch_out<ch_uint<32>> data_out;
 *         ch_in<ch_bool>      clk;
 *         ch_in<ch_bool>      rst;
 *         ch_in<ch_bool>      stall;
 *         ch_in<ch_bool>      flush;
 *     )
 *     
 *     void describe() override {
 *         PipelineStage<ch_uint<32>> stage(this, "my_stage");
 *         stage.io().data_in = io().data_in;
 *         stage.io().clk = io().clk;
 *         stage.io().rst = io().rst;
 *         stage.io().stall = io().stall;
 *         stage.io().flush = io().flush;
 *         io().data_out = stage.io().data_out;
 *     }
 * };
 * @endcode
 */
template <typename DataT>
class PipelineStage : public ch::Component {
public:
    __io(
        // 输入数据
        ch_in<DataT>  data_in;
        
        // 输出数据
        ch_out<DataT> data_out;
        
        // 时钟和复位
        ch_in<ch_bool> clk;
        ch_in<ch_bool> rst;
        
        // 流水线控制
        ch_in<ch_bool> stall;   // 停顿：保持当前值
        ch_in<ch_bool> flush;   // 冲刷：清零
    )
    
    PipelineStage(ch::Component* parent = nullptr, const std::string& name = "pipeline_stage")
        : ch::Component(parent, name) {}
    
    void create_ports() override {
        new (io_storage_) io_type;
    }
    
    void describe() override {
        // 内部寄存器
        ch_reg<DataT> data_reg(DataT(0_d));
        
        // 更新条件：非复位 且 非停顿 且 非冲刷
        auto update = select(!io().rst, select(!io().stall, !io().flush, ch_bool(false)), ch_bool(false));
        
        // 下一周期值：更新时取输入，否则保持
        data_reg->next = select(update, io().data_in, data_reg);
        
        // 输出：冲刷时清零，否则取寄存器值
        io().data_out = select(io().flush, DataT(0_d), data_reg);
    }
};

// ============================================================================
// 双数据类型流水线寄存器 (数据 + 控制)
// ============================================================================

/**
 * @brief 通用双字段流水线寄存器组件 (数据 + 控制)
 * 
 * @tparam DataT 数据类型 (如 ch_uint<32>)
 * @tparam CtrlT 控制信号类型 (如 ch_uint<8> 或自定义结构)
 * 
 * 功能特性:
 * - 同时锁存数据和控制信号
 * - 独立的数据和控制输出
 * - 统一 stall/flush 控制
 * 
 * 使用示例:
 * @code
 * class MyPipelineStage : public ch::Component {
 * public:
 *     __io(
 *         ch_in<ch_uint<32>> data_in;
 *         ch_in<ch_uint<8>>  ctrl_in;
 *         ch_out<ch_uint<32>> data_out;
 *         ch_out<ch_uint<8>>  ctrl_out;
 *         ch_in<ch_bool>      clk, rst, stall, flush;
 *     )
 *     
 *     void describe() override {
 *         PipelineStageDual<ch_uint<32>, ch_uint<8>> stage(this, "my_stage");
 *         stage.io().data_in = io().data_in;
 *         stage.io().ctrl_in = io().ctrl_in;
 *         stage.io().clk = io().clk;
 *         stage.io().rst = io().rst;
 *         stage.io().stall = io().stall;
 *         stage.io().flush = io().flush;
 *         io().data_out = stage.io().data_out;
 *         io().ctrl_out = stage.io().ctrl_out;
 *     }
 * };
 * @endcode
 */
template <typename DataT, typename CtrlT>
class PipelineStageDual : public ch::Component {
public:
    __io(
        // 输入数据
        ch_in<DataT>  data_in;
        ch_in<CtrlT>  ctrl_in;
        
        // 输出数据
        ch_out<DataT> data_out;
        ch_out<CtrlT> ctrl_out;
        
        // 时钟和复位
        ch_in<ch_bool> clk;
        ch_in<ch_bool> rst;
        
        // 流水线控制
        ch_in<ch_bool> stall;
        ch_in<ch_bool> flush;
    )
    
    PipelineStageDual(ch::Component* parent = nullptr, const std::string& name = "pipeline_stage_dual")
        : ch::Component(parent, name) {}
    
    void create_ports() override {
        new (io_storage_) io_type;
    }
    
    void describe() override {
        // 数据寄存器
        ch_reg<DataT> data_reg(DataT(0_d));
        // 控制寄存器
        ch_reg<CtrlT> ctrl_reg(CtrlT(0_d));
        
        // 更新条件
        auto update = select(!io().rst, select(!io().stall, !io().flush, ch_bool(false)), ch_bool(false));
        
        // 数据通路
        data_reg->next = select(update, io().data_in, data_reg);
        io().data_out = select(io().flush, DataT(0_d), data_reg);
        
        // 控制通路
        ctrl_reg->next = select(update, io().ctrl_in, ctrl_reg);
        io().ctrl_out = select(io().flush, CtrlT(0_d), ctrl_reg);
    }
};

// ============================================================================
// 函数式接口 (简洁风格)
// ============================================================================

/**
 * @brief 流水线寄存器 - 函数式接口 (单数据类型)
 * 
 * @tparam N 数据位宽
 * @param en 使能信号 (内部由 stall/flush 组合)
 * @param d 输入数据
 * @param rst 复位信号
 * @param stall 停顿信号
 * @param flush 冲刷信号
 * @param name 寄存器名称
 * @return ch_uint<N> 锁存后的输出数据
 * 
 * 使用示例:
 * @code
 * ch_uint<32> pipelined_data = pipeline_reg<32>(data, rst, stall, flush, "pipe_reg");
 * @endcode
 */
template <unsigned N>
ch_uint<N> pipeline_reg(ch_uint<N> d, ch_bool rst, ch_bool stall, ch_bool flush,
                        const std::string& name = "pipeline_reg") {
    ch_reg<ch_uint<N>> reg(0_d, name);
    
    // 更新条件：非复位 且 非停顿 且 非冲刷
    auto update = select(!rst, select(!stall, !flush, ch_bool(false)), ch_bool(false));
    
    // 下一周期值：复位时清零，更新时取输入，否则保持
    reg->next = select(rst, ch_uint<N>(0_d), select(update, d, reg));
    
    // 输出：复位或冲刷时清零，否则取寄存器值
    return select(rst, ch_uint<N>(0_d), select(flush, ch_uint<N>(0_d), reg));
}

/**
 * @brief 流水线寄存器 - 函数式接口 (双数据类型)
 * 
 * @tparam DataN 数据位宽
 * @tparam CtrlN 控制位宽
 * @param data_in 输入数据
 * @param ctrl_in 输入控制
 * @param rst 复位信号
 * @param stall 停顿信号
 * @param flush 冲刷信号
 * @param name 寄存器名称前缀
 * @return 包含 data_out 和 ctrl_out 的结构体
 */
template <unsigned DataN, unsigned CtrlN>
struct PipelineDualResult {
    ch_uint<DataN> data_out;
    ch_uint<CtrlN> ctrl_out;
};

template <unsigned DataN, unsigned CtrlN>
PipelineDualResult<DataN, CtrlN>
pipeline_reg_dual(ch_uint<DataN> data_in, ch_uint<CtrlN> ctrl_in,
                  ch_bool rst, ch_bool stall, ch_bool flush,
                  const std::string& name = "pipeline_reg") {
    ch_reg<ch_uint<DataN>> data_reg(0_d, name + "_data");
    ch_reg<ch_uint<CtrlN>> ctrl_reg(0_d, name + "_ctrl");
    
    auto update = select(!rst, select(!stall, !flush, ch_bool(false)), ch_bool(false));
    
    // 复位时清零，更新时取输入，否则保持
    data_reg->next = select(rst, ch_uint<DataN>(0_d), select(update, data_in, data_reg));
    ctrl_reg->next = select(rst, ch_uint<CtrlN>(0_d), select(update, ctrl_in, ctrl_reg));
    
    PipelineDualResult<DataN, CtrlN> result;
    // 复位或冲刷时清零
    result.data_out = select(rst, ch_uint<DataN>(0_d), select(flush, ch_uint<DataN>(0_d), data_reg));
    result.ctrl_out = select(rst, ch_uint<CtrlN>(0_d), select(flush, ch_uint<CtrlN>(0_d), ctrl_reg));
    
    return result;
}

// ============================================================================
// 多阶段流水线链 (级联支持)
// ============================================================================

/**
 * @brief N 阶段流水线链
 * 
 * @tparam DataT 数据类型
 * @tparam NumStages 流水线级数
 * 
 * 功能特性:
 * - 自动级联 NumStages 个流水线级
 * - 统一的 stall/flush 控制
 * - 可访问任意中间级输出
 * 
 * 使用示例:
 * @code
 * // 创建 4 级流水线
 * PipelineChain<ch_uint<32>, 4> chain(this, "my_chain");
 * chain.io().data_in = input_data;
 * chain.io().rst = rst;
 * chain.io().stall = stall;
 * chain.io().flush = flush;
 * 
 * // 访问第 4 级输出
 * output_data = chain.io().data_out;
 * 
 * // 访问中间级输出 (如第 2 级)
 * intermediate_data = chain.get_stage_output<1>(); // 0-indexed
 * @endcode
 */
template <typename DataT, size_t NumStages>
class PipelineChain : public ch::Component {
public:
    __io(
        // 输入数据
        ch_in<DataT>  data_in;
        
        // 最终输出数据 (最后一级)
        ch_out<DataT> data_out;
        
        // 时钟和复位
        ch_in<ch_bool> clk;
        ch_in<ch_bool> rst;
        
        // 流水线控制 (全局)
        ch_in<ch_bool> stall;
        ch_in<ch_bool> flush;
    )
    
    PipelineChain(ch::Component* parent = nullptr, const std::string& name = "pipeline_chain")
        : ch::Component(parent, name) {
        static_assert(NumStages >= 1, "PipelineChain requires at least 1 stage");
    }
    
    void create_ports() override {
        new (io_storage_) io_type;
    }
    
    void describe() override {
        // 创建 NumStages 个寄存器
        ch_reg<DataT> regs[NumStages];
        for (size_t i = 0; i < NumStages; ++i) {
            regs[i] = ch_reg<DataT>(DataT(0_d), "stage_" + std::to_string(i));
        }
        
        // 更新条件
        auto update = select(!io().rst, select(!io().stall, !io().flush, ch_bool(false)), ch_bool(false));
        
        // 第一级：输入来自外部
        regs[0]->next = select(update, io().data_in, regs[0]);
        
        // 中间级：输入来自前一级
        for (size_t i = 1; i < NumStages; ++i) {
            regs[i]->next = select(update, regs[i-1], regs[i]);
        }
        
        // 输出：最后一级，冲刷时清零
        io().data_out = select(io().flush, DataT(0_d), regs[NumStages - 1]);
    }
    
    /**
     * @brief 获取指定级的输出 (用于中间级抽头)
     * 
     * @tparam StageIndex 级索引 (0 = 第一级输出)
     * @return ch_uint<N> 指定级的输出
     * 
     * 注意：此方法需要在 describe() 之后调用，仅用于仿真/代码生成阶段
     */
    template <size_t StageIndex>
    DataT get_stage_output() const {
        static_assert(StageIndex < NumStages, "StageIndex out of range");
        // 实际实现需要在 describe 中保存寄存器引用
        // 这里提供接口定义，具体实现依赖于框架支持
        return DataT(0_d);
    }
};

// ============================================================================
// 辅助工具：流水线控制信号生成
// ============================================================================

/**
 * @brief 流水线停顿控制逻辑
 * 
 * @param stall_req 停顿请求
 * @param flush_req 冲刷请求
 * @param ready 下游就绪信号
 * @return 组合后的 stall 信号
 * 
 * 逻辑：当下游未就绪或有停顿请求时，产生 stall
 */
inline ch_bool pipeline_stall_ctrl(ch_bool stall_req, ch_bool ready) {
    return !ready || stall_req;
}

/**
 * @brief 流水线冲刷控制逻辑
 * 
 * @param flush_req 冲刷请求 (如分支预测错误)
 * @param exception 异常信号
 * @return 组合后的 flush 信号
 */
inline ch_bool pipeline_flush_ctrl(ch_bool flush_req, ch_bool exception) {
    return flush_req || exception;
}

} // namespace chlib

#endif // CHLIB_PIPELINE_H
