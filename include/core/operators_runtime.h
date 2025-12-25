#ifndef CH_CORE_OPERATORS_RUNTIME_H
#define CH_CORE_OPERATORS_RUNTIME_H

#include "operators.h"
#include "uint.h"
#include <cstddef>

namespace ch::core {

/**
 * 运行时版本的bit_select函数
 * 根据运行时索引选择输入位
 *
 * @param input 输入的ch_uint<N>值
 * @param index 要选择的位索引
 * @return 选中的位，类型为ch_bool
 */
template <unsigned N>
ch_bool bit_select(const ch_uint<N> &input, unsigned index) {
    switch (index) {
    case 0:
        return bit_select<0>(input);
    case 1:
        return bit_select<1>(input);
    case 2:
        return bit_select<2>(input);
    case 3:
        return bit_select<3>(input);
    case 4:
        return bit_select<4>(input);
    case 5:
        return bit_select<5>(input);
    case 6:
        return bit_select<6>(input);
    case 7:
        return bit_select<7>(input);
    case 8:
        return bit_select<8>(input);
    case 9:
        return bit_select<9>(input);
    case 10:
        return bit_select<10>(input);
    case 11:
        return bit_select<11>(input);
    case 12:
        return bit_select<12>(input);
    case 13:
        return bit_select<13>(input);
    case 14:
        return bit_select<14>(input);
    case 15:
        return bit_select<15>(input);
    case 16:
        return bit_select<16>(input);
    case 17:
        return bit_select<17>(input);
    case 18:
        return bit_select<18>(input);
    case 19:
        return bit_select<19>(input);
    case 20:
        return bit_select<20>(input);
    case 21:
        return bit_select<21>(input);
    case 22:
        return bit_select<22>(input);
    case 23:
        return bit_select<23>(input);
    case 24:
        return bit_select<24>(input);
    case 25:
        return bit_select<25>(input);
    case 26:
        return bit_select<26>(input);
    case 27:
        return bit_select<27>(input);
    case 28:
        return bit_select<28>(input);
    case 29:
        return bit_select<29>(input);
    case 30:
        return bit_select<30>(input);
    case 31:
        return bit_select<31>(input);
    case 32:
        return bit_select<32>(input);
    case 33:
        return bit_select<33>(input);
    case 34:
        return bit_select<34>(input);
    case 35:
        return bit_select<35>(input);
    case 36:
        return bit_select<36>(input);
    case 37:
        return bit_select<37>(input);
    case 38:
        return bit_select<38>(input);
    case 39:
        return bit_select<39>(input);
    case 40:
        return bit_select<40>(input);
    case 41:
        return bit_select<41>(input);
    case 42:
        return bit_select<42>(input);
    case 43:
        return bit_select<43>(input);
    case 44:
        return bit_select<44>(input);
    case 45:
        return bit_select<45>(input);
    case 46:
        return bit_select<46>(input);
    case 47:
        return bit_select<47>(input);
    case 48:
        return bit_select<48>(input);
    case 49:
        return bit_select<49>(input);
    case 50:
        return bit_select<50>(input);
    case 51:
        return bit_select<51>(input);
    case 52:
        return bit_select<52>(input);
    case 53:
        return bit_select<53>(input);
    case 54:
        return bit_select<54>(input);
    case 55:
        return bit_select<55>(input);
    case 56:
        return bit_select<56>(input);
    case 57:
        return bit_select<57>(input);
    case 58:
        return bit_select<58>(input);
    case 59:
        return bit_select<59>(input);
    case 60:
        return bit_select<60>(input);
    case 61:
        return bit_select<61>(input);
    case 62:
        return bit_select<62>(input);
    case 63:
        return bit_select<63>(input);
    default:
        // 对于超出范围的索引，返回false（相当于访问不存在的位）
        return ch_bool(false);
    }
}

/**
 * 运行时版本的bit_select函数，用于ch_in<ch_uint<N>>类型
 * 根据运行时索引选择输入位
 *
 * @param input 输入的ch_in<ch_uint<N>>端口
 * @param index 要选择的位索引
 * @return 选中的位，类型为ch_bool
 */
template <unsigned N>
ch_bool bit_select(const ch_in<ch_uint<N>> &input, unsigned index) {
    return bit_select(ch_uint<N>(input.impl()), index);
}

} // namespace ch::core

#endif // CH_CORE_OPERATORS_RUNTIME_H