#include "catch_amalgamated.hpp"
#include "ch.hpp"
#include "codegen_dag.h"
#include "component.h"
#include "core/bool.h"
#include "core/context.h"
#include "core/io.h"
#include "core/literal.h"
#include "core/reg.h"
#include "core/uint.h"
#include "simulator.h"

using namespace ch::core;

TEST_CASE("test_wires_connection - Basic ch_uint connection",
          "[wires][connection][operator]") {
    context ctx; // 创建context

    ctx_swap ctx_guard(&ctx);

    // 创建两个ch_uint信号 - 使用字面量构造确保有节点
    ch_uint<8> signal_src(42_d);
    ch_uint<8> signal_dst1(0_d); // 修改：使用字面量构造确保有节点
    ch_uint<8> signal_dst2(0_d); // 修改：使用字面量构造确保有节点

    // 使用operator<<=连接两个信号，这相当于硬件中的wire连接
    signal_dst1 = signal_src;
    signal_dst2 <<= signal_src;
    toDAG("wire1.dot", &ctx);

    // 验证连接是否有效（它们应该引用相同的底层实现）
    REQUIRE(signal_dst1.impl() != nullptr);
    REQUIRE(signal_src.impl() != nullptr);
    REQUIRE(signal_dst1.impl() == signal_src.impl());
    REQUIRE(signal_dst2.impl() != signal_src.impl());
}

TEST_CASE("test_wires_connection - ch_uint connection in module",
          "[wires][connection][module]") {
    class WireConnectionModule : public Component {
    public:
        __io(ch_in<ch_uint<8>> input_port; ch_out<ch_uint<8>> output_port;);

        WireConnectionModule(Component *parent = nullptr,
                             const std::string &name = "wire_conn")
            : Component(parent, name) {}

        void create_ports() override { new (this->io_storage_) io_type; }

        void describe() override {
            // 创建一个内部信号作为wire - 使用字面量确保有节点
            ch_uint<8> internal_wire = 0_d;

            // 使用operator<<=连接输入到内部wire
            internal_wire <<= io().input_port;

            // 再次使用operator<<=连接内部wire到输出
            io().output_port <<= internal_wire;
        }
    };

    ch_device<WireConnectionModule> dev;
    toDAG("wire2.dot", dev.context());
    Simulator sim(dev.context());

    auto input_port = dev.io().input_port;
    auto output_port = dev.io().output_port;

    // 测试不同的输入值
    std::vector<uint64_t> test_values = {0, 1, 42, 100, 255};

    for (uint64_t test_val : test_values) {
        sim.set_input_value(input_port, test_val);
        sim.tick();

        auto output_val = sim.get_value(output_port);
        REQUIRE(static_cast<uint64_t>(output_val) == test_val);
    }
}

TEST_CASE("test_wires_connection - Multiple wire connections",
          "[wires][connection][multi]") {
    class MultiWireConnectionModule : public Component {
    public:
        __io(ch_in<ch_uint<4>> input_a; ch_in<ch_uint<4>> input_b;
             ch_out<ch_uint<4>> output_result;);

        MultiWireConnectionModule(Component *parent = nullptr,
                                  const std::string &name = "multi_wire")
            : Component(parent, name) {}

        void create_ports() override { new (this->io_storage_) io_type; }

        void describe() override {
            // 创建多个内部wire信号 - 使用字面量确保有节点
            ch_uint<4> wire1 = 0_d, wire2 = 0_d;
            ch_uint<4> wire3 = 0_d;

            // 使用operator<<=建立多级连接
            wire1 <<= io().input_a;
            wire2 <<= io().input_b;

            // 对wire1和wire2进行操作，结果连接到wire3
            wire3 <<= wire1 + wire2;

            // 最终输出连接
            io().output_result <<= wire3;
        }
    };

    ch_device<MultiWireConnectionModule> dev;
    Simulator sim(dev.context());

    auto input_a = dev.io().input_a;
    auto input_b = dev.io().input_b;
    auto output_result = dev.io().output_result;

    // 测试不同的输入组合
    std::vector<std::tuple<uint64_t, uint64_t, uint64_t>> test_cases = {
        {1, 2, 3},
        {5, 3, 8},
        {7, 8, 15}, // 注意：ch_uint<4>最大值是15，所以15是合法的最大值
        {10, 10, 4} // 20超出了4位范围，变成4 (20 % 16 = 4)
    };

    for (const auto &[a, b, expected] : test_cases) {
        sim.set_input_value(input_a, a);
        sim.set_input_value(input_b, b);
        sim.tick();
        toDAG("wire3.dot", dev.context(), sim);

        auto actual = sim.get_value(output_result);
        CAPTURE(a, b, expected, static_cast<uint64_t>(actual));
        REQUIRE(static_cast<uint64_t>(actual) == expected);
    }
}

