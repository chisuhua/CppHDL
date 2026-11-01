#pragma once

/**
 * @file stream_pipeline.h
 * @brief `chlib` 的 pipe 族（`m2s` / `s2m` / `half`）
 *
 * ---------------------------------------------------------------------------
 * ⭐ **ready 归属**（ADR-055 · capability `chlib-stream-ready-ownership`）
 * ---------------------------------------------------------------------------
 * 三个 pipe 都遵守同一条形态：
 *   ① **输出侧**的 `output.ready` 是一个**带默认驱动、可被消费端覆盖**的字段
 *      （ADR-054 形态 E）⇒ ⇒ 消费端 MUST 用 `result.output.ready <<= consumer_ready`
 *      施加反压；**不覆盖** ⇒ 恒就绪（= 旧语义，存量调用点零迁移）；
 *   ② **上游侧**的 `input_stream.ready` MUST 由 pipe **自己驱动**，且 MUST 用
 *      `<<=`（**数据流边**）—— **MUST NOT** 用 `operator=`：
 *      它只重绑定**本地副本**的指针 ⇒ ⇒ 调用方被**孤儿化** ⇒ ⇒ 上游**永远收不到
 *      反压 ⇒ ⇒ **静默失效**（这正是本文件修复前的形态：三个 pipe 全是
 *      `input_stream.ready = …`，且 `output.ready` **从未被驱动**
 *      ⇒ ⇒ 「只在 `ready = true` 常量接线下活着」= A4 实测的判决性对比）。
 *
 * ⚠️ **接线纪律（调用方义务）**：`input_stream` MUST 先 `as_master()`
 *   （⇒ 字段**有节点**）⇒ ⇒ 否则 pipe 无法把反压钉上去 ⇒ ⇒
 *   `detail::drive_upstream_ready` 会**响亮拒绝**（`CHFATAL_EXCEPTION`）。
 *
 * ⚠️ **为什么 `input.ready` 与 `output.ready` 的「寄存/组合」不同**：
 *   · `m2s`：**组合**通过（`input.ready = output.ready`）⇒ 多一拍反压延迟会
 *     让「下游 hiccup」变成「上游多塞一拍」⇒ 与 SpinalHDL 的 `m2sPipe` 一致；
 *   · `s2m` / `half`：**寄存**（`input.ready = RegNext(output.ready)`）
 *     ⇒ ⇒ ⚠️ 修复前它们寄存的是 **`input_stream.ready`**（= 上游自己的 ready）
 *     ⇒ ⇒ 那是**方向反了**的自环 ⇒ ⇒ 语义无意义。
 */

#include "bundle/stream_bundle.h"
#include "ch.hpp"
#include "core/bool.h"
#include "core/lnodeimpl.h"
#include "core/reg.h"
#include "utils/logger.h"

