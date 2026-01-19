#include "catch_amalgamated.hpp"
#include "chlib/fifo.h"
#include "codegen_dag.h"
#include "core/context.h"
#include "core/literal.h"
#include "simulator.h"
#include "utils/format_utils.h"
#include <bitset>
#include <memory>

using namespace ch::core;
using namespace chlib;

TEST_CASE("Memory: sync FIFO", "[memory][sync_fifo]") {

    SECTION("Basic FIFO operations") {
        auto ctx = std::make_unique<ch::core::context>("test_sync_fifo");
        ch::core::ctx_swap ctx_swapper(ctx.get());
        ch_uint<8> din = 0_d;
        ch_bool wr_en = false;
        ch_bool rd_en = false;

        SyncFifoResult<8, 3> fifo = sync_fifo<8, 3>(wr_en, din, rd_en);

        ch::Simulator sim(ctx.get());
        sim.tick();

        REQUIRE(sim.get_value(fifo.empty) == true);
        REQUIRE(sim.get_value(fifo.full) == false);
        REQUIRE(sim.get_value(fifo.count) == 0);

        sim.tick();

        // Write first value
        sim.set_value(din, 0xAB);
        sim.set_value(wr_en, 1);
        sim.tick();

        REQUIRE(sim.get_value(fifo.count) == 1);
        REQUIRE(sim.get_value(fifo.empty) == false);

        // Write second value
        sim.set_value(din, 0xCD);
        sim.tick();

        REQUIRE(sim.get_value(fifo.count) == 2);

        // Read first value
        sim.set_value(wr_en, 0);
        sim.set_value(rd_en, 1);
        sim.tick();

        REQUIRE(sim.get_value(fifo.q) == 0xAB);
        REQUIRE(sim.get_value(fifo.count) == 1);

        // Read second value
        sim.tick();

        REQUIRE(sim.get_value(fifo.q) == 0xCD);
        REQUIRE(sim.get_value(fifo.count) == 0);
        REQUIRE(sim.get_value(fifo.empty) == true);
    }

    SECTION("FIFO full and empty conditions") {
        auto ctx = std::make_unique<ch::core::context>("test_sync_fifo");
        ch::core::ctx_swap ctx_swapper(ctx.get());
        ch_uint<8> din = 0_d;
        ch_bool wr_en = false;
        ch_bool rd_en = false;

        auto fifo = sync_fifo<8, 2>(wr_en, din, rd_en);

        ch::Simulator sim(ctx.get());
        sim.tick();

        // Fill FIFO completely (depth = 2^2 = 4)
        for (int i = 0; i < 4; ++i) {
            sim.set_value(din, i + 1);
            sim.set_value(wr_en, 1);
            sim.tick();
            std::cout << "SyncFIFO count=" << sim.get_value(fifo.count)
                      << "SyncFIFO full=" << sim.get_value(fifo.full)
                      << std::endl;
        }

        REQUIRE(sim.get_value(fifo.full) == true);
        REQUIRE(sim.get_value(fifo.count) == 4);

        // Try to write more (should not increase count)
        sim.set_value(din, 0xFF);
        sim.tick();

        REQUIRE(sim.get_value(fifo.full) == true);
        REQUIRE(sim.get_value(fifo.count) == 4);

        // Read all values
        for (int i = 0; i < 4; ++i) {
            sim.set_value(wr_en, 0);
            sim.set_value(rd_en, 1);
            sim.tick();
        }

        REQUIRE(sim.get_value(fifo.empty) == true);
        REQUIRE(sim.get_value(fifo.count) == 0);
    }
}

