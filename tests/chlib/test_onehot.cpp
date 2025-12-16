#include "catch_amalgamated.hpp"
#include "chlib/onehot.h"
#include "codegen_verilog.h"
#include "simulator.h"
#include <iostream>

using namespace ch;
using namespace ch::core;
using namespace chlib;

TEST_CASE("OneHotDecoder: Basic functionality test", "[onehot][decoder]") {
    SECTION("Testing 4-bit OneHotDecoder") {
        class Top : public Component {
        public:
            __io(ch_out<ch_uint<2>> out;)

                Top(Component *parent = nullptr,
                    const std::string &name = "top")
                : Component(parent, name) {}

            void create_ports() override { new (io_storage_) io_type; }

            void describe() override {
                CH_MODULE(OneHotDecoder<4>, decoder);

                // 测试各种输入值
                ch_reg<ch_uint<4>> input_reg(0b0001_d);
                decoder.io().in = input_reg;
                io().out = decoder.io().out;

                // 在实际应用中，我们会改变输入值进行测试
                // 这里只验证连接和基本功能
            }
        };

        ch_device<Top> device;
        Simulator simulator(device.context());

        // 验证模块可以正常工作
        REQUIRE_NOTHROW(simulator.tick());
    }

    SECTION("Testing 8-bit OneHotDecoder") {
        class Top : public Component {
        public:
            __io(ch_out<ch_uint<3>> out;)

                Top(Component *parent = nullptr,
                    const std::string &name = "top")
                : Component(parent, name) {}

            void create_ports() override { new (io_storage_) io_type; }

            void describe() override {
                CH_MODULE(OneHotDecoder<8>, decoder);

                ch_reg<ch_uint<8>> input_reg(0b00000001_d);
                decoder.io().in = input_reg;
                io().out = decoder.io().out;
            }
        };

        ch_device<Top> device;
        Simulator simulator(device.context());

        REQUIRE_NOTHROW(simulator.tick());
    }
}

TEST_CASE("OneHotDecoder: Verify decode values",
          "[onehot][decoder][functional]") {
    SECTION("Test decoding of all positions for 4-bit input") {
        class DecoderTester : public Component {
        public:
            __io(ch_in<ch_uint<4>> in; ch_out<ch_uint<2>> out;
                 ch_out<ch_bool> valid;)

                DecoderTester(Component *parent = nullptr,
                              const std::string &name = "decoder_tester")
                : Component(parent, name) {}

            void create_ports() override { new (io_storage_) io_type; }

            void describe() override {
                CH_MODULE(OneHotDecoder<4>, decoder);
                decoder.io().in = io().in;
                io().out = decoder.io().out;

                // 验证恰好只有一个位被设置（即有效的one-hot编码）
                io().valid = (ch_countones(io().in) == 1_d);
            }
        };

        ch_device<DecoderTester> device;
        Simulator simulator(device.context());

        // 测试所有有效的one-hot值
        for (int i = 0; i < 4; i++) {
            ch_uint<4> input_val = ch_uint<4>(1) << i;
            simulator.set_value(device.instance().io().in, input_val.value());
            simulator.tick();

            auto output = simulator.get_value(device.instance().io().out);
            auto valid = simulator.get_value(device.instance().io().valid);

            REQUIRE(valid == 1);
            REQUIRE(output == i);
        }
    }
}

TEST_CASE("OneHotDecoder: Edge cases", "[onehot][decoder][edge]") {
    SECTION("Testing 1-bit OneHotDecoder") {
        class Top : public Component {
        public:
            __io(ch_out<ch_uint<1>> out;)

                Top(Component *parent = nullptr,
                    const std::string &name = "top")
                : Component(parent, name) {}

            void create_ports() override { new (io_storage_) io_type; }

            void describe() override {
                CH_MODULE(OneHotDecoder<1>, decoder);

                ch_reg<ch_uint<1>> input_reg(0b1_d);
                decoder.io().in = input_reg;
                io().out = decoder.io().out;
            }
        };

        ch_device<Top> device;
        Simulator simulator(device.context());

        simulator.tick();
        auto output = simulator.get_value(device.instance().io().out);

        // 对于1位输入，输出应该是0
        REQUIRE(output == 0);
    }

    SECTION("Testing 2-bit OneHotDecoder") {
        class Top : public Component {
        public:
            __io(ch_out<ch_uint<1>> out;)

                Top(Component *parent = nullptr,
                    const std::string &name = "top")
                : Component(parent, name) {}

            void create_ports() override { new (io_storage_) io_type; }

            void describe() override {
                CH_MODULE(OneHotDecoder<2>, decoder);

                ch_reg<ch_uint<2>> input_reg(0b01_d);
                decoder.io().in = input_reg;
                io().out = decoder.io().out;
            }
        };

        ch_device<Top> device;
        Simulator simulator(device.context());

        simulator.tick();
        auto output = simulator.get_value(device.instance().io().out);

        // 对于输入0b01，输出应该是0
        REQUIRE(output == 0);

        // 更改输入为0b10
        simulator.set_value(device.instance().io().in, 0b10);
        simulator.tick();
        output = simulator.get_value(device.instance().io().out);

        // 对于输入0b10，输出应该是1
        REQUIRE(output == 1);
    }
}
