#pragma once

/**
 * @file stream_builder.h
 * @brief Fluent API builder for ch_stream operations
 *
 * Provides a chainable interface for stream operations like:
 * - .queue<depth>()    - Add a FIFO buffer (compile-time depth)
 * - .haltWhen(cond)    - Pause stream when condition is true
 * - .map(func)         - Transform payload
 * - .throwWhen(cond)  - Drop data when condition is true
 * - .takeWhen(cond)   - Only pass data when condition is true
 * - .continueWhen(cond) - Continue stream when condition is true
 *
 * Usage:
 *   src.as_master();                     // ⚠️ 必须先建字段节点（见下）
 *   src.payload <<= payload;
 *   src.valid   <<= valid;
 *   auto built = StreamBuilder<ch_uint<8>>(src)
 *       .queue<4>()
 *       .haltWhen(busy)
 *       .map([](const ch_uint<8>& v) { return v + 1; })
 *       .build();
 *   built.ready <<= consumer_ready;      // 消费端施加反压
 *
 * ---------------------------------------------------------------------------
 * ⭐ **链内闭包不变式**（ADR-055 · capability `chlib-stream-ready-ownership`）
 * ---------------------------------------------------------------------------
 * 链上每一级的**读使能**（= 本级的「输出 ready」字段）MUST 由**下一级的容量
 * 信号**驱动；**链尾**由**消费端 ready** 驱动。
 *
 * 具体到本文件：
 *   · `queue<DEPTH>()` 建出的 FIFO，其 **容量信号** = `push_stream.ready`
 *     （= `!full`）⇒ ⇒ builder MUST 用它驱动**上一级**的输出 ready；
 *   · 过滤类（`haltWhen` / `continueWhen` / `throwWhen` / `takeWhen` / `map`）
 *     **自己**驱动入参 ready（`input_stream.ready <<= result.ready | cond`）
 *     ⇒ ⇒ builder **MUST NOT** 再动它（否则会**抹掉** `| cond` 那一项）；
 *   · 因此 builder 只需要记住「**下一级要驱动**的那个节点」
 *     = 当前级的**输出 ready** 字段（`pending_ready_`）。
 *
 * ⚠️ **为什么这条不变式必须有**（实测，2026-11-01）：
 *   `stream_fifo` **不驱动**入参 `ready`（写使能只用 `valid`，内部
 *   `sync_fifo` 自保护）⇒ ⇒ 未接回接时 builder 链里**没人驱动** `source.ready`
 *   ⇒ ⇒ 它**恒为 0** ⇒ ⇒ **上游遵守 ready 协议 ⇒ ⇒ 永远推不动 = 死路**
 *   ⇒ ⇒ 「FIFO 满 ⇒ 上游暂停」在 builder API 下**不可演示**。
 *
 * ⚠️ **接线纪律（调用方义务）**：`source` MUST 先 `as_master()`（⇒ 字段
 *   **有节点**）再构造 builder，且 `payload` / `valid` MUST 用 `<<=` 接数据流
 *   边；**MUST NOT** 用 `operator=` 给 `source.ready` 喂常量 —— 那会把该字段
 *   重绑定到 **literal 节点**，而 literal **不可被驱动** ⇒ ⇒ 反压**静默丢失**
 *   ⇒ ⇒ builder 会**响亮拒绝**（`CHFATAL_EXCEPTION`，见 `drive_pending_ready`）。
 */

#include "bundle/stream_bundle.h"
#include "chlib/stream.h"
#include "core/lnodeimpl.h"
#include "utils/logger.h"
#include <functional>

namespace chlib {

/**
 * @brief Fluent API builder for stream operations
 *
 * @tparam T The payload type of the stream
 */
template <typename T>
class StreamBuilder {
public:
    /**
     * @brief Construct a builder from a source stream
     * @param source The input stream to build upon
     *
     * ⚠️ `source` MUST 已经 `as_master()`（⇒ `ready` 字段**有节点**）
     *    ⇒ ⇒ 否则第一次 `queue<>()` 会 `CHFATAL_EXCEPTION`（响亮拒绝）。
     */
    explicit StreamBuilder(ch_stream<T> source)
        : current_stream_(source), pending_ready_(source.ready) {}

