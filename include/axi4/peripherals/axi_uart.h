/**
 * AXI4-Lite UART Controller
 *
 * 将 Phase 1+2 的 UART TX 封装成 AXI4 从设备
 *
 * 寄存器映射:
 * - 0x00: TX_DATA  - 发送数据寄存器
 * - 0x04: RX_DATA  - 接收数据寄存器
 * - 0x08: STATUS   - 状态寄存器 (TX_EMPTY, RX_VALID)
 * - 0x0C: CTRL     - 控制寄存器 (ENABLE, INTERRUPT_EN)
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

template <unsigned PRESCALE_WIDTH = 16>
class AxiLiteUart : public ch::Component {
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

        // UART 物理接口
        ch_out<ch_bool> uart_tx;
        ch_in<ch_bool> uart_rx;
        ch_out<ch_bool> irq;

        // 异步复位 (低电平有效)
        ch_in<ch_bool> rst_n;
    )

    AxiLiteUart(ch::Component* parent = nullptr, const std::string& name = "axi_uart")
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
        ch_reg<ch_uint<32>> tx_data_reg(0_d);
        ch_reg<ch_uint<32>> rx_data_reg(0_d);
        ch_reg<ch_uint<32>> status_reg(0_d);
        ch_reg<ch_uint<32>> ctrl_reg(0_d);

        // UART 状态
        ch_reg<ch_bool> tx_empty(ch_bool(true));
        ch_reg<ch_bool> rx_valid(ch_bool(false));

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
        auto sel_tx_data = (addr_bits == ch_uint<32>(0_d));
        auto sel_ctrl = (addr_bits == ch_uint<32>(3_d));

        // 未映射地址（0x04 RX_DATA / 0x08 STATUS 为只读）→ DECERR
        auto write_mapped = sel_tx_data | sel_ctrl;
        io().bresp = select(write_mapped, ch_uint<2>(0_d), ch_uint<2>(3_d));

        auto we_tx_data = select(w_handshake, sel_tx_data, ch_bool(false));
        auto we_ctrl = select(w_handshake, sel_ctrl, ch_bool(false));
        tx_data_reg->next = select(!io().rst_n, ch_uint<32>(0_d),
                                   select(we_tx_data, io().wdata, tx_data_reg));
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
        auto read_sel_tx_data = (ar_addr_bits == ch_uint<32>(0_d));
        auto read_sel_rx_data = (ar_addr_bits == ch_uint<32>(1_d));
        auto read_sel_status = (ar_addr_bits == ch_uint<32>(2_d));
        auto read_sel_ctrl = (ar_addr_bits == ch_uint<32>(3_d));

        ch_uint<32> read_val(0_d);
        read_val = select(read_sel_tx_data, tx_data_reg, read_val);
        read_val = select(read_sel_rx_data, rx_data_reg, read_val);
        read_val = select(read_sel_status, status_reg, read_val);
        read_val = select(read_sel_ctrl, ctrl_reg, read_val);

        io().rdata = read_val;

        // ---------------- UART 状态 ----------------
        // TX 侧：1 深 holding register（tx_data_reg）。写 TX_DATA → tx_empty=0
        // （"缓冲非空"）；下一拍回到 1（简化模型：写即完成，无波特率/移位引擎）。
        tx_empty->next = select(!io().rst_n, ch_bool(true),
                                select(we_tx_data, ch_bool(false), ch_bool(true)));

        // RX 侧：**无 RX 引擎** —— `uart_rx` 输入未使用、`rx_data_reg` 无回接路径，
        // 因此 rx_valid 恒为 false、`irq` 恒为 0 是**设计结果**而非缺陷。
        // 完整的串行收发引擎（波特率 + 移位 + 起始位检测）见 change
        // `axi4-peripheral-deferred-2026-10`。
        rx_valid->next = select(!io().rst_n, ch_bool(false), rx_valid);

        // status_reg **单一赋值点**：bit0 = TX_EMPTY、bit1 = RX_VALID
        auto status_mask = ch_uint<32>(~static_cast<uint32_t>(3));
        auto tx_empty_bit = select(tx_empty, ch_uint<32>(1_d), ch_uint<32>(0_d));
        auto rx_valid_bit = select(rx_valid, ch_uint<32>(2_d), ch_uint<32>(0_d));
        status_reg->next = select(!io().rst_n, ch_uint<32>(0_d),
                                  (status_reg & status_mask) | tx_empty_bit | rx_valid_bit);

        // UART TX：简化实现 —— 无波特率 / 无移位寄存器，
        // uart_tx 恒为空闲高电平（不产生伪串行波形）
        io().uart_tx = ch_bool(true);
        io().irq = rx_valid;

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
