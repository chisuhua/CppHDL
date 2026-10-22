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

TEST_CASE("FIFO: sync_fifo 自保护 —— 指针 MUST NOT 自由运行",
          "[fifo][sync_fifo][regression]") {
    // ⚠️ 2026-10-22 新增（ADR-045 / change `chlib-stream-semantics-2026-10`）。
    //
    // 背景：`sync_fifo` 此前对**写口**与**双指针**都没有空/满门控：
    //   - `memory.write(write_ptr, din, wren)`          ← 满 FIFO 再写 ⇒ 覆写槽位
    //   - `next_write_ptr = select(wren, ...)`           ← 满 FIFO 时写指针照常前进
    //   - `next_read_ptr  = select(rden, ...)`           ← 空 FIFO 时读指针照常前进
    // 而 `count` 用的 `will_write`/`will_read`（`include/chlib/fifo.h:68-69`）
    // **是**带门的 ⇒ 计数与指针脱钩 ⇒ **静默数据损坏**。
    //
    // 本用例是**判别性**断言：每一条都能区分「正确实现」与「指针自由运行」，
    // 而不是只区分「能构造」与「崩溃」。

    SECTION("满 FIFO 再写 MUST NOT 破坏已写入的数据") {
        auto ctx = std::make_unique<ch::core::context>("test_sync_fifo_full_write");
        ch::core::ctx_swap ctx_swapper(ctx.get());
        ch_uint<8> din(0_d);
        ch_bool wren(false), rden(false);
        // ADDR_WIDTH=2 ⇒ 深度 4；写指针是 2 位寄存器（会回绕）
        SyncFifoResult<8, 2> fifo = sync_fifo<8, 2>(wren, din, rden);

        ch::Simulator sim(ctx.get());
        sim.tick();

        // 填满：1, 2, 3, 4
        for (int i = 0; i < 4; ++i) {
            sim.set_value(din, i + 1);
            sim.set_value(wren, true);
            sim.tick();
        }
        REQUIRE(sim.get_value(fifo.full) == true);
        REQUIRE(sim.get_value(fifo.count) == 4);

        // ⚠️ 调用方没检查 full ⇒ 又写了一次 0xFF。
        //    修复前：写指针已回绕到 0 ⇒ **mem[0] 被覆盖** ⇒ 数据静默损坏。
        sim.set_value(din, 0xFF);
        sim.tick();

        // 排空 —— 期望仍是原来的 1,2,3,4（`q` 是 1-cycle sread 视图 ⇒
        // 第 i 拍读到第 i 个元素）
        sim.set_value(wren, false);
        sim.set_value(rden, true);
        for (int i = 0; i < 4; ++i) {
            sim.tick();
            REQUIRE(sim.get_value(fifo.q) == i + 1);
        }
        REQUIRE(sim.get_value(fifo.count) == 0);
    }

    SECTION("读走唯一元素的那一拍，q MUST 即为该元素") {
        auto ctx = std::make_unique<ch::core::context>("test_sync_fifo_read_drift");
        ch::core::ctx_swap ctx_swapper(ctx.get());
        ch_uint<8> din(0_d);
        ch_bool wren(false), rden(true);   // rden 恒高
        SyncFifoResult<8, 2> fifo = sync_fifo<8, 2>(wren, din, rden);

        ch::Simulator sim(ctx.get());

        // ⚠️ 只空转 **1** 拍：2 位读指针绕 4 圈，空转 3 拍会恰好回绕对齐、
        //    让数据「碰巧」飘出来 ⇒ 那是**假绿**。1 拍 ⇒ 漂移 1 ⇒ 队首被跳过。
        sim.tick();

        // 写入 0x5A（落在 write_ptr = 0）
        sim.set_value(din, 0x5A);
        sim.set_value(wren, true);
        sim.tick();
        sim.set_value(wren, false);
        REQUIRE(sim.get_value(fifo.count) == 1);

        // 读走唯一元素的那一拍 ⇒ q MUST 就是 0x5A
        //    修复前：读指针已漂移到 1 ⇒ q 读到 mem[1]（从未写过）⇒ 0x0，
        //    且 count 照样归零 ⇒ **数据丢失**。
        sim.tick();
        REQUIRE(sim.get_value(fifo.q) == 0x5A);
        REQUIRE(sim.get_value(fifo.count) == 0);
    }

    SECTION("写 N 读 N MUST 逐个比对（数据完整性 · 非判别性回归护栏）") {
        // ⚠️ 本组**不是**判别性变异载体（当前实现下它本来就绿）——
        //    它是**非判别性回归护栏**：确保「指针门控」没有改变干净路径的行为。
        //    判别性载体是上面两组 + 下面两组（count / 排空后再写）。
        auto ctx = std::make_unique<ch::core::context>("test_sync_fifo_integrity");
        ch::core::ctx_swap ctx_swapper(ctx.get());
        ch_uint<8> din(0_d);
        ch_bool wren(false), rden(false);
        SyncFifoResult<8, 2> fifo = sync_fifo<8, 2>(wren, din, rden);   // 深度 4

        ch::Simulator sim(ctx.get());
        sim.tick();

        for (int i = 0; i < 4; ++i) {
            sim.set_value(din, 0x10 + i);
            sim.set_value(wren, true);
            sim.tick();
        }
        sim.set_value(wren, false);
        sim.set_value(rden, true);

        // ⚠️ `q` 是 1-cycle `sread` 视图 ⇒ 读指针沿后才前进 ⇒ 读出的第 i 个
        //    值对应第 i 个沿。与既有 "Basic FIFO operations" 用例同一时序约定。
        for (int i = 0; i < 4; ++i) {
            sim.tick();
            REQUIRE(sim.get_value(fifo.q) == 0x10 + i);
        }
        REQUIRE(sim.get_value(fifo.count) == 0);
    }

    SECTION("count MUST 只统计真正发生的传输（空读 / 满写都不改变 count）") {
        // ⚠️ 本组是**变异 #4**（`next_count` 改用裸 `wren`/`rden`）的判别性载体。
        //    若 count 用裸 wren/rden：空 FIFO 上 rden=1 ⇒ count 会下溢成非 0；
        //    满 FIFO 上 wren=1 ⇒ count 会溢出回绕 ⇒ FIFO「满而空」。
        auto ctx = std::make_unique<ch::core::context>("test_sync_fifo_count");
        ch::core::ctx_swap ctx_swapper(ctx.get());
        ch_uint<8> din(0_d);
        ch_bool wren(false), rden(false);
        SyncFifoResult<8, 2> fifo = sync_fifo<8, 2>(wren, din, rden);   // 深度 4

        ch::Simulator sim(ctx.get());
        sim.tick();

        // ---- 空 FIFO 上连续 rden=1 ⇒ count MUST 保持 0 ----
        sim.set_value(rden, true);
        for (int i = 0; i < 3; ++i) {
            sim.tick();
            REQUIRE(sim.get_value(fifo.count) == 0);
            REQUIRE(sim.get_value(fifo.empty) == true);
        }

        // ---- 填满后 wren=1 ⇒ count MUST 保持 4（不得溢出回绕）----
        sim.set_value(rden, false);
        sim.set_value(wren, true);
        for (int i = 0; i < 4; ++i) {
            sim.set_value(din, i + 1);
            sim.tick();
        }
        REQUIRE(sim.get_value(fifo.count) == 4);
        sim.set_value(din, 0xFF);
        sim.tick();
        REQUIRE(sim.get_value(fifo.count) == 4);
        REQUIRE(sim.get_value(fifo.full) == true);
    }

    SECTION("排空后再写 MUST 能被读回（写指针 MUST NOT 越过队尾）") {
        // ⚠️ 本组是**变异 #3**（写指针去掉 `will_write` 门）的判别性载体。
        //    若写指针自由前进：排空后写指针已越过队尾 ⇒ 新写的元素落在
        //    一个「读指针不会到达」的槽位 ⇒ 读回的是旧数据（实测 0x1）。
        auto ctx = std::make_unique<ch::core::context>("test_sync_fifo_rewrap");
        ch::core::ctx_swap ctx_swapper(ctx.get());
        ch_uint<8> din(0_d);
        ch_bool wren(false), rden(false);
        SyncFifoResult<8, 2> fifo = sync_fifo<8, 2>(wren, din, rden);   // 深度 4

        ch::Simulator sim(ctx.get());
        sim.tick();

        // 填满 → 满时多写一次（应被丢弃，不应推进写指针）→ 排空
        for (int i = 0; i < 4; ++i) {
            sim.set_value(din, i + 1);
            sim.set_value(wren, true);
            sim.tick();
        }
        sim.set_value(din, 0xFF);
        sim.tick();
        sim.set_value(wren, false);
        sim.set_value(rden, true);
        for (int i = 0; i < 4; ++i) {
            sim.tick();
        }
        REQUIRE(sim.get_value(fifo.count) == 0);

        // 再写 0x55 ⇒ MUST 能被读回
        sim.set_value(rden, false);
        sim.set_value(wren, true);
        sim.set_value(din, 0x55);
        sim.tick();
        sim.set_value(wren, false);
        sim.set_value(rden, true);
        sim.tick();
        REQUIRE(sim.get_value(fifo.q) == 0x55);
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

        SyncFifoResult<8, 3> result = sync_fifo<8, 3>(wren, din, rden);

        ch::Simulator sim(ctx.get());

        // 设置初始输入值并观察初始状态
        sim.set_value(wren, true);
        sim.set_value(din, 0x55);
        sim.set_value(rden, false);

        // 获取输入值并打印
        auto wren_val = sim.get_value(wren);
        auto din_val = sim.get_value(din);
        auto rden_val = sim.get_value(rden);

        std::cout << "SyncFIFO Input: wren=" << wren_val << ", din=0x"
                  << std::hex << din_val << std::dec << ", rden=" << rden_val
                  << std::endl;

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