    /**
     * @brief Add a FIFO queue with specified depth
     * @tparam DEPTH The compile-time depth of the FIFO
     * @return Reference to this builder for chaining
     *
     * Creates a FIFO buffer that provides backpressure when full.
     * Similar to SpinalHDL's .queue() method.
     *
     * Note: DEPTH must be a compile-time constant.
     *
     * ⚠️ **链内闭包不变式**（ADR-055）：本级的容量信号（`push_stream.ready`
     *    = `!full`）MUST 驱动**上一级**的输出 ready ⇒ ⇒ 否则上游收不到反压
     *    （根因与实测见文件头注释）。
     */
    template <unsigned DEPTH>
    StreamBuilder<T>& queue() {
        // ⚠️ `stream_ready_ownership: intentionally-unwired` —— builder **不需要**
        //    `stream_fifo_connect_ready`：本文件**自己**实现链内闭包不变式
        //    （中间级的读使能 ← 下一级的容量信号；链尾由消费端驱动，见 ADR-055）
        //    ⇒ ⇒ 不调 `connect_ready` 是**正确**形态，不是遗漏。
        auto result = stream_fifo<T, DEPTH>(current_stream_);

        // ① **上游反压**：本级的容量信号 ⇒ ⇒ 驱动上一级的输出 ready
        drive_pending_ready(result.push_stream.ready);

        // ② 本级成为链上的一级 ⇒ ⇒ 它的输出 ready 留给**下一级**驱动
        adopt_stage_output(result.pop_stream);
        return *this;
    }

    /**
     * @brief Halt the stream when condition is true
     * @param halt The condition that causes the stream to pause
     * @return Reference to this builder for chaining
     *
     * When halt is true, valid signal is de-asserted but data is preserved.
     * Similar to SpinalHDL's .haltWhen() method.
     */
    StreamBuilder<T>& haltWhen(ch_bool halt) {
        adopt_stage_output(stream_halt_when(current_stream_, halt));
        return *this;
    }

    /**
     * @brief Continue stream flow when condition is true
     * @param continue_cond The condition that allows stream to flow
     * @return Reference to this builder for chaining
     *
     * Convenience method - equivalent to haltWhen(!continue_cond).
     * Similar to SpinalHDL's .continueWhen() method.
     */
    StreamBuilder<T>& continueWhen(ch_bool continue_cond) {
        adopt_stage_output(stream_continue_when(current_stream_, continue_cond));
        return *this;
    }

    /**
     * @brief Map/transform the stream payload
     * @tparam Func A callable type that transforms T to T
     * @param transform A callable that transforms T to T
     * @return Reference to this builder for chaining
     *
     * Applies a transformation function to each payload.
     * Similar to SpinalHDL's .map() method.
     *
     * Note: The transform must return the same type T.
     */
    template <typename Func>
    StreamBuilder<T>& map(Func transform) {
        adopt_stage_output(
            stream_translate_with<T, T>(current_stream_, transform));
        return *this;
    }

    /**
     * @brief Throw (drop) data when condition is true
     * @param condition The condition that causes data to be dropped
     * @return Reference to this builder for chaining
     *
     * When condition is true, data is consumed but not passed through.
     * Similar to SpinalHDL's .throwWhen() method.
     */
    StreamBuilder<T>& throwWhen(ch_bool condition) {
        adopt_stage_output(stream_throw_when(current_stream_, condition));
        return *this;
    }

    /**
     * @brief Take (pass) data only when condition is true
     * @param condition The condition that allows data to pass
     * @return Reference to this builder for chaining
     *
     * Only passes data when condition is true, otherwise holds back.
     * Similar to SpinalHDL's .takeWhen() method.
     */
    StreamBuilder<T>& takeWhen(ch_bool condition) {
        adopt_stage_output(stream_take_when(current_stream_, condition));
        return *this;
    }

    /**
     * @brief Build the final stream from the builder chain
     * @return The final ch_stream<T> after all operations
     *
     * ⚠️ 返回值的 `ready` 字段 = **链尾**的输出 ready ⇒ ⇒ 消费端 MUST 用
     *    `built.ready <<= consumer_ready` 施加反压（⇒ ⇒ 不接 = 恒就绪，
     *    即 ADR-054 形态 E 的「未覆盖时保持旧语义」）。
     */
    ch_stream<T> build() {
        return current_stream_;
    }

private:
    /**
     * @brief 把**本级**的输出流登记为「链上的一级」。
     *
     * ⚠️ 只保存「**下一级要驱动**」的那个节点（= 输出流的 `ready` 字段），
     *    **不**保存各级的 result 对象 —— 每加一级上一级就被驱动掉
     *    （⇒ ⇒ 不随链长线性增长，且语义等价：见 ADR-055 §2.3）。
     *
     * ⚠️ `ch_bool` 的**拷贝共享同一节点** ⇒ ⇒ `pending_ready_` 与
     *    `stage_output.ready` 是**同一个**节点 ⇒ ⇒ 覆盖**必然传播**。
     */
    void adopt_stage_output(const ch_stream<T> &stage_output) {
        pending_ready_ = stage_output.ready;
        pending_is_source_ = false;
        current_stream_ = stage_output;
    }

