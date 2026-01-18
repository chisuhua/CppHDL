#include "catch_amalgamated.hpp"
#include "codegen_dag.h"
#include "component.h"
#include "core/bool.h"
#include "core/context.h"
#include "core/literal.h"
#include "core/operators.h"
#include "core/uint.h"
#include "simulator.h"

using namespace ch::core;

// ---------- 位操作仿真测试 ----------

TEST_CASE("bit_select: simulation value verification",
          "[operators][bit_operations][simulation]") {
    context ctx("test_bit_select_simulation");
    ctx_swap swap(&ctx);

    // 创建测试数据
    ch_uint<8> data(0b10110101, "test_data");

    // 使用编译时索引版本
    auto bit0 = bit_select<0>(data); // 应该是1
    auto bit1 = bit_select<1>(data); // 应该是0
    auto bit2 = bit_select<2>(data); // 应该是1
    auto bit7 = bit_select<7>(data); // 应该是1

    // 创建模拟器
    Simulator simulator(&ctx);

    // 运行模拟
    simulator.tick();

    toDAG("test_operator.dot", &ctx, simulator);

    // 验证仿真结果
    REQUIRE(static_cast<uint64_t>(simulator.get_value(bit0)) == true); // 位0是1
    REQUIRE(static_cast<uint64_t>(simulator.get_value(bit1)) ==
            false); // 位1是0
    REQUIRE(static_cast<uint64_t>(simulator.get_value(bit2)) == true); // 位2是1
    REQUIRE(static_cast<uint64_t>(simulator.get_value(bit7)) == true); // 位7是1
}

TEST_CASE("bit_select: runtime index simulation verification",
          "[operators][bit_operations][simulation]") {
    context ctx("test_bit_select_runtime_simulation");
    ctx_swap swap(&ctx);

    // 创建测试数据
    ch_uint<8> data(0b10110101, "test_data");

    // 使用运行时索引版本
    auto bit0 = bit_select(data, 0u); // 应该是1
    auto bit1 = bit_select(data, 1u); // 应该是0
    auto bit2 = bit_select(data, 2u); // 应该是1
    auto bit7 = bit_select(data, 7u); // 应该是1

    // 创建模拟器
    Simulator simulator(&ctx);

    // 运行模拟
    simulator.tick();

    // 验证仿真结果
    REQUIRE(static_cast<uint64_t>(simulator.get_value(bit0)) == true); // 位0是1
    REQUIRE(static_cast<uint64_t>(simulator.get_value(bit1)) ==
            false); // 位1是0
    REQUIRE(static_cast<uint64_t>(simulator.get_value(bit2)) == true); // 位2是1
    REQUIRE(static_cast<uint64_t>(simulator.get_value(bit7)) == true); // 位7是1
}

TEST_CASE("bit_select: hardware index simulation verification",
          "[operators][bit_operations][simulation]") {
    context ctx("test_bit_select_hardware_index_simulation");
    ctx_swap swap(&ctx);

    // 创建测试数据
    ch_uint<8> data(0b10110101, "test_data");
    ch_uint<4> idx0(0, "idx0");
    ch_uint<4> idx1(1, "idx1");
    ch_uint<4> idx2(2, "idx2");
    ch_uint<4> idx7(7, "idx7");

    // 使用硬件索引版本
    auto bit0 = bit_select(data, idx0); // 应该是1
    auto bit1 = bit_select(data, idx1); // 应该是0
    auto bit2 = bit_select(data, idx2); // 应该是1
    auto bit7 = bit_select(data, idx7); // 应该是1

    // 创建模拟器
    Simulator simulator(&ctx);

    // 运行模拟
    simulator.tick();

    // 验证仿真结果
    REQUIRE(static_cast<uint64_t>(simulator.get_value(bit0)) == true); // 位0是1
    REQUIRE(static_cast<uint64_t>(simulator.get_value(bit1)) ==
            false); // 位1是0
    REQUIRE(static_cast<uint64_t>(simulator.get_value(bit2)) == true); // 位2是1
    REQUIRE(static_cast<uint64_t>(simulator.get_value(bit7)) == true); // 位7是1
}