TEST_CASE("test_wires_connection - ch_uint to ch_reg connection",
          "[wires][connection][reg]") {
    class WireToRegConnectionModule : public Component {
    public:
        __io(ch_in<ch_uint<8>> input_data; ch_out<ch_uint<8>> output_data;);

        WireToRegConnectionModule(Component *parent = nullptr,
                                  const std::string &name = "wire_to_reg")
            : Component(parent, name) {}

        void create_ports() override { new (this->io_storage_) io_type; }

        void describe() override {
            // 创建一个寄存器
            ch_reg<ch_uint<8>> data_reg(0_d, "data_reg");

            // 创建一个内部wire - 使用字面量确保有节点
            ch_uint<8> internal_wire = 0_d;

            // 连接输入到内部wire
            internal_wire <<= io().input_data;

            // 连接wire到寄存器
            data_reg <<= internal_wire;

            // 输出连接到寄存器
            io().output_data <<= data_reg;
        }
    };

    ch_device<WireToRegConnectionModule> dev;
    Simulator sim(dev.context());

    auto input_data = dev.io().input_data;
    auto output_data = dev.io().output_data;

    // 测试寄存器是否正确捕获输入值
    std::vector<uint64_t> test_values = {10, 20, 30, 40};

    for (uint64_t test_val : test_values) {
        sim.set_input_value(input_data, test_val);
        sim.tick(); // 时钟上升沿，寄存器更新

        auto output_val = sim.get_value(output_data);
        REQUIRE(static_cast<uint64_t>(output_val) == test_val);
    }
}

TEST_CASE("test_wires_connection - Wire chain propagation",
          "[wires][connection][chain]") {
    class WireChainModule : public Component {
    public:
        __io(ch_in<ch_uint<8>> start_signal; ch_out<ch_uint<8>> end_signal;);

        WireChainModule(Component *parent = nullptr,
                        const std::string &name = "wire_chain")
            : Component(parent, name) {}

        void create_ports() override { new (this->io_storage_) io_type; }

        void describe() override {
            // 创建一个长的wire链 - 使用字面量确保有节点
            ch_uint<8> w1 = 0_d, w2 = 0_d, w3 = 0_d, w4 = 0_d;

            // 建立连接链
            w1 <<= io().start_signal;
            w2 <<= w1;
            w3 <<= w2;
            w4 <<= w3;

            // 最后连接到输出
            io().end_signal <<= w4;
        }
    };

    ch_device<WireChainModule> dev;
    Simulator sim(dev.context());

    auto start_signal = dev.io().start_signal;
    auto end_signal = dev.io().end_signal;

    // 测试各种值是否能够正确传播通过整个链
    std::vector<uint64_t> test_values = {0, 1, 55, 128, 255};

    for (uint64_t test_val : test_values) {
        sim.set_input_value(start_signal, test_val);
        sim.tick();

        auto result = sim.get_value(end_signal);
        REQUIRE(static_cast<uint64_t>(result) == test_val);
    }
}

TEST_CASE("test_wires_connection - Different width connections",
          "[wires][connection][width]") {
    class WidthConversionModule : public Component {
    public:
        __io(ch_in<ch_uint<4>> input_4bit; ch_out<ch_uint<8>> output_8bit;);

        WidthConversionModule(Component *parent = nullptr,
                              const std::string &name = "width_conv")
            : Component(parent, name) {}

        void create_ports() override { new (this->io_storage_) io_type; }

        void describe() override {
            // 创建一个内部6位wire - 使用字面量确保有节点
            ch_uint<6> internal_6bit = 0_d;
            ch_uint<8> internal_8bit = 0_d;

            // 连接4位输入到6位wire（自动零扩展）
            internal_6bit <<= io().input_4bit;

            // 连接6位wire到8位wire（继续零扩展）
            internal_8bit <<= internal_6bit;

            // 连接到输出
            io().output_8bit <<= internal_8bit;
        }
    };

    ch_device<WidthConversionModule> dev;
    Simulator sim(dev.context());

    auto input_4bit = dev.io().input_4bit;
    auto output_8bit = dev.io().output_8bit;

    // 测试4位输入是否正确扩展到8位输出
    for (int i = 0; i <= 15; i++) { // 4位值范围 0-15
        sim.set_input_value(input_4bit, i);
        sim.tick();

        auto output_val = sim.get_value(output_8bit);
        REQUIRE(static_cast<uint64_t>(output_val) ==
                static_cast<uint64_t>(i)); // 由于是零扩展，值应该相等
    }
}

TEST_CASE("test_wires_connection - Empty constructed ch_uint behavior",
          "[wires][construction]") {
    // 创建一个组件，在其中创建有节点的ch_uint
    class TestComponent : public Component {
    public:
        __io(ch_uint<8> source;);

        TestComponent(Component *parent = nullptr,
                      const std::string &name = "test_comp")
            : Component(parent, name) {}

        void create_ports() override { new (this->io_storage_) io_type; }

        void describe() override {
            // 初始化source为42_d，这样它就会有节点
            io().source = 42_d;
        }
    };

    ch_device<TestComponent> dev;
    Simulator sim(dev.context());

    // 测试默认构造的ch_uint
    ch_uint<8> default_constructed;

    // 此时node_impl_应该为nullptr
    REQUIRE(default_constructed.impl() == nullptr);

    // 验证source确实有节点
    REQUIRE(dev.io().source.impl() != nullptr);

    // 连接操作会将目标的node_impl_设为源的impl()
    default_constructed <<= dev.io().source;

    // 现在应该有有效的节点
    REQUIRE(default_constructed.impl() != nullptr);
    REQUIRE(default_constructed.impl() == dev.io().source.impl());
}