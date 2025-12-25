#ifndef CHLIB_ONEHOT_DECODER_H
#define CHLIB_ONEHOT_DECODER_H

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
                ch_bool bit_at_i = bit_select(io().in, i);

                // 创建当前索引的字面量值
                ch_uint<OUTPUT_WIDTH> current_value = make_literal(i);

                // 使用select操作创建新的result节点
                result = select(bit_at_i, current_value, result);
            }

            io().out = result;
        }
    }
};

} // namespace chlib

#endif // CHLIB_ONEHOT_DECODER_H