TEST_CASE("FIFO: sync_fifo", "[fifo][sync_fifo]") {

    SECTION("Basic write and read operations") {
        auto ctx = std::make_unique<ch::core::context>("test_sync_fifo");
        ch::core::ctx_swap ctx_swapper(ctx.get());
        // 构建FIFO硬件描述
        ch_bool wren(false);
        ch_uint<8> din(0x00_h);
        ch_bool rden(false);
        ch_uint<3> threshold(0_d);

        SyncFifoResult<8, 3> result =
            sync_fifo<8, 3>(wren, din, rden, threshold);

        ch::Simulator sim(ctx.get());

        // 设置初始输入值并观察初始状态
        sim.set_value(wren, true);
        sim.set_value(din, 0x55);
        sim.set_value(rden, false);
        sim.set_value(threshold, 0);

        // 获取输入值并打印
        auto wren_val = sim.get_value(wren);
        auto din_val = sim.get_value(din);
        auto rden_val = sim.get_value(rden);
        auto threshold_val = sim.get_value(threshold);

        std::cout << "SyncFIFO Input: wren=" << wren_val << ", din=0x"
                  << std::hex << din_val << std::dec << ", rden=" << rden_val
                  << ", threshold=" << threshold_val << std::endl;

        sim.tick();

        // 获取输出值并打印
        auto empty_val = sim.get_value(result.empty);
        auto full_val = sim.get_value(result.full);
        auto q_val = sim.get_value(result.q);
        auto count_val = sim.get_value(result.count);

        std::cout << "SyncFIFO Output: empty=" << empty_val
                  << ", full=" << full_val << ", q=0x" << std::hex << q_val
                  << std::dec << ", count=" << count_val << std::endl;

        // After reset, FIFO should be empty
        REQUIRE(sim.get_value(result.empty) == false);
        REQUIRE(sim.get_value(result.full) == false);
    }

    SECTION("Write single item and read it") {
        auto ctx = std::make_unique<ch::core::context>("test_sync_fifo");
        ch::core::ctx_swap ctx_swapper(ctx.get());
        // 构建FIFO硬件描述
        ch_bool wren(false);
        ch_uint<8> din(0x00_h);
        ch_bool rden(false);

        SyncFifoResult<8, 3> result = sync_fifo<8, 3>(wren, din, rden);

        ch::Simulator sim(ctx.get());

        // 第一步：写入数据
        sim.set_value(wren, true);
        sim.set_value(din, 0x55);
        sim.set_value(rden, false);

        // 获取输入值并打印
        auto wren_val = sim.get_value(wren);
        auto din_val = sim.get_value(din);
        auto rden_val = sim.get_value(rden);

        std::cout << "SyncFIFO Input (write): wren=" << wren_val << ", din=0x"
                  << std::hex << din_val << std::dec << ", rden=" << rden_val
                  << std::endl;

        sim.tick();

        // 获取输出值并打印
        auto empty_val = sim.get_value(result.empty);
        auto full_val = sim.get_value(result.full);
        auto q_val = sim.get_value(result.q);
        auto count_val = sim.get_value(result.count);

        std::cout << "SyncFIFO Output (after write): empty=" << empty_val
                  << ", full=" << full_val << ", q=0x" << std::hex << q_val
                  << std::dec << ", count=" << count_val << std::endl;

        // After writing, FIFO should not be empty
        REQUIRE(sim.get_value(result.empty) == false);

        // 第二步：读取数据
        sim.set_value(wren, false);
        sim.set_value(din, 0x00);
        sim.set_value(rden, true);

        // 获取读取操作的输入值并打印
        auto read_wren_val = sim.get_value(wren);
        auto read_din_val = sim.get_value(din);
        auto read_rden_val = sim.get_value(rden);

        std::cout << "SyncFIFO Input (read): wren=" << read_wren_val
                  << ", din=0x" << std::hex << read_din_val << std::dec
                  << ", rden=" << read_rden_val << std::endl;

        sim.tick();

        // 获取读取操作的输出值并打印
        auto read_empty_val = sim.get_value(result.empty);
        auto read_full_val = sim.get_value(result.full);
        auto read_q_val = sim.get_value(result.q);
        auto read_count_val = sim.get_value(result.count);

        std::cout << "SyncFIFO Output (after read): empty=" << read_empty_val
                  << ", full=" << read_full_val << ", q=0x" << std::hex
                  << read_q_val << std::dec << ", count=" << read_count_val
                  << std::endl;

        REQUIRE(sim.get_value(result.q) == 0x55);
    }
}

