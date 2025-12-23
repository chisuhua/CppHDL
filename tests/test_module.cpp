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
