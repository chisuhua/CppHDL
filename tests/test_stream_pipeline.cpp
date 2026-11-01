#define CATCH_CONFIG_MAIN
#include "catch_amalgamated.hpp"
#include "ch.hpp"
#include "bundle/stream_bundle.h"
#include "core/context.h"
#include "simulator.h"

#include "chlib/stream_pipeline.h"
#include "bundle/stream_bundle_member_inlines.h"

using namespace ch::core;
using namespace chlib;

TEST_CASE("Stream Pipeline: stream_m2s_pipe basic operation", "[stream][pipeline]") {
    auto ctx = std::make_unique<ch::core::context>("test_m2s_pipe");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("Stream M2S pipe - basic 1-cycle delay") {
        ch_stream<ch_uint<8>> input_stream;
        input_stream.as_master();
        input_stream.payload = 0x42_h;
        input_stream.valid = true;
        // ⚠️ MUST NOT 驱动 `input_stream.ready` —— pipe 现在**自己**
        //    驱动它（`detail::drive_upstream_ready`）⇒ 自行驱动会被**响亮拒绝**

        auto pipe_result = stream_m2s_pipe(input_stream);

        ch::Simulator sim(ctx.get());
        
        sim.tick();
        
        REQUIRE(sim.get_value(pipe_result.output.valid) == true);
        REQUIRE(sim.get_value(pipe_result.output.payload) == 0x42);
    }
}

TEST_CASE("Stream Pipeline: stream_m2s_pipe structural tests", "[stream][pipeline][structural]") {
    auto ctx = std::make_unique<ch::core::context>("test_m2s_pipe_structural");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("Stream M2S pipe result structure") {
        ch_stream<ch_uint<8>> input_stream;
        input_stream.as_master();
        
        auto pipe_result = stream_m2s_pipe(input_stream);

        REQUIRE(pipe_result.output.payload.width == 8);
        REQUIRE(pipe_result.output.valid.width == 1);
        REQUIRE(pipe_result.output.ready.width == 1);
        
        REQUIRE(pipe_result.input.payload.width == 8);
        REQUIRE(pipe_result.input.valid.width == 1);
        REQUIRE(pipe_result.input.ready.width == 1);
    }

    SECTION("Stream M2S pipe with different payload widths") {
        ch_stream<ch_uint<16>> input16;
        input16.as_master();
        auto result16 = stream_m2s_pipe(input16);
        REQUIRE(result16.output.payload.width == 16);

        ch_stream<ch_uint<32>> input32;
        input32.as_master();
        auto result32 = stream_m2s_pipe(input32);
        REQUIRE(result32.output.payload.width == 32);
    }
}


TEST_CASE("Stream Pipeline: stream_s2m_pipe basic operation", "[stream][pipeline]") {
    auto ctx = std::make_unique<ch::core::context>("test_s2m_pipe");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("Stream S2M pipe - 0-cycle payload, 1-cycle ready") {
        ch_stream<ch_uint<8>> input_stream;
        input_stream.as_master();
        input_stream.payload = 0x42_h;
        input_stream.valid = true;
        // ⚠️ MUST NOT 驱动 `input_stream.ready` —— pipe 现在**自己**
        //    驱动它（`detail::drive_upstream_ready`）⇒ 自行驱动会被**响亮拒绝**

        auto pipe_result = stream_s2m_pipe(input_stream);

        ch::Simulator sim(ctx.get());
        
        // Payload should be available immediately (0-cycle delay)
        sim.tick();
        REQUIRE(sim.get_value(pipe_result.output.payload) == 0x42);
        
        // Valid should also be immediately available
        REQUIRE(sim.get_value(pipe_result.output.valid) == true);
    }
}

TEST_CASE("Stream Pipeline: stream_s2m_pipe structural tests", "[stream][pipeline][structural]") {
    auto ctx = std::make_unique<ch::core::context>("test_s2m_pipe_structural");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("Stream S2M pipe result structure") {
        ch_stream<ch_uint<8>> input_stream;
        input_stream.as_master();
        
        auto pipe_result = stream_s2m_pipe(input_stream);

        REQUIRE(pipe_result.output.payload.width == 8);
        REQUIRE(pipe_result.output.valid.width == 1);
        REQUIRE(pipe_result.output.ready.width == 1);
        
        REQUIRE(pipe_result.input.payload.width == 8);
        REQUIRE(pipe_result.input.valid.width == 1);
        REQUIRE(pipe_result.input.ready.width == 1);
    }

    SECTION("Stream S2M pipe with different payload widths") {
        ch_stream<ch_uint<16>> input16;
        input16.as_master();
        auto result16 = stream_s2m_pipe(input16);
        REQUIRE(result16.output.payload.width == 16);

        ch_stream<ch_uint<32>> input32;
        input32.as_master();
        auto result32 = stream_s2m_pipe(input32);
        REQUIRE(result32.output.payload.width == 32);
    }
}