TEST_CASE("FIFO: fwft_fifo", "[fifo][fwft_fifo]") {

    SECTION("Basic write and read operations") {
        auto ctx = std::make_unique<ch::core::context>("test_sync_fifo");
        ch::core::ctx_swap ctx_swapper(ctx.get());
        // 构建FIFO硬件描述
        ch_bool wren(0_b);
        ch_uint<8> din(0x00_h);
        ch_bool rden(0_b);

        FwftFifoResult<8, 3> result = fwft_fifo<8, 3>(wren, din, rden);

        ch::Simulator sim(ctx.get());

        // 设置初始输入值
        sim.set_value(wren, false);
        sim.set_value(din, 0x00);
        sim.set_value(rden, false);

        // 获取输入值并打印
        auto wren_val = sim.get_value(wren);
        auto din_val = sim.get_value(din);
        auto rden_val = sim.get_value(rden);

        std::cout << "FWFT FIFO Input: wren=" << wren_val << ", din=0x"
                  << std::hex << din_val << std::dec << ", rden=" << rden_val
                  << std::endl;

        toDAG("1.dot", ctx.get(), sim);
        sim.tick();
        toDAG("2.dot", ctx.get(), sim);

        // 获取输出值并打印
        auto empty_val = sim.get_value(result.empty);
        auto full_val = sim.get_value(result.full);
        auto q_val = sim.get_value(result.q);
        auto count_val = sim.get_value(result.count);

        std::cout << "FWFT FIFO Output: empty=" << empty_val
                  << ", full=" << full_val << ", q=0x" << std::hex << q_val
                  << std::dec << ", count=" << count_val << std::endl;

        // After reset, FIFO should be empty
        REQUIRE(sim.get_value(result.empty) == true);
        REQUIRE(sim.get_value(result.full) == false);
    }
}