namespace chlib {

using namespace ch::core;

namespace detail {

/**
 * @brief 把上游的 `ready` 钉到 `driver` 上（**唯一**允许的连接手段）。
 *
 * ⚠️ MUST 用 `<<=`（数据流边）—— `operator=` 只重绑定**本地副本**的指针
 *    ⇒ ⇒ 调用方被孤儿化 ⇒ ⇒ 上游永远收不到反压。
 *
 * ⚠️⚠️ **三种「驱动不了」MUST 响亮拒绝**（`CHFATAL_EXCEPTION`，⇒ **抛**）：
 *   ① **无节点**（调用方没 `as_master()`）⇒ ⇒ `<<=` 会另建节点并**只绑本地副本**
 *      ⇒ ⇒ 调用方看不到任何反压；
 *   ② **目标本身是 literal**（调用方用 `operator=` 喂了常量）⇒ ⇒ literal
 *      的值不随 src 变化 ⇒ ⇒ 驱动**无效**；
 *   ③ **已被真信号驱动**（不是「默认常量」）⇒ ⇒ 再驱动是 `set_src(0, …)`
 *      = **last-wins** ⇒ ⇒ 静默接管别人的信号。
 *   ⚠️ 判据 ③ **允许**覆盖「当前驱动是 literal」的情况 —— 那是**上一级 pipe /
 *      组合子**的**默认值**（ADR-054 形态 E）⇒ ⇒ 链式 pipe
 *      （`s2m(...).output` 再进 `m2s`）正是靠这条成立的。
 */
template <typename T, typename D>
inline void drive_upstream_ready(ch_stream<T> &upstream, const D &driver,
                                 const char *what) {
    // ⚠️ **判据只有这一处**（B7）：`ch::core::diagnose_drive` 是框架级的
    //    **只读诊断** ⇒ ⇒ 本 helper 与 `stream.h` 的 `drive_master_ready`
    //    **共用**同一判定 ⇒ ⇒ 两份实现不会漂移
    //    ⇒ ⇒ **MUST NOT** 在此重新实现（护栏 R11 会检出）。
    auto *node = upstream.ready.impl();
    switch (ch::core::diagnose_drive(node)) {
    case ch::core::drive_state::no_node:
        CHFATAL_EXCEPTION(
            "[%s] 上游 ready 字段**没有节点**：`<<=` 在空目标上会另建节点并"
            "只绑到本地副本 ⇒ ⇒ 调用方收不到反压（静默失效）。"
            "修法：MUST 先对 input_stream 调 as_master()。", what);
        break;
    case ch::core::drive_state::literal:
        CHFATAL_EXCEPTION(
            "[%s] 上游 ready 字段是**字面量**节点 ⇒ ⇒ literal 的值不随 src "
            "变化 ⇒ ⇒ 驱动无效（反压静默丢失）。"
            "修法：MUST NOT 用 operator= 给 input_stream.ready 喂常量。", what);
        break;
    case ch::core::drive_state::driven:
        CHFATAL_EXCEPTION(
            "[%s] 上游 ready 字段**已被真信号驱动** ⇒ ⇒ 再驱动是 last-wins "
            "⇒ ⇒ 会静默接管调用方的信号。"
            "修法：input_stream.ready 留给 pipe 驱动。", what);
        break;
    default:
        // `undriven`（可驱动）/ `driven_by_literal`（= 上一级的默认值 ⇒ 可覆盖）
        break;
    }
    upstream.ready <<= driver;
}

} // namespace detail

/**
 * Stream M2S Pipe Result - Master-to-Slave pipeline result structure
 * Creates a 1-cycle delay pipeline for valid and payload signals
 */
template <typename T> struct StreamM2SPipeResult {
    ch_stream<T> input;
    ch_stream<T> output;
};

/**
 * stream_m2s_pipe - Create a master-to-slave pipeline with 1-cycle delay
 *
 * Similar to SpinalHDL's m2sPipe() - adds a register stage between master and slave
 * Both valid and payload are delayed by 1 cycle when input.fire() occurs
 *
 * ⚠️ `result.output.ready` = **消费端**要驱动的字段（有默认值 ⇒ 不驱动 = 恒就绪）；
 *    `result.input.ready` = **组合**跟随 `output.ready`（⇒ `input_stream.ready`）。
 */
template <typename T>
StreamM2SPipeResult<T> stream_m2s_pipe(ch_stream<T> input_stream) {
    StreamM2SPipeResult<T> result;

    ch_reg<T> payload_reg{};
    ch_reg<ch_bool> valid_reg{};

    // ① **输出侧** ready ⇒ 带默认驱动、**可被消费端覆盖**（ADR-054 形态 E）
    //    ⚠️ 源 MUST 是**命名局部**（MUST NOT 用临时对象：实测会建不出有效源）
    ch_bool default_ready(true);
    result.output.ready <<= default_ready;

    // ② **上游侧** ready = 「输出寄存器有空位」（**组合**）⇒ ⇒ 由 pipe 驱动
    //    ⚠️ MUST 含 `!valid_reg`（MUST NOT 只写 `output.ready`）：否则寄存器
    //    里有未被消费的数据时上游会被**无谓**挡住（吞吐掉一半）。
    ch_bool out_free = !valid_reg | result.output.ready;
    result.input.ready = out_free;
    detail::drive_upstream_ready(input_stream, result.input.ready,
                                  "stream_m2s_pipe");

    // ③ 数据通路：**保持**未被消费的数据（`valid_reg & !output.ready`）
    //    ⚠️ 修复前 `valid_reg->next = input_fire` ⇒ 下游一停顿就**丢数据**
    //    （而 `input_fire` 又依赖一个无驱动的 `output.ready` ⇒ ⇒ 该路径
    //    从未被真实使用过）。
    ch_bool accept = out_free & input_stream.valid;

    payload_reg->next = ch::core::select(accept, input_stream.payload,
                                        payload_reg);
    valid_reg->next = accept | (valid_reg & !result.output.ready);

    result.output.valid = valid_reg;
    result.output.payload = payload_reg;

    result.input.valid = input_stream.valid;
    result.input.payload = input_stream.payload;

    return result;
}

/**
 * Stream S2M Pipe Result - Slave-to-Master pipeline result structure
 * Creates a 0-cycle delay pipeline for payload, 1-cycle delay for ready signal
 */
template <typename T> struct StreamS2MPipeResult {
    ch_stream<T> input;
    ch_stream<T> output;
};

/**
 * stream_s2m_pipe - Create a slave-to-master pipeline with 0-cycle payload delay
 *
 * Similar to SpinalHDL's s2mPipe() - adds a register stage for ready signal only
 * Payload passes through immediately (0-cycle delay), ready is delayed by 1 cycle
 *
 * ⚠️ **1 拍延迟的是「下游的 ready」**（`input.ready = RegNext(output.ready)`）
 *    —— 修复前寄存的是 `input_stream.ready`（**上游自己的** ready）
 *    ⇒ ⇒ 方向反了的自环 ⇒ ⇒ 语义无意义。
 */
template <typename T>
StreamS2MPipeResult<T> stream_s2m_pipe(ch_stream<T> input_stream) {
    StreamS2MPipeResult<T> result;

    ch_reg<ch_bool> ready_reg{};

    // ① 输出侧 ready（可被消费端覆盖）
    ch_bool default_ready(true);
    result.output.ready <<= default_ready;

    // ② 上游侧 ready = **寄存**后的下游 ready（1 拍）
    ready_reg->next = result.output.ready;
    result.input.ready = ready_reg;
    detail::drive_upstream_ready(input_stream, result.input.ready,
                                  "stream_s2m_pipe");

    // ③ 数据通路：payload / valid **组合**通过（0 拍）
    result.output.payload = input_stream.payload;
    result.output.valid = input_stream.valid;

    result.input.valid = input_stream.valid;
    result.input.payload = input_stream.payload;

    return result;
}

/**
 * Stream Half Pipe Result - Half-bandwidth pipeline result structure
 * All signals are registered, bandwidth is halved
 */
template <typename T> struct StreamHalfPipeResult {
    ch_stream<T> input;
    ch_stream<T> output;
};

/**
 * stream_half_pipe - Create a half-bandwidth pipeline with all signals registered
 *
 * Similar to SpinalHDL's halfPipe() - all signals (valid, payload, ready) are registered
 * Bandwidth is halved by using a toggle mechanism that only accepts data every other cycle
 *
 * ⚠️ `input.ready` = **寄存**后的 `output.ready`（与 `s2m` 同型：下游 ready
 *    延迟 1 拍）；修复前它寄存的是上游自己的 ready ⇒ ⇒ 自环。
 */
template <typename T>
StreamHalfPipeResult<T> stream_half_pipe(ch_stream<T> input_stream) {
    StreamHalfPipeResult<T> result;

    ch_reg<T> payload_reg{};
    ch_reg<ch_bool> valid_reg{};
    ch_reg<ch_bool> toggle_reg{};

    // ① 输出侧 ready（可被消费端覆盖）
    ch_bool default_ready(true);
    result.output.ready <<= default_ready;

    // ② 上游侧 ready = **隔拍**（toggle 自由运行）**且**输出寄存器有空位
    //    ⚠️ 「半带宽」= **输入侧隔拍就绪**（上游自己保持数据）
    //    ⇒ ⇒ **MUST NOT** 是「隔拍丢弃」（修复前 `valid_reg->next = accept_data`
    //    ⇒ ⇒ 未被消费的数据下一拍就**消失** ⇒ ⇒ 下游一停顿就丢数据）
    ch_bool out_free = !valid_reg | result.output.ready;
    ch_bool accept = toggle_reg & out_free;
    result.input.ready = accept;
    detail::drive_upstream_ready(input_stream, result.input.ready,
                                  "stream_half_pipe");

    // ③ 数据通路：隔拍接收 + **保持**未被消费的数据
    ch_bool take = accept & input_stream.valid;

    toggle_reg->next = !toggle_reg; // 自由运行 ⇒ 隔拍（MUST NOT 由传输驱动
                                    // ⇒ 否则「无数据」时 toggle 卡住 ⇒ 死锁）
    payload_reg->next = select(take, input_stream.payload, payload_reg);
    valid_reg->next = take | (valid_reg & !result.output.ready);

    result.output.valid = valid_reg;
    result.output.payload = payload_reg;

    result.input.valid = input_stream.valid;
    result.input.payload = input_stream.payload;

    return result;
}

} // namespace chlib
