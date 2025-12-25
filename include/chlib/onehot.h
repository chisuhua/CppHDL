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
                ch_bool bit_at_i = runtime_bit_select(io().in, i);
                
                // 创建当前索引的字面量值
                ch_uint<OUTPUT_WIDTH> current_value = runtime_make_literal(i);
                
                // 使用select操作创建新的result节点
                result = select(bit_at_i, current_value, result);
            }
            
            io().out = result;
        }
    }

private:
    // 运行时创建字面量的函数 - 使用编译期模板make_literal
    ch_uint<OUTPUT_WIDTH> runtime_make_literal(unsigned value) const {
        // 根据value的值创建对应的字面量，使用switch语句调用编译期模板make_literal
        switch (value) {
            case 0: return ch_uint<OUTPUT_WIDTH>(make_literal<0, OUTPUT_WIDTH>());
            case 1: return ch_uint<OUTPUT_WIDTH>(make_literal<1, OUTPUT_WIDTH>());
            case 2: return ch_uint<OUTPUT_WIDTH>(make_literal<2, OUTPUT_WIDTH>());
            case 3: return ch_uint<OUTPUT_WIDTH>(make_literal<3, OUTPUT_WIDTH>());
            case 4: return ch_uint<OUTPUT_WIDTH>(make_literal<4, OUTPUT_WIDTH>());
            case 5: return ch_uint<OUTPUT_WIDTH>(make_literal<5, OUTPUT_WIDTH>());
            case 6: return ch_uint<OUTPUT_WIDTH>(make_literal<6, OUTPUT_WIDTH>());
            case 7: return ch_uint<OUTPUT_WIDTH>(make_literal<7, OUTPUT_WIDTH>());
            case 8: return ch_uint<OUTPUT_WIDTH>(make_literal<8, OUTPUT_WIDTH>());
            case 9: return ch_uint<OUTPUT_WIDTH>(make_literal<9, OUTPUT_WIDTH>());
            case 10: return ch_uint<OUTPUT_WIDTH>(make_literal<10, OUTPUT_WIDTH>());
            case 11: return ch_uint<OUTPUT_WIDTH>(make_literal<11, OUTPUT_WIDTH>());
            case 12: return ch_uint<OUTPUT_WIDTH>(make_literal<12, OUTPUT_WIDTH>());
            case 13: return ch_uint<OUTPUT_WIDTH>(make_literal<13, OUTPUT_WIDTH>());
            case 14: return ch_uint<OUTPUT_WIDTH>(make_literal<14, OUTPUT_WIDTH>());
            case 15: return ch_uint<OUTPUT_WIDTH>(make_literal<15, OUTPUT_WIDTH>());
            case 16: return ch_uint<OUTPUT_WIDTH>(make_literal<16, OUTPUT_WIDTH>());
            case 17: return ch_uint<OUTPUT_WIDTH>(make_literal<17, OUTPUT_WIDTH>());
            case 18: return ch_uint<OUTPUT_WIDTH>(make_literal<18, OUTPUT_WIDTH>());
            case 19: return ch_uint<OUTPUT_WIDTH>(make_literal<19, OUTPUT_WIDTH>());
            case 20: return ch_uint<OUTPUT_WIDTH>(make_literal<20, OUTPUT_WIDTH>());
            case 21: return ch_uint<OUTPUT_WIDTH>(make_literal<21, OUTPUT_WIDTH>());
            case 22: return ch_uint<OUTPUT_WIDTH>(make_literal<22, OUTPUT_WIDTH>());
            case 23: return ch_uint<OUTPUT_WIDTH>(make_literal<23, OUTPUT_WIDTH>());
            case 24: return ch_uint<OUTPUT_WIDTH>(make_literal<24, OUTPUT_WIDTH>());
            case 25: return ch_uint<OUTPUT_WIDTH>(make_literal<25, OUTPUT_WIDTH>());
            case 26: return ch_uint<OUTPUT_WIDTH>(make_literal<26, OUTPUT_WIDTH>());
            case 27: return ch_uint<OUTPUT_WIDTH>(make_literal<27, OUTPUT_WIDTH>());
            case 28: return ch_uint<OUTPUT_WIDTH>(make_literal<28, OUTPUT_WIDTH>());
            case 29: return ch_uint<OUTPUT_WIDTH>(make_literal<29, OUTPUT_WIDTH>());
            case 30: return ch_uint<OUTPUT_WIDTH>(make_literal<30, OUTPUT_WIDTH>());
            case 31: return ch_uint<OUTPUT_WIDTH>(make_literal<31, OUTPUT_WIDTH>());
            // 可以根据需要继续添加更多case
            default:
                // 对于超出范围的值，返回0（作为fallback）
                return ch_uint<OUTPUT_WIDTH>(0_d);
        }
    }
};

} // namespace chlib

#endif // CHLIB_ONEHOT_DECODER_H