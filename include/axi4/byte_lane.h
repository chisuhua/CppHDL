/**
 * AXI 字节道（byte lane）写掩码工具
 *
 * 语义（登记于 `include/axi4/AGENTS.md` CONVENTIONS → 字节道语义）：
 * 1. `WSTRB` 位 i ↔ 字节道 i（bit0 → byte0 …），每道 8 bit；
 *    `WSTRB` 宽度 = `DATA_WIDTH / 8`，即一条总线有 `DATA_WIDTH / 8` 条字节道
 * 2. 写路径：`new = (old & ~mask) | (wdata & mask)`，`mask` 由 `WSTRB` 逐字节展开
 * 3. `mask` 再与目标寄存器的**有效字节道**相交：宽度 W 的寄存器有效道数为
 *    `ceil(W / 8)`（W=8 → 仅 byte0；W=16 → byte0..1；W=32 → byte0..3）。
 *    落在无效道上的写被忽略（寄存器值不变，不报错）
 * 4. 结果按寄存器宽度 W 截断
 * 5. `WSTRB == 0` → 无字节被选中 → 寄存器保持原值（地址已解码，响应仍 `OKAY`）
 * 6. 只读寄存器不在写路径内（写其地址 → `DECERR`，值不变）
 *
 * 总线宽度由模板参数 `DATA_WIDTH` 决定（8 / 16 / 32 / 64），
 * 32 bit 路径与既有 6 个 AXI4-Lite peripheral 的调用完全兼容。
 */

#pragma once

#include "ch.hpp"

#include <cstdint>
#include <type_traits>
#include <utility>

namespace axi4 {
namespace detail {

/**
 * 把 `DATA_WIDTH / 8` bit 的 `WSTRB` 展开为 `DATA_WIDTH` bit 字节道掩码。
 *
 * bit i → `0xFF << (8 * i)`（`i` 为编译期常量，不走运行期移位 ——
 * `ch_uint::operator<<` 按 RHS 的**类型宽度**推算结果宽度，运行期移位量会把
 * 结果宽度算错）。
 *
 * @tparam DATA_WIDTH 总线位宽，MUST 为 8 的倍数且 ≤ 64
 */
template <unsigned DATA_WIDTH>
inline ch::core::ch_uint<DATA_WIDTH>
byte_lane_mask(ch::core::ch_uint<DATA_WIDTH / 8> wstrb) {
    static_assert(DATA_WIDTH % 8u == 0u, "DATA_WIDTH MUST be a multiple of 8");
    static_assert(DATA_WIDTH >= 8u && DATA_WIDTH <= 64u,
                  "byte_lane_mask supports DATA_WIDTH in [8, 64]");

    using ch::core::ch_uint;
    using ch::core::select;

    constexpr unsigned LANES = DATA_WIDTH / 8u;

    ch_uint<DATA_WIDTH> mask(0_d);
    [&]<std::size_t... I>(std::index_sequence<I...>) {
        ((mask = mask |
                  select((wstrb & ch_uint<LANES>(std::uint64_t{1} << I)) !=
                             ch_uint<LANES>(0_d),
                         ch_uint<DATA_WIDTH>(std::uint64_t{0xFF} << (8u * I)),
                         ch_uint<DATA_WIDTH>(0_d))),
         ...);
    }(std::make_index_sequence<LANES>{});

    return mask;
}

/**
 * 宽度 W 寄存器的有效字节道掩码（在 `BUS_WIDTH` 的字内）。
 *
 * W=32 → 0xFFFF_FFFF；W=16 → 0x0000_FFFF；W=8 → 0x0000_00FF。
 *
 * @tparam W         目标寄存器宽度
 * @tparam BUS_WIDTH 所在总线的位宽（决定掩码宽度，默认 32）
 */
template <unsigned W, unsigned BUS_WIDTH = 32>
inline ch::core::ch_uint<BUS_WIDTH> byte_lane_valid_mask() {
    static_assert(BUS_WIDTH % 8u == 0u && BUS_WIDTH >= 8u && BUS_WIDTH <= 64u,
                  "BUS_WIDTH MUST be a multiple of 8 in [8, 64]");
    static_assert(W >= 1u && W <= BUS_WIDTH,
                  "register width W MUST be in [1, BUS_WIDTH]");

    constexpr unsigned lane_bits =
        (W >= BUS_WIDTH) ? BUS_WIDTH : (((W + 7u) / 8u) * 8u);
    constexpr std::uint64_t value =
        (lane_bits >= 64u) ? ~std::uint64_t{0}
                           : ((std::uint64_t{1} << lane_bits) - 1u);
    return ch::core::ch_uint<BUS_WIDTH>(value);
}

/**
 * 字节写的新值：`new = (old & ~mask) | (wdata & mask)`。
 *
 * `BUS_WIDTH` 刻意放在**非推导上下文**（`std::type_identity_t`）：
 * 实参常是 `ch_reg` / `ch_in` 端口类型，模板实参推导不会做用户定义转换，
 * 若允许推导会让既有 32 bit 调用点全部失配。默认取 `max(W, 32)`，
 * 因此 `byte_lane_apply<DATA_WIDTH>` 对 32 bit 与 64 bit 总线都直接可用。
 *
 * @tparam W         目标寄存器宽度（决定有效字节道；结果按该宽度截断）
 * @tparam BUS_WIDTH 总线位宽（默认 `max(W, 32)`；非 32/64 位总线需显式给出）
 */
template <unsigned W, unsigned BUS_WIDTH = ((W > 32u) ? W : 32u)>
inline ch::core::ch_uint<BUS_WIDTH>
byte_lane_apply(std::type_identity_t<ch::core::ch_uint<BUS_WIDTH>> old_value,
                std::type_identity_t<ch::core::ch_uint<BUS_WIDTH>> wdata,
                std::type_identity_t<ch::core::ch_uint<BUS_WIDTH>> mask) {
    const auto m = mask & byte_lane_valid_mask<W, BUS_WIDTH>();
    return (old_value & ~m) | (wdata & m);
}

} // namespace detail
} // namespace axi4
