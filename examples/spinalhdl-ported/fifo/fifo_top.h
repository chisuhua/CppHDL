// examples/spinalhdl-ported/fifo/fifo_top.h
//
// Stream FIFO 示例的**顶层组件**（三角色分离：设计定义 `*_top.h` → 激励驱动
// `tb_*.cpp` → 验证源 `tb_*.{verif,sv}`）。
//
// ⚠️ **端口 MUST 具名**（ADR-075 E1）。
// ⚠️ **端口面（6 根 · 根自有）**：`din[7:0]` · `push` · `pop` · `dout[7:0]` ·
//   `empty` · `full` ⇒ ⇒ 加框架 2 = 产物端口表 8 行。
//
// ⚠️ **语义（spec-derived = 上游 `samples/fifo_example.cpp` 的经典方案）**：
//   · 容量 = **16**（`addr_width = 4`）；
//   · `empty` = 读指针 == 写指针（**全部位**相等）；
//   · `full`  = **地址位相等 + wrap 位不同**（⇒ 在途 16 ⇒ full=1）；
//   · `dout`  = `mem.aread(rd)` ⇒ **0 拍组合读**（FWFT：非空即可见队首）。
// ⚠️ **D5 修复（2026-11-12）**：旧写法 `full = (wr + 1 == rd)` 在完整指针空间比较
//   ⇒ 门限 31 ⇒ 第 17 次写入起地址混叠（最老元素被覆盖）。现已改为经典方案。

#pragma once

#include "ch.hpp"
#include "chlib/fifo.h"
#include "component.h"
#include "module.h"

#include <string>

namespace fifo_example {

using namespace ch::core;
using namespace chlib;

/// **根组件名**（单一真相源）
inline constexpr const char *kFifoRootName = "top";

/// 流式 FIFO（深度 N · 同步读写 · FWFT 输出）
template <typename T, unsigned N> class FiFo : public ch::Component {
  public:
    static constexpr unsigned addr_width = N <= 1 ? 0 : (N & (N - 1)) ? 0 : __builtin_ctz(N);

    __io(ch_in<T> din{"din"};
         ch_in<ch_bool> push{"push"};
         ch_in<ch_bool> pop{"pop"};
         ch_out<T> dout{"dout"};
         ch_out<ch_bool> empty{"empty"};
         ch_out<ch_bool> full{"full"})

    FiFo(ch::Component *parent = nullptr, const std::string &name = "fifo")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        ch_reg<ch_uint<addr_width + 1>> rd_ptr(0_d, "rd_ptr");
        ch_reg<ch_uint<addr_width + 1>> wr_ptr(0_d, "wr_ptr");

        ch_uint<addr_width> rd_a = bits<addr_width - 1, 0>(rd_ptr);
        ch_uint<addr_width> wr_a = bits<addr_width - 1, 0>(wr_ptr);

        auto reading = io().pop;
        auto writing = io().push;

        rd_ptr->next = select(reading, rd_ptr + 1_b, rd_ptr);
        wr_ptr->next = select(writing, wr_ptr + 1_b, wr_ptr);

        ch_mem<T, N> mem("fifo_mem");
        mem.write(wr_a, io().din, writing);
        io().dout = mem.aread(rd_a);

        io().empty = (rd_ptr == wr_ptr);
        auto wr_msb = bits<addr_width, addr_width>(wr_ptr);
        auto rd_msb = bits<addr_width, addr_width>(rd_ptr);
        io().full = (wr_a == rd_a) & (wr_msb != rd_msb);
    }
};

/// 顶层组件（**具名端口** ⇒ ⇒ 产物端口名 = `top_<C++ 成员名>`）
class FifoTop : public ch::Component {
  public:
    __io(ch_in<ch_uint<8>> din{"din"};
         ch_in<ch_bool> push{"push"};
         ch_in<ch_bool> pop{"pop"};
         ch_out<ch_uint<8>> dout{"dout"};
         ch_out<ch_bool> empty{"empty"};
         ch_out<ch_bool> full{"full"})

    FifoTop(ch::Component *parent = nullptr, const std::string &name = "fifo_top")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        ch::ch_module<FiFo<ch_uint<8>, 16>> fifo_inst{"fifo_inst"};
        fifo_inst.io().din <<= io().din;
        fifo_inst.io().push <<= io().push;
        fifo_inst.io().pop <<= io().pop;
        io().dout <<= fifo_inst.io().dout;
        io().empty <<= fifo_inst.io().empty;
        io().full <<= fifo_inst.io().full;
    }
};

} // namespace fifo_example
