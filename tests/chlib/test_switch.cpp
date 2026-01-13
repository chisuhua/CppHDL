#include "catch_amalgamated.hpp"
#include "chlib/switch.h"
#include "codegen_dag.h"
#include "core/context.h"
#include "simulator.h"
#include <ch.hpp>
#include <memory>

using namespace ch;
using namespace ch::core;
using namespace chlib;

template <typename T> std::string to_binary_string(T value, size_t width) {
    std::bitset<64> bs(static_cast<uint64_t>(value));
    std::string result = bs.to_string();
    // Return only the requested width bits
    return result.substr(64 - width, width);
}

ch_uint<8> common(ch_uint<4> input) {

    // 使用case_函数创建case分支
    auto case1 = case_(make_uint<4>(1), make_uint<8>(10));
    auto case2 = case_(make_uint<4>(2), make_uint<8>(20));
    auto case3 = case_(make_uint<4>(3), make_uint<8>(30));

    // 使用switch_函数实现多路分支
    ch_uint<8> output = switch_(input, make_uint<8>(0), case1, case2, case3);
    return output;
}

TEST_CASE("Basic switch_ functionality", "[switch]") {
    SECTION("Test default case") {
        auto ctx = std::make_unique<context>("test_switch_basic");
        ctx_swap ctx_swapper(ctx.get());
        ch_uint<4> input = make_uint<4>(0);
        auto output = common(input);
        Simulator sim(ctx.get());

        sim.tick();
        toDAG("0.dot", ctx.get(), sim);

        auto inputValue = sim.get_value(input);
        auto outputValue = sim.get_value(output);
        std::cout << "Input: 0b" << to_binary_string(inputValue, 4)
                  << ", Output: 0b" << to_binary_string(outputValue, 8)
                  << std::endl;

        REQUIRE(outputValue == 0);
    }

    SECTION("Test first case") {
        auto ctx = std::make_unique<context>("test_switch_basic");
        ctx_swap ctx_swapper(ctx.get());
        ch_uint<4> input = make_uint<4>(1);
        auto output = common(input);
        Simulator sim(ctx.get());
        sim.tick();
        toDAG("1.dot", ctx.get(), sim);

        auto inputValue = sim.get_value(input);
        auto outputValue = sim.get_value(output);
        std::cout << "Input: 0b" << to_binary_string(inputValue, 4)
                  << ", Output: 0b" << to_binary_string(outputValue, 8)
                  << std::endl;

        REQUIRE(outputValue == 10);
    }

    SECTION("Test second case") {
        auto ctx = std::make_unique<context>("test_switch_basic");
        ctx_swap ctx_swapper(ctx.get());
        ch_uint<4> input = make_uint<4>(2);
        auto output = common(input);
        Simulator sim(ctx.get());
        sim.tick();

        auto inputValue = sim.get_value(input);
        auto outputValue = sim.get_value(output);
        std::cout << "Input: 0b" << to_binary_string(inputValue, 4)
                  << ", Output: 0b" << to_binary_string(outputValue, 8)
                  << std::endl;

        REQUIRE(outputValue == 20);
    }

    SECTION("Test third case") {
        auto ctx = std::make_unique<context>("test_switch_basic");
        ctx_swap ctx_swapper(ctx.get());
        ch_uint<4> input = make_uint<4>(3);
        auto output = common(input);
        Simulator sim(ctx.get());
        sim.tick();

        auto inputValue = sim.get_value(input);
        auto outputValue = sim.get_value(output);
        std::cout << "Input: 0b" << to_binary_string(inputValue, 4)
                  << ", Output: 0b" << to_binary_string(outputValue, 8)
                  << std::endl;

        REQUIRE(outputValue == 30);
    }

    SECTION("Test non-matching case") {
        auto ctx = std::make_unique<context>("test_switch_basic");
        ctx_swap ctx_swapper(ctx.get());
        ch_uint<4> input = make_uint<4>(5);
        auto output = common(input);
        Simulator sim(ctx.get());

        sim.tick();

        auto inputValue = sim.get_value(input);
        auto outputValue = sim.get_value(output);
        std::cout << "Input: 0b" << to_binary_string(inputValue, 4)
                  << ", Output: 0b" << to_binary_string(outputValue, 8)
                  << std::endl;

        REQUIRE(outputValue == 0); // default case
    }
}

