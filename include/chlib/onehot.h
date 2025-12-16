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
    // 确保至少有一个位
    static_assert(N > 0, "OneHotDecoder must have at least 1 bit");

    // 输出宽度应该足够容纳N个可能的值 (0 到 N-1)
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

    /**
     * 创建IO端口
     */
    void create_ports() override { new (io_storage_) io_type; }

    /**
     * 描述组合逻辑
     */
    void describe() override {

        // 使用条件运算符链来解码one-hot编码
        ch_uint<OUTPUT_WIDTH> result = 0_d;

        // 对每一位检查是否设置，如果是则返回对应的位置值
        if constexpr (N == 1) {
            // 特殊情况：只有1位
            result = 0_d;
        } else {
            // 多位情况：逐位检查
            for (unsigned i = 0; i < N; i++) {
                // 使用条件选择操作符，如果第i位被设置，则选择i，否则保持原值
                result = select(bit_select<ch_uint<N>, i>(io().in),
                                ch_uint<OUTPUT_WIDTH>(i), result);
            }
        }

        io().out = result;
    }
};

} // namespace chlib

#endif // CHLIB_ONEHOT_DECODER_H