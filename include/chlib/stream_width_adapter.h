#pragma once

/**
 * @file stream_width_adapter.h
 * @brief Stream Width Adapters for CppHDL
 *
 * Provides SpinalHDL-like stream width conversion:
 * - Narrow to Wide: Accumulate multiple narrow transfers into one wide transfer
 * - Wide to Narrow: Split one wide transfer into multiple narrow transfers
 *
 * ⚠️⚠️ **接线纪律（ADR-064 D2）**：组合子要驱动上游的 `ready`，
 *   而 `ch_stream` 的 `ready` 字段**默认没有节点** ⇒ ⇒ **调用方 MUST 先
 *   `as_master()`**（或 `as_slave()`，取决于方向），否则组合子的驱动**无处可写**
 *   ⇒ ⇒ 上游收不到反压（静默失效）。
 *
 * ⚠️ **输出形态（ADR-064 D3）**：`valid` / `payload` 都是**寄存器**（+1 拍延迟）。
 *   理由不是实现偏好 —— 而是**组合输出在本仓测试读模型下不可观测**：
 *   `set_input → tick() → get_port_value()` 读到的是**沿后**组合值，而组合输出的
 *   正确值出现在**沿前** ⇒ 两者错位一拍 ⇒ 写不出有效的 parity 判据。
 *
 * Usage Example:
 * @code{.cpp}
 * ch_stream<ch_uint<8>> narrow;
 * narrow.as_master();                    // ⚠️ MUST：否则 ready 无节点
 * auto wide = stream_narrow_to_wide<ch_uint<32>, ch_uint<8>>(narrow);
 * @endcode
 */

#include "chlib/stream.h"
#include "ch.hpp"
#include "core/reg.h"
#include "core/uint.h"
#include "core/bool.h"
#include "core/operators.h"
#include <cstdint>

