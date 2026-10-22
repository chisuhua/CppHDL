// samples/fifo_bundle.cpp
#include "bundle/common_bundles.h"
#include "ch.hpp"
#include "codegen_verilog.h"
#include "component.h"
#include "core/bundle/bundle_utils.h"
#include "module.h"
#include "simulator.h"
#include <iostream>

using namespace ch::core;

// 辅助函数
constexpr bool ispow2(unsigned n) { return n > 0 && (n & (n - 1)) == 0; }

constexpr unsigned log2ceil(unsigned n) {
    if (n <= 1)
        return 0;
    unsigned result = 0;
    while (n > 1) {
        result++;
        n >>= 1;
    }
    return result;
}

template <typename T, unsigned N> class FiFo : public ch::Component {
public:
    static_assert(ispow2(N), "FIFO size must be power of 2");
    static constexpr unsigned addr_width = log2ceil(N);

    // 使用Bundle作为IO接口
    fifo_bundle<T> io;

    FiFo(ch::Component *parent = nullptr, const std::string &name = "fifo")
        : ch::Component(parent, name) {
        io.as_slave();
    }

    void describe() override {
        // 创建读写指针寄存器（addr_width + 1 位用于满/空检测）
        ch_reg<ch_uint<addr_width + 1>> rd_ptr(0_d, "rd_ptr");
        ch_reg<ch_uint<addr_width + 1>> wr_ptr(0_d, "wr_ptr");

        // 提取地址位（低 addr_width 位）
        ch_uint<addr_width> rd_a, wr_a;

        if constexpr (addr_width == 1) {
            // 特殊处理1位地址的情况
            auto rd_bit = bit_select<0>(rd_ptr);
            auto wr_bit = bit_select<0>(wr_ptr);
            rd_a = zext<addr_width>(rd_bit);
            wr_a = zext<addr_width>(wr_bit);
        } else {
            // 一般情况使用bits提取
            rd_a = bits<addr_width - 1, 0>(rd_ptr);
            wr_a = bits<addr_width - 1, 0>(wr_ptr);
        }

        // 控制信号
        auto reading = io.pop;
        auto writing = io.push;

        // 指针更新逻辑
        rd_ptr->next = select(reading, rd_ptr + 1_b, rd_ptr);
        wr_ptr->next = select(writing, wr_ptr + 1_b, wr_ptr);

        // 创建内存
        ch_mem<T, N> mem("fifo_mem");

        // 写操作
        mem.write(wr_a, io.data, writing);

        // 读操作（异步读取）—— **读数据通路**（ADR-044）
        //
        // ⚠️ 此前这里算出 `data_out` 之后**直接丢弃**（注释：「io.data 是输入，
        //    不能赋值……数据输出通过其他方式提供」）⇒ **FIFO 永远送不出数据**。
        //    根因：`fifo_bundle` 当时**只有一个数据字段** `data`，它被
        //    **读写双工共用** ⇒ slave 侧无处输出读到的数据。
        // ⇒ ADR-044 给 `fifo_bundle` 加了**独立的** `rdata` 字段
        //    （master IN / slave OUT）⇒ 读数据终于有地方可去。
        //
        // ⚠️ MUST 用 `<<=`（数据流边）而不是 `=`（节点重绑定）——
        //    `=` 会把字段指向另一个节点 ⇒ 若该赋值发生在 connect() **之后**，
        //    已建立的连接边仍指向被孤儿化的旧节点 ⇒ master 侧读到 0。
        //    （与下面 `io.empty` / `io.full` 同一模式）
        //
        // ⚠️ FIFO 协议（ADR-044 裁决 5）：`aread` 是 0-cycle ⇒ `dout` 在
        //    pop 沿**之后**指向**下一个**队首 ⇒ 消费者 MUST 在 pop 沿
        //    **之前**采样才能拿到**正在被弹出**的那个元素。
        io.rdata <<= mem.aread(rd_a);

        // 状态信号 - 使用更标准的FIFO空/满检测
        //
        // ⚠️ MUST 用 `<<=`（数据流边）而不是 `=`（节点重绑定）：
        // `=` 会把字段指向另一个节点 ⇒ 若该赋值发生在 connect() **之后**，
        // 已建立的连接边仍指向被孤儿化的旧节点 ⇒ master 侧读到 0。
        // （OpenSpec change `bundle-master-slave-io-2026-10`）
        // 空状态：读指针等于写指针
        io.empty <<= (rd_ptr == wr_ptr);
        // 满状态：写指针+1等于读指针
        auto wr_plus_one = wr_ptr + 1_b;
        io.full <<= (wr_plus_one == rd_ptr);
    }
};

