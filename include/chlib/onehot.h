#ifndef CHLIB_ONEHOT_H
#define CHLIB_ONEHOT_H

#include "ch.hpp"
#include "component.h"
#include "core/bool.h"
#include "core/literal.h"
#include "core/operators.h"
#include "core/operators_runtime.h"
#include "core/uint.h"
#include <cassert>

using namespace ch::core;

namespace chlib {

/**
 * OneHotDec - 解码one-hot编码输入
 *
 * 该模块接收N位宽的one-hot编码输入，并输出对应的索引值。
 * 例如：对于4位输入，如果输入是0b0100，则输出为2（从0开始计数）
 * 如果没有位被设置或者设置了多个位，则输出未定义
 */
template <unsigned N> class onehot_dec {
public:
    static_assert(N > 0, "OneHotDecoder must have at least 1 bit");
    static constexpr unsigned OUTPUT_WIDTH =
        (N > 1) ? compute_bit_width(N - 1) : 1;

    ch_uint<OUTPUT_WIDTH> operator(ch_uint<N> in) {
        if constexpr (N == 1) {
            return ch_uint<OUTPUT_WIDTH>(0_d);
        } else {
            ch_uint<OUTPUT_WIDTH> result = 0_d;

            for (unsigned i = 0; i < N; ++i) {
                ch_bool bit_at_i = bit_select(io().in, i);
                ch_uint<OUTPUT_WIDTH> current_value = make_literal(i);
                result = select(bit_at_i, current_value, result);
            }

            return result;
        }
    }
};

} // namespace chlib

#endif // CHLIB_ONEHOT_DECODER_H