namespace chlib {

using namespace ch::core;

/**
 * @brief Compute number of narrow transfers needed for one wide transfer
 */
template <typename TWide, typename TNarrow>
constexpr unsigned compute_transfer_ratio() {
    constexpr size_t wide_bits = TWide::ch_width;
    constexpr size_t narrow_bits = TNarrow::ch_width;
    return (wide_bits + narrow_bits - 1) / narrow_bits;  // Ceiling division
}

/**
 * @brief Narrow to Wide Stream Adapter
 *
 * 累积 RATIO 个窄字为一个宽字（第 k 个窄字落在 `[k*NB, (k+1)*NB)`）。
 *
 * 契约（ADR-064 D4）：
 * - 宽字 MUST **包含最后一个窄字**；
 * - 一组完成 MUST **清零**累积器（否则跨组污染）；
 * - 输入 `valid=0` 的拍 MUST NOT 推进计数器 / 改变累积器；
 * - 满时（等最后一个字）`input.ready` MUST 反映输出寄存器能否被腾空。
 *
 * @tparam TWide Wide output type (e.g., ch_uint<32>)
 * @tparam TNarrow Narrow input type (e.g., ch_uint<8>)
 * @param input Narrow input stream（调用方 MUST `as_master()`）
 * @return Wide output stream
 */
template <typename TWide, typename TNarrow>
ch_stream<TWide> stream_narrow_to_wide(ch_stream<TNarrow> input) {
    static_assert(TWide::ch_width > TNarrow::ch_width,
                  "stream_narrow_to_wide requires TWide wider than TNarrow");
    constexpr unsigned RATIO = compute_transfer_ratio<TWide, TNarrow>();
    constexpr unsigned NARROW_BITS = TNarrow::ch_width;

    ch_stream<TWide> result;

    // ⚠️ MUST 先建默认驱动，再读 `result.ready`（ADR-054 形态 E / ADR-064 D2）
    ch_bool default_ready(true);
    result.ready <<= default_ready;

    // ── 状态 ──────────────────────────────────────────────────────────
    ch_reg<TWide>       accumulator(TWide(0_d));
    ch_reg<ch_uint<8>>  count(ch_uint<8>(0_d));
    ch_reg<ch_bool>     out_valid(ch_bool(false));
    ch_reg<TWide>       out_payload(TWide(0_d));

    ch_bool is_last    = (count == ch_uint<8>(RATIO - 1));
    ch_uint<8> shift   = count * ch_uint<8>(NARROW_BITS);
    TWide merged       = accumulator | (TWide(input.payload) << shift);

    ch_bool accept     = input.valid & input.ready;
    ch_bool out_free   = !out_valid | result.ready;   // 输出寄存器能否被腾空
    ch_bool out_fire   = out_valid & result.ready;

    // ⚠️ 最后一个字被接受 ⇒ 清零（否则下一组 OR 上旧数据 ⇒ 跨组污染）
    accumulator->next = select(accept, select(is_last, TWide(0_d), merged), accumulator);
    count->next = select(accept,
                         select(is_last, ch_uint<8>(0_d), count + ch_uint<8>(1_d)),
                         count);

    // ⚠️ 输出寄存器装载的是 `merged`（**含当前窄字**）
    out_payload->next = select(accept & is_last, merged, out_payload);
    out_valid->next   = select(accept & is_last, ch_bool(true),
                               select(out_fire, ch_bool(false), out_valid));

    result.payload = out_payload;
    result.valid   = out_valid;

    // ⚠️ 统一用 `out_free` 门控：输出寄存器已满且下游未取 ⇒ **停止接受**
    //    （否则下一组填满后再装载会**覆盖未取走的数据**）
    input.ready <<= out_free;

    return result;
}

/**
 * @brief Wide to Narrow Stream Adapter
 *
 * 把一个宽字拆成 RATIO 个窄字（第 k 个窄字 = `data >> k*NB`）。
 *
 * 契约（ADR-064 D5）：数据通路沿用既有设计（已核对正确），只修 `ready` 所有权。
 *
 * @tparam TNarrow Narrow output type (e.g., ch_uint<8>)
 * @tparam TWide Wide input type (e.g., ch_uint<32>)
 * @param input Wide input stream（调用方 MUST `as_master()`）
 * @return Narrow output stream
 */
template <typename TNarrow, typename TWide>
ch_stream<TNarrow> stream_wide_to_narrow(ch_stream<TWide> input) {
    static_assert(TWide::ch_width > TNarrow::ch_width,
                  "stream_wide_to_narrow requires TWide wider than TNarrow");
    constexpr unsigned RATIO = compute_transfer_ratio<TWide, TNarrow>();
    constexpr unsigned NARROW_BITS = TNarrow::ch_width;

    ch_stream<TNarrow> result;

    ch_bool default_ready(true);
    result.ready <<= default_ready;

    ch_reg<TWide>      data_reg(TWide(0_d));
    ch_reg<ch_uint<8>> count(ch_uint<8>(0_d));
    ch_reg<ch_bool>    out_valid(ch_bool(false));

    ch_bool is_last = (count == ch_uint<8>(RATIO - 1));
    ch_uint<8> shift = count * ch_uint<8>(NARROW_BITS);

    // ⚠️ `input.ready` MUST **先**定义、**再**参与 `accept`（否则读的是空节点 ⇒ 崩溃）
    ch_bool input_ready = !out_valid;          // 输出寄存器空 ⇒ 可接收新宽字
    ch_bool accept      = input.valid & input_ready;
    ch_bool out_fire    = out_valid & result.ready;

    data_reg->next  = select(accept, input.payload, data_reg);
    count->next     = select(accept, ch_uint<8>(0_d),
                             select(out_fire,
                                    select(is_last, ch_uint<8>(0_d),
                                           count + ch_uint<8>(1_d)),
                                    count));
    out_valid->next = select(accept, ch_bool(true),
                             select(out_fire & is_last, ch_bool(false), out_valid));

    // `data_reg` / `count` 都是寄存器 ⇒ 该切片在 `tick()` 后可观测
    result.payload = TNarrow(data_reg >> shift);
    result.valid   = out_valid;

    input.ready <<= input_ready;

    return result;
}

/**
 * @brief Generic Stream Width Adapter
 *
 * Automatically chooses direction based on input/output types.
 *
 * @tparam TOut Output type
 * @tparam TIn Input type
 * @param input Input stream（调用方 MUST `as_master()`）
 * @return Output stream with different width
 */
template <typename TOut, typename TIn>
ch_stream<TOut> stream_width_adapter(ch_stream<TIn> input) {
    if constexpr (TOut::ch_width > TIn::ch_width) {
        return stream_narrow_to_wide<TOut, TIn>(input);
    } else if constexpr (TOut::ch_width < TIn::ch_width) {
        return stream_wide_to_narrow<TOut, TIn>(input);
    } else {
        // 同宽直通：零状态 ⇒ 形态 E（默认驱动 + 可覆盖）
        ch_stream<TOut> result;
        ch_bool default_ready(true);
        result.ready <<= default_ready;
        result.payload = TOut(input.payload);
        result.valid   = input.valid;
        input.ready <<= result.ready;
        return result;
    }
}

// ⚠️ `stream_extend` / `stream_truncate` 已于 2026-11-03 **物理删除**
//    （ADR-064 D7）：零消费者 + 零测试 + 静默失效（`input.ready = result.ready`
//    是**节点重绑定** ⇒ 上游收不到反压）⇒ 按 Zero-Debt 删除而非留坑。
//    ⚠️ 它们的「单拍零扩展 / 截断」语义若将来需要，MUST 走**独立立项**
//    （并由本文件的形态 E 纪律驱动 ready），MUST NOT 复活旧实现。

} // namespace chlib
