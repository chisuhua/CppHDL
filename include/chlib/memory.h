#ifndef CHLIB_MEMORY_H
#define CHLIB_MEMORY_H

#include "ch.hpp"
#include "chlib/logic.h"
#include "component.h"
#include "core/bool.h"
#include "core/literal.h"
#include "core/mem.h"
#include "core/operators.h"
#include "core/operators_runtime.h"
#include "core/uint.h"
#include <cassert>

using namespace ch::core;

namespace chlib {

/**
 * 单端口RAM - 函数式接口
 *
 * 单端口随机访问存储器，支持读写操作
 */
template <unsigned DATA_WIDTH, unsigned ADDR_WIDTH>
ch_uint<DATA_WIDTH>
single_port_ram(ch_uint<ADDR_WIDTH> addr, ch_uint<DATA_WIDTH> din, ch_bool we,
                const std::string &name = "single_port_ram") {

    ch_mem<ch_uint<DATA_WIDTH>, (1U << ADDR_WIDTH)> mem(name);

    // 执行写操作
    auto write_port = mem.write(addr, din, we);

    // 执行读操作
    auto read_port = mem.sread(addr, !we);

    ch_uint<DATA_WIDTH> read_data;
    read_data <<= read_port;

    return read_data;
}

/**
 * 双端口RAM - 函数式接口
 *
 * 双端口随机访问存储器，支持独立的读写端口
 */
template <unsigned DATA_WIDTH, unsigned ADDR_WIDTH> struct DualPortRAMResult {
    ch_uint<DATA_WIDTH> dout_a; // 端口A输出数据
    ch_uint<DATA_WIDTH> dout_b; // 端口B输出数据
};

template <unsigned DATA_WIDTH, unsigned ADDR_WIDTH>
DualPortRAMResult<DATA_WIDTH, ADDR_WIDTH>
dual_port_ram(ch_uint<ADDR_WIDTH> addr_a, ch_uint<DATA_WIDTH> din_a,
              ch_bool we_a, ch_uint<ADDR_WIDTH> addr_b,
              ch_uint<DATA_WIDTH> din_b, ch_bool we_b,
              const std::string &name = "dual_port_ram") {

    ch_mem<ch_uint<DATA_WIDTH>, (1U << ADDR_WIDTH)> mem(name);

    // 端口A操作
    auto write_port_a = mem.write(addr_a, din_a, we_a);
    auto read_port_a = mem.sread(addr_a, !we_a);

    // 端口B操作
    auto write_port_b = mem.write(addr_b, din_b, we_b);
    auto read_port_b = mem.sread(addr_b, !we_b);

    DualPortRAMResult<DATA_WIDTH, ADDR_WIDTH> result;
    // 如果写使能有效，返回写入数据，否则返回读出数据
    result.dout_a <<= read_port_a;
    result.dout_b <<= read_port_b;

    return result;
}

/**
 * 双端口RAM - 单时钟版本
 *
 * 双端口RAM的简化版本，使用单一时钟
 */
// template <unsigned DATA_WIDTH, unsigned ADDR_WIDTH>
// DualPortRAMResult<DATA_WIDTH, ADDR_WIDTH>
// dual_port_ram_single_clk(ch_uint<ADDR_WIDTH> addr_a, ch_uint<DATA_WIDTH>
// din_a,
//                          ch_bool we_a, ch_uint<ADDR_WIDTH> addr_b,
//                          ch_uint<DATA_WIDTH> din_b, ch_bool we_b,
//                          const std::string &name =
//                          "dual_port_ram_single_clk") {

//     ch_mem<ch_uint<DATA_WIDTH>, (1U << ADDR_WIDTH)> mem(name);

//     // 端口A操作
//     auto write_port_a = mem.write(addr_a, din_a, we_a);
//     auto read_port_a = mem.sread(addr_a, ch_bool(true));

//     // 端口B操作
//     auto write_port_b = mem.write(addr_b, din_b, we_b);
//     auto read_port_b = mem.sread(addr_b, ch_bool(true));

//     DualPortRAMResult<DATA_WIDTH, ADDR_WIDTH> result;
//     result.dout_a = select(we_a, din_a, read_port_a);
//     result.dout_b = select(we_b, din_b, read_port_b);

//     return result;
// }

/**
 * FIFO - 函数式接口
 *
 * 先进先出队列，支持同步读写
 */
template <unsigned DATA_WIDTH, unsigned ADDR_WIDTH> struct FIFOResult {
    ch_uint<DATA_WIDTH> dout;      // 输出数据
    ch_bool empty;                 // 空标志
    ch_bool full;                  // 满标志
    ch_uint<ADDR_WIDTH + 1> count; // 当前元素数量
};

// template <unsigned DATA_WIDTH, unsigned ADDR_WIDTH>
// FIFOResult<DATA_WIDTH, ADDR_WIDTH>
// sync_fifo(ch_uint<DATA_WIDTH> din, ch_bool wr_en, ch_bool rd_en,
//           const std::string &name = "sync_fifo") {

//     static constexpr unsigned DEPTH = (1U << ADDR_WIDTH);
//     ch_mem<ch_uint<DATA_WIDTH>, DEPTH> mem(name + "_mem");

//     // 使用寄存器实现FIFO的指针和计数器
//     ch_reg<ch_uint<ADDR_WIDTH>> wr_ptr(0, name + "_wr_ptr");
//     ch_reg<ch_uint<ADDR_WIDTH>> rd_ptr(0, name + "_rd_ptr");
//     ch_reg<ch_uint<ADDR_WIDTH + 1>> count_reg(0, name + "_count");

//     ch_reg<ch_bool> empty_reg(true, name + "_empty");
//     ch_reg<ch_bool> full_reg(false, name + "_full");

//     // 计算写入和读取使能信号
//     ch_bool wr_active = wr_en && !full_reg;
//     ch_bool rd_active = rd_en && !empty_reg;

//     // 写入操作
//     auto write_port = mem.write(wr_ptr, din, wr_active);

//     // 读取操作
//     auto read_port = mem.sread(rd_ptr, rd_active);

//     // 使用select语句计算下一个时钟周期的值
//     ch_uint<ADDR_WIDTH> next_wr_ptr = select(wr_active, wr_ptr + 1_d,
//     wr_ptr); ch_uint<ADDR_WIDTH> next_rd_ptr = select(rd_active, rd_ptr +
//     1_d, rd_ptr);

//     ch_uint<ADDR_WIDTH + 1> next_count = count_reg;
//     next_count = select(wr_active && !rd_active, count_reg + 1_d,
//     next_count); next_count = select(!wr_active && rd_active, count_reg -
//     1_d, next_count);

//     ch_bool next_empty = (next_count == 0_d);
//     ch_bool next_full = (next_count == DEPTH);

//     // 连接寄存器的下一个值
//     wr_ptr->next = next_wr_ptr;
//     rd_ptr->next = next_rd_ptr;
//     count_reg->next = next_count;
//     empty_reg->next = next_empty;
//     full_reg->next = next_full;

//     FIFOResult<DATA_WIDTH, ADDR_WIDTH> result;

//     // 使用组合逻辑输出 - 直接输出read_port
//     result.dout <<= read_port;
//     result.empty = empty_reg;
//     result.full = full_reg;
//     result.count = count_reg;

//     return result;
// }

/**
 * Content Addressable Memory (CAM) - 相联存储器
 *
 * 根据内容查找地址的存储器，支持并行查找
 */
// template <unsigned ADDR_WIDTH, unsigned DATA_WIDTH> struct CamResult {
//     ch_bool hit;              // 是否找到匹配项
//     ch_uint<ADDR_WIDTH> addr; // 匹配项的地址
//     ch_uint<DATA_WIDTH> data; // 匹配项的数据
// };

// template <unsigned ADDR_WIDTH, unsigned DATA_WIDTH> class Cam {
// private:
//     ch_mem<ch_uint<DATA_WIDTH>, (1U << ADDR_WIDTH)> memory;
//     ch_reg<ch_uint<(1U << ADDR_WIDTH)>> valid_bits; // 有效位寄存器

// public:
//     Cam(const std::string &name = "cam")
//         : memory(name + "_memory"), valid_bits(0_d, name + "_valid_bits") {}

//     CamResult<ADDR_WIDTH, DATA_WIDTH> search(ch_uint<DATA_WIDTH> search_data)
//     {
//         CamResult<ADDR_WIDTH, DATA_WIDTH> result;
//         result.hit = 0_b;
//         result.addr = 0_d;
//         result.data = 0_d;

//         // 遍历所有存储位置，查找匹配项
//         for (unsigned i = 0; i < (1U << ADDR_WIDTH); ++i) {
//             ch_uint<DATA_WIDTH> stored_data =
//                 memory.sread(make_literal(i), 1_b);
//             ch_bool is_valid = bit_select(valid_bits, i);
//             ch_bool data_matches = (stored_data == search_data);

//             // 如果数据匹配且有效，则标记为命中
//             ch_bool match = data_matches && is_valid;
//             result.hit = result.hit || match;

//             // 更新地址和数据（返回第一个匹配项）
//             result.addr =
//                 select(match && !result.hit, make_literal(i), result.addr);
//             result.data =
//                 select(match && !result.hit, stored_data, result.data);
//         }

//         return result;
//     }

//     void write(ch_uint<ADDR_WIDTH> addr, ch_uint<DATA_WIDTH> data,
//                ch_bool write_enable) {
//         memory.write(addr, data, write_enable);

//         // 更新有效位
//         ch_uint<(1U << ADDR_WIDTH)> mask = ch_uint<(1U << ADDR_WIDTH)>(1_d)
//                                            << addr;
//         ch_uint<(1U << ADDR_WIDTH)> new_valid =
//             select(write_enable, valid_bits | mask, valid_bits);
//         valid_bits = new_valid;
//     }

//     void invalidate(ch_uint<ADDR_WIDTH> addr, ch_bool invalidate_enable) {
//         if_(invalidate_enable) {
//             ch_uint<(1U << ADDR_WIDTH)> mask =
//                 ~(ch_uint<(1U << ADDR_WIDTH)>(1_d) << addr);
//             valid_bits = valid_bits & mask;
//         }
//     }

//     ch_uint<DATA_WIDTH> read(ch_uint<ADDR_WIDTH> addr) {
//         return memory.sread(addr, true_d);
//     }
// };

// /**
//  * Ternary Content Addressable Memory (TCAM) - 三态相联存储器
//  *
//  * 支持通配符匹配的相联存储器
//  */
// template <unsigned ADDR_WIDTH, unsigned DATA_WIDTH> struct TcamResult {
//     ch_bool hit;              // 是否找到匹配项
//     ch_uint<ADDR_WIDTH> addr; // 匹配项的地址
//     ch_uint<DATA_WIDTH> data; // 匹配项的数据
// };

// template <unsigned ADDR_WIDTH, unsigned DATA_WIDTH> class Tcam {
// private:
//     ch_mem<ch_uint<DATA_WIDTH>, (1U << ADDR_WIDTH)> memory;
//     ch_mem<ch_uint<DATA_WIDTH>, (1U << ADDR_WIDTH)> mask_memory; // 掩码存储
//     ch_reg<ch_uint<(1U << ADDR_WIDTH)>> valid_bits; // 有效位寄存器

// public:
//     Tcam(const std::string &name = "tcam")
//         : memory(name + "_memory"), mask_memory(name + "_mask"),
//           valid_bits(0_d, name + "_valid_bits") {}

//     TcamResult<ADDR_WIDTH, DATA_WIDTH> search(ch_uint<DATA_WIDTH>
//     search_data) {
//         TcamResult<ADDR_WIDTH, DATA_WIDTH> result;
//         result.hit = false_d;
//         result.addr = 0_d;
//         result.data = 0_d;

//         // 遍历所有存储位置，查找匹配项
//         for (unsigned i = 0; i < (1U << ADDR_WIDTH); ++i) {
//             ch_uint<DATA_WIDTH> stored_data =
//                 memory.sread(make_literal(i), true_d);
//             ch_uint<DATA_WIDTH> mask =
//                 mask_memory.sread(make_literal(i), true_d);
//             ch_bool is_valid = bit_select(valid_bits, i);

//             // 应用掩码，比较数据
//             ch_uint<DATA_WIDTH> masked_search = search_data & mask;
//             ch_uint<DATA_WIDTH> masked_stored = stored_data & mask;
//             ch_bool data_matches = (masked_search == masked_stored);

//             // 如果数据匹配且有效，则标记为命中
//             ch_bool match = data_matches && is_valid;
//             result.hit = result.hit || match;

//             // 更新地址和数据（返回第一个匹配项）
//             result.addr =
//                 select(match && !result.hit, make_literal(i), result.addr);
//             result.data =
//                 select(match && !result.hit, stored_data, result.data);
//         }

//         return result;
//     }

//     void write(ch_uint<ADDR_WIDTH> addr, ch_uint<DATA_WIDTH> data,
//                ch_uint<DATA_WIDTH> mask, ch_bool write_enable) {
//         memory.write(addr, data, write_enable);
//         mask_memory.write(addr, mask, write_enable);

//         // 更新有效位
//         ch_uint<(1U << ADDR_WIDTH)> valid_mask =
//             ch_uint<(1U << ADDR_WIDTH)>(1_d) << addr;
//         ch_uint<(1U << ADDR_WIDTH)> new_valid =
//             select(write_enable, valid_bits | valid_mask, valid_bits);
//         valid_bits = new_valid;
//     }

//     void invalidate(ch_uint<ADDR_WIDTH> addr, ch_bool invalidate_enable) {
//         if_(invalidate_enable) {
//             ch_uint<(1U << ADDR_WIDTH)> mask =
//                 ~(ch_uint<(1U << ADDR_WIDTH)>(1_d) << addr);
//             valid_bits = valid_bits & mask;
//         }
//     }
// };

} // namespace chlib

#endif // CHLIB_MEMORY_H