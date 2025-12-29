#ifndef CHLIB_STREAM_H
#define CHLIB_STREAM_H

#include "ch.hpp"
#include "component.h"
#include "core/bool.h"
#include "core/literal.h"
#include "core/operators.h"
#include "core/operators_runtime.h"
#include "core/uint.h"
#include "chlib/logic.h"
#include "chlib/fifo.h"
#include <cassert>
#include <vector>
#include <array>

using namespace ch::core;

namespace chlib {

/**
 * Stream - 带反压的数据流接口
 * 包含 payload, valid, ready 信号
 */
template<typename T>
struct Stream : public bundle_base<Stream<T>> {
    using Self = Stream<T>;
    T payload;        // 数据载荷
    ch_bool valid;    // 有效信号
    ch_bool ready;    // 就绪信号

    Stream() = default;
    explicit Stream(const std::string& prefix) {
        this->set_name_prefix(prefix);
    }

    CH_BUNDLE_FIELDS(Self, payload, valid, ready)

    void as_master() override {
        // Master: 输出数据和有效信号，接收就绪信号
        this->make_output(payload, valid);
        this->make_input(ready);
    }

    void as_slave() override {
        // Slave: 接收数据和有效信号，输出就绪信号
        this->make_input(payload, valid);
        this->make_output(ready);
    }
};

/**
 * Flow - 无反压的数据流接口
 * 包含 payload, valid 信号（无ready信号）
 */
template<typename T>
struct Flow : public bundle_base<Flow<T>> {
    using Self = Flow<T>;
    T payload;        // 数据载荷
    ch_bool valid;    // 有效信号

    Flow() = default;
    explicit Flow(const std::string& prefix) {
        this->set_name_prefix(prefix);
    }

    CH_BUNDLE_FIELDS(Self, payload, valid)

    void as_master() override {
        // Master: 输出数据和有效信号
        this->make_output(payload, valid);
    }