TEST_CASE("Stream Pipeline: stream_half_pipe basic operation", "[stream][pipeline]") {
    auto ctx = std::make_unique<ch::core::context>("test_half_pipe");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("Stream Half pipe - all signals registered (bandwidth halved)") {
        ch_stream<ch_uint<8>> input_stream;
        input_stream.as_master();
        input_stream.payload = 0x42_h;
        input_stream.valid = true;
        // ⚠️ MUST NOT 驱动 `input_stream.ready` —— pipe 现在**自己**
        //    驱动它（`detail::drive_upstream_ready`）⇒ 自行驱动会被**响亮拒绝**

        auto pipe_result = stream_half_pipe(input_stream);

        ch::Simulator sim(ctx.get());
        
        // First tick: toggle starts at 0, so data not accepted yet
        sim.tick();
        REQUIRE(sim.get_value(pipe_result.output.valid) == false);
        
        // Second tick: toggle now high, data is accepted
        sim.tick();
        REQUIRE(sim.get_value(pipe_result.output.valid) == true);
        REQUIRE(sim.get_value(pipe_result.output.payload) == 0x42);
    }
}

TEST_CASE("Stream Pipeline: stream_half_pipe structural tests", "[stream][pipeline][structural]") {
    auto ctx = std::make_unique<ch::core::context>("test_half_pipe_structural");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("Stream Half pipe result structure") {
        ch_stream<ch_uint<8>> input_stream;
        input_stream.as_master();
        
        auto pipe_result = stream_half_pipe(input_stream);

        REQUIRE(pipe_result.output.payload.width == 8);
        REQUIRE(pipe_result.output.valid.width == 1);
        REQUIRE(pipe_result.output.ready.width == 1);
        
        REQUIRE(pipe_result.input.payload.width == 8);
        REQUIRE(pipe_result.input.valid.width == 1);
        REQUIRE(pipe_result.input.ready.width == 1);
    }

    SECTION("Stream Half pipe with different payload widths") {
        ch_stream<ch_uint<16>> input16;
        input16.as_master();
        auto result16 = stream_half_pipe(input16);
        REQUIRE(result16.output.payload.width == 16);

        ch_stream<ch_uint<32>> input32;
        input32.as_master();
        auto result32 = stream_half_pipe(input32);
        REQUIRE(result32.output.payload.width == 32);
    }
}


TEST_CASE("Stream Pipeline: ch_stream member functions - m2sPipe", "[stream][pipeline][member]") {
    auto ctx = std::make_unique<ch::core::context>("test_m2sPipe_member");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("ch_stream.m2sPipe() member function") {
        ch_stream<ch_uint<8>> input_stream;
        input_stream.as_master();
        input_stream.payload = 0x42_h;
        input_stream.valid = true;
        // ⚠️ MUST NOT 驱动 `input_stream.ready` —— pipe 现在**自己**
        //    驱动它（`detail::drive_upstream_ready`）⇒ 自行驱动会被**响亮拒绝**

        auto pipe_result = input_stream.m2sPipe();

        ch::Simulator sim(ctx.get());
        
        sim.tick();
        
        REQUIRE(sim.get_value(pipe_result.output.valid) == true);
        REQUIRE(sim.get_value(pipe_result.output.payload) == 0x42);
    }
}

TEST_CASE("Stream Pipeline: ch_stream member functions - stage", "[stream][pipeline][member]") {
    auto ctx = std::make_unique<ch::core::context>("test_stage_member");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("ch_stream.stage() member function (alias for m2sPipe)") {
        ch_stream<ch_uint<8>> input_stream;
        input_stream.as_master();
        input_stream.payload = 0xAB_h;
        input_stream.valid = true;
        // ⚠️ MUST NOT 驱动 `input_stream.ready` —— pipe 现在**自己**
        //    驱动它（`detail::drive_upstream_ready`）⇒ 自行驱动会被**响亮拒绝**

        auto pipe_result = input_stream.stage();

        ch::Simulator sim(ctx.get());
        
        sim.tick();
        
        REQUIRE(sim.get_value(pipe_result.output.valid) == true);
        REQUIRE(sim.get_value(pipe_result.output.payload) == 0xAB);
    }
}

