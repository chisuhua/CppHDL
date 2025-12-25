#ifndef CHLIB_ONEHOT_DECODER_H
#define CHLIB_ONEHOT_DECODER_H

#include "ch.hpp"
#include "component.h"
#include "core/bool.h"
#include "core/literal.h"
#include "core/operators.h"
#include "core/uint.h"
#include <cassert>

using namespace ch::core;

namespace chlib {

/**
 * OneHotDecoder - 解码one-hot编码输入
 *
 * 该模块接收N位宽的one-hot编码输入，并输出对应的索引值。
 * 例如：对于4位输入，如果输入是0b0100，则输出为2（从0开始计数）
 * 如果没有位被设置或者设置了多个位，则输出未定义
 */
template <unsigned N> class onehot_decoder : public ch::Component {
public:
    static_assert(N > 0, "OneHotDecoder must have at least 1 bit");

    static constexpr unsigned OUTPUT_WIDTH =
        (N > 1) ? compute_bit_width(N - 1) : 1;

    __io(ch_in<ch_uint<N>> in;              // N位 one-hot 输入
         ch_out<ch_uint<OUTPUT_WIDTH>> out; // 解码后的输出值
         )

        /**
         * 构造函数
         * @param parent 父组件
         * @param name 组件名称
         */
        onehot_decoder(ch::Component *parent = nullptr,
                       const std::string &name = "onehot_decoder")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        if constexpr (N == 1) {
            io().out = ch_uint<OUTPUT_WIDTH>(0_d);
        } else {
            // 使用运行期for循环，每次迭代创建新的节点
            ch_uint<OUTPUT_WIDTH> result = 0_d;

            // 循环遍历每一位
            for (unsigned i = 0; i < N; ++i) {
                // 创建一个运行时位选择操作
                ch_bool bit_at_i = runtime_bit_select(io().in, i);

                // 创建当前索引的字面量值
                ch_uint<OUTPUT_WIDTH> current_value = make_literal(i);

                // 使用select操作创建新的result节点
                // result = select(bit_at_i, make_literal(i), result);
                // result = select(bit_at_i, current_value, result);
                result = select(bit_at_i, i, result);
            }

            io().out = result;
        }
    }

private:
    // 运行时位选择函数
    ch_bool runtime_bit_select(const ch_in<ch_uint<N>> &input,
                               unsigned index) const {
        // 使用一个switch来处理不同的索引值，生成相应的位选择操作
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
        // 可以根据需要继续添加更多case
        default:
            // 对于超出范围的索引，返回第0位的值（作为fallback）
            return bit_select<0>(input);
        }
    }
};

} // namespace chlib

#endif // CHLIB_ONEHOT_DECODER_H