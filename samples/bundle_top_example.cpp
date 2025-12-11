// samples/bundle_top_example.cpp
#include "ch.hpp"
#include "codegen_verilog.h"
#include "component.h"
#include "core/bundle/bundle_utils.h"
#include "io/common_bundles.h"
#include "module.h"
#include "simulator.h"
#include <iostream>

using namespace ch::core;

// 简单的组合逻辑模块示例
template <typename T> class SimpleLogic : public ch::Component {
public:
    // 使用Bundle作为IO接口
    fifo_bundle<T> io;

    SimpleLogic(ch::Component *parent = nullptr,
                const std::string &name = "simple_logic")
        : ch::Component(parent, name) {
        io.as_slave();
    }

    void describe() override {
        // 简单的组合逻辑：输出等于输入
        io.data = io.data;
        io.full = io.push;
        io.empty = !io.pop;
    }
};

// 顶层模块 - 直接使用Bundle作为IO接口
class Top : public ch::Component {
public:
    // 直接使用Bundle作为顶层IO接口
    fifo_bundle<ch_uint<2>> io_bundle;

    Top(ch::Component *parent = nullptr, const std::string &name = "top")
        : ch::Component(parent, name) {
        // 设置IO方向为master（作为顶层模块，对外提供接口）
        io_bundle.as_master();
    }

    void describe() override {
        // 创建子模块实例
        ch::ch_module<SimpleLogic<ch_uint<2>>> logic_inst{"logic_inst"};

        // 连接Bundle
        connect(io_bundle, logic_inst.instance().io);
    }
};

int main() {
    std::cout << "Bundle Top Example - demonstrating direct Bundle usage as "
                 "top-level IO"
              << std::endl;

    // 创建顶层模块设备
    ch::ch_device<Top> top_device;

    // 注意：这个示例展示的是接口设计，实际仿真需要额外的工作来连接到仿真器

    std::cout << "Example completed successfully!" << std::endl;
    std::cout << "This demonstrates how to use Bundles directly as top-level "
                 "IO interfaces."
              << std::endl;
    std::cout << "In a full implementation, we would also need to connect "
                 "these to the simulator."
              << std::endl;

    return 0;
}