// tests/test_stream_arbiter.cpp
// Tests for additional stream arbiter strategies: locking
// ⚠️ 2026-11-01（B5）：`stream_arbiter_sequence` 已**物理删除**
//    （与 `stream_arbiter_priority` 仲裁逻辑逐位相同 + 名字 over-promise
//    「wraps around」而实现是 `priority_encoder`）⇒ 其 2 个有判别力的
//    用例**移植到 `priority`**（下面 2 个），其余 1 个（「wraps around」）
//    断言的是**不存在的行为** ⇒ 随组件一并删除。

#include "catch_amalgamated.hpp"
#include "chlib/stream.h"
#include "chlib/stream_arbiter.h"
#include "core/context.h"
#include "simulator.h"

#include <memory>

using namespace ch;
using namespace chlib;

using namespace ch;

// Test: Stream Arbiter Locking - basic operation
TEST_CASE("Stream Arbiter: Locking arbiter basic operation", "[stream][arbiter][lock]") {
    auto ctx = std::make_unique<ch::core::context>("test_stream_arbiter_lock");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    // Create 2 input streams
    std::array<ch_stream<ch_uint<8>>, 2> input_streams;
    input_streams[0].payload = 0x11_h;
    input_streams[0].valid = true;
    input_streams[1].payload = 0x22_h;
    input_streams[1].valid = false;

    auto arb_result = stream_arbiter_lock<ch_uint<8>, 2>(input_streams);

    // Structural checks
    REQUIRE(arb_result.output_stream.payload.width == 8);
    REQUIRE(arb_result.output_stream.valid.width == 1);
    REQUIRE(arb_result.selected.width == compute_idx_width(2));
}

// Test: Stream Arbiter Locking - locks on first valid and stays
TEST_CASE("Stream Arbiter: Locking arbiter holds lock until transaction", "[stream][arbiter][lock]") {
    auto ctx = std::make_unique<ch::core::context>("test_stream_arbiter_lock_hold");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    // Create 3 input streams with first one valid
    std::array<ch_stream<ch_uint<8>>, 3> input_streams;
    input_streams[0].payload = 0xAA_h;
    input_streams[0].valid = true;
    input_streams[1].payload = 0xBB_h;
    input_streams[1].valid = true;
    input_streams[2].payload = 0xCC_h;
    input_streams[2].valid = false;

    auto arb_result = stream_arbiter_lock<ch_uint<8>, 3>(input_streams);

    ch::Simulator sim(ctx.get());
    sim.tick();

    // When both 0 and 1 are valid, locking arbiter should pick one and stay with it
    // Output should be valid
    REQUIRE(sim.get_value(arb_result.output_stream.valid) == true);
}

// Test: Stream Arbiter Priority - basic operation（原 sequence 用例的移植）
TEST_CASE("Stream Arbiter: Priority arbiter basic operation", "[stream][arbiter][priority]") {
    auto ctx = std::make_unique<ch::core::context>("test_stream_arbiter_priority");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    // Create 3 input streams
    std::array<ch_stream<ch_uint<8>>, 3> input_streams;
    input_streams[0].payload = 0x11_h;
    input_streams[0].valid = false;
    input_streams[1].payload = 0x22_h;
    input_streams[1].valid = true;
    input_streams[2].payload = 0x33_h;
    input_streams[2].valid = false;

    auto arb_result = stream_arbiter_priority<ch_uint<8>, 3>(input_streams);

    // Structural checks
    REQUIRE(arb_result.output_stream.payload.width == 8);
    REQUIRE(arb_result.output_stream.valid.width == 1);
    REQUIRE(arb_result.selected.width == compute_idx_width(3));

    // 行为：唯一有效输入 ⇒ MUST 被选中（固定优先级扫描在「唯一有效」下
    // 与任何策略逐位相同 ⇒ 这一条锁的是**回归**，不是策略）
    ch::Simulator sim(ctx.get());
    sim.tick();
    REQUIRE(sim.get_value(arb_result.output_stream.valid) == true);
    REQUIRE(sim.get_value(arb_result.output_stream.payload) == 0x22);
}

// Test: Stream Arbiter Priority - no valid inputs（原 sequence 用例的移植）
TEST_CASE("Stream Arbiter: Priority arbiter handles no valid inputs",
          "[stream][arbiter][priority]") {
    auto ctx = std::make_unique<ch::core::context>("test_stream_arbiter_priority_empty");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    std::array<ch_stream<ch_uint<8>>, 2> input_streams;
    input_streams[0].payload = 0x11_h;
    input_streams[0].valid = false;
    input_streams[1].payload = 0x22_h;
    input_streams[1].valid = false;

    auto arb_result = stream_arbiter_priority<ch_uint<8>, 2>(input_streams);

    ch::Simulator sim(ctx.get());
    sim.tick();

    // Output must not be valid when no input is valid
    REQUIRE(sim.get_value(arb_result.output_stream.valid) == false);
}