TEST_CASE("switch_parallel functionality", "[switch]") {
    SECTION("Test default case") {
        auto ctx = std::make_unique<context>("test_switch_parallel_default");
        ctx_swap ctx_swapper(ctx.get());

        ch_uint<4> input = make_uint<4>(0);

        // 并行实现示例
        auto case1 = case_(make_uint<4>(1), make_uint<8>(10));
        auto case2 = case_(make_uint<4>(2), make_uint<8>(20));
        auto case3 = case_(make_uint<4>(3), make_uint<8>(30));
        ch_uint<8> output =
            switch_parallel(input, make_uint<8>(0), case1, case2, case3);

        Simulator sim(ctx.get());
        sim.tick();
        toDAG("10.dot", ctx.get(), sim);

        auto inputValue = sim.get_value(input);
        auto outputValue = sim.get_value(output);
        std::cout << "Input: 0b" << to_binary_string(inputValue, 4)
                  << ", Output: 0b" << to_binary_string(outputValue, 8)
                  << std::endl;

        REQUIRE(outputValue == 0);
    }

    SECTION("Test first case") {
        auto ctx = std::make_unique<context>("test_switch_parallel_first");
        ctx_swap ctx_swapper(ctx.get());

        ch_uint<4> input = make_uint<4>(1);

        // 并行实现示例
        auto case1 = case_(make_uint<4>(1), make_uint<8>(10));
        auto case2 = case_(make_uint<4>(2), make_uint<8>(20));
        auto case3 = case_(make_uint<4>(3), make_uint<8>(30));
        ch_uint<8> output =
            switch_parallel(input, make_uint<8>(0), case1, case2, case3);

        Simulator sim(ctx.get());
        sim.tick();

        auto inputValue = sim.get_value(input);
        auto outputValue = sim.get_value(output);
        std::cout << "Input: 0b" << to_binary_string(inputValue, 4)
                  << ", Output: 0b" << to_binary_string(outputValue, 8)
                  << std::endl;

        REQUIRE(outputValue == 10);
    }

    SECTION("Test second case") {
        auto ctx = std::make_unique<context>("test_switch_parallel_second");
        ctx_swap ctx_swapper(ctx.get());

        ch_uint<4> input = make_uint<4>(2);

        // 并行实现示例
        auto case1 = case_(make_uint<4>(1), make_uint<8>(10));
        auto case2 = case_(make_uint<4>(2), make_uint<8>(20));
        auto case3 = case_(make_uint<4>(3), make_uint<8>(30));
        ch_uint<8> output =
            switch_parallel(input, make_uint<8>(0), case1, case2, case3);

        Simulator sim(ctx.get());
        sim.tick();

        auto inputValue = sim.get_value(input);
        auto outputValue = sim.get_value(output);
        std::cout << "Input: 0b" << to_binary_string(inputValue, 4)
                  << ", Output: 0b" << to_binary_string(outputValue, 8)
                  << std::endl;

        REQUIRE(outputValue == 20);
    }

    SECTION("Test third case") {
        auto ctx = std::make_unique<context>("test_switch_parallel_third");
        ctx_swap ctx_swapper(ctx.get());

        ch_uint<4> input = make_uint<4>(3);

        // 并行实现示例
        auto case1 = case_(make_uint<4>(1), make_uint<8>(10));
        auto case2 = case_(make_uint<4>(2), make_uint<8>(20));
        auto case3 = case_(make_uint<4>(3), make_uint<8>(30));
        ch_uint<8> output =
            switch_parallel(input, make_uint<8>(0), case1, case2, case3);

        Simulator sim(ctx.get());
        sim.tick();

        auto inputValue = sim.get_value(input);
        auto outputValue = sim.get_value(output);
        std::cout << "Input: 0b" << to_binary_string(inputValue, 4)
                  << ", Output: 0b" << to_binary_string(outputValue, 8)
                  << std::endl;

        REQUIRE(outputValue == 30);
    }
}

TEST_CASE("switch_ with literal values", "[switch]") {
    auto ctx = std::make_unique<context>("test_switch_literal");
    ctx_swap ctx_swapper(ctx.get());

    ch_uint<4> input = make_uint<4>(0);

    // 测试使用字面量值的情况
    ch_uint<8> output = switch_(input, 00_d, case_(1_d, 10_d), case_(2_d, 20_d),
                                case_(3_d, 30_d));

    SECTION("Test default case with literals") {
        input = make_uint<4>(0);
        Simulator sim(ctx.get());
        sim.tick();

        auto inputValue = sim.get_value(input);
        auto outputValue = sim.get_value(output);
        std::cout << "Input: 0b" << to_binary_string(inputValue, 4)
                  << ", Output: 0b" << to_binary_string(outputValue, 8)
                  << std::endl;

        REQUIRE(outputValue == 0);
    }

    SECTION("Test first case with literals") {
        input = make_uint<4>(1);
        Simulator sim(ctx.get());
        sim.tick();

        auto inputValue = sim.get_value(input);
        auto outputValue = sim.get_value(output);
        std::cout << "Input: 0b" << to_binary_string(inputValue, 4)
                  << ", Output: 0b" << to_binary_string(outputValue, 8)
                  << std::endl;

        REQUIRE(outputValue == 10);
    }

    SECTION("Test second case with literals") {
        input = make_uint<4>(2);
        Simulator sim(ctx.get());
        sim.tick();

        auto inputValue = sim.get_value(input);
        auto outputValue = sim.get_value(output);
        std::cout << "Input: 0b" << to_binary_string(inputValue, 4)
                  << ", Output: 0b" << to_binary_string(outputValue, 8)
                  << std::endl;

        REQUIRE(outputValue == 20);
    }
}