// 顶层模块 —— 顶层用**普通端口**承载 TB 接口，bundle 只用于**模块间**连接。
//
// ⚠️ 为什么顶层不用 bundle（OpenSpec change `bundle-master-slave-io-2026-10` 的 D6）：
// 一个 bundle 成员**不会**被注册为组件的 IO ⇒ 它的信号不会成为生成的 Verilog 的
// **顶层端口**（实测：改前生成的 fifo_bundle.v 顶层只有 default_clock /
// default_reset，bundle 的 data/push/pop/full/empty 全是内部 logic）
// ⇒ 生成的 Verilog 无法被外部 TB 驱动。
// 根 `AGENTS.md` CONVENTIONS 亦有明文裁决：
// 「Bundle as IO: Use individual `ch_bool`/`ch_uint` ports, NOT bundle struct」。
class Top : public ch::Component {
public:
    __io(ch_in<ch_uint<2>> data; ch_in<ch_bool> push; ch_in<ch_bool> pop;
         ch_out<ch_uint<2>> rdata;   // 读数据（ADR-044）
         ch_out<ch_bool> full; ch_out<ch_bool> empty;)

    Top(ch::Component *parent = nullptr, const std::string &name = "top")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        ch::ch_module<FiFo<ch_uint<2>, 2>> fifo_inst{"fifo_inst"};

        // 顶层端口 → FIFO 模块的 bundle 字段（逐字段接线）
        // ⚠️ 这里的 bundle 是 **slave** 侧：`push` / `pop` / `data` 是它的输入，
        //    `full` / `empty` 是它的输出。
        fifo_inst.instance().io.push <<= io().push;
        fifo_inst.instance().io.pop <<= io().pop;
        fifo_inst.instance().io.data <<= io().data;
        // ⚠️ 读数据通路（ADR-044）：slave 侧的 `rdata` → 顶层输出端口
        io().rdata <<= fifo_inst.instance().io.rdata;
        io().full <<= fifo_inst.instance().io.full;
        io().empty <<= fifo_inst.instance().io.empty;
    }
};

