#include "catch_amalgamated.hpp"
#include "ch.hpp"
#include "codegen_dag.h"
#include "codegen_verilog.h"
#include "component.h"
#include "module.h"
#include "simulator.h"
#include <bitset>
#include <cstdint>
#include <iostream>

using namespace ch;
using namespace ch::core;

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

// 测试OneHotDecoder模块
template <unsigned N> class OneHotDecoderTestTop : public Component {
public:
    // 计算输出宽度
    static constexpr unsigned OUTPUT_WIDTH = (N > 1) ? compute_bit_width(N - 1) : 1;

    __io(ch_in<ch_uint<N>> in;                        // one-hot输入
         ch_out<ch_uint<OUTPUT_WIDTH>> decoded_value; // 解码后的值
         ch_out<ch_bool> valid;                       // 输入是否有效（恰好一个位被设置）
         )

    OneHotDecoderTestTop(Component *parent = nullptr,
                         const std::string &name = "onehot_decoder_test_top")
        : Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        // 实例化OneHotDecoder模块
        CH_MODULE(onehot_decoder<N>, decoder);

        // 连接输入
        decoder.io().in <<= io().in;

        // 连接输出
        io().decoded_value <<= decoder.io().out;

        // 验证输入确实是有效的one-hot编码（只有一个位被设置）
        io().valid = (popcount(io().in) == 1_d);
    }
};

TEST_CASE("OneHotDecoder - Basic Functionality", "[onehot][decoder][basic]") {
    SECTION("4-bit onehot decoder") {
        ch_device<OneHotDecoderTestTop<4>> device;
        Simulator simulator(device.context());

        // 测试所有有效的one-hot值
        for (int i = 0; i < 4; i++) {
            uint64_t input = 1 << i;
            simulator.set_input_value(device.instance().io().in, input);
            simulator.tick();

            auto decoded_value = simulator.get_value(device.instance().io().decoded_value);
            auto valid = simulator.get_value(device.instance().io().valid);

            // 验证结果
            REQUIRE(valid.is_value(1));
            REQUIRE(decoded_value.is_value(i));
        }
    }

    SECTION("1-bit onehot decoder") {
        ch_device<OneHotDecoderTestTop<1>> device;
        Simulator simulator(device.context());

        // 对于1位输入，输出应始终为0
        simulator.set_input_value(device.instance().io().in, 1);
        simulator.tick();

        auto decoded_value = simulator.get_value(device.instance().io().decoded_value);
        REQUIRE(decoded_value.is_value(0));
    }

    SECTION("2-bit onehot decoder") {
        ch_device<OneHotDecoderTestTop<2>> device;
        Simulator simulator(device.context());

        // 测试2位输入的有效值
        for (int i = 0; i < 2; i++) {
            uint64_t input = 1 << i;
            simulator.set_input_value(device.instance().io().in, input);
            simulator.tick();

            auto decoded_value = simulator.get_value(device.instance().io().decoded_value);
            auto valid = simulator.get_value(device.instance().io().valid);

            REQUIRE(valid.is_value(1));
            REQUIRE(decoded_value.is_value(i));
        }
    }
}

TEST_CASE("OneHotDecoder - Invalid Inputs", "[onehot][decoder][invalid]") {
    SECTION("Testing invalid inputs for 4-bit decoder") {
        ch_device<OneHotDecoderTestTop<4>> device;
        Simulator simulator(device.context());

        // 测试无效输入（全0）
        simulator.set_input_value(device.instance().io().in, 0);
        simulator.tick();

        auto valid = simulator.get_value(device.instance().io().valid);
        REQUIRE(!valid.is_value(1));  // 无效输入时valid应为false

        // 测试无效输入（多个位设置）
        simulator.set_input_value(device.instance().io().in, 0b0101);
        simulator.tick();

        valid = simulator.get_value(device.instance().io().valid);
        REQUIRE(!valid.is_value(1));  // 无效输入时valid应为false
    }
}

TEST_CASE("OneHotDecoder - Code Generation", "[onehot][decoder][codegen]") {
    SECTION("4-bit decoder code generation") {
        ch_device<OneHotDecoderTestTop<4>> device;

        // 测试Verilog代码生成
        REQUIRE_NOTHROW(toVerilog("test_onehot_decoder.v", device.context()));

        // 测试DAG图生成
        REQUIRE_NOTHROW(toDAG("test_onehot_decoder.dot", device.context()));
    }

    SECTION("8-bit decoder code generation") {
        ch_device<OneHotDecoderTestTop<8>> device;

        // 测试Verilog代码生成
        REQUIRE_NOTHROW(toVerilog("test_onehot_decoder_8bit.v", device.context()));
    }
}

TEST_CASE("OneHotDecoder - Component Hierarchy", "[onehot][decoder][hierarchy]") {
    ch_device<OneHotDecoderTestTop<4>> device;
    auto& top = device.instance();
    
    // 验证组件层次结构
    REQUIRE(top.child_count() == 1);  // 应该有一个子组件，即onehot_decoder
    
    // 获取子组件并验证其类型
    const auto& children = top.children();
    REQUIRE(!children.empty());
    auto child = children[0];
    REQUIRE(child != nullptr);
}