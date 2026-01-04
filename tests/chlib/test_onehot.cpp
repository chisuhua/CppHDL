#include "catch_amalgamated.hpp"
#include "chlib/onehot.h"
#include "codegen_dag.h"
#include "codegen_verilog.h"
#include "simulator.h"
#include <cstdint>
#include <iostream>

using namespace ch;
using namespace ch::core;
using namespace chlib;

// 测试函数式 onehot 解码器
template <unsigned N> class OneHotDecoderFunctionExample : public Component {
public:
    static constexpr unsigned OUTPUT_WIDTH =
        (N > 1) ? compute_bit_width(N - 1) : 1;

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

// 测试模块式 onehot 解码器
template <unsigned N> class OneHotDecoderModuleExample : public Component {
public:
    static constexpr unsigned OUTPUT_WIDTH =
        (N > 1) ? compute_bit_width(N - 1) : 1;

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

// 测试函数式 onehot 编码器
template <unsigned N> class OneHotEncoderFunctionExample : public Component {
public:
    static constexpr unsigned INPUT_WIDTH =
        (N > 1) ? compute_bit_width(N - 1) : 1;

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

// 测试模块式 onehot 编码器
template <unsigned N> class OneHotEncoderModuleExample : public Component {
public:
    static constexpr unsigned INPUT_WIDTH =
        (N > 1) ? compute_bit_width(N - 1) : 1;

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

TEST_CASE("OneHotDecoder: Basic functionality test", "[onehot][decoder]") {
    SECTION("Testing 4-bit OneHotDecoder (Function Style)") {
        ch_device<OneHotDecoderFunctionExample<4>> device;
        Simulator simulator(device.context());

        // 测试所有有效的one-hot值
        for (int i = 0; i < 4; i++) {
            uint64_t input = 1ULL << i; // 0001, 0010, 0100, 1000
            simulator.set_input_value(device.instance().io().in, input);
            simulator.tick();

            auto result = simulator.get_value(device.instance().io().out);

            REQUIRE(static_cast<uint64_t>(result) == i);
        }
    }

    SECTION("Testing 4-bit OneHotDecoder (Module Style)") {
        ch_device<OneHotDecoderModuleExample<4>> device;
        Simulator simulator(device.context());

        // 测试所有有效的one-hot值
        for (int i = 0; i < 4; i++) {
            uint64_t input = 1ULL << i; // 0001, 0010, 0100, 1000
            simulator.set_input_value(device.instance().io().in, input);
            simulator.tick();

            auto result = simulator.get_value(device.instance().io().out);

            REQUIRE(static_cast<uint64_t>(result) == i);
        }
    }

    SECTION("Testing 8-bit OneHotDecoder (Function Style)") {
        ch_device<OneHotDecoderFunctionExample<8>> device;
        Simulator simulator(device.context());

        // 测试几个有效的one-hot值
        for (int i = 0; i < 8; i++) {
            uint64_t input = 1ULL << i;
            simulator.set_input_value(device.instance().io().in, input);
            simulator.tick();

            auto result = simulator.get_value(device.instance().io().out);

            REQUIRE(static_cast<uint64_t>(result) == i);
        }
    }
}

TEST_CASE("OneHotEncoder: Basic functionality test", "[onehot][encoder]") {
    SECTION("Testing 4-bit OneHotEncoder (Function Style)") {
        ch_device<OneHotEncoderFunctionExample<4>> device;
        Simulator simulator(device.context());

        // 测试所有可能的索引值
        for (int i = 0; i < 4; i++) {
            simulator.set_input_value(device.instance().io().in, i);
            simulator.tick();

            auto result = simulator.get_value(device.instance().io().out);
            uint64_t result_val = static_cast<uint64_t>(result);

            // 验证 onehot 格式：只有一位是1
            REQUIRE(result_val == (1ULL << i));
        }
    }

    SECTION("Testing 4-bit OneHotEncoder (Module Style)") {
        ch_device<OneHotEncoderModuleExample<4>> device;
        Simulator simulator(device.context());

        // 测试所有可能的索引值
        for (int i = 0; i < 4; i++) {
            simulator.set_input_value(device.instance().io().in, i);
            simulator.tick();

            auto result = simulator.get_value(device.instance().io().out);
            uint64_t result_val = static_cast<uint64_t>(result);

            // 验证 onehot 格式：只有一位是1
            REQUIRE(result_val == (1ULL << i));
        }
    }

    SECTION("Testing 8-bit OneHotEncoder (Function Style)") {
        ch_device<OneHotEncoderFunctionExample<8>> device;
        Simulator simulator(device.context());

        // 测试几个可能的索引值
        for (int i = 0; i < 8; i++) {
            simulator.set_input_value(device.instance().io().in, i);
            simulator.tick();

            auto result = simulator.get_value(device.instance().io().out);
            uint64_t result_val = static_cast<uint64_t>(result);

            // 验证 onehot 格式：只有一位是1
            REQUIRE(result_val == (1ULL << i));
        }
    }
}

TEST_CASE("OneHotDecoder: Verify decode values",
          "[onehot][decoder][functional]") {
    SECTION("Test decoding of all positions for 4-bit input (Function Style)") {
        class DecoderTester : public Component {
        public:
            __io(ch_in<ch_uint<4>> in; ch_out<ch_uint<2>> out;
                 ch_out<ch_bool> valid;)

                DecoderTester(Component *parent = nullptr,
                              const std::string &name = "decoder_tester")
                : Component(parent, name) {}

            void create_ports() override { new (io_storage_) io_type; }

            void describe() override {
                // 使用函数式 onehot 解码器
                onehot_dec<4> decoder;
                io().out = decoder(io().in);

                // 验证恰好只有一个位被设置（即有效的one-hot编码）
                io().valid = (popcount(io().in) == 1_d);
            }
        };

        ch_device<DecoderTester> device;
        Simulator simulator(device.context());

        // 测试所有有效的one-hot值
        for (int i = 0; i < 4; i++) {
            uint64_t input = 1 << i;
            simulator.set_input_value(device.instance().io().in, input);
            simulator.tick();

            auto output = simulator.get_value(device.instance().io().out);
            auto valid = simulator.get_value(device.instance().io().valid);

            REQUIRE(static_cast<uint64_t>(valid) == 1);
            REQUIRE(static_cast<uint64_t>(output) == i);
        }
    }

    SECTION("Test decoding of all positions for 4-bit input (Module Style)") {
        class DecoderTester : public Component {
        public:
            __io(ch_in<ch_uint<4>> in; ch_out<ch_uint<2>> out;
                 ch_out<ch_bool> valid;)

                DecoderTester(Component *parent = nullptr,
                              const std::string &name = "decoder_tester")
                : Component(parent, name) {}

            void create_ports() override { new (io_storage_) io_type; }

            void describe() override {
                // 使用模块式 onehot 解码器
                CH_MODULE(onehot_dec_module<4>, decoder);
                decoder.io().in <<= io().in;
                io().out <<= decoder.io().out;

                // 验证恰好只有一个位被设置（即有效的one-hot编码）
                io().valid = (popcount(io().in) == 1_d);
            }
        };

        ch_device<DecoderTester> device;
        Simulator simulator(device.context());

        // 测试所有有效的one-hot值
        for (int i = 0; i < 4; i++) {
            uint64_t input = 1 << i;
            simulator.set_input_value(device.instance().io().in, input);
            simulator.tick();

            auto output = simulator.get_value(device.instance().io().out);
            auto valid = simulator.get_value(device.instance().io().valid);

            REQUIRE(static_cast<uint64_t>(valid) == 1);
            REQUIRE(static_cast<uint64_t>(output) == i);
        }
    }
}

TEST_CASE("OneHotEncoder: Verify encode values",
          "[onehot][encoder][functional]") {
    SECTION(
        "Test encoding of all positions for 4-bit output (Function Style)") {
        class EncoderTester : public Component {
        public:
            __io(ch_in<ch_uint<2>> in; ch_out<ch_uint<4>> out;)

                EncoderTester(Component *parent = nullptr,
                              const std::string &name = "encoder_tester")
                : Component(parent, name) {}

            void create_ports() override { new (io_storage_) io_type; }

            void describe() override {
                // 使用函数式 onehot 编码器
                onehot_enc<4> encoder;
                io().out = encoder(io().in);
            }
        };

        ch_device<EncoderTester> device;
        Simulator simulator(device.context());

        // 测试所有可能的索引值
        for (int i = 0; i < 4; i++) {
            simulator.set_input_value(device.instance().io().in, i);
            simulator.tick();

            auto output = simulator.get_value(device.instance().io().out);
            uint64_t output_val = static_cast<uint64_t>(output);

            // 验证 onehot 格式：只有一位是1
            REQUIRE(output_val == (1ULL << i));
            // 验证只有一个位被设置
            REQUIRE(__builtin_popcountll(output_val) == 1);
        }
    }

    SECTION("Test encoding of all positions for 4-bit output (Module Style)") {
        class EncoderTester : public Component {
        public:
            __io(ch_in<ch_uint<2>> in; ch_out<ch_uint<4>> out;)

                EncoderTester(Component *parent = nullptr,
                              const std::string &name = "encoder_tester")
                : Component(parent, name) {}

            void create_ports() override { new (io_storage_) io_type; }

            void describe() override {
                // 使用模块式 onehot 编码器
                CH_MODULE(onehot_enc_module<4>, encoder);
                encoder.io().in <<= io().in;
                io().out <<= encoder.io().out;
            }
        };

        ch_device<EncoderTester> device;
        Simulator simulator(device.context());

        // 测试所有可能的索引值
        for (int i = 0; i < 4; i++) {
            simulator.set_input_value(device.instance().io().in, i);
            simulator.tick();

            auto output = simulator.get_value(device.instance().io().out);
            uint64_t output_val = static_cast<uint64_t>(output);

            // 验证 onehot 格式：只有一位是1
            REQUIRE(output_val == (1ULL << i));
            // 验证只有一个位被设置
            REQUIRE(__builtin_popcountll(output_val) == 1);
        }
    }
}

TEST_CASE("OneHot: Encoder-Decoder combination test",
          "[onehot][encoder][decoder][combination]") {
    SECTION("Testing 4-bit Encoder-Decoder combination (Function Style)") {
        class EncoderDecoderTester : public Component {
        public:
            __io(ch_in<ch_uint<2>> in; ch_out<ch_uint<2>> out;)

                EncoderDecoderTester(Component *parent = nullptr,
                                     const std::string &name = "enc_dec_tester")
                : Component(parent, name) {}

            void create_ports() override { new (io_storage_) io_type; }

            void describe() override {
                // 使用函数式 onehot 编码器和解码器
                onehot_enc<4> encoder;
                onehot_dec<4> decoder;

                ch_uint<4> encoded = encoder(io().in);
                io().out = decoder(encoded);
            }
        };

        ch_device<EncoderDecoderTester> device;
        Simulator simulator(device.context());
        toDAG("onehot1.dot", device.context());

        // 测试所有可能的索引值
        for (int i = 0; i < 4; i++) {
            simulator.set_input_value(device.instance().io().in, i);
            simulator.tick();

            auto output = simulator.get_value(device.instance().io().out);

            // 验证往返正确性：编码后再解码应该得到原始值
            REQUIRE(static_cast<uint64_t>(output) == i);
        }
    }

    SECTION("Testing 4-bit Encoder-Decoder combination (Module Style)") {
        class EncoderDecoderTester : public Component {
        public:
            __io(ch_in<ch_uint<2>> in; ch_out<ch_uint<2>> out;)

                EncoderDecoderTester(Component *parent = nullptr,
                                     const std::string &name = "enc_dec_tester")
                : Component(parent, name) {}

            void create_ports() override { new (io_storage_) io_type; }

            void describe() override {
                // 使用模块式 onehot 编码器和解码器
                CH_MODULE(onehot_enc_module<4>, encoder);
                CH_MODULE(onehot_dec_module<4>, decoder);

                encoder.io().in <<= io().in;
                io().out <<= decoder.io().out;
                decoder.io().in <<= encoder.io().out;
            }
        };

        ch_device<EncoderDecoderTester> device;
        Simulator simulator(device.context());

        // 测试所有可能的索引值
        for (int i = 0; i < 4; i++) {
            simulator.set_input_value(device.instance().io().in, i);
            simulator.tick();

            auto output = simulator.get_value(device.instance().io().out);

            // 验证往返正确性：编码后再解码应该得到原始值
            REQUIRE(static_cast<uint64_t>(output) == i);
        }
    }

    SECTION("Testing 4-bit Encoder-Decoder combination (Mixed Style)") {
        class EncoderDecoderTester : public Component {
        public:
            __io(ch_in<ch_uint<2>> in; ch_out<ch_uint<2>> out;)

                EncoderDecoderTester(Component *parent = nullptr,
                                     const std::string &name = "enc_dec_tester")
                : Component(parent, name) {}

            void create_ports() override { new (io_storage_) io_type; }

            void describe() override {
                // 使用函数式编码器和模块式解码器
                onehot_enc<4> encoder;
                CH_MODULE(onehot_dec_module<4>, decoder);

                ch_uint<4> encoded = encoder(io().in);
                decoder.io().in <<=
                    encoded; // 使用连接操作符连接函数式输出到模块式输入
                io().out <<= decoder.io().out; // 使用连接操作符连接输出
            }
        };

        ch_device<EncoderDecoderTester> device;
        Simulator simulator(device.context());
        toDAG("onehot.dot", device.context());

        // 测试所有可能的索引值
        for (int i = 0; i < 4; i++) {
            simulator.set_input_value(device.instance().io().in, i);
            simulator.tick();

            auto output = simulator.get_value(device.instance().io().out);

            // 验证往返正确性：编码后再解码应该得到原始值
            REQUIRE(static_cast<uint64_t>(output) == i);
        }
    }
}

TEST_CASE("OneHotDecoder: Edge cases", "[onehot][decoder][edge]") {
    SECTION("Testing 1-bit OneHotDecoder (Function Style)") {
        ch_device<OneHotDecoderFunctionExample<1>> device;
        Simulator simulator(device.context());

        simulator.tick();
        auto output = simulator.get_value(device.instance().io().out);

        // 对于1位输入，输出应该是0
        REQUIRE(static_cast<uint64_t>(output) == 0);
    }

    SECTION("Testing 1-bit OneHotDecoder (Module Style)") {
        ch_device<OneHotDecoderModuleExample<1>> device;
        Simulator simulator(device.context());

        simulator.tick();
        auto output = simulator.get_value(device.instance().io().out);

        // 对于1位输入，输出应该是0
        REQUIRE(static_cast<uint64_t>(output) == 0);
    }

    SECTION("Testing 2-bit OneHotDecoder (Function Style)") {
        ch_device<OneHotDecoderFunctionExample<2>> device;
        Simulator simulator(device.context());

        // 测试输入为0b01
        simulator.set_input_value(device.instance().io().in,
                                  static_cast<uint64_t>(01_b));
        simulator.tick();
        auto output = simulator.get_value(device.instance().io().out);

        // 对于输入0b01，输出应该是0
        REQUIRE(static_cast<uint64_t>(output) == 0);

        // 更改输入为0b10
        simulator.set_input_value(device.instance().io().in, 10_b);
        simulator.tick();
        output = simulator.get_value(device.instance().io().out);

        // 对于输入0b10，输出应该是1
        REQUIRE(static_cast<uint64_t>(output) == 1);
    }

    SECTION("Testing 2-bit OneHotDecoder (Module Style)") {
        ch_device<OneHotDecoderModuleExample<2>> device;
        Simulator simulator(device.context());

        // 测试输入为0b01
        simulator.set_input_value(device.instance().io().in, 01_b);
        simulator.tick();
        auto output = simulator.get_value(device.instance().io().out);

        // 对于输入0b01，输出应该是0
        REQUIRE(static_cast<uint64_t>(output) == 0);

        // 更改输入为0b10
        simulator.set_input_value(device.instance().io().in, 10_b);
        simulator.tick();
        output = simulator.get_value(device.instance().io().out);

        // 对于输入0b10，输出应该是1
        REQUIRE(static_cast<uint64_t>(output) == 1);
    }
}

TEST_CASE("OneHotEncoder: Edge cases", "[onehot][encoder][edge]") {
    SECTION("Testing 1-bit OneHotEncoder (Function Style)") {
        ch_device<OneHotEncoderFunctionExample<1>> device;
        Simulator simulator(device.context());

        simulator.set_input_value(device.instance().io().in, 0);
        simulator.tick();
        auto output = simulator.get_value(device.instance().io().out);

        // 对于1位编码器，输入0应该输出1
        REQUIRE(static_cast<uint64_t>(output) == 1);
    }

    SECTION("Testing 1-bit OneHotEncoder (Module Style)") {
        ch_device<OneHotEncoderModuleExample<1>> device;
        Simulator simulator(device.context());

        simulator.set_input_value(device.instance().io().in, 0);
        simulator.tick();
        auto output = simulator.get_value(device.instance().io().out);

        // 对于1位编码器，输入0应该输出1
        REQUIRE(static_cast<uint64_t>(output) == 1);
    }

    SECTION("Testing 2-bit OneHotEncoder (Function Style)") {
        ch_device<OneHotEncoderFunctionExample<2>> device;
        Simulator simulator(device.context());

        // 测试输入为0
        simulator.set_input_value(device.instance().io().in,
                                  static_cast<uint64_t>(0_b));
        simulator.tick();
        auto output = simulator.get_value(device.instance().io().out);

        // 对于输入0，输出应该是0b01
        REQUIRE(static_cast<uint64_t>(output) == static_cast<uint64_t>(01_b));

        // 测试输入为1
        simulator.set_input_value(device.instance().io().in,
                                  static_cast<uint64_t>(1_b));
        simulator.tick();
        output = simulator.get_value(device.instance().io().out);

        // 对于输入1，输出应该是0b10
        REQUIRE(static_cast<uint64_t>(output) == static_cast<uint64_t>(10_b));
    }

    SECTION("Testing 2-bit OneHotEncoder (Module Style)") {
        ch_device<OneHotEncoderModuleExample<2>> device;
        Simulator simulator(device.context());

        // 测试输入为0
        simulator.set_input_value(device.instance().io().in,
                                  static_cast<uint64_t>(0));
        simulator.tick();
        auto output = simulator.get_value(device.instance().io().out);

        // 对于输入0，输出应该是0b01
        REQUIRE(static_cast<uint64_t>(output) == static_cast<uint64_t>(01_b));

        // 测试输入为1
        simulator.set_input_value(device.instance().io().in,
                                  static_cast<uint64_t>(1));
        simulator.tick();
        output = simulator.get_value(device.instance().io().out);

        // 对于输入1，输出应该是0b10
        REQUIRE(static_cast<uint64_t>(output) == static_cast<uint64_t>(10_b));
    }
}