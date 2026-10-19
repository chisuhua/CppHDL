/**
 * @file test_uint_compare.cpp
 * @brief ch_uint<32> 比较操作测试 — dual-backend (Phase 1)
 *
 * 验证 ch_uint 比较运算（<, >, ==）在 InterpreterBackend 与
 * VerilatorBackend 下的逐周期 parity。
 */

#include "catch_amalgamated.hpp"
#include "ch.hpp"
#include "dual_backend_fixture.h"
#include "simulator.h"

using namespace ch;
using namespace ch::core;

namespace {

struct TestCompare : public ch::Component {
    __io(
        ch_in<ch_uint<32>> a;
        ch_in<ch_uint<32>> b;
        ch_out<ch_bool> less;
    )

    TestCompare(ch::Component *parent = nullptr,
                const std::string &name = "test_compare")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override { io().less = io().a < io().b; }
};

struct TestCompareGT : public ch::Component {
    __io(
        ch_in<ch_uint<32>> a;
        ch_in<ch_uint<32>> b;
        ch_out<ch_bool> greater;
    )

    TestCompareGT(ch::Component *parent = nullptr,
                  const std::string &name = "test_compare_gt")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override { io().greater = io().a > io().b; }
};

struct TestCompareEq : public ch::Component {
    __io(
        ch_in<ch_uint<32>> a;
        ch_in<ch_uint<32>> b;
        ch_out<ch_bool> equal;
    )

    TestCompareEq(ch::Component *parent = nullptr,
                  const std::string &name = "test_compare_eq")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override { io().equal = io().a == io().b; }
};

} // namespace

// dual-backend-test-coverage (Phase 1): 3 TEST_CASEs (less / greater /
// equal) driven through both backends.
DUAL_BACKEND_TEST_PAIR(
    TestCompare,
    "ch_uint<32> comparison - less than",
    "[uint][compare][less][dual]",
    [&]() {
        sim.set_input_value(dev.instance().io().a, 10);
        sim.set_input_value(dev.instance().io().b, 20);
        sim.tick();
        auto less = static_cast<uint64_t>(
            sim.get_port_value(dev.instance().io().less));
        REQUIRE(less == 1);

        sim.set_input_value(dev.instance().io().a, 30);
        sim.set_input_value(dev.instance().io().b, 20);
        sim.tick();
        less = static_cast<uint64_t>(
            sim.get_port_value(dev.instance().io().less));
        REQUIRE(less == 0);

        sim.set_input_value(dev.instance().io().a, 20);
        sim.set_input_value(dev.instance().io().b, 20);
        sim.tick();
        less = static_cast<uint64_t>(
            sim.get_port_value(dev.instance().io().less));
        REQUIRE(less == 0);
    });

DUAL_BACKEND_TEST_PAIR(
    TestCompareGT,
    "ch_uint<32> comparison - greater than",
    "[uint][compare][greater][dual]",
    [&]() {
        sim.set_input_value(dev.instance().io().a, 20);
        sim.set_input_value(dev.instance().io().b, 10);
        sim.tick();
        auto greater = static_cast<uint64_t>(
            sim.get_port_value(dev.instance().io().greater));
        REQUIRE(greater == 1);

        sim.set_input_value(dev.instance().io().a, 10);
        sim.set_input_value(dev.instance().io().b, 20);
        sim.tick();
        greater = static_cast<uint64_t>(
            sim.get_port_value(dev.instance().io().greater));
        REQUIRE(greater == 0);
    });

DUAL_BACKEND_TEST_PAIR(
    TestCompareEq,
    "ch_uint<32> comparison - equal",
    "[uint][compare][equals][dual]",
    [&]() {
        sim.set_input_value(dev.instance().io().a, 42);
        sim.set_input_value(dev.instance().io().b, 42);
        sim.tick();
        auto equal = static_cast<uint64_t>(
            sim.get_port_value(dev.instance().io().equal));
        REQUIRE(equal == 1);

        sim.set_input_value(dev.instance().io().a, 42);
        sim.set_input_value(dev.instance().io().b, 43);
        sim.tick();
        equal = static_cast<uint64_t>(
            sim.get_port_value(dev.instance().io().equal));
        REQUIRE(equal == 0);
    });