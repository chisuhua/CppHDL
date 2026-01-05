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
 * onehot_dec - 函数式 one-hot 解码器
 *
 * 该函数接收N位宽的one-hot编码输入，并输出对应的索引值。
 * 例如：对于4位输入，如果输入是0b0100，则输出为2（从0开始计数）
 * 如果没有位被设置或者设置了多个位，则输出未定义
 */
template <unsigned N> class onehot_dec {
public:
    static_assert(N > 0, "OneHotDecoder must have at least 1 bit");
    static constexpr unsigned OUTPUT_WIDTH = compute_idx_width(N);

    ch_uint<OUTPUT_WIDTH> operator()(ch_uint<N> in) {
        if constexpr (N == 1) {
            return ch_uint<OUTPUT_WIDTH>(0_d);
        } else {
            ch_uint<OUTPUT_WIDTH> result = 0_d;

            for (unsigned i = 0; i < N; ++i) {
                ch_bool bit_at_i = bit_select(in, i);
                ch_uint<OUTPUT_WIDTH> current_value =
                    make_uint<OUTPUT_WIDTH>(i);
                result = select(bit_at_i, current_value, result);
            }

            return result;
        }
    }
};

/**
 * onehot_dec_module - 解码one-hot编码输入的模块
 *
 * 该模块接收N位宽的one-hot编码输入，并输出对应的索引值。
 * 例如：对于4位输入，如果输入是0b0100，则输出为2（从0开始计数）
 * 如果没有位被设置或者设置了多个位，则输出未定义
 */
template <unsigned N> class onehot_dec_module : public ch::Component {
public:
    static_assert(N > 0, "OneHotDecoder must have at least 1 bit");

    static constexpr unsigned OUTPUT_WIDTH = compute_idx_width(N);

    __io(ch_in<ch_uint<N>> in;              // N位 one-hot 输入
         ch_out<ch_uint<OUTPUT_WIDTH>> out; // 解码后的输出值
         )

        /**
         * 构造函数
         * @param parent 父组件
         * @param name 组件名称
         */
        onehot_dec_module(ch::Component *parent = nullptr,
                          const std::string &name = "onehot_dec_module")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        if constexpr (N == 1) {
            io().out = ch_uint<OUTPUT_WIDTH>(0_d);
        } else {
            ch_uint<OUTPUT_WIDTH> result = 0_d;

            // 循环遍历每一位
            for (unsigned i = 0; i < N; ++i) {
                // 创建一个位选择操作
                ch_bool bit_at_i = bit_select(io().in, i);

                // 创建当前索引的字面量值
                ch_uint<OUTPUT_WIDTH> current_value =
                    make_uint<OUTPUT_WIDTH>(i);

                // 使用select操作创建新的result节点
                result = select(bit_at_i, current_value, result);
            }

            io().out = result;
        }
    }
};

/**
 * onehot_enc - 函数式 one-hot 编码器
 *
 * 将索引值编码为 one-hot 格式
 */
template <unsigned N> class onehot_enc {
public:
    static_assert(N > 0, "OneHotEncoder must have at least 1 bit");
    static constexpr unsigned INPUT_WIDTH = compute_idx_width(N);

    ch_uint<N> operator()(ch_uint<INPUT_WIDTH> idx) {
        ch_uint<N> result = 0_d;

        for (unsigned i = 0; i < N; ++i) {
            ch_bool idx_matches = (idx == make_uint<INPUT_WIDTH>(i));
            ch_uint<N> one_hot_val = make_literal<1, N>()
                                     << make_uint<INPUT_WIDTH>(i);
            result = select(idx_matches, one_hot_val, result);
        }

        return result;
    }
};

/**
 * onehot_enc_module - 编码器模块
 *
 * 将索引值编码为 one-hot 格式
 */
template <unsigned N> class onehot_enc_module : public ch::Component {
public:
    static_assert(N > 0, "OneHotEncoder must have at least 1 bit");
    static constexpr unsigned INPUT_WIDTH = compute_idx_width(N);

    __io(ch_in<ch_uint<INPUT_WIDTH>> in; // 输入索引
         ch_out<ch_uint<N>> out;         // one-hot 输出
         )

        onehot_enc_module(ch::Component *parent = nullptr,
                          const std::string &name = "onehot_enc_module")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        ch_uint<N> result = 0_d;

        for (unsigned i = 0; i < N; ++i) {
            ch_bool idx_matches = (ch_uint<INPUT_WIDTH>(io().in.impl()) ==
                                   make_uint<INPUT_WIDTH>(i));
            ch_uint<N> one_hot_val = make_literal<1, N>()
                                     << make_uint<INPUT_WIDTH>(i);
            result = select(idx_matches, one_hot_val, result);
        }

        io().out = result;
    }
};

} // namespace chlib

#endif // CHLIB_ONEHOT_H