/**
 * AXI4-Lite Timer Controller
 *
 * 将 Phase 1+2 的 Timer 封装成 AXI4 从设备
 *
 * 寄存器映射:
 * - 0x00: LOAD     - 重载值寄存器
 * - 0x04: COUNT    - 当前计数值
 * - 0x08: CTRL     - 控制寄存器 (ENABLE, AUTO_RELOAD, INTERRUPT_EN)
 * - 0x0C: STATUS   - 状态寄存器 (TIMEOUT)
 *
 * 通道形态（AXI4-Lite 规范形态，见 openspec change
 * fix-axi4-peripheral-rst-n-port-2026-10）:
 * - AW 与 W 相位分离（W 可在 AW 之后的任意周期被接受）
 * - BVALID 在 WDATA 被接受之后拉高，并保持到 BREADY
 * - RVALID 保持到 RREADY
 * - 全部状态寄存器带 rst_n（低电平有效）复位
 */

#pragma once

#include "ch.hpp"
#include "component.h"

namespace axi4 {

using namespace ch::core;

template <unsigned COUNTER_WIDTH = 32>
class AxiLiteTimer : public ch::Component {
public:
    __io(
        // AXI4-Lite 接口
        ch_in<ch_uint<32>> awaddr;
        ch_in<ch_uint<2>> awprot;
        ch_in<ch_bool> awvalid;
        ch_out<ch_bool> awready;

        ch_in<ch_uint<32>> wdata;
        ch_in<ch_uint<4>> wstrb;
        ch_in<ch_bool> wvalid;
        ch_out<ch_bool> wready;

        ch_out<ch_uint<2>> bresp;
        ch_out<ch_bool> bvalid;
        ch_in<ch_bool> bready;

        ch_in<ch_uint<32>> araddr;
        ch_in<ch_uint<2>> arprot;
        ch_in<ch_bool> arvalid;
        ch_out<ch_bool> arready;

        ch_out<ch_uint<32>> rdata;
        ch_out<ch_uint<2>> rresp;
        ch_out<ch_bool> rvalid;
        ch_in<ch_bool> rready;

        // Timer 物理接口
        ch_out<ch_bool> irq;

        // 异步复位 (低电平有效)
        ch_in<ch_bool> rst_n;
    )

    AxiLiteTimer(ch::Component* parent = nullptr, const std::string& name = "axi_timer")
        : ch::Component(parent, name) {}

    void create_ports() override {
        new (io_storage_) io_type;
    }

