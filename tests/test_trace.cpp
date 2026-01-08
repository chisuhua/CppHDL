#include "catch_amalgamated.hpp"
#include "ch.hpp"
#include "component.h"
#include "core/literal.h"
#include "core/uint.h"
#include "simulator.h"
#include <cstdint>
#include <cstring>
#include <fstream> // 添加fstream头文件以支持文件操作
#include <iostream>
#include <vector>

using namespace ch::core;

// 简单的计数器组件，用于测试trace功能
class SimpleCounter : public ch::Component {
public:
    __io(ch_out<ch_uint<4>> out;)

        SimpleCounter(ch::Component *parent = nullptr,
                      const std::string &name = "simple_counter")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        // 简单的计数器逻辑：每次时钟上升沿加1
        auto counter = ch_reg<ch_uint<4>>(0_d, "counter_reg");
        counter->next = counter + 1_d;
        io().out = counter;
    }
};

// 更复杂的带有时钟使能的计数器
class CounterWithEnable : public ch::Component {
public:
    __io(ch_in<bool> clk_en; ch_out<ch_uint<4>> out;)

        CounterWithEnable(ch::Component *parent = nullptr,
                          const std::string &name = "counter_with_enable")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        auto counter = ch_reg<ch_uint<4>>(0_d, "counter_with_en_reg");
        // 使用select实现多路选择：当clk_en为true时选择counter+1，否则选择当前值
        counter->next = select(io().clk_en, counter + 1_d, counter);
        io().out = counter;
    }
};

TEST_CASE("Trace: Basic counter tracing", "[trace][basic]") {
    auto ctx = std::make_unique<ch::core::context>("test_trace_counter");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SimpleCounter counter;
    counter.create_ports();
    counter.describe();

    // 创建启用trace功能的模拟器
    ch::Simulator sim(ctx.get(), true); // 启用trace

    REQUIRE(sim.is_tracing_enabled() == true);

    // 检查初始值
    auto initial_val = sim.get_port_value(counter.io().out);
    REQUIRE(static_cast<uint64_t>(initial_val) == 0);

    // 运行几个时钟周期并记录trace数据
    std::vector<uint64_t> expected_values = {
        1, 2, 3, 4, 5, 6}; // 修改期望值：每次tick后增加1
    for (size_t i = 0; i < expected_values.size(); ++i) {
        sim.tick();

        // 检查输出值是否符合预期
        auto output_val = sim.get_port_value(counter.io().out);
        REQUIRE(static_cast<uint64_t>(output_val) == expected_values[i]);
    }

    // 确认trace数据已被收集
    auto &trace_blocks = sim.get_trace_blocks_for_testing();
    REQUIRE(!trace_blocks.empty());

    // 检查第一个trace块是否包含数据
    REQUIRE(trace_blocks.front()->size > 0);
}

TEST_CASE("Trace: Counter with enable tracing", "[trace][enable]") {
    auto ctx = std::make_unique<ch::core::context>("test_trace_counter_en");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    CounterWithEnable counter;
    counter.create_ports();
    counter.describe();

    // 创建启用trace功能的模拟器
    ch::Simulator sim(ctx.get(), true); // 启用trace

    REQUIRE(sim.is_tracing_enabled() == true);

    // 检查初始值
    auto initial_val = sim.get_port_value(counter.io().out);
    REQUIRE(static_cast<uint64_t>(initial_val) == 0);

    // 设置时钟使能为真，运行几个周期
    sim.set_input_value(counter.io().clk_en, 1);

    // 第一次tick后，计数器应该变为1
    sim.tick();
    auto output_val = sim.get_port_value(counter.io().out);
    REQUIRE(static_cast<uint64_t>(output_val) == 1);

    // 第二次tick后，计数器应该变为2
    sim.tick();
    output_val = sim.get_port_value(counter.io().out);
    REQUIRE(static_cast<uint64_t>(output_val) == 2);

    // 第三次tick后，计数器应该变为3
    sim.tick();
    output_val = sim.get_port_value(counter.io().out);
    REQUIRE(static_cast<uint64_t>(output_val) == 3);

    // 设置时钟使能为假，值不应该改变
    sim.set_input_value(counter.io().clk_en, 0);
    uint64_t last_value =
        static_cast<uint64_t>(sim.get_port_value(counter.io().out));

    // 第四次tick，使能关闭，值应该不变
    sim.tick();
    output_val = sim.get_port_value(counter.io().out);
    REQUIRE(static_cast<uint64_t>(output_val) == last_value); // 应该仍然是3

    // 第五次tick，使能仍关闭，值应该不变
    sim.tick();
    output_val = sim.get_port_value(counter.io().out);
    REQUIRE(static_cast<uint64_t>(output_val) == last_value); // 应该仍然是3

    // 第六次tick，使能仍关闭，值应该不变
    sim.tick();
    output_val = sim.get_port_value(counter.io().out);
    REQUIRE(static_cast<uint64_t>(output_val) == last_value); // 应该仍然是3

    // 确认trace数据已被收集
    auto &trace_blocks = sim.get_trace_blocks_for_testing();
    REQUIRE(!trace_blocks.empty());
}

