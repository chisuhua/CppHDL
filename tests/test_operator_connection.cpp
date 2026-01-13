#include "catch_amalgamated.hpp"
#include "ch.hpp"
#include "component.h"
#include "core/bool.h"
#include "core/context.h"
#include "core/io.h"
#include "core/reg.h"
#include "core/uint.h"
#include "simulator.h"

using namespace ch::core;

TEST_CASE("operator<<= - ch_uint connection", "[connection][operator]") {
    context ctx;

    ch_uint<8> signal1 = 10_d;
    ch_uint<8> signal2 = 20_d;

    // 使用operator<<=连接两个信号
    signal1 <<= signal2;

    // 创建一个简单的模块来测试连接
    class TestModule : public Component {
    public:
        __io(ch_in<ch_uint<8>> in_port; ch_out<ch_uint<8>> out_port;)

            TestModule(Component *parent = nullptr,
                       const std::string &name = "test_mod")
            : Component(parent, name) {}

        void create_ports() override { new (this->io_storage_) io_type; }

        void describe() override {
            ch_uint<8> temp_sig = io().in_port;
            io().out_port <<= temp_sig; // 使用operator<<=连接
        }
    };

    ch_device<TestModule> test_dev;
    Simulator sim_test(test_dev.context());

    // 设置输入并检查输出
    auto in_port = test_dev.io().in_port;
    auto out_port = test_dev.io().out_port;

    sim_test.set_input_value(in_port, 42);
    sim_test.tick();
    REQUIRE(sim_test.get_value(out_port) == 42);
}

TEST_CASE("operator<<= - ch_reg connection",
          "[connection][register][operator]") {
    context ctx;

    ch_reg<ch_uint<8>> reg(0_d, "test_reg"); // 添加初始值参数
    ch_uint<8> signal = 15_d;

    // 使用operator<<=连接寄存器
    reg <<= signal;

    // 创建一个模块测试寄存器连接
    class CounterModule : public Component {
    public:
        __io(ch_in<ch_bool> clk; ch_in<ch_bool> rst;
             ch_out<ch_uint<8>> count_out;)

            CounterModule(Component *parent = nullptr,
                          const std::string &name = "counter")
            : Component(parent, name) {}

        void create_ports() override { new (this->io_storage_) io_type; }

        void describe() override {
            ch_reg<ch_uint<8>> counter_reg(0_d, "counter");

            // 使用operator<<=连接寄存器的下一个值
            counter_reg <<= select(io().rst, 0_d, counter_reg + 1_d);

            io().count_out <<= counter_reg;
        }
    };

    ch_device<CounterModule> counter_dev;
    Simulator sim_counter(counter_dev.context());

    auto clk = counter_dev.io().clk;
    auto rst = counter_dev.io().rst;
    auto count_out = counter_dev.io().count_out;

    // 测试复位
    sim_counter.set_input_value(rst, 1);
    sim_counter.set_input_value(clk, 1);
    sim_counter.tick();
    REQUIRE(sim_counter.get_value(count_out) == 0);

    // 释放复位并递增计数
    sim_counter.set_input_value(rst, 0);
    sim_counter.tick();
    REQUIRE(sim_counter.get_value(count_out) == 1);

    sim_counter.tick();
    REQUIRE(sim_counter.get_value(count_out) == 2);
}

TEST_CASE("operator<<= - complex connection scenario",
          "[connection][complex][operator]") {
    context ctx;

    // 创建一个更复杂的连接场景
    class ComplexConnectionModule : public Component {
    public:
        __io(ch_in<ch_uint<4>> input_a; ch_in<ch_uint<4>> input_b;
             ch_out<ch_uint<4>> output_sum; ch_out<ch_uint<4>> output_max;)

            ComplexConnectionModule(Component *parent = nullptr,
                                    const std::string &name = "complex_conn")
            : Component(parent, name) {}

        void create_ports() override { new (this->io_storage_) io_type; }

        void describe() override {
            // 计算输入之和
            ch_uint<5> sum = io().input_a + io().input_b;

            // 计算最大值
            ch_uint<4> max_val =
                select(io().input_a > io().input_b, io().input_a, io().input_b);

            // 使用operator<<=连接到输出
            io().output_sum <<= bits<3, 0>(sum); // 修复：使用模板函数进行切片
            io().output_max <<= max_val;
        }
    };

    ch_device<ComplexConnectionModule> complex_dev;
    Simulator sim_complex(complex_dev.context());

    auto input_a = complex_dev.io().input_a;
    auto input_b = complex_dev.io().input_b;
    auto output_sum = complex_dev.io().output_sum;
    auto output_max = complex_dev.io().output_max;

    // 测试不同的输入组合
    struct TestCase {
        uint64_t a, b;
        uint64_t expected_sum, expected_max;
    };

    std::vector<TestCase> test_cases = {
        {2, 3, 5, 3},
        {10, 5, 15, 10},
        {0, 7, 7, 7},
        {15, 15, 14, 15} // 和会溢出，所以取低4位是14
    };

    for (const auto &tc : test_cases) {
        sim_complex.set_input_value(input_a, tc.a);
        sim_complex.set_input_value(input_b, tc.b);
        sim_complex.tick();

        REQUIRE(sim_complex.get_value(output_sum) == tc.expected_sum);
        REQUIRE(sim_complex.get_value(output_max) == tc.expected_max);
    }
}

TEST_CASE("operator<<= - ch_bool connection", "[connection][bool][operator]") {
    context ctx;

    class BoolConnectionModule : public Component {
    public:
        __io(ch_in<ch_bool> input_flag; ch_out<ch_bool> output_flag;
             ch_out<ch_uint<1>> output_uint1;)

            BoolConnectionModule(Component *parent = nullptr,
                                 const std::string &name = "bool_conn")
            : Component(parent, name) {}

        void create_ports() override { new (this->io_storage_) io_type; }

        void describe() override {
            // 连接bool到bool
            io().output_flag <<= io().input_flag;

            // 连接bool到uint<1>
            io().output_uint1 <<= io().input_flag;
        }
    };

    ch_device<BoolConnectionModule> bool_dev;
    Simulator sim_bool(bool_dev.context());

    auto input_flag = bool_dev.io().input_flag;
    auto output_flag = bool_dev.io().output_flag;
    auto output_uint1 = bool_dev.io().output_uint1;

    // 测试false/0
    sim_bool.set_input_value(input_flag, 0);
    sim_bool.tick();
    REQUIRE(sim_bool.get_value(output_flag) == 0);
    REQUIRE(sim_bool.get_value(output_uint1) == 0);

    // 测试true/1
    sim_bool.set_input_value(input_flag, 1);
    sim_bool.tick();
    REQUIRE(sim_bool.get_value(output_flag) == 1);
    REQUIRE(sim_bool.get_value(output_uint1) == 1);
}