    /**
     * @brief 用 `capacity` 驱动 `pending_ready_`（= 上一级的输出 ready）。
     *
     * ⚠️ **MUST 用 `<<=`**（数据流边）；**MUST NOT** `operator=`（它只重绑定
     *    指针 ⇒ ⇒ 上一级的驱动被**孤儿化** ⇒ ⇒ 反压静默丢失）。
     *
     * ⚠⚠ **三种「驱动不了」的形态 MUST 响亮拒绝**（`CHFATAL_EXCEPTION`）：
     *   ① **无节点**（`source` 未 `as_master()`）⇒ ⇒ `<<=` 会**另建**一个
     *      assign 节点并**只绑到本地副本** ⇒ ⇒ 调用方**看不到**任何反压
     *      ⇒ ⇒ **静默失效**；
     *   ② **literal 节点**（调用方用 `operator=` 喂了常量）⇒ ⇒ literal
     *      **不可被 `set_src`** ⇒ ⇒ 同样**静默失效**；
     *   ③ **链首且已被驱动**（调用方自己驱动了 `source.ready`）⇒ ⇒ 再驱动是
     *      `set_src(0, …)` = **last-wins** ⇒ ⇒ **静默接管**调用方的信号。
     *    ⇒ ⇒ ⚠️ `CHERROR` / `CHFATAL` 只打日志**不抛**（ADR-050 已踩过）
     *    ⇒ ⇒ ⇒ **MUST** 用 `CHFATAL_EXCEPTION`（抛 `std::runtime_error`）。
     *
     * ⚠️ 多驱动判据**只对链首**（调用方的 `source.ready`）生效：非链首的
     *    `pending_ready_` 是**上一级组合子自己**的默认驱动（如 `stream_fifo`
     *    的 `default_ready_source` / 过滤类的 `default_ready`）⇒ ⇒ 覆盖它是
     *    **设计意图**（ADR-054 形态 E「默认值 + 可覆盖」）⇒ ⇒ MUST NOT 报错。
     */
    void drive_pending_ready(const ch_bool &capacity) {
        auto *node = pending_ready_.impl();
        if (!node) {
            CHFATAL_EXCEPTION(
                "[StreamBuilder] 链首的 ready 字段**没有节点** "
                "(如 source.ready)：`<<=` 在空目标上会另建节点并只绑到本地"
                "副本 ⇒ ⇒ 调用方收不到反压（静默失效）。"
                "修法：构造 builder **之前**调 source.as_master()。");
        }
        if (node->type() == lnodetype::type_lit) {
            CHFATAL_EXCEPTION(
                "[StreamBuilder] 链首的 ready 字段是**字面量**节点 "
                "⇒ ⇒ literal 不可被驱动 ⇒ ⇒ 反压会静默丢失。"
                "修法：MUST NOT 用 operator= 给 source.ready 喂常量；"
                "用 as_master() 建字段节点，数据用 <<= 接。");
        }
        if (pending_is_source_ && node->num_srcs() > 0) {
            CHFATAL_EXCEPTION(
                "[StreamBuilder] 链首的 ready 字段**已被驱动** "
                "⇒ ⇒ 再驱动是 last-wins ⇒ ⇒ 会静默接管调用方的信号。"
                "修法：source.ready 留给 builder 驱动（MUST NOT 自行 <<=）。");
        }
        pending_ready_ <<= capacity;
    }

    /// 当前级的**输出**流（链尾 = `build()` 的返回值）
    ch_stream<T> current_stream_;

    /// 「**下一级要驱动**」的节点 = 当前级的**输出 ready** 字段
    ch_bool pending_ready_;

    /// `pending_ready_` 是否仍是**调用方的** `source.ready`（⇒ 需要多驱动守卫）
    bool pending_is_source_ = true;
};

/**
 * @brief Helper function to create a StreamBuilder
 * @tparam T The payload type
 * @param source The source stream
 * @return StreamBuilder<T> instance
 *
 * Usage:
 *   auto result = make_stream_builder(source).queue<4>().map(func).build();
 */
template <typename T>
StreamBuilder<T> make_stream_builder(ch_stream<T> source) {
    return StreamBuilder<T>(source);
}

} // namespace chlib
