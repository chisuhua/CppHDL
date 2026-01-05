#include "ch.hpp"
#include "chlib/onehot.h"
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
using namespace chlib;

/**
 * 示例演示如何使用OneHot模块
 * 包括编码器和解码器的使用
 */

// 示例1: 使用函数式 onehot 解码器
template <unsigned N> class OneHotDecoderFunctionExample : public Component {
public:
    static constexpr unsigned OUTPUT_WIDTH = compute_idx_width(N);

    __io(ch_in<ch_uint<N>> in;              // one-hot输入
         ch_out<ch_uint<OUTPUT_WIDTH>> out; // 解码后的输出
         )

        OneHotDecoderFunctionExample(
            Component *parent = nullptr,
            const std::string &name = "onehot_dec_module_func_example")
        : Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        // 使用函数式 onehot 解码器
        onehot_dec<N> decoder;
        io().out = decoder(io().in); // 现在可以直接传递端口，利用隐式转换
    }
};

// 示例2: 使用模块式 onehot 解码器
template <unsigned N> class OneHotDecoderModuleExample : public Component {
public:
    static constexpr unsigned OUTPUT_WIDTH = compute_idx_width(N);

    __io(ch_in<ch_uint<N>> in;              // one-hot输入
         ch_out<ch_uint<OUTPUT_WIDTH>> out; // 解码后的输出
         )

        OneHotDecoderModuleExample(
            Component *parent = nullptr,
            const std::string &name = "onehot_dec_module_mod_example")
        : Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        // 实例化模块式 onehot 解码器
        CH_MODULE(onehot_dec_module<N>, decoder);

        // 连接输入输出 - 使用隐式转换
        decoder.io().in <<= io().in;   // 隐式转换
        io().out <<= decoder.io().out; // 隐式转换
    }
};

// 示例3: 使用函数式 onehot 编码器
template <unsigned N> class OneHotEncoderFunctionExample : public Component {
public:
    static constexpr unsigned INPUT_WIDTH = compute_idx_width(N);

    __io(ch_in<ch_uint<INPUT_WIDTH>> in; // 输入索引
         ch_out<ch_uint<N>> out;         // one-hot输出
         )

        OneHotEncoderFunctionExample(
            Component *parent = nullptr,
            const std::string &name = "onehot_enc_module_func_example")
        : Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        // 使用函数式 onehot 编码器
        onehot_enc<N> encoder;
        io().out = encoder(io().in); // 现在可以直接传递端口，利用隐式转换
    }
};

// 示例4: 使用模块式 onehot 编码器
template <unsigned N> class OneHotEncoderModuleExample : public Component {
public:
    static constexpr unsigned INPUT_WIDTH = compute_idx_width(N);

    __io(ch_in<ch_uint<INPUT_WIDTH>> in; // 输入索引
         ch_out<ch_uint<N>> out;         // one-hot输出
         )

        OneHotEncoderModuleExample(
            Component *parent = nullptr,
            const std::string &name = "onehot_enc_module_mod_example")
        : Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        // 实例化模块式 onehot 编码器
        CH_MODULE(onehot_enc_module<N>, encoder);

        // 连接输入输出 - 使用隐式转换
        encoder.io().in <<= io().in;   // 隐式转换
        io().out <<= encoder.io().out; // 隐式转换
    }
};

// 示例5: 使用 onehot 编码器和解码器组合
// template <unsigned N> class OneHotEncoderDecoderExample : public Component {
// public:
//     static constexpr unsigned INDEX_WIDTH =
//         (N > 1) ? compute_bit_width(N - 1) : 1;

//     __io(ch_in<ch_uint<INDEX_WIDTH>> index_in;     // 输入索引
//          ch_out<ch_uint<N>> onehot_out;            // one-hot输出
//          ch_out<ch_uint<INDEX_WIDTH>> decoded_out; // 解码后的索引输出
//          )

//         OneHotEncoderDecoderExample(
//             Component *parent = nullptr,
//             const std::string &name = "onehot_enc_dec_example")
//         : Component(parent, name) {}

//     void create_ports() override { new (io_storage_) io_type; }

//     void describe() override {
//         // 使用函数式编码器
//         onehot_enc<N> encoder;
//         ch_uint<N> encoded = encoder(io().index_in); // 隐式转换

//         // 连接到输出
//         io().onehot_out = encoded;

//         // 使用模块式解码器
//         CH_MODULE(onehot_dec_module<N>, decoder);
//         decoder.io().in = encoded;
//         io().decoded_out <<= decoder.io().out; // 隐式转换
//     }
// };

