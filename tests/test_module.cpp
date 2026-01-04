#include "catch_amalgamated.hpp"
#include "ch.hpp"
#include "codegen_dag.h"
#include "codegen_verilog.h"
#include "module.h"
#include "simulator.h"
#include <cstdint>

using namespace ch;
using namespace ch::core;

// 定义一个简单的测试模块
template <unsigned N> class SimpleModule : public Component {
public:
    __io(ch_in<ch_uint<N>> in_port; ch_out<ch_uint<N>> out_port;)

        SimpleModule(Component *parent = nullptr,
                     const std::string &name = "SimpleModule")
        : Component(parent, name) {}

    void create_ports() override { new (this->io_storage_) io_type; }

    void describe() override {
        // 直接连接输入到输出
        // 使用新的显式operator<<=替换赋值操作
        io().out_port <<= io().in_port;
    }
};

TEST_CASE("CH_MODULE - Basic Instantiation", "[module][basic]") {
    class Top : public Component {
    public:
        __io()

            Top(Component *parent = nullptr, const std::string &name = "top")
            : Component(parent, name) {}

        void create_ports() override { new (this->io_storage_) io_type; }

        void describe() override {
            // 在父组件中创建子模块
            CH_MODULE(SimpleModule<4>, child_module);
        }
    };

    ch_device<Top> device;

    // 检查子模块是否创建成功
    REQUIRE(device.instance().child_count() == 1);
}

TEST_CASE("CH_MODULE - Code Generation", "[module][codegen]") {
    class Top : public Component {
    public:
        __io(ch_in<ch_uint<4>> in_data; ch_out<ch_uint<4>> out_data;)

            Top(Component *parent = nullptr, const std::string &name = "top")
            : Component(parent, name) {}

        void create_ports() override { new (this->io_storage_) io_type; }

        void describe() override {
            CH_MODULE(SimpleModule<4>, mod);

            // 检查子模块是否创建成功
            REQUIRE(mod.io().in_port.impl() != nullptr);
            REQUIRE(mod.io().out_port.impl() != nullptr);
            // 连接端口: 顶层输入 -> 子模块输入，子模块输出 -> 顶层输出
            // 使用新的operator<<=替代connect函数调用
            mod.io().in_port <<= io().in_data;
            io().out_data <<= mod.io().out_port;
        }
    };

    ch_device<Top> device;

    // 测试代码生成功能
    REQUIRE_NOTHROW(toVerilog("test_module_codgen.v", device.context()));
    REQUIRE_NOTHROW(toDAG("test_module_codgen.dot", device.context()));
}

TEST_CASE("CH_MODULE - Simulation Value Transfer", "[module][simulation]") {
    class Top : public Component {
    public:
        __io(ch_in<ch_uint<4>> in_data; ch_out<ch_uint<4>> out_data;)

            Top(Component *parent = nullptr, const std::string &name = "top")
            : Component(parent, name) {}

        void create_ports() override { new (this->io_storage_) io_type; }

        void describe() override {
            CH_MODULE(SimpleModule<4>, mod);

            // 连接端口: 顶层输入 -> 子模块输入，子模块输出 -> 顶层输出
            mod.io().in_port <<= io().in_data;
            io().out_data <<= mod.io().out_port;
        }
    };

    ch_device<Top> device;
    Simulator sim(device.context());

    // 测试不同的输入值
    auto in_data = device.io().in_data;
    auto out_data = device.io().out_data;

    for (uint64_t i = 0; i < 16; ++i) {
        sim.set_input_value(in_data, i);
        sim.tick();
        REQUIRE(sim.get_value(out_data) == i);
    }
}

TEST_CASE("CH_MODULE - Connection Between Child Modules",
          "[module][child-connection]") {
    class Top : public Component {
    public:
        __io(ch_in<ch_uint<4>> in_data; ch_out<ch_uint<4>> out_data;)

            Top(Component *parent = nullptr, const std::string &name = "top")
            : Component(parent, name) {}

        void create_ports() override { new (this->io_storage_) io_type; }

        void describe() override {
            CH_MODULE(SimpleModule<4>, mod1);
            CH_MODULE(SimpleModule<4>, mod2);

            // 修复循环连接：正确的级联应该是 顶层输入 -> mod1 -> mod2 ->
            // 顶层输出
            mod1.io().in_port <<= io().in_data;
            mod2.io().in_port <<= mod1.io().out_port;
            io().out_data <<= mod2.io().out_port;
        }
    };

    ch_device<Top> device;
    Simulator sim(device.context());

    // 测试不同的输入值
    auto in_data = device.io().in_data;
    auto out_data = device.io().out_data;

    for (uint64_t i = 0; i < 16; ++i) {
        sim.set_input_value(in_data, i);
        sim.tick();
        // 因为mod1和mod2都只是直接传递输入到输出，所以输出应该和输入相同
        REQUIRE(sim.get_value(out_data) == i);
    }
}

TEST_CASE("CH_MODULE - Connection Between Different IO Directions",
          "[module][io-direction]") {
    class DataProcessor : public Component {
    public:
        __io(ch_in<ch_uint<4>> input; ch_out<ch_uint<4>> output;
             ch_in<bool> enable;)

            DataProcessor(Component *parent = nullptr,
                          const std::string &name = "DataProcessor")
            : Component(parent, name) {}

        void create_ports() override { new (this->io_storage_) io_type; }

        void describe() override {
            // 简单的条件传递逻辑：如果enable为true，则传递input到output
            io().output <<= select(io().enable, io().input, 0_d);
        }
    };

    class Top : public Component {
    public:
        __io(ch_in<ch_uint<4>> in_data; ch_out<ch_uint<4>> out_data;
             ch_in<bool> enable1; ch_in<bool> enable2;)

            Top(Component *parent = nullptr, const std::string &name = "top")
            : Component(parent, name) {}

        void create_ports() override { new (this->io_storage_) io_type; }

        void describe() override {
            CH_MODULE(DataProcessor, proc1);
            CH_MODULE(DataProcessor, proc2);

            // 连接端口: 顶层输入 -> proc1输入 -> proc2输入 -> 顶层输出
            proc1.io().input <<= io().in_data;
            proc1.io().enable <<= io().enable1;
            proc2.io().input <<= proc1.io().output;
            proc2.io().enable <<= io().enable2;
            io().out_data <<= proc2.io().output;
        }
    };

    ch_device<Top> device;
    Simulator sim(device.context());

    // 测试不同的输入值和使能信号组合
    auto in_data = device.io().in_data;
    auto out_data = device.io().out_data;
    auto enable1 = device.io().enable1;
    auto enable2 = device.io().enable2;

    // 测试使能信号都为真时
    sim.set_input_value(enable1, 1);
    sim.set_input_value(enable2, 1);
    for (uint64_t i = 0; i < 16; ++i) {
        sim.set_input_value(in_data, i);
        sim.tick();
        REQUIRE(sim.get_value(out_data) == i);
    }

    // 测试使能信号不同组合
    sim.set_input_value(in_data, 5);
    sim.set_input_value(enable1, 1); // proc1使能
    sim.set_input_value(enable2, 0); // proc2不使能
    sim.tick();
    REQUIRE(sim.get_value(out_data) == 0); // 应该是0，因为proc2输出0

    sim.set_input_value(enable1, 0); // proc1不使能
    sim.set_input_value(enable2, 1); // proc2使能
    sim.tick();
    REQUIRE(sim.get_value(out_data) == 0); // 应该是0，因为proc1输出0
}