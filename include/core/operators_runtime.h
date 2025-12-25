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

/**
 * 运行时版本的bits函数
 * 根据运行时参数提取输入位范围
 *
 * @param input 输入的ch_uint<N>值
 * @param msb 最高位索引
 * @param lsb 最低位索引
 * @return 提取的位范围，类型为ch_uint<WIDTH>，其中WIDTH = msb - lsb + 1
 */

// 定义宏来生成bits函数的case分支
// 正确的计算：要提取width位，从lsb开始，则msb = lsb + width - 1
#define CASE_WIDTH_N(WIDTH)                                                    \
    case WIDTH:                                                                \
        switch (lsb) {                                                         \
        case 0:                                                                \
            return bits<WIDTH - 1, 0>(input);                                  \
        case 1:                                                                \
            return bits<WIDTH, 1>(input);                                      \
        case 2:                                                                \
            return bits<WIDTH + 1, 2>(input);                                  \
        case 3:                                                                \
            return bits<WIDTH + 2, 3>(input);                                  \
        case 4:                                                                \
            return bits<WIDTH + 3, 4>(input);                                  \
        case 5:                                                                \
            return bits<WIDTH + 4, 5>(input);                                  \
        case 6:                                                                \
            return bits<WIDTH + 5, 6>(input);                                  \
        case 7:                                                                \
            return bits<WIDTH + 6, 7>(input);                                  \
        case 8:                                                                \
            return bits<WIDTH + 7, 8>(input);                                  \
        case 9:                                                                \
            return bits<WIDTH + 8, 9>(input);                                  \
        case 10:                                                               \
            return bits<WIDTH + 9, 10>(input);                                 \
        case 11:                                                               \
            return bits<WIDTH + 10, 11>(input);                                \
        case 12:                                                               \
            return bits<WIDTH + 11, 12>(input);                                \
        case 13:                                                               \
            return bits<WIDTH + 12, 13>(input);                                \
        case 14:                                                               \
            return bits<WIDTH + 13, 14>(input);                                \
        case 15:                                                               \
            return bits<WIDTH + 14, 15>(input);                                \
        case 16:                                                               \
            return bits<WIDTH + 15, 16>(input);                                \
        case 17:                                                               \
            return bits<WIDTH + 16, 17>(input);                                \
        case 18:                                                               \
            return bits<WIDTH + 17, 18>(input);                                \
        case 19:                                                               \
            return bits<WIDTH + 18, 19>(input);                                \
        case 20:                                                               \
            return bits<WIDTH + 19, 20>(input);                                \
        case 21:                                                               \
            return bits<WIDTH + 20, 21>(input);                                \
        case 22:                                                               \
            return bits<WIDTH + 21, 22>(input);                                \
        case 23:                                                               \
            return bits<WIDTH + 22, 23>(input);                                \
        case 24:                                                               \
            return bits<WIDTH + 23, 24>(input);                                \
        case 25:                                                               \
            return bits<WIDTH + 24, 25>(input);                                \
        case 26:                                                               \
            return bits<WIDTH + 25, 26>(input);                                \
        case 27:                                                               \
            return bits<WIDTH + 26, 27>(input);                                \
        case 28:                                                               \
            return bits<WIDTH + 27, 28>(input);                                \
        case 29:                                                               \
            return bits<WIDTH + 28, 29>(input);                                \
        case 30:                                                               \
            return bits<WIDTH + 29, 30>(input);                                \
        case 31:                                                               \
            return bits<WIDTH + 30, 31>(input);                                \
        case 32:                                                               \
            return bits<WIDTH + 31, 32>(input);                                \
        case 33:                                                               \
            return bits<WIDTH + 32, 33>(input);                                \
        case 34:                                                               \
            return bits<WIDTH + 33, 34>(input);                                \
        case 35:                                                               \
            return bits<WIDTH + 34, 35>(input);                                \
        case 36:                                                               \
            return bits<WIDTH + 35, 36>(input);                                \
        case 37:                                                               \
            return bits<WIDTH + 36, 37>(input);                                \
        case 38:                                                               \
            return bits<WIDTH + 37, 38>(input);                                \
        case 39:                                                               \
            return bits<WIDTH + 38, 39>(input);                                \
        case 40:                                                               \
            return bits<WIDTH + 39, 40>(input);                                \
        case 41:                                                               \
            return bits<WIDTH + 40, 41>(input);                                \
        case 42:                                                               \
            return bits<WIDTH + 41, 42>(input);                                \
        case 43:                                                               \
            return bits<WIDTH + 42, 43>(input);                                \
        case 44:                                                               \
            return bits<WIDTH + 43, 44>(input);                                \
        case 45:                                                               \
            return bits<WIDTH + 44, 45>(input);                                \
        case 46:                                                               \
            return bits<WIDTH + 45, 46>(input);                                \
        case 47:                                                               \
            return bits<WIDTH + 46, 47>(input);                                \
        case 48:                                                               \
            return bits<WIDTH + 47, 48>(input);                                \
        case 49:                                                               \
            return bits<WIDTH + 48, 49>(input);                                \
        case 50:                                                               \
            return bits<WIDTH + 49, 50>(input);                                \
        case 51:                                                               \
            return bits<WIDTH + 50, 51>(input);                                \
        case 52:                                                               \
            return bits<WIDTH + 51, 52>(input);                                \
        case 53:                                                               \
            return bits<WIDTH + 52, 53>(input);                                \
        case 54:                                                               \
            return bits<WIDTH + 53, 54>(input);                                \
        case 55:                                                               \
            return bits<WIDTH + 54, 55>(input);                                \
        case 56:                                                               \
            return bits<WIDTH + 55, 56>(input);                                \
        case 57:                                                               \
            return bits<WIDTH + 56, 57>(input);                                \
        case 58:                                                               \
            return bits<WIDTH + 57, 58>(input);                                \
        case 59:                                                               \
            return bits<WIDTH + 58, 59>(input);                                \
        case 60:                                                               \
            return bits<WIDTH + 59, 60>(input);                                \
        case 61:                                                               \
            return bits<WIDTH + 60, 61>(input);                                \
        case 62:                                                               \
            return bits<WIDTH + 61, 62>(input);                                \
        case 63:                                                               \
            return bits<WIDTH + 62, 63>(input);                                \
        default:                                                               \
            return bits<WIDTH - 1, 0>(input);                                  \
        }