int main() {
    std::cout << "=== OneHot Module Example ===" << std::endl;

    try {
        // 测试4位 onehot 解码器（函数式）
        std::cout
            << "\nTesting OneHotDecoder (Function Style) with 4-bit input:"
            << std::endl;
        {
            ch_device<OneHotDecoderFunctionExample<4>> device;
            Simulator simulator(device.context());

            for (int i = 0; i < 4; i++) {
                uint64_t input = 1ULL << i; // 0001, 0010, 0100, 1000
                simulator.set_input_value(device.instance().io().in, input);
                simulator.tick();

                auto result = simulator.get_value(device.instance().io().out);
                std::cout << "Input: 0b" << std::bitset<4>(input)
                          << " -> Decoded: " << result << std::endl;

                if (!result.is_value(i)) {
                    std::cerr << "Error: Expected " << i << ", got " << result
                              << std::endl;
                    return 1;
                }
            }
        }

        // 测试4位 onehot 解码器（模块式）
        std::cout
            << "\nTesting OneHotDecoder (Module Style) with 4-bit input : "
            << std::endl;
        {
            ch_device<OneHotDecoderModuleExample<4>> device;
            Simulator simulator(device.context());

            for (int i = 0; i < 4; i++) {
                uint64_t input = 1ULL << i; // 0001, 0010, 0100, 1000
                simulator.set_input_value(device.instance().io().in, input);
                simulator.tick();

                auto result = simulator.get_value(device.instance().io().out);
                std::cout << "Input: 0b" << std::bitset<4>(input)
                          << " -> Decoded: " << result << std::endl;

                if (!result.is_value(i)) {
                    std::cerr << "Error: Expected " << i << ", got " << result
                              << std::endl;
                    return 1;
                }
            }
        }

        // 测试4位 onehot 编码器（函数式）
        std::cout
            << "\nTesting OneHotEncoder (Function Style) with 4-bit output:"
            << std::endl;
        {
            ch_device<OneHotEncoderFunctionExample<4>> device;
            Simulator simulator(device.context());
            // toDAG("tmp.dot", device.context());
            // toVerilog("tmp.v", device.context());

            for (int i = 0; i < 4; i++) {
                simulator.set_input_value(device.instance().io().in, i);
                simulator.tick();

                auto result = simulator.get_value(device.instance().io().out);
                uint64_t result_val = static_cast<uint64_t>(result);
                std::cout << "Index: " << i << " -> OneHot: 0b"
                          << std::bitset<4>(result_val) << std::endl;

                // 验证 onehot 格式：只有一位是1
                if (result_val != (1ULL << i)) {
                    std::cerr << "Error: Expected 0b"
                              << std::bitset<4>(1ULL << i) << ", got 0b"
                              << std::bitset<4>(result_val) << std::endl;
                    return 1;
                }
            }
        }

        // 测试4位 onehot 编码器（模块式）
        std::cout << "\nTesting OneHotEncoder (Module Style) with 4-bit output:"
                  << std::endl;
        {
            ch_device<OneHotEncoderModuleExample<4>> device;
            Simulator simulator(device.context());

            for (int i = 0; i < 4; i++) {
                simulator.set_input_value(device.instance().io().in, i);
                simulator.tick();

                auto result = simulator.get_value(device.instance().io().out);
                uint64_t result_val = static_cast<uint64_t>(result);
                std::cout << "Index: " << i << " -> OneHot: 0b"
                          << std::bitset<4>(result_val) << std::endl;

                // 验证 onehot 格式：只有一位是1
                if (result_val != (1ULL << i)) {
                    std::cerr << "Error: Expected 0b"
                              << std::bitset<4>(1ULL << i) << ", got 0b"
                              << std::bitset<4>(result_val) << std::endl;
                    return 1;
                }
            }
        }

        // 测试编码器-解码器组合（验证往返正确性）
        // std::cout << "\nTesting OneHot Encoder-Decoder combination:"
        //           << std::endl;
        // {
        //     ch_device<OneHotEncoderDecoderExample<4>> device;
        //     Simulator simulator(device.context());

        //     for (int i = 0; i < 4; i++) {
        //         simulator.set_input_value(device.instance().io().index_in,
        //         i); simulator.tick();

        //         auto onehot_val =
        //             simulator.get_value(device.instance().io().onehot_out);
        //         auto decoded_val =
        //             simulator.get_value(device.instance().io().decoded_out);

        //         std::cout << "Index: " << i << " -> OneHot: 0b"
        //                   <<
        //                   std::bitset<4>(static_cast<uint64_t>(onehot_val))
        //                   << " -> Decoded: " << decoded_val << std::endl;

        //         if (!decoded_val.is_value(i)) {
        //             std::cerr << "Error: Expected " << i << ", got "
        //                       << decoded_val << std::endl;
        //             return 1;
        //         }
        //     }
        // }

        // 生成Verilog代码
        // std::cout << "\nGenerating Verilog code..." << std::endl;
        // {
        //     ch_device<OneHotEncoderDecoderExample<4>> device;
        //     ch::toVerilog("onehot_example.v", device.context());
        // }

        // // 生成DAG图
        // std::cout << "Generating DAG representation..." << std::endl;
        // {
        //     ch_device<OneHotDecoderFunctionExample<4>> device;
        //     ch::toDAG("onehot_dec_module_dag.dot", device.context());
        // }

        std::cout << "\nAll tests passed successfully!" << std::endl;

    } catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}