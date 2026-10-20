/**
 * AXI4-Lite 字节道（byte lane）写掩码工具
 *
 * 语义（登记于 `include/axi4/AGENTS.md` CONVENTIONS → 字节道语义）：
 * 1. `WSTRB` 位 i ↔ 字节道 i（bit0 → byte0 … bit3 → byte3），每道 8 bit
 * 2. 写路径：`new = (old & ~mask) | (wdata & mask)`，`mask` 由 `WSTRB` 逐字节展开
 * 3. `mask` 再与目标寄存器的**有效字节道**相交：宽度 W 的寄存器有效道数为
 *    `ceil(W / 8)`（W=8 → 仅 byte0；W=16 → byte0..1；W=32 → byte0..3）。
 *    落在无效道上的写被忽略（寄存器值不变，不报错）
 * 4. 结果按寄存器宽度 W 截断
 * 5. `WSTRB == 0` → 无字节被选中 → 寄存器保持原值（地址已解码，响应仍 `OKAY`）
 * 6. 只读寄存器不在写路径内（写其地址 → `DECERR`，值不变）
 */

#pragma once

#include "ch.hpp"

#include <cstdint>

namespace axi4 {
namespace detail {

/**
 * 把 4-bit `WSTRB` 展开为 32-bit 字节道掩码。
 *
 * bit0 → 0x0000_00FF（byte0）
 * bit1 → 0x0000_FF00（byte1）
 * bit2 → 0x00FF_0000（byte2）
 * bit3 → 0xFF00_0000（byte3）
 */
inline ch::core::ch_uint<32> byte_lane_mask(ch::core::ch_uint<4> wstrb) {
    using ch::core::ch_uint;
    using ch::core::select;

    auto lane0 = (wstrb & ch_uint<4>(1_d)) != ch_uint<4>(0_d);
    auto lane1 = (wstrb & ch_uint<4>(2_d)) != ch_uint<4>(0_d);
    auto lane2 = (wstrb & ch_uint<4>(4_d)) != ch_uint<4>(0_d);
    auto lane3 = (wstrb & ch_uint<4>(8_d)) != ch_uint<4>(0_d);

    const ch_uint<32> mask0(0x000000FFu);
    const ch_uint<32> mask1(0x0000FF00u);
    const ch_uint<32> mask2(0x00FF0000u);
    const ch_uint<32> mask3(0xFF000000u);

    ch_uint<32> m0 = select(lane0, mask0, ch_uint<32>(0_d));
    ch_uint<32> m1 = select(lane1, mask1, ch_uint<32>(0_d));
    ch_uint<32> m2 = select(lane2, mask2, ch_uint<32>(0_d));
    ch_uint<32> m3 = select(lane3, mask3, ch_uint<32>(0_d));

    return m0 | m1 | m2 | m3;
}

/**
 * 宽度 W 寄存器的有效字节道掩码。
 *
 * W=32 → 0xFFFF_FFFF；W=16 → 0x0000_FFFF；W=8 → 0x0000_00FF。
 */
template <unsigned W>
inline ch::core::ch_uint<32> byte_lane_valid_mask() {
    constexpr unsigned lane_bits = (W >= 32u) ? 32u : (((W + 7u) / 8u) * 8u);
    constexpr std::uint32_t value =
        (lane_bits >= 32u) ? 0xFFFFFFFFu : ((1u << lane_bits) - 1u);
    return ch::core::ch_uint<32>(value);
}

/**
 * 字节写的新值：`new = (old & ~mask) | (wdata & mask)`。
 *
 * @tparam W 目标寄存器宽度（决定有效字节道；结果按该宽度截断）
 */
template <unsigned W>
inline ch::core::ch_uint<32> byte_lane_write(ch::core::ch_uint<32> old_value,
                                              ch::core::ch_uint<32> wdata,
                                              ch::core::ch_uint<4> wstrb) {
    const auto mask = byte_lane_mask(wstrb) & byte_lane_valid_mask<W>();
    return (old_value & ~mask) | (wdata & mask);
}

/**
 * 同上，但掩码由调用方预先展开（一个 peripheral 只展开一次，多个寄存器共用）。
 *
 * @tparam W 目标寄存器宽度（决定有效字节道；结果按该宽度截断）
 */
template <unsigned W>
inline ch::core::ch_uint<32> byte_lane_apply(ch::core::ch_uint<32> old_value,
                                               ch::core::ch_uint<32> wdata,
                                               ch::core::ch_uint<32> mask) {
    const auto m = mask & byte_lane_valid_mask<W>();
    return (old_value & ~m) | (wdata & m);
}

} // namespace detail
} // namespace axi4
