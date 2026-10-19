#include "catch_amalgamated.hpp"
#include "ch.hpp"
#include "codegen_dag.h"
#include "codegen_verilog.h"
#include "dual_backend_fixture.h"
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
            ch::ch_module<SimpleModule<4>> child_module{"child_module"};
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
            ch::ch_module<SimpleModule<4>> mod{"mod"};

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

// dual-backend-test-coverage (Phase 1): CH_MODULE - Simulation Value
// Transfer driven through InterpreterBackend + VerilatorBackend.
// Pass-through SimpleModule<4> (Top.io().in_data → mod → Top.io().out_data).
class ModuleValueTransferTop : public Component {
public:
    __io(ch_in<ch_uint<4>> in_data; ch_out<ch_uint<4>> out_data;)

    ModuleValueTransferTop(Component *parent = nullptr,
                           const std::string &name = "top_xfer")
        : Component(parent, name) {}

    void create_ports() override { new (this->io_storage_) io_type; }

    void describe() override {
        ch::ch_module<SimpleModule<4>> mod{"mod"};
        mod.io().in_port <<= io().in_data;
        io().out_data <<= mod.io().out_port;
    }
};

DUAL_BACKEND_TEST_PAIR(
    ModuleValueTransferTop,
    "CH_MODULE - Simulation Value Transfer",
    "[module][simulation][dual]",
    [&]() {
        for (uint64_t i = 0; i < 16; ++i) {
            sim.set_input_value(dev.instance().io().in_data, i);
            sim.tick();
            REQUIRE(sim.get_value(dev.instance().io().out_data) == i);
        }
    });

// dual-backend-test-coverage (Phase 1): CH_MODULE - Connection Between
// Child Modules driven through both backends. Two pass-through
// SimpleModule<4> chained: top.in → mod1 → mod2 → top.out.
class ModuleChainTop : public Component {
public:
    __io(ch_in<ch_uint<4>> in_data; ch_out<ch_uint<4>> out_data;)

    ModuleChainTop(Component *parent = nullptr,
                   const std::string &name = "top_chain")
        : Component(parent, name) {}

    void create_ports() override { new (this->io_storage_) io_type; }

    void describe() override {
        ch::ch_module<SimpleModule<4>> mod1{"mod1"};
        ch::ch_module<SimpleModule<4>> mod2{"mod2"};
        mod1.io().in_port <<= io().in_data;
        mod2.io().in_port <<= mod1.io().out_port;
        io().out_data <<= mod2.io().out_port;
    }
};

DUAL_BACKEND_TEST_PAIR(
    ModuleChainTop,
    "CH_MODULE - Connection Between Child Modules",
    "[module][child-connection][dual]",
    [&]() {
        for (uint64_t i = 0; i < 16; ++i) {
            sim.set_input_value(dev.instance().io().in_data, i);
            sim.tick();
            REQUIRE(sim.get_value(dev.instance().io().out_data) == i);
        }
    });

// dual-backend-test-coverage (Phase 1): CH_MODULE - Connection Between
// Different IO Directions (select-based DataProcessor chained) driven
// through both backends.
class ModuleDataProcessor : public Component {
public:
    __io(ch_in<ch_uint<4>> input; ch_out<ch_uint<4>> output;
         ch_in<bool> enable;)

    ModuleDataProcessor(Component *parent = nullptr,
                        const std::string &name = "DataProc")
        : Component(parent, name) {}

    void create_ports() override { new (this->io_storage_) io_type; }

    void describe() override {
        io().output <<= select(io().enable, io().input, 0_d);
    }
};

class ModuleIoDirectionTop : public Component {
public:
    __io(ch_in<ch_uint<4>> in_data; ch_out<ch_uint<4>> out_data;
         ch_in<bool> enable1; ch_in<bool> enable2;)

    ModuleIoDirectionTop(Component *parent = nullptr,
                         const std::string &name = "top_io_dir")
        : Component(parent, name) {}

    void create_ports() override { new (this->io_storage_) io_type; }

    void describe() override {
        ch::ch_module<ModuleDataProcessor> proc1{"proc1"};
        ch::ch_module<ModuleDataProcessor> proc2{"proc2"};
        proc1.io().input <<= io().in_data;
        proc1.io().enable <<= io().enable1;
        proc2.io().input <<= proc1.io().output;
        proc2.io().enable <<= io().enable2;
        io().out_data <<= proc2.io().output;
    }
};

DUAL_BACKEND_TEST_PAIR(
    ModuleIoDirectionTop,
    "CH_MODULE - Connection Between Different IO Directions",
    "[module][io-direction][dual]",
    [&]() {
        // Enable both processors — data passes through.
        sim.set_input_value(dev.instance().io().enable1, 1);
        sim.set_input_value(dev.instance().io().enable2, 1);
        for (uint64_t i = 0; i < 16; ++i) {
            sim.set_input_value(dev.instance().io().in_data, i);
            sim.tick();
            REQUIRE(sim.get_value(dev.instance().io().out_data) == i);
        }

        // proc2 disabled — outputs 0.
        sim.set_input_value(dev.instance().io().in_data, 5);
        sim.set_input_value(dev.instance().io().enable1, 1);
        sim.set_input_value(dev.instance().io().enable2, 0);
        sim.tick();
        REQUIRE(sim.get_value(dev.instance().io().out_data) == 0);

        // proc1 disabled — outputs 0.
        sim.set_input_value(dev.instance().io().enable1, 0);
        sim.set_input_value(dev.instance().io().enable2, 1);
        sim.tick();
        REQUIRE(sim.get_value(dev.instance().io().out_data) == 0);
    });