TEST_CASE("Stream Pipeline: ch_stream member functions - s2mPipe", "[stream][pipeline][member]") {
    auto ctx = std::make_unique<ch::core::context>("test_s2mPipe_member");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("ch_stream.s2mPipe() member function") {
        ch_stream<ch_uint<8>> input_stream;
        input_stream.as_master();
        input_stream.payload = 0x55_h;
        input_stream.valid = true;
        // ⚠️ MUST NOT 驱动 `input_stream.ready` —— pipe 现在**自己**
        //    驱动它（`detail::drive_upstream_ready`）⇒ 自行驱动会被**响亮拒绝**

        auto pipe_result = input_stream.s2mPipe();

        ch::Simulator sim(ctx.get());
        
        sim.tick();
        REQUIRE(sim.get_value(pipe_result.output.payload) == 0x55);
        REQUIRE(sim.get_value(pipe_result.output.valid) == true);
    }
}

TEST_CASE("Stream Pipeline: ch_stream member functions - halfPipe", "[stream][pipeline][member]") {
    auto ctx = std::make_unique<ch::core::context>("test_halfPipe_member");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("ch_stream.halfPipe() member function") {
        ch_stream<ch_uint<8>> input_stream;
        input_stream.as_master();
        input_stream.payload = 0x77_h;
        input_stream.valid = true;
        // ⚠️ MUST NOT 驱动 `input_stream.ready` —— pipe 现在**自己**
        //    驱动它（`detail::drive_upstream_ready`）⇒ 自行驱动会被**响亮拒绝**

        auto pipe_result = input_stream.halfPipe();

        ch::Simulator sim(ctx.get());
        
        sim.tick();
        REQUIRE(sim.get_value(pipe_result.output.valid) == false);
        
        sim.tick();
        REQUIRE(sim.get_value(pipe_result.output.valid) == true);
        REQUIRE(sim.get_value(pipe_result.output.payload) == 0x77);
    }
}

// ===========================================================================
// ⭐ 行为判据（ADR-055 · B6）：correct 接线下的**数据通过** + **上游反压**
// ===========================================================================
//
// ⚠️ **为什么必须有**（A4 实测）：三个 pipe 此前**只在 `ready = true` 常量
//    接线下活着** —— `output.ready` **从未被驱动**，而 `input_stream.ready`
//    用 **`operator=`** 重绑定（⇒ 调用方被孤儿化）⇒ ⇒ 「正确接线」下
//    **零数据通过**（判决性对比 46/14 vs 23/14 test cases）。
//    ⇒ ⇒ 只断言 width / 常量接线下的波形 MUST NOT 被当作「pipe 可用」的证据。
//
// ⚠️ **接线纪律**：`source.as_master()`（⇒ 字段有节点）+ 数据用 `<<=`；
//    `source.ready` **留给 pipe 驱动**（自行驱动会被响亮拒绝）。
TEST_CASE("Stream Pipeline: m2s pipe - 正确接线下数据通过 + 上游反压可达",
          "[stream][pipeline][ready]") {
    auto ctx = std::make_unique<ch::core::context>("test_m2s_ready_ownership");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    ch_uint<8> payload(0_d);
    ch_bool valid(false);
    ch_bool consumer_ready(false);

    ch_stream<ch_uint<8>> source;
    source.as_master();
    source.payload <<= payload;
    source.valid <<= valid;

    auto pipe = stream_m2s_pipe(source);
    // 消费端施加反压 ⇒ 覆盖 pipe 的**默认**驱动（ADR-054 形态 E）
    pipe.output.ready <<= consumer_ready;

    // ① **结构判据**：上游 ready MUST 有驱动
    REQUIRE(source.ready.impl() != nullptr);
    REQUIRE(source.ready.impl()->num_srcs() > 0);

    ch::Simulator sim(ctx.get());

    auto in_ready = [&] {
        return static_cast<uint64_t>(sim.get_value(source.ready));
    };
    auto out_valid = [&] {
        return static_cast<uint64_t>(sim.get_value(pipe.output.valid));
    };
    auto out_payload = [&] {
        return static_cast<uint64_t>(sim.get_value(pipe.output.payload));
    };

    // 结算拍（`Simulator` 的 data_map 在第一次 tick 时建立 ⇒ ⇒ 先跑一拍）
    sim.tick();

    // ② **空 ⇒ 可接收**（判别性：修复前该节点无驱动 ⇒ 恒 0）
    REQUIRE(in_ready() == 1u);

    // ③ 收下一个词 ⇒ 下游不就绪 ⇒ 寄存器满 ⇒ 上游 MUST 被挡住
    sim.set_value(valid, 1);
    sim.set_value(payload, 0xA0);
    sim.tick();
    REQUIRE(out_valid() == 1u);
    REQUIRE(out_payload() == 0xA0u);
    REQUIRE(in_ready() == 0u);          // 反压**真的**到达上游
    sim.tick();                          // 再等一拍 ⇒ 数据 MUST 被**保持**
    REQUIRE(out_valid() == 1u);
    REQUIRE(out_payload() == 0xA0u);

    // ④ 消费端就绪 ⇒ 弹走 ⇒ 上游恢复
    sim.set_value(valid, 0);
    sim.set_value(consumer_ready, 1);
    sim.tick();
    REQUIRE(out_valid() == 0u);
    REQUIRE(in_ready() == 1u);

    // ⑤ **数据通过**：payload 每拍换值 ⇒ 1 拍寄存器 ⇒ 输出逐个滞后一拍出现
    //    手推金标：tick k 后输出 = 第 k 次推入的那个词（0x10, 0x11, 0x12）
    sim.set_value(valid, 1);
    for (unsigned k = 0; k < 3; ++k) {
        sim.set_value(payload, 0x10 + k);
        sim.tick();
        REQUIRE(out_valid() == 1u);
        REQUIRE(out_payload() == 0x10u + k);
    }
}

