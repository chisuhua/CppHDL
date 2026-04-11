/**
 * @file test_uint_compare.cpp
 * @brief ch_uint<32> 比较操作测试
 * 
 * 验证 ch_uint 比较运算（<, >, ==）在仿真中的正确性
 */

#include "catch_amalgamated.hpp"
#include "ch.hpp"
#include "simulator.h"

using namespace ch;
using namespace ch::core;

TEST_CASE("ch_uint<32> comparison - less than", "[uint][compare]") {
    struct TestCompare : public ch::Component {
        __io(
            ch_in<ch_uint<32>> a;
            ch_in<ch_uint<32>> b;
            ch_out<ch_bool> less;
        )

        TestCompare(ch::Component* parent = nullptr, const std::string& name = "test_compare")
            : ch::Component(parent, name) {}

        void create_ports() override {
            new (io_storage_) io_type;
        }

        void describe() override {
            io().less = io().a < io().b;
        }
    };

    auto ctx = std::make_unique<ch::core::context>("test_uint_less");
    ch::core::ctx_swap ctx_guard(ctx.get());

    ch::ch_device<TestCompare> unit;
    ch::Simulator sim(unit.context());

    // 测试 1: 10 < 20 → true
    sim.set_input_value(unit.instance().io().a, 10);
    sim.set_input_value(unit.instance().io().b, 20);
    sim.tick();
    auto less = static_cast<uint64_t>(sim.get_port_value(unit.instance().io().less));
    REQUIRE(less == 1);

    // 测试 2: 30 < 20 → false
    sim.set_input_value(unit.instance().io().a, 30);
    sim.set_input_value(unit.instance().io().b, 20);
    sim.tick();
    less = static_cast<uint64_t>(sim.get_port_value(unit.instance().io().less));
    REQUIRE(less == 0);

    // 测试 3: 20 < 20 → false
    sim.set_input_value(unit.instance().io().a, 20);
    sim.set_input_value(unit.instance().io().b, 20);
    sim.tick();
    less = static_cast<uint64_t>(sim.get_port_value(unit.instance().io().less));
    REQUIRE(less == 0);
}

TEST_CASE("ch_uint<32> comparison - greater than", "[uint][compare]") {
    struct TestCompareGT : public ch::Component {
        __io(
            ch_in<ch_uint<32>> a;
            ch_in<ch_uint<32>> b;
            ch_out<ch_bool> greater;
        )

        TestCompareGT(ch::Component* parent = nullptr, const std::string& name = "test_compare_gt")
            : ch::Component(parent, name) {}

        void create_ports() override {
            new (io_storage_) io_type;
        }

        void describe() override {
            io().greater = io().a > io().b;
        }
    };

    auto ctx = std::make_unique<ch::core::context>("test_uint_greater");
    ch::core::ctx_swap ctx_guard(ctx.get());

    ch::ch_device<TestCompareGT> unit;
    ch::Simulator sim(unit.context());

    // 测试 1: 20 > 10 → true
    sim.set_input_value(unit.instance().io().a, 20);
    sim.set_input_value(unit.instance().io().b, 10);
    sim.tick();
    auto greater = static_cast<uint64_t>(sim.get_port_value(unit.instance().io().greater));
    REQUIRE(greater == 1);

    // 测试 2: 10 > 20 → false
    sim.set_input_value(unit.instance().io().a, 10);
    sim.set_input_value(unit.instance().io().b, 20);
    sim.tick();
    greater = static_cast<uint64_t>(sim.get_port_value(unit.instance().io().greater));
    REQUIRE(greater == 0);
}

TEST_CASE("ch_uint<32> comparison - equal", "[uint][compare][equals]") {
    struct TestCompareEq : public ch::Component {
        __io(
            ch_in<ch_uint<32>> a;
            ch_in<ch_uint<32>> b;
            ch_out<ch_bool> equal;
        )

        TestCompareEq(ch::Component* parent = nullptr, const std::string& name = "test_compare_eq")
            : ch::Component(parent, name) {}

        void create_ports() override {
            new (io_storage_) io_type;
        }

        void describe() override {
            io().equal = io().a == io().b;
        }
    };

    auto ctx = std::make_unique<ch::core::context>("test_uint_equal");
    ch::core::ctx_swap ctx_guard(ctx.get());

    ch::ch_device<TestCompareEq> unit;
    ch::Simulator sim(unit.context());

    // 测试 1: 42 == 42 → true
    sim.set_input_value(unit.instance().io().a, 42);
    sim.set_input_value(unit.instance().io().b, 42);
    sim.tick();
    auto equal = static_cast<uint64_t>(sim.get_port_value(unit.instance().io().equal));
    REQUIRE(equal == 1);

    // 测试 2: 42 == 43 → false
    sim.set_input_value(unit.instance().io().a, 42);
    sim.set_input_value(unit.instance().io().b, 43);
    sim.tick();
    equal = static_cast<uint64_t>(sim.get_port_value(unit.instance().io().equal));
    REQUIRE(equal == 0);
}
