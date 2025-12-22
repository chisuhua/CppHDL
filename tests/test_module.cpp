#include "catch_amalgamated.hpp"
#include "ch.hpp"
#include "codegen_verilog.h"
#include "module.h"
#include "simulator.h"
#include <cstdint>
#include <iostream>

using namespace ch;
using namespace ch::core;

// 定义一个简单的测试模块，用于测试输入和输出
template <unsigned N> class TestModule : public Component {
public:
    __io(ch_in<ch_uint<N>> in_port; ch_out<ch_uint<N>> out_port;
         ch_out<ch_uint<N>> incremented;)

        TestModule(Component *parent = nullptr,
                   const std::string &name = "TestModule")
        : Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        // 直接连接输入到输出
        io().out_port = io().in_port;
        // 输出一个递增的值
        io().incremented = io().in_port + 1_d;
    }
};

// 定义另一个测试模块，用于测试多个模块连接
template <unsigned N> class AdderModule : public Component {
public:
    __io(ch_in<ch_uint<N>> a; ch_in<ch_uint<N>> b; ch_out<ch_uint<N + 1>> sum;)

        AdderModule(Component *parent = nullptr,
                    const std::string &name = "AdderModule")
        : Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override { io().sum = io().a + io().b; }
};

// 顶层模块，用于测试CH_MODULE实例化和连接
class TopModuleTest : public Component {
public:
    __io(ch_in<ch_uint<8>> in_data; ch_out<ch_uint<8>> out_data;
         ch_out<ch_uint<8>> incremented_data; ch_out<ch_uint<9>> summed_data;)

        TopModuleTest(Component *parent = nullptr,
                      const std::string &name = "TopModuleTest")
        : Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        // 实例化第一个模块
        CH_MODULE(TestModule<8>, test_module);

        // 实例化第二个模块
        CH_MODULE(AdderModule<8>, adder_module);

        // 连接信号
        // 从顶层输入到test_module输入
        test_module.io().in_port = io().in_data;

        // 从test_module输出到顶层输出
        io().out_data = test_module.io().out_port;
        io().incremented_data = test_module.io().incremented;

        // 连接两个模块到adder_module
        adder_module.io().a = test_module.io().in_port;
        adder_module.io().b = test_module.io().incremented;
        io().summed_data = adder_module.io().sum;
    }
};

TEST_CASE("CH_MODULE - Basic Instantiation", "[module][instantiation]") {
    class TestTop : public Component {
    public:
        __io(ch_out<ch_uint<4>> out;)

            TestTop(Component *parent = nullptr,
                    const std::string &name = "test_top")
            : Component(parent, name) {}

        void create_ports() override { new (io_storage_) io_type; }

        void describe() override {
            CH_MODULE(TestModule<4>, mod);
            io().out = mod.io().in_port; // 简单连接以测试实例化
        }
    };

    REQUIRE_NOTHROW([&]() { ch_device<TestTop> device; }());
}

TEST_CASE("CH_MODULE - Signal Connection", "[module][connection]") {
    ch_device<TopModuleTest> device;
    Simulator simulator(device.context());

    // 设置输入值
    simulator.set_input_value(device.instance().io().in_data, 42);

    // 执行仿真
    simulator.tick();

    // 检查输出值
    auto out_data = simulator.get_value(device.instance().io().out_data);
    auto incremented_data =
        simulator.get_value(device.instance().io().incremented_data);
    auto summed_data = simulator.get_value(device.instance().io().summed_data);

    REQUIRE(out_data == 42);
    REQUIRE(incremented_data == 43);
    REQUIRE(static_cast<uint64_t>(summed_data) == 85); // 42 + 43 = 85
}

TEST_CASE("CH_MODULE - Hierarchical Naming", "[module][hierarchy]") {
    ch_device<TopModuleTest> device;

    // 检查模块是否正确创建了层次结构
    REQUIRE(device.instance().child_count() == 2);

    const auto &children = device.instance().children();
    REQUIRE(children.size() == 2);

    // 检查子模块名称
    bool found_test_module = false;
    bool found_adder_module = false;

    for (const auto &child : children) {
        if (child->name() == "test_module") {
            found_test_module = true;
        } else if (child->name() == "adder_module") {
            found_adder_module = true;
        }
    }

    REQUIRE(found_test_module);
    REQUIRE(found_adder_module);
}

// 测试嵌套模块
class NestedChild : public Component {
public:
    __io(ch_in<ch_uint<4>> in_port; ch_out<ch_uint<4>> out_port;)

        NestedChild(Component *parent = nullptr,
                    const std::string &name = "NestedChild")
        : Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override { io().out_port = io().in_port; }
};

class NestedParent : public Component {
public:
    __io(ch_in<ch_uint<4>> in_port; ch_out<ch_uint<4>> out_port;)

        NestedParent(Component *parent = nullptr,
                     const std::string &name = "NestedParent")
        : Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        CH_MODULE(NestedChild, child);
        child.io().in_port = io().in_port;
        io().out_port = child.io().out_port;
    }
};

TEST_CASE("CH_MODULE - Nested Modules", "[module][nested]") {
    class NestedTop : public Component {
    public:
        __io(ch_in<ch_uint<4>> in_port; ch_out<ch_uint<4>> out_port;)

            NestedTop(Component *parent = nullptr,
                      const std::string &name = "NestedTop")
            : Component(parent, name) {}

        void create_ports() override { new (io_storage_) io_type; }

        void describe() override {
            CH_MODULE(NestedParent, parent);
            parent.io().in_port = io().in_port;
            io().out_port = parent.io().out_port;
        }
    };

    ch_device<NestedTop> device;
    Simulator simulator(device.context());

    simulator.set_input_value(device.instance().io().in_port, 10);
    simulator.tick();

    auto result = simulator.get_value(device.instance().io().out_port);
    REQUIRE(static_cast<uint64_t>(result) == 10);
}