int main() {
    {
        ch::ch_device<Top> top_device;

        ch::Simulator sim(top_device.context());

        // 初始化输入（顶层是普通端口）
        auto &io = top_device.instance().io();
        sim.set_input_value(io.data, 0);
        sim.set_input_value(io.push, 0);
        sim.set_input_value(io.pop, 0);

        std::cout << "Starting FIFO Bundle simulation..." << std::endl;
        std::cout << "FIFO size: 2, addr_width: 1, pointer width: 2"
                  << std::endl;

        // ⚠️ **期望值表**（ADR-044 裁决 5）：`aread` 是 0-cycle ⇒ `rdata` 跟随
        //    `rd_ptr` ⇒ **在 pop 沿之后**它指向**下一个**队首 ⇒ 消费者 MUST
        //    在 pop 沿**之前**采样才能拿到**正在被弹出**的那个元素。
        //    本 sample 的循环是「tick → 读 → 写激励」⇒ 读到的 `rdata` 是
        //    **本拍 tick 之后**的值 ⇒ 期望值表必须按这个语义写。
        //
        //    ⚠️ FIFO 为空时 `rdata` 是 **don't-care**（ADR-044 裁决 4）
        //    ⇒ 只在 `!empty` 时断言。
        struct RdataExpect {
            int cycle;
            int expected;   // -1 = 不检查 `rdata`（空 FIFO ⇒ don't-care）
        };
        static const RdataExpect kRdataExpect[] = {
            {0, -1},   // 初始：空 ⇒ don't-care
            {1, 0x1},  // 刚写入 1 ⇒ rd_ptr 仍指 0 ⇒ rdata = 1
            {2, 0x1},  // 又写入 2（排在后面）⇒ 队首仍是 1
            {3, 0x2},  // pop 沿生效 ⇒ rd_ptr 前进 ⇒ rdata 指向**下一个**队首 2
            {4, -1},   // 已弹空 ⇒ don't-care
        };

        for (int cycle = 0; cycle <= 12; ++cycle) {
            sim.tick();

            // 读取状态（由 slave 侧的 bundle 字段驱动，穿过连接到达顶层端口）
            uint64_t data_val =
                static_cast<uint64_t>(sim.get_port_value(io.data));
            uint64_t full_val =
                static_cast<uint64_t>(sim.get_port_value(io.full));
            uint64_t empty_val =
                static_cast<uint64_t>(sim.get_port_value(io.empty));
            // ⚠️ **读数据**（ADR-044）：`rdata` 是 FIFO 真正**弹出**的元素 ——
            //    此前 `fifo_bundle` 没有独立的读数据字段 ⇒ 这里打印的是 TB 刚
            //    写进去的值，**不是** FIFO 弹出的元素。
            uint64_t rdata_val =
                static_cast<uint64_t>(sim.get_port_value(io.rdata));

            std::cout << "Cycle " << cycle << ": data=0x" << std::hex
                      << data_val << ", rdata=0x" << rdata_val
                      << ", full=" << full_val
                      << ", empty=" << empty_val << std::dec << std::endl;

            // ⚠️ 断言读数据（ADR-044）—— 只在**期望值表**给出的 cycle 检查
            for (const auto &e : kRdataExpect) {
                if (e.cycle != cycle) {
                    continue;
                }
                if (e.expected < 0) {
                    // 空 FIFO ⇒ `rdata` 是 don't-care ⇒ 只断言「确实是空的」
                    if (empty_val != 1) {
                        std::cerr << "ERROR: cycle " << cycle
                                  << ": expected empty FIFO, got empty=" << empty_val
                                  << std::endl;
                        return 1;
                    }
                    break;
                }
                if (rdata_val != static_cast<uint64_t>(e.expected)) {
                    std::cerr << "ERROR: cycle " << cycle << ": rdata=0x" << std::hex
                              << rdata_val << " expected 0x" << e.expected << std::dec
                              << std::endl;
                    return 1;
                }
            }

            // 测试激励
            switch (cycle) {
            case 0:
                // 初始状态检查：空且不满
                std::cout << "  Initial state check..." << std::endl;
                if (empty_val != 1 || full_val != 0) {
                    std::cerr << "ERROR: Initial state incorrect!" << std::endl;
                    return 1;
                }
                // 写入数据 1
                sim.set_input_value(io.data, 1);
                sim.set_input_value(io.push, 1);
                std::cout << "  Writing data 1 to FIFO" << std::endl;
                break;
            case 1:
                // 继续写入数据 2
                std::cout << "  Continuing write of data 1, writing data 2"
                          << std::endl;
                sim.set_input_value(io.data, 2);
                sim.set_input_value(io.push, 1);
                break;
            case 2:
                // 停止写入，开始读取
                std::cout << "  Checking FIFO state after first write"
                          << std::endl;
                sim.set_input_value(io.push, 0);
                sim.set_input_value(io.pop, 1);
                std::cout << "  Preparing to read first data from FIFO"
                          << std::endl;
                break;
            case 3:
                // 继续弹出
                std::cout << "  Continuing read" << std::endl;
                sim.set_input_value(io.pop, 1);
                break;
            case 4:
                // 停止读取：此时 FIFO 应回到「空」
                std::cout << "  Checking FIFO state after the pops"
                          << std::endl;
                sim.set_input_value(io.pop, 0);
                if (empty_val != 1) {
                    std::cerr << "ERROR: FIFO should be empty after popping "
                              << "both elements!" << std::endl;
                    return 1;
                }
                std::cout << "  Stopping read" << std::endl;
                break;
            default:
                // 保持当前状态
                break;
            }
        }

        // 生成Verilog
        // aread codegen note: this FIFO emits `assign dout = mem[rd_ptr]` (0-cycle
        // combinational read, no downstream register). The Simulator matches this
        // 0-cycle timing exactly — after the pop edge `dout` follows the advanced
        // head pointer, so sample `dout` before the pop edge to get the element
        // being popped. See docs/simulation/ARCHITECTURE.md "Memory read port
        // timing".
        ch::toVerilog("fifo_bundle.v", top_device.context());
    }

    std::cout << "FIFO Bundle simulation completed successfully!" << std::endl;
    return 0;
}