template <unsigned N>
auto bits(const ch_uint<N> &input, unsigned msb, unsigned lsb) {
    // 计算输出宽度
    constexpr unsigned output_width = N; // 实际输出宽度将由具体实现确定

    // 使用嵌套switch语句实现运行时参数到编译时模板参数的映射
    // 为简化，这里仅展示部分常用范围，可根据需要扩展
    if (msb >= lsb && msb < N) {
        unsigned width = msb - lsb + 1;

        // 处理常用宽度，使用switch语句分发到对应的模板实例
        switch (width) {
            CASE_WIDTH_N(1)
            CASE_WIDTH_N(2)
            CASE_WIDTH_N(3)
            CASE_WIDTH_N(4)
            CASE_WIDTH_N(5)
            CASE_WIDTH_N(6)
            CASE_WIDTH_N(7)
            CASE_WIDTH_N(8)
            CASE_WIDTH_N(9)
            CASE_WIDTH_N(10)
            CASE_WIDTH_N(11)
            CASE_WIDTH_N(12)
            CASE_WIDTH_N(13)
            CASE_WIDTH_N(14)
            CASE_WIDTH_N(15)
            CASE_WIDTH_N(16)
            CASE_WIDTH_N(17)
            CASE_WIDTH_N(18)
            CASE_WIDTH_N(19)
            CASE_WIDTH_N(20)
            CASE_WIDTH_N(21)
            CASE_WIDTH_N(22)
            CASE_WIDTH_N(23)
            CASE_WIDTH_N(24)
            CASE_WIDTH_N(25)
            CASE_WIDTH_N(26)
            CASE_WIDTH_N(27)
            CASE_WIDTH_N(28)
            CASE_WIDTH_N(29)
            CASE_WIDTH_N(30)
            CASE_WIDTH_N(31)
            CASE_WIDTH_N(32)
            CASE_WIDTH_N(33)
            CASE_WIDTH_N(34)
            CASE_WIDTH_N(35)
            CASE_WIDTH_N(36)
            CASE_WIDTH_N(37)
            CASE_WIDTH_N(38)
            CASE_WIDTH_N(39)
            CASE_WIDTH_N(40)
            CASE_WIDTH_N(41)
            CASE_WIDTH_N(42)
            CASE_WIDTH_N(43)
            CASE_WIDTH_N(44)
            CASE_WIDTH_N(45)
            CASE_WIDTH_N(46)
            CASE_WIDTH_N(47)
            CASE_WIDTH_N(48)
            CASE_WIDTH_N(49)
            CASE_WIDTH_N(50)
            CASE_WIDTH_N(51)
            CASE_WIDTH_N(52)
            CASE_WIDTH_N(53)
            CASE_WIDTH_N(54)
            CASE_WIDTH_N(55)
            CASE_WIDTH_N(56)
            CASE_WIDTH_N(57)
            CASE_WIDTH_N(58)
            CASE_WIDTH_N(59)
            CASE_WIDTH_N(60)
            CASE_WIDTH_N(61)
            CASE_WIDTH_N(62)
            CASE_WIDTH_N(63)
        // 可以根据需要继续添加更多宽度的情况
        default:
            // 默认返回0宽度的值（作为fallback）
            return ch_uint<1>(0_d);
        }
    } else {
        // 参数无效时返回默认值
        return ch_uint<1>(0_d);
    }
}
template <unsigned N>
ch_bool bits(const ch_in<ch_uint<N>> &input, unsigned msb, unsigned lsb) {
    return bitt(ch_uint<N>(input.impl()), msb, lsb);
}

} // namespace ch::core

#endif // CH_CORE_OPERATORS_RUNTIME_H