TEST_CASE("Stream Pipeline: half pipe - 隔拍就绪（半带宽）且不丢数据",
          "[stream][pipeline][ready]") {
    auto ctx = std::make_unique<ch::core::context>("test_half_ready_ownership");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    ch_uint<8> payload(0_d);
    ch_bool valid(false);
    ch_bool consumer_ready(true);

    ch_stream<ch_uint<8>> source;
    source.as_master();
    source.payload <<= payload;
    source.valid <<= valid;

    auto pipe = stream_half_pipe(source);
    pipe.output.ready <<= consumer_ready;

    REQUIRE(source.ready.impl() != nullptr);
    REQUIRE(source.ready.impl()->num_srcs() > 0);

    ch::Simulator sim(ctx.get());

    auto in_ready = [&] {
        return static_cast<uint64_t>(sim.get_value(source.ready));
    };
    auto out_valid = [&] {
        return static_cast<uint64_t>(sim.get_value(pipe.output.valid));
    };
    auto out_payload = [&] {
        return static_cast<uint64_t>(sim.get_value(pipe.output.payload));
    };

    // ① **半带宽**：输入侧 ready 隔拍（toggle 自由运行 ⇒ 0/1 交替）
    //    手推：toggle 初值 0 ⇒ 第 1 拍**不收**；第 2 拍 toggle==1 ⇒ 收
    sim.set_value(valid, 1);
    sim.set_value(payload, 0x20);
    sim.tick();                      // 第 1 拍：toggle==0 ⇒ 不收
    REQUIRE(out_valid() == 0u);
    REQUIRE(in_ready() == 1u);       // toggle 已翻到 1 ⇒ 本拍可接收
    sim.tick();
    REQUIRE(out_valid() == 1u);
    REQUIRE(out_payload() == 0x20u);
    REQUIRE(in_ready() == 0u);       // 隔拍：下一拍不收（半带宽）

    // ② **不丢数据**：下游不就绪 ⇒ 已接收的数据 MUST 被**保持**
    //    （修复前 `valid_reg->next = accept_data` ⇒ 下一拍就丢）
    sim.set_value(consumer_ready, 0);
    sim.tick();
    REQUIRE(out_valid() == 1u);
    REQUIRE(out_payload() == 0x20u);
    sim.tick();
    REQUIRE(out_valid() == 1u);      // 仍然保持
    REQUIRE(out_payload() == 0x20u);

    // ③ 消费端就绪 ⇒ 正常取走
    sim.set_value(consumer_ready, 1);
    sim.tick();
    REQUIRE(out_valid() == 0u);
}