    void describe() override {
        // 状态机寄存器组
        ch_reg<ch_bool> busy(ch_bool(false));
        ch_reg<ch_bool> aw_done(ch_bool(false));   // AW 已接收，等待 W
        ch_reg<ch_bool> b_valid(ch_bool(false));   // W 已接收，B 响应待取
        ch_reg<ch_bool> r_valid(ch_bool(false));   // AR 已接收，R 响应待取

        // 寄存器
        ch_reg<ch_uint<32>> load_reg(0_d);
        ch_reg<ch_uint<32>> count_reg(0_d);
        ch_reg<ch_uint<32>> ctrl_reg(0_d);
        ch_reg<ch_uint<32>> status_reg(0_d);

        // 地址锁存寄存器：AXI 只要求 AWADDR/ARADDR 保持到对应握手，
        // 握手后 master 即可改写它们；W 可能延后多个周期到达，
        // 因此地址必须在握手时锁存，否则写会落到改变后 AWADDR 指向的寄存器。
        ch_reg<ch_uint<32>> addr_reg(0_d);
        ch_reg<ch_uint<32>> ar_addr_reg(0_d);

        // ---------------- 写通道：AW 与 W 相位分离 ----------------
        // 写地址握手：busy 期间不接受新的 AW
        auto aw_handshake = select(io().awvalid, select(busy, ch_bool(false), ch_bool(true)), ch_bool(false));
        io().awready = aw_handshake;

        // 写地址解码 —— 基于**锁存**的地址
        auto addr_bits = addr_reg >> ch_uint<32>(2_d);
        addr_bits = addr_bits & ch_uint<32>(3_d);

        // 写数据握手：AW 完成后才接受 W
        auto w_handshake = select(io().wvalid, aw_done, ch_bool(false));
        io().wready = w_handshake;

        // 写响应：WDATA 被接受之后才拉高，并保持到 BREADY
        io().bvalid = select(b_valid, ch_bool(true), ch_bool(false));

        // 写寄存器
        auto sel_load = (addr_bits == ch_uint<32>(0_d));
        auto sel_ctrl = (addr_bits == ch_uint<32>(2_d));

        // 未映射地址（0x04 COUNT / 0x0C STATUS 为只读）→ DECERR
        auto write_mapped = sel_load | sel_ctrl;
        io().bresp = select(write_mapped, ch_uint<2>(0_d), ch_uint<2>(3_d));

        auto we_load = select(w_handshake, sel_load, ch_bool(false));
        auto we_ctrl = select(w_handshake, sel_ctrl, ch_bool(false));
        load_reg->next = select(!io().rst_n, ch_uint<32>(0_d),
                                select(we_load, io().wdata, load_reg));
        ctrl_reg->next = select(!io().rst_n, ch_uint<32>(0_d),
                                select(we_ctrl, io().wdata, ctrl_reg));

        // ---------------- 读通道 ----------------
        auto ar_handshake = select(io().arvalid, select(busy, ch_bool(false), ch_bool(true)), ch_bool(false));
        io().arready = ar_handshake;

        // 读地址解码 —— 基于**锁存**的地址
        auto ar_addr_bits = ar_addr_reg >> ch_uint<32>(2_d);
        ar_addr_bits = ar_addr_bits & ch_uint<32>(3_d);

        // 读响应：保持到 RREADY
        io().rvalid = select(r_valid, ch_bool(true), ch_bool(false));
        io().rresp = ch_uint<2>(0_d);

        // 读寄存器
        auto read_sel_load = (ar_addr_bits == ch_uint<32>(0_d));
        auto read_sel_count = (ar_addr_bits == ch_uint<32>(1_d));
        auto read_sel_ctrl = (ar_addr_bits == ch_uint<32>(2_d));
        auto read_sel_status = (ar_addr_bits == ch_uint<32>(3_d));

        ch_uint<32> read_val(0_d);
        read_val = select(read_sel_load, load_reg, read_val);
        read_val = select(read_sel_count, count_reg, read_val);
        read_val = select(read_sel_ctrl, ctrl_reg, read_val);
        read_val = select(read_sel_status, status_reg, read_val);

        io().rdata = read_val;

        // ---------------- 计数器 ----------------
        auto enable = (ctrl_reg & ch_uint<32>(1_d)) != ch_uint<32>(0_d);
        auto timeout = (count_reg == ch_uint<32>(0_d));

        count_reg->next = select(!io().rst_n, ch_uint<32>(0_d),
                                 select(enable,
                                        select(timeout, load_reg, count_reg - ch_uint<32>(1_d)),
                                        count_reg));

        status_reg->next = select(!io().rst_n, ch_uint<32>(0_d),
                                  select(timeout, status_reg | ch_uint<32>(1_d), status_reg));

        io().irq = (status_reg & ch_uint<32>(1_d)) != ch_uint<32>(0_d);

        // ---------------- 状态更新 ----------------
        // 事务真正完成的条件（不是裸 bready/rready）
        auto write_done = select(b_valid, io().bready, ch_bool(false));
        auto read_done = select(r_valid, io().rready, ch_bool(false));

        // aw_done 仅在 B 握手完成时清零
        // （若在裸 bready 上清零，master 常态拉高 BREADY 时 W 永远不会被接受）
        aw_done->next = select(!io().rst_n, ch_bool(false),
                               select(aw_handshake, ch_bool(true),
                                      select(write_done, ch_bool(false), aw_done)));
        b_valid->next = select(!io().rst_n, ch_bool(false),
                               select(w_handshake, ch_bool(true),
                                      select(io().bready, ch_bool(false), b_valid)));
        r_valid->next = select(!io().rst_n, ch_bool(false),
                               select(ar_handshake, ch_bool(true),
                                      select(io().rready, ch_bool(false), r_valid)));
        // 地址锁存（AW / AR 握手时采样）
        addr_reg->next = select(!io().rst_n, ch_uint<32>(0_d),
                                select(aw_handshake, io().awaddr, addr_reg));
        ar_addr_reg->next = select(!io().rst_n, ch_uint<32>(0_d),
                                   select(ar_handshake, io().araddr, ar_addr_reg));
        busy->next = select(!io().rst_n, ch_bool(false),
                            select(aw_handshake, ch_bool(true),
                                   select(ar_handshake, ch_bool(true),
                                          select(write_done, ch_bool(false),
                                                 select(read_done, ch_bool(false), busy)))));
    }
};

} // namespace axi4
