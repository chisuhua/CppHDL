#ifndef CHLIB_FIFO_H
#define CHLIB_FIFO_H

#include "ch.hpp"
#include "chlib/logic.h"
#include "chlib/memory.h"
#include "component.h"
#include "core/bool.h"
#include "core/context.h"
#include "core/literal.h"
#include "core/operators.h"
#include "core/operators_runtime.h"
#include "core/uint.h"
#include <cassert>

using namespace ch::core;

namespace chlib {
/**
 * 辅助函数：判断计数器是否为空
 */
template <unsigned N> ch_bool is_empty(ch_uint<N> count) {
    return count == 0_d;
}

/**
 * 辅助函数：判断计数器是否为满
 */
template <unsigned N> ch_bool is_full(ch_uint<N> count) {
    static constexpr unsigned MAX_COUNT =
        (1 << N) - 1; // 修复：使用N而不是ADDR_WIDTH
    return count == make_literal<MAX_COUNT>();
}

/**
 * 同步FIFO (First In First Out) - 函数式接口
 *
 * 支持同步读写操作的先进先出队列
 */
template <unsigned DATA_WIDTH, unsigned ADDR_WIDTH> struct SyncFifoResult {
    ch_bool empty;
    ch_bool full;
    ch_uint<DATA_WIDTH> q;
    ch_uint<ADDR_WIDTH + 1> count;
};

template <unsigned DATA_WIDTH, unsigned ADDR_WIDTH>
SyncFifoResult<DATA_WIDTH, ADDR_WIDTH>
sync_fifo(ch_bool wren, ch_uint<DATA_WIDTH> din, ch_bool rden,
          ch_uint<ADDR_WIDTH> threshold = 0_d) {
    static_assert(DATA_WIDTH > 0, "Data width must be greater than 0");
    static_assert(ADDR_WIDTH > 0, "Address width must be greater than 0");

    // 内部存储器
    ch_mem<ch_uint<DATA_WIDTH>, (1 << ADDR_WIDTH)> memory("sync_fifo_memory");

    // 读写指针
    ch_reg<ch_uint<ADDR_WIDTH>> read_ptr(0_d, "sync_fifo_read_ptr");
    ch_reg<ch_uint<ADDR_WIDTH>> write_ptr(0_d, "sync_fifo_write_ptr");

    // 计数器
    ch_reg<ch_uint<ADDR_WIDTH + 1>> count(0_d, "sync_fifo_count");

    // 写入操作
    ch_bool write_enable = wren && !is_full(count);
    memory.write(write_ptr, din, write_enable);

    // 更新写指针
    ch_uint<ADDR_WIDTH> next_write_ptr =
        select(write_enable, write_ptr + 1_d, write_ptr);
    write_ptr->next = next_write_ptr;

    // 读取操作 - 创建读端口
    ch_uint<DATA_WIDTH> read_data(0_d);
    auto read_port = memory.sread(read_ptr, ch_bool(true));
    read_data <<= read_port;
    ch_bool read_enable = rden && !is_empty(count);

    // 更新读指针
    ch_uint<ADDR_WIDTH> next_read_ptr =
        select(read_enable, read_ptr + 1_d, read_ptr);
    read_ptr->next = next_read_ptr;

    // 更新计数器
    ch_uint<ADDR_WIDTH + 1> next_count;
    next_count =
        select(write_enable && !read_enable, count + 1_d,
               select(!write_enable && read_enable, count - 1_d, count));
    count->next = next_count;

    SyncFifoResult<DATA_WIDTH, ADDR_WIDTH> result;
    result.empty = is_empty(count);
    result.full = is_full(count);
    result.q = read_data;
    result.count = count;

    return result;
}

/**
 * First-Word Fall-Through (FWFT) FIFO - 先字后进FIFO
 *
 * 输出端口始终显示下一个可读取的数据
 */
template <unsigned DATA_WIDTH, unsigned ADDR_WIDTH> struct FwftFifoResult {
    ch_bool empty;
    ch_bool full;
    ch_uint<DATA_WIDTH> q; // 始终输出下一个可读取的数据
    ch_uint<ADDR_WIDTH + 1> count;
};

template <unsigned DATA_WIDTH, unsigned ADDR_WIDTH>
FwftFifoResult<DATA_WIDTH, ADDR_WIDTH>
fwft_fifo(ch_bool wren, ch_uint<DATA_WIDTH> din, ch_bool rden) {
    static_assert(DATA_WIDTH > 0, "Data width must be greater than 0");
    static_assert(ADDR_WIDTH > 0, "Address width must be greater than 0");

    // 内部存储器
    ch_mem<ch_uint<DATA_WIDTH>, (1 << ADDR_WIDTH)> memory("fwft_fifo_memory");

    // 获取默认时钟和复位信号

    // 读写指针
    ch_reg<ch_uint<ADDR_WIDTH>> read_ptr(0_d, "fwft_fifo_read_ptr");
    ch_reg<ch_uint<ADDR_WIDTH>> write_ptr(0_d, "fwft_fifo_write_ptr");

    // 计数器
    ch_reg<ch_uint<ADDR_WIDTH + 1>> count(0_d, "fwft_fifo_count");

    // 数据输出寄存器
    ch_reg<ch_uint<DATA_WIDTH>> output_reg(0_d, "fwft_fifo_output");

    // 写入操作
    ch_bool write_enable = wren && !is_full(count);
    memory.write(write_ptr, din, write_enable);

    // 更新写指针
    ch_uint<ADDR_WIDTH> next_write_ptr =
        select(write_enable, write_ptr + 1_d, write_ptr);
    write_ptr->next = next_write_ptr;

    // 读取操作 - 创建读端口
    ch_uint<DATA_WIDTH> read_port(0_d);
    read_port <<= memory.sread(read_ptr, ch_bool(1_b));
    ch_bool read_enable = rden && !is_empty(count);

    // 更新读指针
    ch_uint<ADDR_WIDTH> next_read_ptr =
        select(read_enable, read_ptr + 1_d, read_ptr);
    read_ptr->next = next_read_ptr;

    // 更新输出寄存器
    ch_uint<DATA_WIDTH> new_output =
        select(is_empty(count), // 如果为空，则输出保持不变
               output_reg,      // 否则更新为下一个可读取的数据
               read_port);      // 使用读端口数据
    output_reg->next =
        select(read_enable || is_empty(count + 1_d), // 如果读使能或下个周期为空
               new_output,                           // 更新输出
               output_reg                            // 否则保持不变
        );

    // 更新计数器
    ch_uint<ADDR_WIDTH + 1> next_count;
    next_count =
        select(write_enable && !read_enable, count + 1_d,
               select(!write_enable && read_enable, count - 1_d, count));
    count->next = next_count;

    FwftFifoResult<DATA_WIDTH, ADDR_WIDTH> result;
    result.empty = is_empty(count);
    result.full = is_full(count);
    result.q = output_reg;
    result.count = count;

    return result;
}

/**
 * 异步FIFO - 异步读写操作
 *
 * FIXME: 目前暂不支持，因为需要两个不同的时钟域
 * 支持不同时钟域的读写操作
 */
/*
template <unsigned DATA_WIDTH, unsigned ADDR_WIDTH> struct AsyncFifoResult {
    ch_bool empty;
    ch_bool full;
    ch_uint<DATA_WIDTH> q;
};

template <unsigned DATA_WIDTH, unsigned ADDR_WIDTH>
AsyncFifoResult<DATA_WIDTH, ADDR_WIDTH>
async_fifo(ch_bool wr_clk, ch_bool wr_rst, ch_bool wren,
           ch_uint<DATA_WIDTH> din, ch_bool rd_clk, ch_bool rd_rst,
           ch_bool rden) {
    static_assert(DATA_WIDTH > 0, "Data width must be greater than 0");
    static_assert(ADDR_WIDTH > 0, "Address width must be greater than 0");

    // 内部存储器
    ch_mem<1 << ADDR_WIDTH, DATA_WIDTH> memory("async_fifo_memory");

    // 写时钟域
    ch_reg<ch_uint<ADDR_WIDTH>> wr_ptr(0_d, "async_fifo_wr_ptr");
    ch_reg<ch_uint<ADDR_WIDTH>> rd_ptr_gray_sync(0_d,
                                                 "async_fifo_rd_ptr_gray_sync");
    ch_reg<ch_uint<ADDR_WIDTH + 1>> wr_count(0_d, "async_fifo_wr_count");

    // 读时钟域
    ch_reg<ch_uint<ADDR_WIDTH>> rd_ptr(0_d, "async_fifo_rd_ptr");
    ch_reg<ch_uint<ADDR_WIDTH>> wr_ptr_gray_sync(0_d,
                                                 "async_fifo_wr_ptr_gray_sync");
    ch_reg<ch_uint<ADDR_WIDTH + 1>> rd_count(0_d, "async_fifo_rd_count");

    // 同步器 - 用于跨时钟域传输指针
    ch_reg<ch_uint<ADDR_WIDTH>> sync_stage1(0_d, "sync_stage1");
    ch_reg<ch_uint<ADDR_WIDTH>> sync_stage2(0_d, "sync_stage2");

    // 写时钟域逻辑
    wr_ptr->clk = wr_clk;
    wr_ptr->rst = wr_rst;
    rd_ptr_gray_sync->clk = wr_clk;
    rd_ptr_gray_sync->rst = wr_rst;
    wr_count->clk = wr_clk;
    wr_count->rst = wr_rst;

    // 读时钟域逻辑
    rd_ptr->clk = rd_clk;
    rd_ptr->rst = rd_rst;
    wr_ptr_gray_sync->clk = rd_clk;
    wr_ptr_gray_sync->rst = rd_rst;
    rd_count->clk = rd_clk;
    rd_count->rst = rd_rst;

    // 格雷码转换辅助函数（在实际实现中需要）
    auto to_gray = [](ch_uint<ADDR_WIDTH> binary) -> ch_uint<ADDR_WIDTH> {
        return binary ^ (binary >> 1_d);
    };

    auto from_gray = [](ch_uint<ADDR_WIDTH> gray) -> ch_uint<ADDR_WIDTH> {
        ch_uint<ADDR_WIDTH> binary = gray;
        for (int i = ADDR_WIDTH - 2; i >= 0; --i) {
            ch_bool bit = bit_select(binary, i + 1) ^ bit_select(gray, i);
            binary = binary | (ch_uint<ADDR_WIDTH>(bit) << make_literal(i));
        }
        return binary;
    };

    // 写入操作
    ch_uint<ADDR_WIDTH> wr_ptr_gray = to_gray(wr_ptr);
    ch_bool wr_full = (to_gray(rd_ptr_gray_sync) ==
                       wr_ptr_gray + (1 << make_literal(ADDR_WIDTH - 1)));
    ch_bool write_enable = wren && !wr_full;

    memory.write(wr_clk, wr_ptr, din, write_enable);

    // 更新写指针
    wr_ptr->next = select(write_enable, wr_ptr + 1_d, wr_ptr);

    // 读取操作
    ch_uint<ADDR_WIDTH> rd_ptr_gray = to_gray(rd_ptr);
    ch_bool rd_empty = (wr_ptr_gray_sync == rd_ptr_gray);
    ch_bool read_enable = rden && !rd_empty;
    ch_uint<DATA_WIDTH> output_data = memory.read(rd_ptr);

    // 更新读指针
    rd_ptr->next = select(read_enable, rd_ptr + 1_d, rd_ptr);

    // 跨时钟域同步
    // 同步读指针到写时钟域
    sync_stage1->clk = wr_clk;
    sync_stage1->rst = wr_rst;
    sync_stage2->clk = wr_clk;
    sync_stage2->rst = wr_rst;
    sync_stage1->next = to_gray(rd_ptr);
    sync_stage2->next = sync_stage1;
    rd_ptr_gray_sync->next = sync_stage2;

    // 同步写指针到读时钟域
    sync_stage1->clk = rd_clk;
    sync_stage1->rst = rd_rst;
    sync_stage2->clk = rd_clk;
    sync_stage2->rst = rd_rst;
    sync_stage1->next = wr_ptr_gray;
    sync_stage2->next = sync_stage1;
    wr_ptr_gray_sync->next = sync_stage2;

    AsyncFifoResult<DATA_WIDTH, ADDR_WIDTH> result;
    result.empty = rd_empty;
    result.full = wr_full;
    result.q = output_data;

    return result;
}
*/

/**
 * LIFO Stack - 后进先出栈
 *
 * 支持栈操作的数据结构
 */
template <unsigned DATA_WIDTH, unsigned ADDR_WIDTH> struct LifoResult {
    ch_bool empty;
    ch_bool full;
    ch_uint<DATA_WIDTH> q;
};

template <unsigned DATA_WIDTH, unsigned ADDR_WIDTH>
LifoResult<DATA_WIDTH, ADDR_WIDTH>
lifo_stack(ch_bool push, ch_uint<DATA_WIDTH> din, ch_bool pop) {
    static_assert(DATA_WIDTH > 0, "Data width must be greater than 0");
    static_assert(ADDR_WIDTH > 0, "Address width must be greater than 0");

    // 内部存储器
    ch_mem<ch_uint<DATA_WIDTH>, (1 << ADDR_WIDTH)> memory("lifo_memory");

    // 栈指针
    ch_reg<ch_uint<ADDR_WIDTH>> stack_ptr(0_d, "lifo_stack_ptr");

    // push操作
    ch_bool push_enable = push && !is_full(stack_ptr);
    memory.write(stack_ptr, din, push_enable);

    // 更新栈指针
    ch_uint<ADDR_WIDTH> next_stack_ptr =
        select(push_enable && !pop, stack_ptr + 1_d,
               select(!push_enable && pop && !is_empty(stack_ptr),
                      stack_ptr - 1_d, stack_ptr));
    stack_ptr->next = next_stack_ptr;

    // pop操作
    ch_bool pop_enable = pop && !is_empty(stack_ptr);
    ch_uint<ADDR_WIDTH> pop_addr =
        select(pop_enable, stack_ptr - 1_d, stack_ptr);
    ch_uint<DATA_WIDTH> read_port(0_d);
    read_port <<= memory.sread(pop_addr, ch_bool(true));

    LifoResult<DATA_WIDTH, ADDR_WIDTH> result;
    result.empty = is_empty(stack_ptr);
    result.full = is_full(stack_ptr);
    result.q = read_port; // 直接使用读端口

    return result;
}

} // namespace chlib

#endif // CHLIB_FIFO_H