    void as_slave() override {
        // Slave: 接收数据和有效信号
        this->make_input(payload, valid);
    }
};

/**
 * Stream FIFO - 带反压的FIFO
 */
template<typename T, unsigned DEPTH>
struct StreamFifoResult {
    Stream<T> push_stream;   // 推入端口
    Stream<T> pop_stream;    // 弹出端口
    ch_uint<log2_ceil<DEPTH>::value + 1> occupancy;  // 占用计数
    ch_bool full;                  // 满标志
    ch_bool empty;                 // 空标志
};

template<typename T, unsigned DEPTH>
StreamFifoResult<T, DEPTH> stream_fifo(
    ch_bool clk,
    ch_bool rst,
    Stream<T> input_stream
) {
    StreamFifoResult<T, DEPTH> result;
    
    // 创建内部FIFO
    auto fifo_result = sync_fifo<T, DEPTH>(
        clk, 
        rst, 
        input_stream.valid && input_stream.ready,  // 写使能
        input_stream.payload,                      // 写数据
        result.pop_stream.ready && result.pop_stream.valid  // 读使能
    );
    
    // 连接输入到FIFO
    result.push_stream.payload = input_stream.payload;
    result.push_stream.valid = input_stream.valid;
    result.push_stream.ready = !fifo_result.full;  // FIFO未满时就绪
    
    // 连接FIFO到输出
    result.pop_stream.payload = fifo_result.q;
    result.pop_stream.valid = !fifo_result.empty;
    result.pop_stream.ready = true;  // 假设输出端总是就绪
    
    // 占用计数和状态
    result.occupancy = fifo_result.count;
    result.full = fifo_result.full;
    result.empty = fifo_result.empty;
    
    return result;
}

/**
 * Stream Fork - 将输入流复制到多个输出流
 * synchronous=true: 所有输出必须同时就绪才传输
 * synchronous=false: 任意输出就绪即可传输
 */
template<typename T, unsigned N_OUTPUTS>
struct StreamForkResult {
    Stream<T> input_stream;
    std::array<Stream<T>, N_OUTPUTS> output_streams;
};

template<typename T, unsigned N_OUTPUTS>
StreamForkResult<T, N_OUTPUTS> stream_fork(
    Stream<T> input_stream,
    bool synchronous = true
) {
    StreamForkResult<T, N_OUTPUTS> result;
    
    // 复制输入数据到所有输出
    for (unsigned i = 0; i < N_OUTPUTS; i++) {
        result.output_streams[i].payload = input_stream.payload;
        result.output_streams[i].valid = input_stream.valid;
    }
    
    if (synchronous) {
        // 同步模式：所有输出都就绪时输入才就绪
        ch_bool all_ready = true;
        for (unsigned i = 0; i < N_OUTPUTS; i++) {
            all_ready = all_ready && result.output_streams[i].ready;
        }
        result.input_stream.ready = all_ready;
        
        // 输入有效且所有输出就绪时，所有输出都有效
        for (unsigned i = 0; i < N_OUTPUTS; i++) {
            result.output_streams[i].ready = result.input_stream.ready;
        }
    } else {
        // 非同步模式：任意输出就绪时输入就绪
        ch_bool any_ready = false;
        for (unsigned i = 0; i < N_OUTPUTS; i++) {
            any_ready = any_ready || result.output_streams[i].ready;
        }
        result.input_stream.ready = input_stream.valid ? any_ready : true;
        
        // 每个输出独立控制
        for (unsigned i = 0; i < N_OUTPUTS; i++) {
            // 仅当输入有效且就绪时，输出才有效
            result.output_streams[i].valid = input_stream.valid && input_stream.ready;
        }
    }
    
    // 输入流的信号
    result.input_stream.payload = input_stream.payload;
    result.input_stream.valid = input_stream.valid;
    
    return result;
}

/**
 * Stream Join - 等待所有输入流都有效后传输
 */
template<typename T, unsigned N_INPUTS>
struct StreamJoinResult {
    std::array<Stream<T>, N_INPUTS> input_streams;
    Stream<T> output_stream;
};

template<typename T, unsigned N_INPUTS>
StreamJoinResult<T, N_INPUTS> stream_join(
    std::array<Stream<T>, N_INPUTS> input_streams
) {
    StreamJoinResult<T, N_INPUTS> result;
    
    // 检查所有输入是否都有效
    ch_bool all_valid = true;
    for (unsigned i = 0; i < N_INPUTS; i++) {
        all_valid = all_valid && input_streams[i].valid;
        result.input_streams[i] = input_streams[i];
    }
    
    // 输出有效当且仅当所有输入都有效
    result.output_stream.valid = all_valid;
    
    // 当输出就绪时，所有输入都就绪
    for (unsigned i = 0; i < N_INPUTS; i++) {
        result.input_streams[i].ready = result.output_stream.ready && all_valid;
    }
    
    // 合并所有输入的payload（这里简化为取第一个，实际应用可能需要合并函数）
    result.output_stream.payload = input_streams[0].payload;
    result.output_stream.ready = true;  // 假设输出端总是就绪
    
    return result;
}

/**
 * Stream Arbiter - 流仲裁器（轮询方式）
 */
template<typename T, unsigned N_INPUTS>
struct StreamArbiterResult {
    std::array<Stream<T>, N_INPUTS> input_streams;
    Stream<T> output_stream;
    ch_uint<log2_ceil<N_INPUTS>::value> selected;  // 选择的输入索引
};

template<typename T, unsigned N_INPUTS>
StreamArbiterResult<T, N_INPUTS> stream_arbiter_round_robin(
    ch_bool clk,
    ch_bool rst,
    std::array<Stream<T>, N_INPUTS> input_streams
) {
    StreamArbiterResult<T, N_INPUTS> result;
    
    // 跟踪当前轮询位置的寄存器
    static ch_reg<ch_uint<log2_ceil<N_INPUTS>::value>> current_channel(0_d);
    current_channel->clk = clk;
    current_channel->rst = rst;
    
    // 初始化输入流
    for (unsigned i = 0; i < N_INPUTS; i++) {
        result.input_streams[i] = input_streams[i];
    }
    
    // 查找有效的输入流（从当前通道开始轮询）
    ch_bool found_valid = false;
    ch_uint<log2_ceil<N_INPUTS>::value> selected_idx = 0_d;
    T selected_payload = input_streams[0].payload;
    
    for (unsigned i = 0; i < N_INPUTS; i++) {
        unsigned idx = (current_channel + i) % N_INPUTS;
        ch_bool is_valid = input_streams[idx].valid;
        ch_bool can_select = is_valid && !found_valid;
        
        selected_idx = select(can_select, make_literal(idx), selected_idx);
        selected_payload = select(can_select, input_streams[idx].payload, selected_payload);
        found_valid = found_valid || can_select;
    }
    
    // 输出数据
    result.output_stream.payload = selected_payload;
    result.output_stream.valid = found_valid;
    
    // 只有当输出就绪时，选中的输入才就绪
    for (unsigned i = 0; i < N_INPUTS; i++) {
        ch_bool is_selected = (selected_idx == make_literal(i));
        result.input_streams[i].ready = is_selected && result.output_stream.ready && found_valid;
    }
    
    // 更新下一个轮询位置（当输出传输完成时）
    ch_uint<log2_ceil<N_INPUTS>::value> next_channel = current_channel;
    for (unsigned i = 0; i < N_INPUTS; i++) {
        unsigned idx = (current_channel + i) % N_INPUTS;
        ch_bool is_selected = (selected_idx == make_literal(idx));
        next_channel = select(is_selected, 
                            make_literal((idx + 1) % N_INPUTS), 
                            next_channel);
    }
    
    current_channel->next = select(result.output_stream.ready && found_valid, 
                                  next_channel, 
                                  current_channel);
    
    result.selected = selected_idx;
    
    return result;
}

/**
 * Stream Mux - 流多路选择器
 */
template<typename T, unsigned N_INPUTS>
Stream<T> stream_mux(
    Stream<T> selected_input,
    std::array<Stream<T>, N_INPUTS> input_streams
) {
    Stream<T> result;
    
    // 简化实现：直接使用第一个输入
    result = input_streams[0];
    
    // 实际实现会根据selected_input选择对应的输入流
    // 这里是占位符实现
    return result;
}

/**
 * Stream Demux - 流解复用器
 */
template<typename T, unsigned N_OUTPUTS>
struct StreamDemuxResult {
    Stream<T> input_stream;
    std::array<Stream<T>, N_OUTPUTS> output_streams;
    ch_uint<log2_ceil<N_OUTPUTS>::value> select;
};

template<typename T, unsigned N_OUTPUTS>
StreamDemuxResult<T, N_OUTPUTS> stream_demux(
    Stream<T> input_stream,
    ch_uint<log2_ceil<N_OUTPUTS>::value> select
) {
    StreamDemuxResult<T, N_OUTPUTS> result;
    result.input_stream = input_stream;
    result.select = select;
    
    // 将输入数据复制到所有输出
    for (unsigned i = 0; i < N_OUTPUTS; i++) {
        result.output_streams[i].payload = input_stream.payload;
        result.output_streams[i].valid = input_stream.valid && (select == make_literal(i));
    }
    
    // 只有选中的输出才就绪
    ch_bool selected_ready = false;
    for (unsigned i = 0; i < N_OUTPUTS; i++) {
        ch_bool is_selected = (select == make_literal(i));
        result.output_streams[i].ready = is_selected && input_stream.ready;
        selected_ready = selected_ready || (is_selected && result.output_streams[i].ready);
    }
    
    result.input_stream.ready = selected_ready;
    
    return result;
}

} // namespace chlib

#endif // CHLIB_STREAM_H