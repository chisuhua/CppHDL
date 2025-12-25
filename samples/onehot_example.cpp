#include "ch.hpp"
#include "chlib/onehot_module.h"
#include "codegen_dag.h"
#include "codegen_verilog.h"
#include "component.h"
#include "module.h"
#include "simulator.h"
#include <bitset>
#include <cstdint>
#include <iostream>
#include <memory>

using namespace ch;
using namespace ch::core;

/**
 * 示例演示如何使用OneHotDecoder模块
 * 该模块将N位宽的one-hot编码输入转换为对应的整数值输出
 */

template <unsigned N> class OneHotDecoderTop : public Component {
public:
    // 计算输出宽度
    static constexpr unsigned OUTPUT_WIDTH =
        (N > 1) ? compute_bit_width(N - 1) : 1;

    __io(ch_in<ch_uint<N>> in;                        // one-hot输入
         ch_out<ch_uint<OUTPUT_WIDTH>> decoded_value; // 解码后的值
         ch_out<ch_bool> valid; // 输入是否有效（恰好一个位被设置）
         )

        OneHotDecoderTop(Component *parent = nullptr,
                         const std::string &name = "onehot_decoder_top")
        : Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        // 实例化OneHotDecoder模块
        CH_MODULE(chlib::onehot_decoder<N>, decoder);

        // 连接输入
        decoder.io().in <<= io().in;

        // 连接输出
        io().decoded_value <<= decoder.io().out;

        // 验证输入确实是有效的one-hot编码（只有一个位被设置）
        io().valid = (popcount(io().in) == 1_d);
    }
};

int main() {
    std::cout << "=== OneHotDecoder Example ===" << std::endl;

    try {
        // 创建设备和仿真器
        ch_device<OneHotDecoderTop<4>> device;
        Simulator simulator(device.context());

        std::cout << "Testing OneHotDecoder with 4-bit input:" << std::endl;

        // 测试所有有效的one-hot值
        for (int i = 0; i < 4; i++) {
            // ch_uint<4> input = ch_uint<4>(1) << i;
            uint64_t input = 1 << i;
            simulator.set_input_value(device.instance().io().in,
                                      static_cast<uint64_t>(input));
            simulator.tick();

            auto decoded_value =
                simulator.get_value(device.instance().io().decoded_value);
            auto valid = simulator.get_value(device.instance().io().valid);

            std::cout << "Input: 0b"
                      << std::bitset<4>(static_cast<uint64_t>(input))
                      << " -> Decoded value: "
                      << decoded_value
                      //   << " (Valid: " << (valid.is_one() ? "true" : "false")
                      << ")" << std::endl;

            // 验证结果
            if (!valid.is_value(1)) {
                std::cerr << "Error: Input should be valid!" << std::endl;
                return 1;
            }

            if (!decoded_value.is_value(i)) {
                std::cerr << "Error: Expected " << i << ", got "
                          << decoded_value << std::endl;
                return 1;
            }
        }
        // // 测试无效输入（全0）
        // simulator.set_input_value(device.instance().io().in, 0);
        // simulator.tick();

        // auto valid = simulator.get_value(device.instance().io().valid);
        // std::cout << "Input: 0b0000 -> Valid: "
        //           << (valid.is_one() ? "true" : "false") << std::endl;

        // // 测试无效输入（多个位设置）
        // simulator.set_input_value(device.instance().io().in, 0b0101);
        // simulator.tick();

        // valid = simulator.get_value(device.instance().io().valid);
        // std::cout << "Input: 0b0101 -> Valid: "
        //           << (valid.is_one() ? "true" : "false") << std::endl;

        std::cout << "\nGenerating Verilog code..." << std::endl;
        toVerilog("onehot_decoder.v", device.context());

        std::cout << "Generating DAG diagram..." << std::endl;
        toDAG("onehot_decoder.dot", device.context());

        std::cout << "OneHotDecoder example completed successfully!"
                  << std::endl;

    } catch (const std::exception &e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