TEST_CASE("FIFO: lifo_stack", "[fifo][lifo_stack]") {

    SECTION("Basic push and pop operations") {
        auto ctx = std::make_unique<ch::core::context>("test_sync_fifo");
        ch::core::ctx_swap ctx_swapper(ctx.get());
        // 构建LIFO栈硬件描述
        ch_bool push(false);
        ch_uint<8> din(0x00_h);
        ch_bool pop(false);

        LifoResult<8, 3> result = lifo_stack<8, 3>(push, din, pop);

        ch::Simulator sim(ctx.get());

        // 第一步：推送第一个值
        sim.set_value(push, true);
        sim.set_value(din, 0x12);
        sim.set_value(pop, false);

        // 获取输入值并打印
        auto push_val = sim.get_value(push);
        auto din_val = sim.get_value(din);
        auto pop_val = sim.get_value(pop);

        std::cout << "LIFO Stack Input (first push): push=" << push_val
                  << ", din=0x" << std::hex << din_val << std::dec
                  << ", pop=" << pop_val << std::endl;

        sim.tick();

        // 获取输出值并打印
        auto empty_val = sim.get_value(result.empty);
        auto full_val = sim.get_value(result.full);
        auto q_val = sim.get_value(result.q);

        std::cout << "LIFO Stack Output (after first push): empty=" << empty_val
                  << ", full=" << full_val << ", q=0x" << std::hex << q_val
                  << std::dec << std::endl;

        // After pushing, FIFO should not be empty
        REQUIRE(sim.get_value(result.empty) == false);

        // 第二步：推送第二个值
        sim.set_value(push, true);
        sim.set_value(din, 0x34);
        sim.set_value(pop, false);

        // 获取第二步推送的输入值并打印
        auto push2_val = sim.get_value(push);
        auto din2_val = sim.get_value(din);
        auto pop2_val = sim.get_value(pop);

        std::cout << "LIFO Stack Input (second push): push=" << push2_val
                  << ", din=0x" << std::hex << din2_val << std::dec
                  << ", pop=" << pop2_val << std::endl;

        sim.tick();

        // 获取第二步推送的输出值并打印
        auto empty2_val = sim.get_value(result.empty);
        auto full2_val = sim.get_value(result.full);
        auto q2_val = sim.get_value(result.q);

        std::cout << "LIFO Stack Output (after second push): empty="
                  << empty2_val << ", full=" << full2_val << ", q=0x"
                  << std::hex << q2_val << std::dec << std::endl;

        // 第三步：弹出最后的值
        sim.set_value(push, false);
        sim.set_value(din, 0x00);
        sim.set_value(pop, true);

        // 获取弹出操作的输入值并打印
        auto push3_val = sim.get_value(push);
        auto din3_val = sim.get_value(din);
        auto pop3_val = sim.get_value(pop);

        std::cout << "LIFO Stack Input (pop): push=" << push3_val << ", din=0x"
                  << std::hex << din3_val << std::dec << ", pop=" << pop3_val
                  << std::endl;

        sim.tick();

        // 获取弹出操作的输出值并打印
        auto empty3_val = sim.get_value(result.empty);
        auto full3_val = sim.get_value(result.full);
        auto q3_val = sim.get_value(result.q);

        std::cout << "LIFO Stack Output (after pop): empty=" << empty3_val
                  << ", full=" << full3_val << ", q=0x" << std::hex << q3_val
                  << std::dec << std::endl;

        REQUIRE(sim.get_value(result.q) == 0x34);
    }

    SECTION("Push and pop sequence") {
        auto ctx = std::make_unique<ch::core::context>("test_sync_fifo");
        ch::core::ctx_swap ctx_swapper(ctx.get());
        // 构建LIFO栈硬件描述
        ch_bool push(false);
        ch_uint<8> din(0x00_h);
        ch_bool pop(false);

        LifoResult<8, 3> result = lifo_stack<8, 3>(push, din, pop);

        ch::Simulator sim(ctx.get());

        // 第一步：推送值0x11
        sim.set_value(push, true);
        sim.set_value(din, 0x11);
        sim.set_value(pop, false);

        // 获取输入值并打印
        auto push1_val = sim.get_value(push);
        auto din1_val = sim.get_value(din);
        auto pop1_val = sim.get_value(pop);

        std::cout << "LIFO Sequence Input (first push): push=" << push1_val
                  << ", din=0x" << std::hex << din1_val << std::dec
                  << ", pop=" << pop1_val << std::endl;

        sim.tick();

        // 获取输出值并打印
        auto empty1_val = sim.get_value(result.empty);
        auto full1_val = sim.get_value(result.full);
        auto q1_val = sim.get_value(result.q);

        std::cout << "LIFO Sequence Output (after first push): empty="
                  << empty1_val << ", full=" << full1_val << ", q=0x"
                  << std::hex << q1_val << std::dec << std::endl;

        // 第二步：推送值0x22
        sim.set_value(push, true);
        sim.set_value(din, 0x22);
        sim.set_value(pop, false);

        // 获取输入值并打印
        auto push2_val = sim.get_value(push);
        auto din2_val = sim.get_value(din);
        auto pop2_val = sim.get_value(pop);

        std::cout << "LIFO Sequence Input (second push): push=" << push2_val
                  << ", din=0x" << std::hex << din2_val << std::dec
                  << ", pop=" << pop2_val << std::endl;

        sim.tick();

        // 获取输出值并打印
        auto empty2_val = sim.get_value(result.empty);
        auto full2_val = sim.get_value(result.full);
        auto q2_val = sim.get_value(result.q);

        std::cout << "LIFO Sequence Output (after second push): empty="
                  << empty2_val << ", full=" << full2_val << ", q=0x"
                  << std::hex << q2_val << std::dec << std::endl;

        // 第三步：推送值0x33
        sim.set_value(push, true);
        sim.set_value(din, 0x33);
        sim.set_value(pop, false);

        // 获取输入值并打印
        auto push3_val = sim.get_value(push);
        auto din3_val = sim.get_value(din);
        auto pop3_val = sim.get_value(pop);

        std::cout << "LIFO Sequence Input (third push): push=" << push3_val
                  << ", din=0x" << std::hex << din3_val << std::dec
                  << ", pop=" << pop3_val << std::endl;

        sim.tick();

        // 获取输出值并打印
        auto empty3_val = sim.get_value(result.empty);
        auto full3_val = sim.get_value(result.full);
        auto q3_val = sim.get_value(result.q);

        std::cout << "LIFO Sequence Output (after third push): empty="
                  << empty3_val << ", full=" << full3_val << ", q=0x"
                  << std::hex << q3_val << std::endl;

        // 第四步：弹出 - 应该是0x33
        sim.set_value(push, false);
        sim.set_value(din, 0x00);
        sim.set_value(pop, true);

        // 获取输入值并打印
        auto pop1_push_val = sim.get_value(push);
        auto pop1_din_val = sim.get_value(din);
        auto pop1_pop_val = sim.get_value(pop);

        std::cout << "LIFO Sequence Input (first pop): push=" << pop1_push_val
                  << ", din=0x" << std::hex << pop1_din_val << std::dec
                  << ", pop=" << pop1_pop_val << std::endl;

        sim.tick();

        // 获取输出值并打印
        auto pop1_empty_val = sim.get_value(result.empty);
        auto pop1_full_val = sim.get_value(result.full);
        auto pop1_q_val = sim.get_value(result.q);

        std::cout << "LIFO Sequence Output (after first pop): empty="
                  << pop1_empty_val << ", full=" << pop1_full_val << ", q=0x"
                  << std::hex << pop1_q_val << std::dec << std::endl;

        REQUIRE(sim.get_value(result.q) == 0x33);

        // 第五步：弹出 - 应该是0x22
        sim.set_value(push, false);
        sim.set_value(din, 0x00);
        sim.set_value(pop, true);

        // 获取输入值并打印
        auto pop2_push_val = sim.get_value(push);
        auto pop2_din_val = sim.get_value(din);
        auto pop2_pop_val = sim.get_value(pop);

        std::cout << "LIFO Sequence Input (second pop): push=" << pop2_push_val
                  << ", din=0x" << std::hex << pop2_din_val << std::dec
                  << ", pop=" << pop2_pop_val << std::endl;

        sim.tick();

        // 获取输出值并打印
        auto pop2_empty_val = sim.get_value(result.empty);
        auto pop2_full_val = sim.get_value(result.full);
        auto pop2_q_val = sim.get_value(result.q);

        std::cout << "LIFO Sequence Output (after second pop): empty="
                  << pop2_empty_val << ", full=" << pop2_full_val << ", q=0x"
                  << std::hex << pop2_q_val << std::dec << std::endl;

        REQUIRE(sim.get_value(result.q) == 0x22);

        // 第六步：弹出 - 应该是0x11
        sim.set_value(push, false);
        sim.set_value(din, 0x00);
        sim.set_value(pop, true);

        // 获取输入值并打印
        auto pop3_push_val = sim.get_value(push);
        auto pop3_din_val = sim.get_value(din);
        auto pop3_pop_val = sim.get_value(pop);

        std::cout << "LIFO Sequence Input (third pop): push=" << pop3_push_val
                  << ", din=0x" << std::hex << pop3_din_val << std::dec
                  << ", pop=" << pop3_pop_val << std::endl;

        sim.tick();

        // 获取输出值并打印
        auto pop3_empty_val = sim.get_value(result.empty);
        auto pop3_full_val = sim.get_value(result.full);
        auto pop3_q_val = sim.get_value(result.q);

        std::cout << "LIFO Sequence Output (after third pop): empty="
                  << pop3_empty_val << ", full=" << pop3_full_val << ", q=0x"
                  << std::hex << pop3_q_val << std::dec << std::endl;

        REQUIRE(sim.get_value(result.q) == 0x11);
    }
}

// 异步FIFO功能当前已被禁用，需要重新设计双时钟域支持
// 已移除相关的测试用例