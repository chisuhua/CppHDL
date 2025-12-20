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
            io().out = 0_d;
        } else {
            ch_uint<OUTPUT_WIDTH> result = 0_d;

            // C++20: 使用模板 Lambda + 折叠表达式实现编译期展开循环
            [&]<std::size_t... Is>(std::index_sequence<Is...>) {
                ((result = select(bit_select<Is>(io().in),
                                  ch_literal<Is, OUTPUT_WIDTH>{}, result)),
                 ...);
            }(std::make_index_sequence<N>{});

            io().out = result;
        }
    }
};

} // namespace chlib

#endif // CHLIB_ONEHOT_DECODER_H