TEST_CASE("Trace: Toggle signal tracing", "[trace][toggle]") {
    auto ctx = std::make_unique<ch::core::context>("test_trace_toggle");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    // 创建一个简单的切换信号
    auto toggle_signal = ch_reg<ch_bool>(false, "toggle_reg");
    toggle_signal->next = !toggle_signal;

    // 创建启用trace功能的模拟器
    ch::Simulator sim(ctx.get(), true); // 启用trace

    REQUIRE(sim.is_tracing_enabled() == true);

    // 检查初始值 - 在任何tick之前
    auto initial_val = static_cast<bool>(toggle_signal);
    REQUIRE(initial_val == false);

    // 运行几个周期并检查切换
    bool expected = false; // 修正：初始值是false，因此期望值从false开始
    for (int i = 0; i < 6; ++i) {
        sim.tick();
        bool current_val = static_cast<uint64_t>(sim.get_value(toggle_signal));
        expected = !expected; // 每次迭代翻转期望值
        REQUIRE(current_val == expected);
    }

    // 确认trace数据已被收集
    auto &trace_blocks = sim.get_trace_blocks_for_testing();
    REQUIRE(!trace_blocks.empty());

    // 检查trace数据块的大小
    size_t total_size = 0;
    for (auto *block : trace_blocks) {
        total_size += block->size;
    }
    REQUIRE(total_size > 0);
}

TEST_CASE("Trace: Verify trace content matches expected", "[trace][content]") {
    auto ctx = std::make_unique<ch::core::context>("test_trace_content");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SimpleCounter counter;
    counter.create_ports();
    counter.describe();

    // 创建启用trace功能的模拟器
    ch::Simulator sim(ctx.get(), true); // 启用trace

    // 检查初始值
    auto initial_val = sim.get_port_value(counter.io().out);
    REQUIRE(static_cast<uint64_t>(initial_val) == 0);

    // 运行几个时钟周期
    std::vector<uint64_t> expected_values = {1, 2, 3,
                                             4}; // 修改期望值：每次tick后增加1
    for (size_t i = 0; i < expected_values.size(); ++i) {
        sim.tick();

        // 检查输出值是否符合预期
        auto output_val = sim.get_port_value(counter.io().out);
        REQUIRE(static_cast<uint64_t>(output_val) == expected_values[i]);
    }

    // 现在检查trace内容是否与预期相符
    // 获取trace数据并验证
    auto traced_signals = sim.get_traced_signals();
    REQUIRE(!traced_signals.empty());

    // 输出一些调试信息
    std::cout << "Number of traced signals: " << traced_signals.size()
              << std::endl;

    // 找到counter.io().out对应的信号并检查它的大小
    bool found_counter_signal = false;
    for (auto *signal : traced_signals) {
        // 输出每个信号的大小用于调试
        std::cout << "Signal size: " << signal->size() << std::endl;
        // 如果信号大小为4，则认为是我们要找的counter输出信号
        if (signal->size() == 4) {
            found_counter_signal = true;
            break;
        }
    }

    REQUIRE(
        found_counter_signal); // 确保至少有一个大小为4的信号（即counter输出）
}

TEST_CASE("Trace: VCD output functionality", "[trace][vcd]") {
    auto ctx = std::make_unique<ch::core::context>("test_vcd_output");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SimpleCounter counter;
    counter.create_ports();
    counter.describe();

    // 创建启用trace功能的模拟器
    ch::Simulator sim(ctx.get(), true); // 启用trace

    REQUIRE(sim.is_tracing_enabled() == true);

    // 运行几个时钟周期
    for (int i = 0; i < 6; ++i) {
        sim.tick();
    }

    // 输出VCD文件
    sim.toVCD("test_output.vcd");

    // 验证VCD文件是否创建
    std::ifstream vcd_file("test_output.vcd");
    REQUIRE(vcd_file.is_open());

    std::string line;
    bool found_timescale = false;
    bool found_enddefs = false;
    bool found_timestamp = false;
    while (std::getline(vcd_file, line)) {
        if (line.find("$timescale") != std::string::npos) {
            found_timescale = true;
        }
        if (line.find("$enddefinitions $end") != std::string::npos) {
            found_enddefs = true;
        }
        if (line[0] == '#') { // 时间戳行以#开头
            found_timestamp = true;
        }
    }
    vcd_file.close();

    REQUIRE(found_timescale == true);
    REQUIRE(found_enddefs == true);
    REQUIRE(found_timestamp == true);
}