TEST_CASE("switch_case with mixed literal and non-literal values", "[switch]") {
    auto ctx = std::make_unique<context>("test_switch_mixed");
    ctx_swap ctx_swapper(ctx.get());

    ch_uint<4> input = make_uint<4>(0);

    // 测试混合字面量和非字面量值的情况
    ch_uint<8> output = switch_case(input, 0_d, 1_d, make_uint<8>(10), 2_d,
                                    make_uint<8>(20), 3_d, make_uint<8>(30));

    SECTION("Test default case") {
        input = make_uint<4>(0);
        Simulator sim(ctx.get());
        sim.tick();

        auto inputValue = sim.get_value(input);
        auto outputValue = sim.get_value(output);
        std::cout << "Input: 0b" << to_binary_string(inputValue, 4)
                  << ", Output: 0b" << to_binary_string(outputValue, 8)
                  << std::endl;

        REQUIRE(outputValue == 0);
    }

    SECTION("Test first case") {
        input = make_uint<4>(1);
        Simulator sim(ctx.get());
        sim.tick();

        auto inputValue = sim.get_value(input);
        auto outputValue = sim.get_value(output);
        std::cout << "Input: 0b" << to_binary_string(inputValue, 4)
                  << ", Output: 0b" << to_binary_string(outputValue, 8)
                  << std::endl;

        REQUIRE(outputValue == 10);
    }

    SECTION("Test second case") {
        input = make_uint<4>(2);
        Simulator sim(ctx.get());
        sim.tick();

        auto inputValue = sim.get_value(input);
        auto outputValue = sim.get_value(output);
        std::cout << "Input: 0b" << to_binary_string(inputValue, 4)
                  << ", Output: 0b" << to_binary_string(outputValue, 8)
                  << std::endl;

        REQUIRE(outputValue == 20);
    }
}

TEST_CASE("Priority handling in switch_", "[switch]") {
    auto ctx = std::make_unique<context>("test_switch_priority");
    ctx_swap ctx_swapper(ctx.get());

    ch_uint<4> input = make_uint<4>(1);

    // 测试优先级处理，第一个匹配的case应该胜出
    auto case1 = case_(make_uint<4>(1), make_uint<8>(10));
    auto case2 =
        case_(make_uint<4>(1), make_uint<8>(99)); // 同样的条件，但优先级较低
    ch_uint<8> output = switch_(input, make_uint<8>(0), case1, case2);

    SECTION("First case should win due to priority") {
        Simulator sim(ctx.get());
        sim.tick();

        auto inputValue = sim.get_value(input);
        auto outputValue = sim.get_value(output);
        std::cout << "Input: 0b" << to_binary_string(inputValue, 4)
                  << ", Output: 0b" << to_binary_string(outputValue, 8)
                  << std::endl;

        REQUIRE(outputValue == 10); // 第一个case应该胜出
    }
}

TEST_CASE("Priority handling in switch_parallel", "[switch]") {
    auto ctx = std::make_unique<context>("test_switch_parallel_priority");
    ctx_swap ctx_swapper(ctx.get());

    ch_uint<4> input = make_uint<4>(1);

    // 测试并行实现中的优先级处理
    auto case1 = case_(make_uint<4>(1), make_uint<8>(10));
    auto case2 =
        case_(make_uint<4>(1), make_uint<8>(99)); // 同样的条件，但优先级较低
    ch_uint<8> output = switch_parallel(input, make_uint<8>(0), case1, case2);

    SECTION(
        "First case should win due to priority in parallel implementation") {
        Simulator sim(ctx.get());
        sim.tick();

        auto inputValue = sim.get_value(input);
        auto outputValue = sim.get_value(output);
        std::cout << "Input: 0b" << to_binary_string(inputValue, 4)
                  << ", Output: 0b" << to_binary_string(outputValue, 8)
                  << std::endl;

        REQUIRE(outputValue == 10); // 第一个case应该胜出
    }
}