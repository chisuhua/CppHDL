#include "catch_amalgamated.hpp"
#include "ch.hpp"
#include "codegen_dag.h"
#include "component.h"
#include "core/bool.h"
#include "core/context.h"
#include "core/io.h"
#include "core/reg.h"
#include "core/uint.h"
#include "simulator.h"

using namespace ch::core;

TEST_CASE("ch_bool operator<<= with ch_bool", "[bool][connection][operator]") {
    class BoolConnectionModule : public Component {
    public:
        __io(ch_in<ch_bool> input_bool; ch_out<ch_bool> output_bool;)

            BoolConnectionModule(Component *parent = nullptr,
                                 const std::string &name = "bool_conn")
            : Component(parent, name) {}

        void create_ports() override { new (this->io_storage_) io_type; }

        void describe() override {
            ch_bool temp_signal;

            // 使用operator<<=连接ch_bool信号
            temp_signal <<= io().input_bool;
            io().output_bool <<= temp_signal;
        }
    };

    ch_device<BoolConnectionModule> dev;
    Simulator sim(dev.context());

    auto input_bool = dev.io().input_bool;
    auto output_bool = dev.io().output_bool;

    // 测试false/0
    sim.set_input_value(input_bool, 0);
    sim.tick();
    REQUIRE(sim.get_value(output_bool) == 0);

    // 测试true/1
    sim.set_input_value(input_bool, 1);
    sim.tick();
    toDAG("bool1.dot", dev.context(), sim);
    REQUIRE(sim.get_value(output_bool) == 1);
}

TEST_CASE("ch_bool operator<<= with ch_uint<1>",
          "[bool][connection][operator][ch_uint]") {
    class BoolUintConnectionModule : public Component {
    public:
        __io(ch_in<ch_uint<1>> input_uint1; ch_out<ch_bool> output_bool;)

            BoolUintConnectionModule(Component *parent = nullptr,
                                     const std::string &name = "bool_uint_conn")
            : Component(parent, name) {}

        void create_ports() override { new (this->io_storage_) io_type; }

        void describe() override {
            ch_bool bool_signal;

            // 使用operator<<=连接ch_uint<1>到ch_bool
            bool_signal <<= io().input_uint1;
            io().output_bool <<= bool_signal;
        }
    };

    ch_device<BoolUintConnectionModule> dev;
    Simulator sim(dev.context());

    auto input_uint1 = dev.io().input_uint1;
    auto output_bool = dev.io().output_bool;

    // 测试0
    sim.set_input_value(input_uint1, 0);
    sim.tick();
    REQUIRE(sim.get_value(output_bool) == 0);

    // 测试1
    sim.set_input_value(input_uint1, 1);
    sim.tick();
    REQUIRE(sim.get_value(output_bool) == 1);
}

TEST_CASE("ch_bool operator<<= with ch_reg<ch_bool>",
          "[bool][connection][operator][ch_reg]") {
    class BoolRegConnectionModule : public Component {
    public:
        __io(ch_in<ch_bool> input_bool; ch_in<ch_bool> clk_in;
             ch_in<ch_bool> rst_in; ch_out<ch_bool> output_bool;)

            BoolRegConnectionModule(Component *parent = nullptr,
                                    const std::string &name = "bool_reg_conn")
            : Component(parent, name) {}

        void create_ports() override { new (this->io_storage_) io_type; }

        void describe() override {
            ch_reg<ch_bool> bool_reg(0_b, "bool_reg");

            // 使用operator<<=连接ch_bool到ch_reg<ch_bool>
            bool_reg <<= io().input_bool;
            io().output_bool <<= bool_reg;
        }
    };

    ch_device<BoolRegConnectionModule> dev;
    Simulator sim(dev.context());

    auto input_bool = dev.io().input_bool;
    auto clk_in = dev.io().clk_in;
    auto rst_in = dev.io().rst_in;
    auto output_bool = dev.io().output_bool;

    // 测试false/0
    sim.set_input_value(input_bool, 0);
    sim.set_input_value(clk_in, 1);
    sim.set_input_value(rst_in, 0);
    sim.tick();
    REQUIRE(sim.get_value(output_bool) == 0);

    // 测试true/1
    sim.set_input_value(input_bool, 1);
    sim.tick();
    REQUIRE(sim.get_value(output_bool) == 1);
}

TEST_CASE("ch_bool operator<<= with literal",
          "[bool][connection][operator][literal]") {
    class BoolLiteralConnectionModule : public Component {
    public:
        __io(ch_out<ch_bool> output_bool;)

            BoolLiteralConnectionModule(
                Component *parent = nullptr,
                const std::string &name = "bool_lit_conn")
            : Component(parent, name) {}

        void create_ports() override { new (this->io_storage_) io_type; }

        void describe() override {
            ch_bool bool_signal(1_b);
            io().output_bool <<= bool_signal;
        }
    };

    ch_device<BoolLiteralConnectionModule> dev;
    Simulator sim(dev.context());

    auto output_bool = dev.io().output_bool;

    sim.tick();
    REQUIRE(sim.get_value(output_bool) == 1);
}