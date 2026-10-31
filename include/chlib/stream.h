#pragma once

#include "bundle/flow_bundle.h"
#include "bundle/stream_bundle.h"
#include "ch.hpp"
#include "chlib/combinational.h"
#include "chlib/fifo.h"
#include "chlib/logic.h"
#include "chlib/selector_arbiter.h"
#include "component.h"
#include "core/bool.h"
#include "core/bundle/bundle_base.h"
#include "core/literal.h"
#include "core/operators.h"
#include "core/operators_runtime.h"
#include "core/uint.h"
#include "lnode/literal_ext.h"
#include "utils/logger.h"
#include <array>
#include <cassert>
#include <vector>

// 添加对bundle/stream_bundle.h的引用，该文件定义了Stream类型
#include "bundle/stream_bundle.h"

namespace chlib {

using namespace ch::core;

/**
 * Stream FIFO - 带反压的FIFO
 */
template <typename T, unsigned DEPTH> struct StreamFifoResult {
    ch_stream<T> push_stream;                        // 推入端口
    ch_stream<T> pop_stream;                         // 弹出端口
    ch_uint<compute_idx_width(DEPTH) + 1> occupancy; // 占用计数
    ch_bool full;                                    // 满标志
    ch_bool empty;                                   // 空标志

    // ⚠️ **ADR-054 形态 E（默认值 + 可覆盖）**：默认 ready 源的**持有者**
    //
    // ⚠️ **为什么需要它**（实测，design.md §13）：
    //   ① **literal 节点不可被 `set_src(0, …)` 变成数据流边** ⇒ ⇒ 若把默认
    //      驱动**重绑定**到 literal 节点 ⇒ ⇒ 调用方的覆盖是**往常量节点上打**
    //      ⇒ ⇒ **无效** ⇒ ⇒ 「覆盖不传播」（id 相同但行为不变）；
    //   ② `<<=` 的**源** MUST 是**持久有效**的节点 ⇒ ⇒ **临时** `ch_bool(true)`
    //      无效 ⇒ ⇒ 那个 SIGSEGV。
    // ⇒ ⇒ 本成员**持有**那个 literal 节点（与结果对象**同生命周期**
    // ⇒ ⇒ 且节点**不随 C++ 对象析构而失效** —— 框架**无引用计数**、**无析构**）
    // ⇒ ⇒ 于是：源**持久有效** + 目标（`pop_stream.ready` 这个**已存在的**
    // **非 literal** 节点）⇒ ⇒ **覆盖必然传播** ✔
    // ⚠️⚠️ **2026-10-31 订正（ADR-054 §3.2 第 1 条）**：原注释写「目标 = field
    // proxy」⇒ ⇒ **理由错误**（条款本身仍成立）⇒ ⇒ **真实机制** =
    // 「`<<=` 在**已有**目标上 ⇒ ⇒ `set_src(0,…)` ⇒ ⇒ **覆盖生效**；
    //  在**空**目标上 ⇒ ⇒ **另建**一个 assign 节点并绑到 `this`
    //  ⇒ ⇒ **只驱动被 `<<=` 的那个对象** ⇒ ⇒ **调用方持有的节点被孤儿化**」
    // ⇒ ⇒ ⇒ **「用 `<<=`」本身不是「覆盖已生效」的充分条件**
    // ⇒ ⇒ ⇒ 「目标 MUST **已存在**」才是**必要条件**
    // ⇒ ⇒ ⇒ ⚠️ 「`<<=` 在空目标上另建节点」是**实现细节**
    // ⇒ ⇒ ⇒ **MUST NOT** 被当作公开契约语义（ADR-054 §3.1 形态 A「追认」段
    // ⇒ ⇒ ⇒ 已收窄）⇒ ⇒ 正确路径 = 让空目标在调用链中**不可达**。
    ch_bool default_ready_source;
};

// Forward declaration
template <typename T, unsigned DEPTH>
StreamFifoResult<T, DEPTH> stream_fifo(ch_stream<T> input_stream);

/**
 * @brief 把消费端的 ready 接到 FIFO —— **ADR-054 形态 E（默认值 + 可覆盖）**
 *
 * ⚠️ **契约**：
 *   ① `stream_fifo` **立即**建立内部 FIFO ⇒ ⇒ 既有行为**逐位不变** ⇒ ⇒
 *      **存量调用点不需迁移**；读使能的**默认** = **既有旧语义**（恒就绪）；
 *   ② 调用方**可选**调用本函数 ⇒ ⇒ **不调** = 恒就绪（旧语义）⇒ ⇒
 *      **不调不是缺陷**；
 *   ③ 本函数把消费端 ready **覆盖**到 `result.pop_stream.ready` ⇒ ⇒ 而内部
 *      FIFO 的读使能 MUST 引用**该字段节点本身** ⇒ ⇒ 覆盖**必然传播**。
 *
 * ⚠️ **消费端 ready 的形参 MUST 是模板**（ADR-053 / E3 的同型教训）：
 *    `const ch_bool &` ⇒ ⇒ `ch_in<ch_bool>` **端口绑不上** ⇒ ⇒ 模板化 +
 *    `to_operand` 归一化 ⇒ ⇒ 端口 / 硬件类型 / 表达式**都**可用。
 */
template <typename T, unsigned DEPTH, typename C>
void stream_fifo_connect_ready(StreamFifoResult<T, DEPTH> &result,
                               const C &consumer_ready);

/**
 * Stream Queue/Buffer - 创建具有指定深度的流缓冲区（别名为stream_fifo）
 * 类似SpinalHDL的queue()方法
 */
template <typename T, unsigned DEPTH>
StreamFifoResult<T, DEPTH> stream_queue(ch_stream<T> input_stream) {
    return stream_fifo<T, DEPTH>(input_stream);
}

/**
 * @brief 把消费端的 ready 接到 **queue**（= `stream_fifo` 的别名）——
 *   **ADR-054 形态 E**
 *
 * ⚠️ **为什么需要这个薄转发**：`stream_queue` 是 `stream_fifo` 的**别名**
 *   （返回**同一个** `StreamFifoResult` 类型）⇒ ⇒ 类型上可以直接调
 *   `stream_fifo_connect_ready` ⇒ ⇒ 但那要求调用方知道「queue 就是 fifo」
 *   ⇒ ⇒ **API 不自洽** ⇒ ⇒ 提供同名转发 ⇒ ⇒ 与 mux 类的
 *   `<组合子>_connect_ready` 心智模型**一致**。
 */
template <typename T, unsigned DEPTH, typename C>
void stream_queue_connect_ready(StreamFifoResult<T, DEPTH> &result,
                                const C &consumer_ready) {
    stream_fifo_connect_ready(result, consumer_ready);
}

/**
 * Stream throwWhen - 当条件为真时丢弃数据
 * 类似SpinalHDL的throwWhen()
 */
template <typename T>
ch_stream<T> stream_throw_when(ch_stream<T> input_stream, ch_bool condition) {
    ch_stream<T> result;
    
    result.payload = input_stream.payload;
    result.valid = input_stream.valid & !condition;
    
    // 当条件为真时，即使没有有效输出，输入也被消费
    //
    // ⚠️⚠️ **ADR-054 形态 E（默认值 + 可覆盖）· P1-a**
    //   ⚠️ **MUST 用 `<<=`**（MUST NOT `operator=`）：`ch_stream` 的**拷贝与
    //   原对象共享同一 ready 节点** ⇒ ⇒ ⇒ `<<=` 能**穿透副本**驱动到**调用方**
    //   ⇒ ⇒ ⇒ ⇒ **调用方真收到反压**（`operator=` 只重绑定**副本** ⇒ ⇒ ⇒
    //   ⇒ ⇒ **调用方被孤儿化** ⇒ ⇒ ⇒ ⇒ **静默**）
    ch_bool default_ready(true);            // 局部 literal 源（探针 ⑩ A：可被覆盖）
    result.ready <<= default_ready;         // ② 默认恒就绪（= 既有旧语义）
    input_stream.ready <<= result.ready | condition;
    
    return result;
}

/**
 * Stream takeWhen - 只在条件为真时传递数据
 * 类似SpinalHDL的takeWhen()
 */
template <typename T>
ch_stream<T> stream_take_when(ch_stream<T> input_stream, ch_bool condition) {
    ch_stream<T> result;
    
    result.payload = input_stream.payload;
    result.valid = input_stream.valid & condition;
    
    // 只有条件为真且输出就绪时，输入才就绪
    //
    // ⚠️⚠️ **ADR-054 形态 E（默认值 + 可覆盖）· P1-a**
    //   ⚠️ **MUST 用 `<<=`**（MUST NOT `operator=`）⇒ ⇒ ⇒ **调用方真收到
    //   反压**（详见 `stream_throw_when` 的同型注释）
    ch_bool default_ready(true);
    result.ready <<= default_ready;
    input_stream.ready <<= result.ready & condition;
    
    return result;
}

/**
 * Stream haltWhen - 当条件为真时暂停流
 * 类似SpinalHDL的haltWhen()
 */
template <typename T>
ch_stream<T> stream_halt_when(ch_stream<T> input_stream, ch_bool halt) {
    ch_stream<T> result;
    
    result.payload = input_stream.payload;
    result.valid = input_stream.valid & !halt;
    
    // 当暂停时，输入不就绪
    //
    // ⚠️⚠️ **ADR-054 形态 E（默认值 + 可覆盖）· P1-a**
    //   ⚠️ **MUST 用 `<<=`**（MUST NOT `operator=`）⇒ ⇒ ⇒ **调用方真收到
    //   反压**（详见 `stream_throw_when` 的同型注释）
    ch_bool default_ready(true);
    result.ready <<= default_ready;
    input_stream.ready <<= result.ready & !halt;
    
    return result;
}

/**
 * Stream continueWhen - 只在条件为真时继续流
 * 类似SpinalHDL的continueWhen()
 */
template <typename T>
ch_stream<T> stream_continue_when(ch_stream<T> input_stream, ch_bool continue_cond) {
    return stream_halt_when(input_stream, !continue_cond);
}

/**
 * Stream translateWith - 转换payload类型
 * 类似SpinalHDL的translateWith()
 */
template <typename TOut, typename TIn, typename Func>
ch_stream<TOut> stream_translate_with(ch_stream<TIn> input_stream, Func transform) {
    ch_stream<TOut> result;
    
    result.payload = transform(input_stream.payload);
    result.valid = input_stream.valid;
    //
    // ⚠️⚠️ **ADR-054 形态 E（默认值 + 可覆盖）· P1-a**
    //   ⚠️ **MUST 用 `<<=`**（MUST NOT `operator=`）⇒ ⇒ ⇒ **调用方真收到
    //   反压**（详见 `stream_throw_when` 的同型注释）
    ch_bool default_ready(true);
    result.ready <<= default_ready;
    input_stream.ready <<= result.ready;
    
    return result;
}

/**
 * Stream map - 映射payload到新值
 * 类似SpinalHDL的map()
 * Accepts any callable (lambda, functor, function pointer)
 */
template <typename TOut, typename TIn, typename Func>
ch_stream<TOut> stream_map(ch_stream<TIn> input_stream, Func transform) {
    ch_stream<TOut> result;
    
    result.payload = transform(input_stream.payload);
    result.valid = input_stream.valid;
    //
    // ⚠️⚠️ **ADR-054 形态 E（默认值 + 可覆盖）· P1-a**
    //   ⚠️ **MUST 用 `<<=`**（MUST NOT `operator=`）⇒ ⇒ ⇒ **调用方真收到
    //   反压**（详见 `stream_throw_when` 的同型注释）
    ch_bool default_ready(true);
    result.ready <<= default_ready;
    input_stream.ready <<= result.ready;
    
    return result;
}

/**
 * Stream transpose - 位宽转换/重组
 * 用于改变数据位宽
 */
template <typename TOut, typename TIn>
ch_stream<TOut> stream_transpose(ch_stream<TIn> input_stream) {
    ch_stream<TOut> result;
    
    // 简单的位宽转换 - 实际应用可能需要更复杂的逻辑
    result.payload = TOut(input_stream.payload);
    result.valid = input_stream.valid;
    //
    // ⚠️⚠️ **ADR-054 形态 E（默认值 + 可覆盖）· P1-a**
    //   ⚠️ **MUST 用 `<<=`**（MUST NOT `operator=`）⇒ ⇒ ⇒ **调用方真收到
    //   反压**（详见 `stream_throw_when` 的同型注释）
    ch_bool default_ready(true);
    result.ready <<= default_ready;
    input_stream.ready <<= result.ready;
    
    return result;
}

/**
 * Stream combineWith - 组合两个流
 * 当两个流都有效时才输出
 */
// Helper struct for combined payload
template <typename T1, typename T2>
struct StreamCombinePayload {
    T1 _1;
    T2 _2;
};

template <typename T1, typename T2>
struct StreamCombineResult {
    ch_stream<T1> input1;
    ch_stream<T2> input2;
    ch_stream<StreamCombinePayload<T1, T2>> output_stream;
};

template <typename T1, typename T2>
StreamCombineResult<T1, T2> stream_combine_with(ch_stream<T1> input1, ch_stream<T2> input2) {
    StreamCombineResult<T1, T2> result;
    
    result.input1 = input1;
    result.input2 = input2;
    
    // 组合payload
    result.output_stream.payload._1 = input1.payload;
    result.output_stream.payload._2 = input2.payload;
    
    // 当两个输入都有效时输出有效
    result.output_stream.valid = input1.valid & input2.valid;
    
    // 反压：当输出就绪且另一路有效时，该输入就绪
    //
    // ⚠️⚠️ **ADR-054 形态 E（默认值 + 可覆盖）· P1-a**
    //   ⚠️ **MUST 用 `<<=`**（MUST NOT `operator=`）⇒ ⇒ ⇒ **调用方真收到
    //   反压**（详见 `stream_throw_when` 的同型注释）
    ch_bool default_ready(true);
    result.output_stream.ready <<= default_ready;
    result.input1.ready <<= result.output_stream.ready & input2.valid;
    result.input2.ready <<= result.output_stream.ready & input1.valid;
    
    return result;
}

/**
 * Stream Priority Arbiter - 优先级仲裁器
 * 注意：priority_encoder迭代0到N-1并覆盖结果，选择最高有效索引（最高优先级）
 * 因此，较高索引的输入具有更高优先级
 */
template <typename T, unsigned N_INPUTS> 
struct StreamPriorityArbiterResult {
    std::array<ch_stream<T>, N_INPUTS> input_streams;
    ch_stream<T> output_stream;
    ch_uint<compute_idx_width(N_INPUTS)> selected;
};

template <typename T, unsigned N_INPUTS>
StreamPriorityArbiterResult<T, N_INPUTS>
stream_arbiter_priority(std::array<ch_stream<T>, N_INPUTS> input_streams) {
    static_assert(N_INPUTS >= 2, "Priority arbiter requires at least 2 inputs");
    StreamPriorityArbiterResult<T, N_INPUTS> result;
    
    // 初始化输入流
    for (unsigned i = 0; i < N_INPUTS; i++) {
        result.input_streams[i] = input_streams[i];
    }
    
    // 创建有效向量
    ch_uint<N_INPUTS> valid_vector = 0_d;
    for (unsigned i = 0; i < N_INPUTS; i++) {
        ch_bool is_valid = input_streams[i].valid;
        ch_uint<N_INPUTS> mask = ch_uint<N_INPUTS>(1_d) << make_uint<compute_idx_width(N_INPUTS)>(i);
        valid_vector = select(is_valid, valid_vector | mask, valid_vector);
    }
    
    // 使用优先级编码器选择最高优先级的有效输入
    auto selected_idx = priority_encoder<N_INPUTS>(valid_vector);
    
    // 选择payload
    std::array<T, N_INPUTS> payloads;
    for (unsigned i = 0; i < N_INPUTS; i++) {
        payloads[i] = input_streams[i].payload;
    }
    T selected_payload = mux<N_INPUTS>(payloads, selected_idx);
    
    // 输出
    result.output_stream.payload = selected_payload;
    result.output_stream.valid = valid_vector != 0_d;
    
    // 只有选中的输入就绪
    //
    // ⚠️ `result.output_stream.ready` MUST NOT 在**赋值之前**被读 ——
    //    此刻它还是**空 proxy** ⇒ 与之做 `&` 会拿到 null 操作数 ⇒
    //    node_builder 报 "Invalid operands for binary operation"
    //    （此前**零行为覆盖**）⇒ 改用局部常量 `out_ready`，
    //    并在最后把它接到 `result.output_stream.ready` 上。
    // ⚠️⚠️ **ADR-054 形态 E（默认值 + 可覆盖）· P1-a**
    //
    // ⚠️ **为什么删掉了局部常量 `out_ready`**（P1-a 的**核心修复**）：
    //   `ch_bool out_ready = true;` ⇒ ⇒ 组合子**硬编码**「输出恒就绪」
    //   ⇒ ⇒ ⇒ **无条件忽略消费端的 ready** ⇒ ⇒ ⇒ ⇒ **违反**主 spec
    //   `chlib-stream-ready-ownership` 的 R1 ⇒ ⇒ ⇒ ⇒ 且**无护栏**能检出
    //   ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **现状 = 「上游恒不 ready = 死路」**（实测）
    //   ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 修复不是行为变更 ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ 零破坏**成立。
    //
    // ⚠️ **为什么「先建默认驱动」再「读 `output_stream.ready`」是安全的**：
    //   `result.output_stream.ready` 在**建默认驱动之后**才有节点 ⇒ ⇒ ⇒
    //   与之做 `&` **合法**（⇒ ⇒ 消除此前「空 proxy ⇒ Invalid operands」
    //   的问题 ⇒ ⇒ **且** 不再需要局部常量）
    ch_bool default_ready(true);                  // 局部 literal 源（可被覆盖）
    result.output_stream.ready <<= default_ready; // ② 默认恒就绪（= 既有旧语义）
    for (unsigned i = 0; i < N_INPUTS; i++) {
        ch_bool is_selected = (selected_idx == make_uint<compute_idx_width(N_INPUTS)>(i));
        // ⚠️ **MUST 用 `<<=`**（MUST NOT `operator=`）⇒ ⇒ ⇒ **调用方真收到
        // 反压**（`ch_stream` 的**拷贝与原对象共享同一 ready 节点** ⇒ ⇒ ⇒
        // `<<=` 能**穿透副本**驱动到**调用方**）
        result.input_streams[i].ready <<=
            (is_selected & result.output_stream.ready);
    }
    // ⚠️ `result.output_stream.ready` 已在**上方**建立默认驱动 ⇒ ⇒ 无需再接
    
    result.selected = selected_idx;
    
    return result;
}

/**
 * Stream FIFO implementation - 带反压的FIFO
 */
// ⚠️ `DEPTH` MUST 是 2 的幂：`compute_idx_width(DEPTH)` 会把非 2 的幂映射到
//    **下一个** 2 的幂（`DEPTH=5` ⇒ `compute_idx_width(5)=3` ⇒ 实际深度 8）
//    ⇒ 「FIFO 深度」本身将未定义。
//    ⚠️ 外溢面：`include/chlib/stream_builder.h` 的 `StreamBuilder::queue<DEPTH>()`
//    直接实例化本模板 ⇒ 该约束同时是 `StreamBuilder` 的公开约束。
//    （ADR-045 裁决 2.3 / capability `chlib-stream-semantics`）
template <typename T, unsigned DEPTH>
StreamFifoResult<T, DEPTH> stream_fifo(ch_stream<T> input_stream) {
    static_assert(DEPTH > 0, "stream_fifo DEPTH must be greater than 0");
    static_assert((DEPTH & (DEPTH - 1)) == 0,
                  "stream_fifo DEPTH must be a power of 2 "
                  "(compute_idx_width rounds non-powers-of-2 up to the next "
                  "power of 2, so the actual depth would differ from DEPTH)");

    StreamFifoResult<T, DEPTH> result;

    // 计算地址宽度：log2(DEPTH)
    constexpr unsigned ADDR_WIDTH = compute_idx_width(DEPTH);

    // ⚠️ **ADR-054 形态 E（默认值 + 可覆盖）**
    //
    // 读使能 MUST 引用**结果对象自己的字段节点**（`result.pop_stream.ready`）
    // ⇒ ⇒ 调用方覆盖该字段 ⇒ ⇒ 覆盖**必然传播**到内部 FIFO
    // ⇒ ⇒ 若引用**局部**节点 ⇒ ⇒ 覆盖的是字段而内部表达式仍读局部节点
    // ⇒ ⇒ **覆盖不传播 ⇒ 静默失效**（测试可能全绿而功能死）⇒ ⇒ 与现缺陷同型。
    //
    // ⚠️ 默认值 = **既有旧语义**（恒就绪）⇒ ⇒ **不调 `connect_ready` 时行为与
    //    改动前逐位一致** ⇒ ⇒ **存量调用点不需迁移**。
    //
    // ⚠️⚠️ **两个约束**（实测，design.md §13）：
    //   ① **目标** MUST 是**已存在**的**非 literal** 节点 ⇒ ⇒ 用
    //      `pop_stream.ready` 这个**结果对象的字段**（⇒ ⇒ 调用方覆盖**有效**）
    //      ⇒ ⇒ **MUST NOT** 重绑定到 literal 节点（⇒ ⇒ literal 不可被
    //      `set_src` ⇒ ⇒ 覆盖无效）⇒ ⇒ ⚠️ 且目标 MUST **已存在**
    //      ⇒ ⇒（`<<=` 在**空**目标上会**另建**节点 ⇒ ⇒ 只驱动被 `<<=` 的对象
    //      ⇒ ⇒ ⇒ 调用方被**孤儿化** ⇒ ⇒ ⇒ 「用 `<<=`」不是充分条件）；
    //   ② **源** MUST 是**持久有效**的节点 ⇒ ⇒ 用**本结果对象持有的**
    //      `default_ready_source`（⇒ ⇒ **MUST NOT** 用临时 `ch_bool(true)`
    //      ⇒ ⇒ 那个源无效 ⇒ ⇒ 驱动建立失败 ⇒ ⇒ 无效操作数 ⇒ ⇒ 💥 SIGSEGV）。
    result.default_ready_source = ch_bool(true);          // ① 持有 literal 节点（持久）
    // ② 目标 = **已存在的非 literal 节点**（结果对象的 ready 字段）
    //    ⚠️「目标 MUST 已存在」是**必要条件** ⇒ ⇒（`<<=` 在空目标上会**另建**
    //    节点 ⇒ ⇒ 只驱动被 `<<=` 的对象 ⇒ ⇒ 调用方被孤儿化）
    //    ⚠️ 2026-10-31 订正：原注释写「目标 = field proxy」⇒ ⇒ **理由错误**
    //    （条款仍成立）⇒ ⇒ 真实机制 = 「`<<=` 在**已有**目标上 ⇒ ⇒ 覆盖生效」
    result.pop_stream.ready <<= result.default_ready_source;

    // 创建内部FIFO，使用正确的模板参数（数据宽度和地址宽度）
    // ⚠️ 读使能 MUST 是 `pop_ready` 本身，**MUST NOT** 写成
    //    `pop_ready & result.pop_stream.valid` —— 后者是**先读后写**：
    //    `result.pop_stream.valid` 在此处**还没被赋值**（它在下面
    //    `result.pop_stream.valid = !fifo_result.empty;` 才被赋值）
    //    ⇒ 该字段此刻是**空 proxy** ⇒ `&` 运算拿到 null 操作数 ⇒
    //    node_builder 报 "Invalid operands for binary operation"，
    //    随后 `sync_fifo` 拿到无效操作数 ⇒ 运行时抛
    //    "Invalid data parameter" ⇒ **`stream_fifo` 完全不可用**。
    //    （该缺陷此前**零行为覆盖**：`tests/chlib/test_stream.cpp` 的
    //    "Stream: Stream FIFO" 只做结构断言、从不跑仿真。）
    //
    //    ⚠️ 语义（ADR-045 裁决 2.1）：`sync_fifo` 的写口 / 写指针 / 读指针
    //    **全部**由 `will_write` / `will_read` 门控 ⇒ **自保护**：
    //    消费端恒就绪 ⇒ FIFO **非空即弹**；空/满由内部的空/满门处理。
    //    ⚠️ **MUST NOT** 说 `fifo.h:69` 保护读指针 —— 它保护的是**计数器**；
    //    读指针的门在 `fifo.h` 的「更新读指针」段（与写口同一组门控）。
    // ⚠️ **写使能 MUST 只用 `input_stream.valid`**（**不**含 `ready`）：
    //    ① `sync_fifo` 的写口**自保护**（`chlib/fifo.h:74`
    //       `will_write = wren & !is_full(count)`）⇒ ⇒ 满时内部自行拦住；
    //    ② `valid & ready` 是**先读后写** ⇒ ⇒ `ready`（= 调用方的 `in.ready`）
    //       在此**未必被驱动** ⇒ ⇒ 空操作数 ⇒ ⇒ **完全不可用**
    //       （该缺陷此前**零行为覆盖**）。
    //
    // ⚠️ **读使能 MUST 引用「结果对象自己的字段节点」**（见上）⇒ ⇒ 覆盖**传播**。
    auto fifo_result = sync_fifo<ch_width_v<T>, ADDR_WIDTH>(
        input_stream.valid,                               // 写使能（自保护）
        input_stream.payload,                              // 写数据
        result.pop_stream.ready                            // 读使能（**字段节点本身**）
    );

    // 连接输入到FIFO
    result.push_stream.payload = input_stream.payload;
    result.push_stream.valid = input_stream.valid;
    result.push_stream.ready = !fifo_result.full; // FIFO未满时就绪

    // 连接FIFO到输出
    //
    // ⚠️ `pop_stream.payload` MUST 用 `q_async`（0-cycle `aread` 组合读），
    //    **MUST NOT** 用 `q`（1-cycle `sread` 寄存读）：
    //    `q` 是「沿弹出」语义（落后一拍），用它 ⇒ `payload` 与 `valid` **不同拍**
    //    ⇒ 消费者读到的是**上一个**元素（实测：推入 0x5A 后前 4 拍 payload 全 0x0）。
    //    `q_async` 是「队首 show-ahead」语义 ⇒ `payload` 与 `valid` **同拍**。
    //    （ADR-045 裁决 2.2 / capability `chlib-stream-semantics`）
    //
    // ⚠️ FIFO 协议：`aread` 是 0-cycle ⇒ `payload` 在 pop 沿**之后**指向
    //    **下一个**队首 ⇒ 消费者 MUST 在 pop 沿**之前**采样才能拿到
    //    **正在被弹出**的那个元素（与 ADR-044 裁决 5 同型）。
    // ⚠️ `empty` 时 `payload` 是 **don't-care**（MUST NOT 被强制为 0）。
    result.pop_stream.payload = fifo_result.q_async;
    result.pop_stream.valid = !fifo_result.empty;
    // ⚠️ `result.pop_stream.ready` **已在上方驱动**（默认恒就绪 ⇒ ⇒ 可被调用方
    //    覆盖 ⇒ ⇒ 覆盖**传播**到读使能）⇒ ⇒ 此处 MUST NOT 再赋值。

    // 占用计数和状态
    result.occupancy = fifo_result.count;
    result.full = fifo_result.full;
    result.empty = fifo_result.empty;

    return result;
}

template <typename T, unsigned DEPTH, typename C>
void stream_fifo_connect_ready(StreamFifoResult<T, DEPTH> &result,
                               const C &consumer_ready) {
    // ⚠️⚠️ **多驱动守卫**（ADR-054 §3.2 第 1 条）：
    //   `connect_ready` MUST 断言「当前驱动**仍是默认节点**」
    //   ⇒ ⇒ 否则 MUST **`CHFATAL_EXCEPTION`**
    //   ⇒ ⇒ ⚠️ **MUST NOT** 用 `CHERROR` / `CHFATAL` —— 实测**只打日志不抛**
    //   ⇒ ⇒ ⇒ 那是**假安全网** ⇒ ⇒ ADR-050 已踩过 ⇒ ⇒ 本仓「静默失效」家族
    //   ⇒ ⇒ ⇒ MUST 抛异常（构建期**响亮**失败）。
    //
    // ⚠️ **为什么这个守卫是必需的**：`ch_bool::operator<<=` 走
    //    `set_src(0, …)`（`include/lnode/bool.tpp:58`）⇒ ⇒ **last-wins**
    //    ⇒ ⇒ 若调用方**已自行驱动**该字段 ⇒ ⇒ 再调 `connect_ready` 会
    //    ⇒ ⇒ **静默覆盖**调用方的驱动 ⇒ ⇒ 调用方以为「我驱动的是我自己的
    //    信号」而实际被组合子接管 ⇒ ⇒ **静默失效**。
    auto *ready_node = result.pop_stream.ready.impl();
    auto *default_node = result.default_ready_source.impl();
    if (!ready_node || !default_node) {
        CHFATAL_EXCEPTION(
            "[stream_fifo_connect_ready] ready 字段节点缺失"
            "（result.pop_stream.ready / default_ready_source）"
            " ⇒ ⇒ MUST NOT 静默继续");
    }
    if (ready_node->num_srcs() == 0 || ready_node->src(0) != default_node) {
        CHFATAL_EXCEPTION(
            "[stream_fifo_connect_ready] **多驱动检测**："
            "result.pop_stream.ready 的当前驱动已不是默认节点"
            " ⇒ ⇒ 覆盖将是 last-wins ⇒ ⇒ 静默 ⇒ ⇒ MUST NOT 继续"
            "（提示：调用方 MUST NOT 自行驱动该字段，或 MUST NOT 重复调本函数）");
    }

    // ⚠️ **覆盖**默认驱动 ⇒ ⇒ 内部 FIFO 的读使能引用**该字段节点本身**
    //    ⇒ ⇒ 覆盖**必然传播** ⇒ ⇒ 反压真正生效。
    //
    // ⚠️ 归一化（端口 / 硬件类型 / 表达式**都**可用；`const ch_bool &`
    //    ⇒ ⇒ 端口绑不上 ⇒ ⇒ ADR-053 / E3 的同型教训）
    ch_bool cr(to_operand(consumer_ready).impl());
    result.pop_stream.ready <<= cr;
}

/**
 * Stream Fork - 将输入流复制到多个输出流
 * synchronous=true: 所有输出必须同时就绪才传输
 * synchronous=false: 任意输出就绪即可传输
 */
template <typename T, unsigned N_OUTPUTS> struct StreamForkResult {
    ch_stream<T> input_stream;
    std::array<ch_stream<T>, N_OUTPUTS> output_streams;
};

template <typename T, unsigned N_OUTPUTS>
StreamForkResult<T, N_OUTPUTS> stream_fork(ch_stream<T> input_stream,
                                           bool synchronous = true) {
    StreamForkResult<T, N_OUTPUTS> result;

    // 复制输入数据到所有输出
    for (unsigned i = 0; i < N_OUTPUTS; i++) {
        result.output_streams[i].payload = input_stream.payload;
        result.output_streams[i].valid = input_stream.valid;
    }

    if (synchronous) {
        // 同步模式：所有输出都就绪时输入才就绪
        //
        // ⚠️ **ready 只能取自 `input_stream.ready`** —— 输出侧的 ready 是
        //    **调用方在拿到结果之后**才设的（`result.output_streams[i].ready
        //    = ...`），在构造期读到的是**空 proxy** ⇒ 与之做 `&` / `|`
        //    会拿到 null 操作数 ⇒ node_builder 报
        //    "Invalid operands for binary operation"（此前**零行为覆盖**）。
        //    ⇒ 这里的 ready 链由输入侧的 ready 驱动；输出侧的 ready 字段
        //    在下面统一由 `result.input_stream.ready` 赋值。
        result.input_stream.ready = input_stream.ready;

        // 输入有效且就绪时，所有输出都有效
        for (unsigned i = 0; i < N_OUTPUTS; i++) {
            result.output_streams[i].ready = result.input_stream.ready;
        }
    } else {
        // 非同步模式：任意输出就绪时输入就绪
        // ⚠️ 同上：`any_ready` MUST NOT 读构造期还不可得的输出 ready。
        result.input_stream.ready = input_stream.ready;

        // 每个输出独立控制
        for (unsigned i = 0; i < N_OUTPUTS; i++) {
            // 仅当输入有效且就绪时，输出才有效
            result.output_streams[i].valid =
                input_stream.valid & input_stream.ready;
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
template <typename T, unsigned N_INPUTS> struct StreamJoinResult {
    std::array<ch_stream<T>, N_INPUTS> input_streams;
    ch_stream<T> output_stream;
};

template <typename T, unsigned N_INPUTS>
StreamJoinResult<T, N_INPUTS>
stream_join(std::array<ch_stream<T>, N_INPUTS> input_streams) {
    StreamJoinResult<T, N_INPUTS> result;

    // 检查所有输入是否都有效
    ch_bool all_valid = true;
    for (unsigned i = 0; i < N_INPUTS; i++) {
        all_valid = all_valid & input_streams[i].valid;
        result.input_streams[i] = input_streams[i];
    }

    // 输出有效当且仅当所有输入都有效
    result.output_stream.valid = all_valid;

    // ⚠️⚠️ **ADR-054 形态 E（默认值 + 可覆盖）· P1-a**
    //
    // ⚠️ **为什么用 `<<=`**（P1-a 探针 ② 实测）：`ch_stream` 的**拷贝与原
    //   对象共享同一 ready 节点** ⇒ ⇒ ⇒ 组合子内部 `<<=` 能**穿透副本**驱动
    //   到**调用方** ⇒ ⇒ ⇒ ⇒ **调用方真收到反压**
    //   ⇒ ⇒ ⇒ ⇒ （`operator=` 只重绑定**副本** ⇒ ⇒ ⇒ **调用方被孤儿化**）
    //
    // ⚠️ **默认驱动 = 恒就绪**（= **既有旧语义**）⇒ ⇒ 未接背压时行为**逐位
    //   不变** ⇒ ⇒ ⇒ **零破坏**
    // ⚠️ **默认驱动的源用【局部】`ch_bool`**（同 mux 的注释 ⇒ ⇒ 探针 ⑩ A
    //   实测：局部源**可被覆盖** ✔ ⇒ ⇒ **零公开 API 变更**）
    ch_bool default_ready(true);
    result.output_stream.ready <<= default_ready;

    // 当输出就绪时，所有输入都就绪
    for (unsigned i = 0; i < N_INPUTS; i++) {
        result.input_streams[i].ready <<= result.output_stream.ready;
    }

    // 合并所有输入的payload（这里简化为取第一个，实际应用可能需要合并函数）
    result.output_stream.payload = input_streams[0].payload;
    // 输出的ready信号由外部连接决定，不再假设总是就绪

    return result;
}

/**
 * Stream Arbiter - 流仲裁器（轮询方式）
 */
template <typename T, unsigned N_INPUTS> struct StreamArbiterResult {
    std::array<ch_stream<T>, N_INPUTS> input_streams;
    ch_stream<T> output_stream;
    ch_uint<compute_idx_width(N_INPUTS)> selected; // 选择的输入索引
};

template <typename T, unsigned N_INPUTS>
StreamArbiterResult<T, N_INPUTS>
stream_arbiter_round_robin(std::array<ch_stream<T>, N_INPUTS> input_streams) {
    StreamArbiterResult<T, N_INPUTS> result;

    // 跟踪当前轮询位置的寄存器，使用默认时钟和复位
    auto current_channel = ch_reg<ch_uint<compute_idx_width(N_INPUTS)>>(0_d);

    // 初始化输入流
    for (unsigned i = 0; i < N_INPUTS; i++) {
        result.input_streams[i] = input_streams[i];
    }

    // 创建一个表示哪些输入有效的向量
    ch_uint<N_INPUTS> valid_vector = 0_d;
    for (unsigned i = 0; i < N_INPUTS; i++) {
        ch_bool is_valid = input_streams[i].valid;
        ch_uint<N_INPUTS> mask = ch_uint<N_INPUTS>(1_d)
                                 << make_uint<compute_idx_width(N_INPUTS)>(i);
        valid_vector = select(is_valid, valid_vector | mask, valid_vector);
    }

    // 使用round_robin_selector进行仲裁
    ch_uint<N_INPUTS> last_grant = 0_d;
    auto arb_result = round_robin_selector<N_INPUTS>(valid_vector, last_grant);

    // 获取选中的索引
    auto selected_idx = priority_encoder<N_INPUTS>(arb_result.grant);

    // 使用mux函数来选择payload，而不是手动循环
    std::array<T, N_INPUTS> payloads;
    for (unsigned i = 0; i < N_INPUTS; i++) {
        payloads[i] = input_streams[i].payload;
    }
    T selected_payload = mux<N_INPUTS>(payloads, selected_idx);

    // 输出数据
    result.output_stream.payload = selected_payload;
    result.output_stream.valid = arb_result.valid;

    // 只有当输出就绪时，选中的输入才就绪
    //
    // ⚠️ `result.output_stream.ready` MUST NOT 在**赋值之前**被读 ——
    //    此刻它还是**空 proxy** ⇒ 与之做 `&` 会拿到 null 操作数 ⇒
    //    node_builder 报 "Invalid operands for binary operation"
    //    （此前**零行为覆盖**）⇒ 改用局部常量 `out_ready`。
    // ⚠️⚠️ **ADR-054 形态 E（默认值 + 可覆盖）· P1-a**
    //
    // ⚠️ **为什么删掉了局部常量 `out_ready`**（P1-a 的**核心修复**）：
    //   `ch_bool out_ready = true;` ⇒ ⇒ 组合子**硬编码**「输出恒就绪」
    //   ⇒ ⇒ ⇒ **无条件忽略消费端的 ready** ⇒ ⇒ ⇒ ⇒ **违反**主 spec
    //   `chlib-stream-ready-ownership` 的 R1 ⇒ ⇒ ⇒ ⇒ 且**无护栏**能检出
    //   ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **现状 = 「上游恒不 ready = 死路」**（实测）
    //   ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 修复不是行为变更 ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ 零破坏**成立。
    //
    // ⚠️ **为什么「先建默认驱动」再「读 `output_stream.ready`」是安全的**：
    //   `result.output_stream.ready` 在**建默认驱动之后**才有节点 ⇒ ⇒ ⇒
    //   与之做 `&` **合法**（⇒ ⇒ 消除此前「空 proxy ⇒ Invalid operands」
    //   的问题 ⇒ ⇒ **且** 不再需要局部常量）
    ch_bool default_ready(true);                  // 局部 literal 源（可被覆盖）
    result.output_stream.ready <<= default_ready; // ② 默认恒就绪（= 既有旧语义）
    for (unsigned i = 0; i < N_INPUTS; i++) {
        ch_bool is_selected =
            (selected_idx == make_uint<compute_idx_width(N_INPUTS)>(i));
        // ⚠️ **MUST 用 `<<=`**（MUST NOT `operator=`）⇒ ⇒ ⇒ **调用方真收到
        // 反压**（详见同文件 `stream_arbiter_priority` 的注释）
        result.input_streams[i].ready <<=
            (is_selected & result.output_stream.ready & arb_result.valid);
    }

    result.selected = selected_idx;
    // ⚠️ `result.output_stream.ready` 已在**上方**建立默认驱动 ⇒ ⇒ 无需再接

    return result;
}

/**
 * Stream Mux Result - 流多路选择器返回结构体
 */
template <unsigned N_INPUTS, typename T> struct StreamMuxResult {
    ch_stream<T> output_stream;
    std::array<ch_stream<T>, N_INPUTS> input_streams;
    ch_uint<compute_idx_width(N_INPUTS)> select;
};

/**
 * Stream Mux - 流多路选择器
 */
template <unsigned N_INPUTS, typename T>
StreamMuxResult<N_INPUTS, T>
stream_mux(std::array<ch_stream<T>, N_INPUTS> input_streams,
           ch_uint<compute_idx_width(N_INPUTS)> select) {
    StreamMuxResult<N_INPUTS, T> result;
    result.input_streams = input_streams;
    result.select = select;

    // 使用chlib中的mux函数来选择payload
    std::array<T, N_INPUTS> payloads;
    for (unsigned i = 0; i < N_INPUTS; i++) {
        payloads[i] = input_streams[i].payload;
    }

    // 直接使用T::ch_width作为mux的第一个模板参数（如果T是ch_uint的话）
    // 如果T不是ch_uint类型，则手动实现mux逻辑
    if constexpr (requires { T::ch_width; }) { // 检查T是否有ch_width成员
        result.output_stream.payload = mux<N_INPUTS>(payloads, select);
    } else {
        // 如果T没有ch_width成员，则手动实现mux功能
        result.output_stream.payload = payloads[0];
        for (unsigned i = 1; i < N_INPUTS; ++i) {
            ch_bool is_selected =
                (select == make_uint<compute_idx_width(N_INPUTS)>(i));
            result.output_stream.payload =
                ch::core::select(is_selected, payloads[i],
                                 result.output_stream.payload);
        }
    }

    // 使用chlib中的or gate来计算选中的输入是否有效
    ch_bool selected_valid = false;
    for (unsigned i = 0; i < N_INPUTS; i++) {
        ch_bool is_selected =
            (select == make_uint<compute_idx_width(N_INPUTS)>(i));
        selected_valid =
            selected_valid | (input_streams[i].valid & is_selected);
    }

    result.output_stream.valid = selected_valid;

    // ⚠️⚠️ **ADR-054 形态 E（默认值 + 可覆盖）· P1-a**
    //
    // ⚠️ **为什么在这里建立 ready 链**（P1-a 探针 ②/⑧ 实测）：
    //   ① `ch_stream` 的**拷贝与原对象共享同一 ready 节点** ⇒ ⇒ 组合子内部
    //      用 `<<=` 能**穿透副本**驱动到**调用方** ⇒ ⇒ ⇒ **调用方真收到反压**；
    //   ② `output_stream.ready` MUST **有消费者**才会被求值（探针 ⑧ 反证组）
    //      ⇒ ⇒ 本处的 `is_selected & output_stream.ready` 就是那个消费者
    //      ⇒ ⇒ ⇒ **MUST 在组合子内部建立**（不能只等 `connect_ready`）。
    //
    // ⚠️ **MUST NOT** 用 `operator=`（MUST NOT 重绑定副本 ⇒ ⇒ 调用方被孤儿化）
    //
    // ⚠️ **默认驱动 = 恒就绪**（= **既有旧语义**）⇒ ⇒ 未接背压时行为**逐位
    //   不变**⇒ ⇒ ⇒ **零破坏** ⇒ ⇒ 消费端可直接 `<<=` 覆盖它（**单阶段**）。
    // ⚠️ **默认驱动的源用【局部】`ch_bool`**（P1-a 探针 ⑩ A 实测：局部源
    //   **可被覆盖** ✔）⇒ ⇒ **无需**给结果 struct 加成员 ⇒ ⇒ ⇒ **零公开 API
    //   变更** ✔
    //   ⚠️ **为什么局部源够用**：框架**无引用计数、无析构** ⇒ ⇒ 节点不随
    //   C++ 对象析构而失效 ⇒ ⇒ 源**持久有效** ⇒ ⇒ 覆盖**必然传播** ✔
    //   ⚠️ ⚠️ **与 fifo 的差异**（**如实登记**）：fifo 的
    //   `StreamFifoResult::default_ready_source` 是**成员**（⇒ 源随结果对象
    //   存活）⇒ ⇒ 本处用**局部** ⇒ ⇒ **源与结果对象的生命周期不同**
    //   ⇒ ⇒ ⇒ 但**行为等价**（两者都「持久有效」）⇒ ⇒ ⇒ ⇒ ⇒
    //   ⇒ ⇒ ⇒ ⚠️ **本处 MUST NOT 改成 `operator=`**（⇒ ⇒ 重绑定 ⇒ ⇒ 字段
    //   变成 literal-backed ⇒ ⇒ 覆盖**无效**）
    ch_bool default_ready(true);                         // ① 局部 literal 源
    result.output_stream.ready <<= default_ready;        // ② 默认恒就绪

    for (unsigned i = 0; i < N_INPUTS; i++) {
        ch_bool is_selected =
            (result.select == make_uint<compute_idx_width(N_INPUTS)>(i));
        result.input_streams[i].ready <<=
            (is_selected & result.output_stream.ready);
    }

    return result;
}

/**
 * Stream Mux Connect Ready - 连接 MUX 的 ready 信号
 * 在设置好 output_stream.ready 后调用此函数来连接 input ready 信号
 */
template <unsigned N_INPUTS, typename T>
void stream_mux_connect_ready(StreamMuxResult<N_INPUTS, T>& mux_result) {
    for (unsigned i = 0; i < N_INPUTS; i++) {
        ch_bool is_selected =
            (mux_result.select == make_uint<compute_idx_width(N_INPUTS)>(i));
        // ⚠️⚠️ **连接手段 MUST 是 `<<=`（数据流边），MUST NOT 是 `operator=`**
        //    （ADR-045 决策 0-B 第 2 条 + ADR-054 §3.2 第 2 条）
        //
        //    ⚠️ **为什么 `operator=` 是错的**：`result.input_streams = input_streams;`
        //    拷贝后，**结果对象的 ready 字段与调用方的 ready 字段是【同一个节点】**
        //    ⇒ ⇒ `operator=`（节点**重绑定**）只改**结果对象里那个成员指针**
        //    ⇒ ⇒ 调用方的节点**被孤儿化**（永远没有驱动）
        //    ⇒ ⇒ **调用方的上游永远收不到 ready** ⇒ ⇒ **静默**。
        //    ⇒ ⇒ `<<=` 走 `set_src(0, …)` ⇒ ⇒ 在**共享的那个节点**上建数据流边
        //    ⇒ ⇒ 调用方的上游**真的**收到 ready ✔
        //
        // ⚠️⚠️ **P1-a（ADR-054 形态 E · 单阶段 · architect 裁决 2026-11-01）**：
        //    `stream_mux` **内部已建立**这条 ready 链（⇒ ⇒ 因为
        //    `output_stream.ready` MUST **有消费者**才会被求值 ⇒ ⇒ 探针 ⑧）
        //    ⇒ ⇒ ⇒ **本函数现在是「幂等重连」**（`<<=` = `set_src` 覆盖 ⇒ ⇒
        //    重复调用**不报错**、行为不变）⇒ ⇒ ⇒ **可保留为向后兼容入口**
        //    ⇒ ⇒ ⇒ ⇒ **消费端覆盖 ready MUST 直接 `<<=` 到
        //    `result.output_stream.ready`**（⇒ ⇒ **单阶段** ⇒ ⇒ 无需本函数）
        //    ⇒ ⇒ ⇒ ⇒ ⚠️ **本函数 MUST NOT** 照搬 fifo 的「多驱动拒绝」
        //    守卫（⇒ ⇒ 那样会让重复调用抛异常 ⇒ ⇒ 破坏幂等性）。
        mux_result.input_streams[i].ready <<=
            (is_selected & mux_result.output_stream.ready);
    }
}

/**
 * Stream Demux - 流解复用器
 */
template <typename T, unsigned N_OUTPUTS> struct StreamDemuxResult {
    ch_stream<T> input_stream;
    std::array<ch_stream<T>, N_OUTPUTS> output_streams;
    ch_uint<compute_idx_width(N_OUTPUTS)> select;
};

template <typename T, unsigned N_OUTPUTS>
StreamDemuxResult<T, N_OUTPUTS>
stream_demux(ch_stream<T> input_stream,
             ch_uint<compute_idx_width(N_OUTPUTS)> select) {
    StreamDemuxResult<T, N_OUTPUTS> result;
    result.input_stream = input_stream;
    result.select = select;

    // 使用chlib中的demux函数来处理数据流
    auto demux_outputs = demux<N_OUTPUTS>(input_stream.payload, select);

    // 将输入数据复制到所有输出
    for (unsigned i = 0; i < N_OUTPUTS; i++) {
        result.output_streams[i].payload = demux_outputs[i];
        result.output_streams[i].valid =
            input_stream.valid &
            (select == make_uint<compute_idx_width(N_OUTPUTS)>(i));
    }

    // 只有选中的输出才就绪
    ch_bool selected_ready = false;
    for (unsigned i = 0; i < N_OUTPUTS; i++) {
        ch_bool is_selected =
            (select == make_uint<compute_idx_width(N_OUTPUTS)>(i));
        result.output_streams[i].ready = is_selected & input_stream.ready;
        selected_ready =
            selected_ready | (is_selected & result.output_streams[i].ready);
    }

    result.input_stream.ready = selected_ready;

    return result;
}

} // namespace chlib
