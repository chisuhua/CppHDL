/**
 * AXI4-Lite UART Controller
 *
 * 带**真实串行收发引擎**的 AXI4-Lite 从设备（见 openspec change
 * `axi4-peripheral-deferred-2026-10` D2）。
 *
 * 寄存器映射:
 * - 0x00: TX_DATA  - 发送数据寄存器（写入即启动一帧发送；**仅低 8 bit 有效**）
 * - 0x04: RX_DATA  - 接收数据寄存器（只读；读走即清 `rx_valid`；
 *                    **仅低 8 bit 有效**，高字节恒 0）
 * - 0x08: STATUS   - 状态寄存器
 *   - bit0: TX_EMPTY        - 发送移位寄存器为空
 *   - bit1: RX_VALID        - 收到完整帧
 *   - bit2: RX_FRAME_ERROR  - 停止位错误（仅由 `rst_n` 清除）
 * - 0x0C: CTRL     - 控制寄存器（**当前保留**，不参与门控）
 *
 * 串行时序（8N1，无校验）：
 * - 帧格式：1 起始位(0) + 8 数据位(LSB 先出) + 1 停止位(1)
 * - **bit 周期 = `UART_BIT_PERIOD` 个时钟周期**（波特率 = 时钟频率 / bit 周期）
 * - TX：写 `TX_DATA` 启动发送（发送 `TX_DATA[7:0]`，高字节被忽略）；
 *   `uart_tx` 空闲为高电平
 * - RX：检测 `uart_rx` 下降沿（起始位），等待 1.5 bit 后按 bit 周期采样 8 个数据位，
 *   再校验停止位；停止位为高 → 写入 `RX_DATA[7:0]` 并置 `rx_valid`，
 *   停止位为低 → 置 `RX_FRAME_ERROR`
 * - `uart_rx` 空闲电平为**高**（低电平被视作起始位）
 *
 * 通道形态（AXI4-Lite 规范形态，见 openspec change
 * fix-axi4-peripheral-rst-n-port-2026-10）：
 * - AW 与 W 相位分离（W 可在 AW 之后的任意周期被接受）
 * - BVALID 在 WDATA 被接受之后拉高，并保持到 BREADY
 * - RVALID 保持到 RREADY
 * - 全部状态寄存器带 rst_n（低电平有效）复位
 */

#pragma once

#include "ch.hpp"
#include "component.h"
#include "axi4/byte_lane.h"

namespace axi4 {

using namespace ch::core;

/**
 * @tparam UART_BIT_PERIOD 一个 bit 占用的时钟周期数（bit 周期）。
 *        波特率 = 系统时钟频率 / UART_BIT_PERIOD。
 */
template <unsigned UART_BIT_PERIOD = 16>
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
        ch_reg<ch_bool> rx_valid(ch_bool(false));
        ch_reg<ch_bool> rx_frame_err(ch_bool(false));  // 停止位错误（仅 rst_n 清除）

        // ---------------- 串行引擎（TX） ----------------
        // bit 周期 = UART_BIT_PERIOD 个时钟周期
        constexpr unsigned BIT_PERIOD = (UART_BIT_PERIOD < 2) ? 2 : UART_BIT_PERIOD;
        ch_reg<ch_uint<8>> tx_sr(0_d);        // 数据移位寄存器（LSB 先出）
        ch_reg<ch_uint<4>> tx_phase(0_d);     // 0=起始位 1..8=数据位 9=停止位
        ch_reg<ch_uint<16>> tx_div(0_d);       // bit 周期分频计数
        ch_reg<ch_bool> tx_busy(ch_bool(false));

        // ---------------- 串行引擎（RX） ----------------
        ch_reg<ch_uint<4>> rx_state(0_d);     // 0=idle 1=起始位 2=数据位 3=停止位
        ch_reg<ch_uint<16>> rx_div(0_d);       // bit 周期分频计数
        ch_reg<ch_uint<4>> rx_bits(0_d);       // 已采样的数据位数
        ch_reg<ch_uint<8>> rx_sr(0_d);         // 接收移位寄存器

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

        // 字节写：WSTRB 逐字节展开（语义见 include/axi4/AGENTS.md 字节道语义）
        const auto wstrb_mask = axi4::detail::byte_lane_mask(io().wstrb);
        tx_data_reg->next = select(!io().rst_n, ch_uint<32>(0_d),
                                   select(we_tx_data,
                                          axi4::detail::byte_lane_apply<32>(tx_data_reg, io().wdata, wstrb_mask),
                                          tx_data_reg));
        ctrl_reg->next = select(!io().rst_n, ch_uint<32>(0_d),
                                select(we_ctrl,
                                       axi4::detail::byte_lane_apply<32>(ctrl_reg, io().wdata, wstrb_mask),
                                       ctrl_reg));

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

        // ---------------- 串行 TX 引擎 ----------------
        // 帧格式：起始位(0) + 8 数据位(LSB 先出) + 停止位(1)
        // bit 周期 = BIT_PERIOD 个时钟周期
        //
        // 发送启动：写 TX_DATA 且当前不在发送中
        auto tx_start = select(we_tx_data,
                               select(tx_busy, ch_bool(false), ch_bool(true)),
                               ch_bool(false));
        // bit 周期到点（仅在发送中计数）
        auto tx_tick = (tx_div == ch_uint<16>(BIT_PERIOD - 1));
        auto tx_advance = select(tx_busy, tx_tick, ch_bool(false));
        // 数据位阶段（phase 1..8）
        auto tx_in_data = select(tx_phase >= ch_uint<4>(1_d),
                                 select(tx_phase <= ch_uint<4>(8_d),
                                        ch_bool(true), ch_bool(false)),
                                 ch_bool(false));

        // 输出位：起始位=0，数据位=tx_sr[0]，停止位=1
        auto tx_data_bit = bits<0, 0>(tx_sr);
        auto tx_out_bit = select(tx_phase == ch_uint<4>(0_d), ch_bool(false),
                                 select(tx_in_data,
                                        select(tx_data_bit != ch_uint<1>(0_d),
                                               ch_bool(true), ch_bool(false)),
                                        ch_bool(true)));
        // 空闲（未发送）时 uart_tx 恒为高电平
        io().uart_tx = select(tx_busy, tx_out_bit, ch_bool(true));

        // 发送移位寄存器：启动时装载 wdata[7:0]，每个数据位结束时右移一位
        tx_sr->next = select(!io().rst_n, ch_uint<8>(0_d),
                             select(tx_start,
                                    ch_uint<8>(io().wdata & ch_uint<32>(255_d)),
                                    select(select(tx_in_data, tx_tick, ch_bool(false)),
                                           tx_sr >> ch_uint<8>(1_d),
                                           tx_sr)));

        // 相位推进：起始位(0) → 数据位(1..8) → 停止位(9) → 结束
        //
        // ⚠️ 判定用**下一个**相位值（而非当前值）：本框架的组合求值是单遍、
        // 无拓扑序的（`src/simulator.cpp::eval_combinational`），寄存器比较的
        // 求值时机受节点创建顺序影响 —— 用当前值判定会让"结束"判定早/晚一拍，
        // 进而丢掉最后一个数据位。用 next 值判定可对该抖动免疫。
        auto tx_phase_next = select(tx_advance,
                                    tx_phase + ch_uint<4>(1_d),
                                    tx_phase);
        auto tx_finish = select(tx_advance,
                                select(tx_phase_next == ch_uint<4>(10_d),
                                       ch_bool(true), ch_bool(false)),
                                ch_bool(false));
        tx_phase->next = select(!io().rst_n, ch_uint<4>(0_d), tx_phase_next);
        // bit 周期分频计数（仅发送中计数，到点归零）
        tx_div->next = select(!io().rst_n, ch_uint<16>(0_d),
                              select(tx_start, ch_uint<16>(0_d),
                                     select(tx_busy,
                                            select(tx_tick, ch_uint<16>(0_d),
                                                   tx_div + ch_uint<16>(1_d)),
                                            tx_div)));
        tx_busy->next = select(!io().rst_n, ch_bool(false),
                               select(tx_start, ch_bool(true),
                                      select(tx_finish, ch_bool(false), tx_busy)));

        // TX_EMPTY：移位寄存器为空 = 未在发送（组合视图，与 tx_busy 严格一致）
        auto tx_empty = select(tx_busy, ch_bool(false), ch_bool(true));

        // ---------------- 串行 RX 引擎 ----------------
        // 状态：0=idle（等起始位） 1=起始位（等 1.5 bit） 2=数据位 3=停止位
        auto rx_is_idle = (rx_state == ch_uint<4>(0_d));
        auto rx_is_start = (rx_state == ch_uint<4>(1_d));
        auto rx_is_data = (rx_state == ch_uint<4>(2_d));
        auto rx_is_stop = (rx_state == ch_uint<4>(3_d));

        // 空闲时检测到低电平 → 起始位
        auto rx_detect = select(rx_is_idle,
                                select(io().uart_rx, ch_bool(false), ch_bool(true)),
                                ch_bool(false));
        // 采样点：起始位阶段在 1.5 bit 后采样首个数据位；数据位阶段每 bit 采一次
        auto rx_start_sample = select(rx_is_start,
                                      select(rx_div == ch_uint<16>(BIT_PERIOD + BIT_PERIOD / 2),
                                             ch_bool(true), ch_bool(false)),
                                      ch_bool(false));
        auto rx_data_sample = select(rx_is_data,
                                     select(rx_div == ch_uint<16>(BIT_PERIOD - 1),
                                            ch_bool(true), ch_bool(false)),
                                     ch_bool(false));
        auto rx_stop_sample = select(rx_is_stop,
                                     select(rx_div == ch_uint<16>(BIT_PERIOD - 1),
                                            ch_bool(true), ch_bool(false)),
                                     ch_bool(false));
        // 采样到的位（1 → 0x80，0 → 0x00），右移并插入最高位 → LSB 先收
        auto rx_sampled = select(io().uart_rx, ch_uint<8>(128_d), ch_uint<8>(0_d));
        auto rx_sr_next = (rx_sr >> ch_uint<8>(1_d)) | rx_sampled;
        // 位计数的**下一个**值（采样时 +1）
        auto rx_bits_next = select(rx_data_sample,
                                   rx_bits + ch_uint<4>(1_d),
                                   rx_bits);
        // 第 8 个数据位采完 → 进入停止位校验
        // ⚠️ 用 next 值判定（见上方 TX 相位推进的说明）：若用当前值判定，
        // 单遍求值的抖动会让状态机在第 8 次采样**之前**就离开 DATA 态，
        // 导致最后一个数据位丢失（收到 7 位的字节）。
        auto rx_data_done = select(rx_is_data,
                                   select(rx_bits_next == ch_uint<4>(8_d),
                                          ch_bool(true), ch_bool(false)),
                                   ch_bool(false));
        // 停止位为高 → 帧有效；为低 → 帧错误
        auto rx_frame_ok = select(rx_stop_sample,
                                  select(io().uart_rx, ch_bool(true), ch_bool(false)),
                                  ch_bool(false));
        auto rx_frame_bad = select(rx_stop_sample,
                                   select(io().uart_rx, ch_bool(false), ch_bool(true)),
                                   ch_bool(false));

        rx_sr->next = select(!io().rst_n, ch_uint<8>(0_d),
                             select(select(rx_start_sample, ch_bool(true),
                                           select(rx_data_sample, ch_bool(true), ch_bool(false))),
                                    rx_sr_next, rx_sr));
        rx_bits->next = select(!io().rst_n, ch_uint<4>(0_d),
                               select(rx_start_sample, ch_uint<4>(1_d), rx_bits_next));

        // RX 状态机
        auto rx_next_state = select(rx_is_idle,
                                    select(rx_detect, ch_uint<4>(1_d), ch_uint<4>(0_d)),
                                    select(rx_is_start,
                                           select(rx_start_sample, ch_uint<4>(2_d), ch_uint<4>(1_d)),
                                           select(rx_is_data,
                                                  select(rx_data_done, ch_uint<4>(3_d), ch_uint<4>(2_d)),
                                                  select(rx_is_stop,
                                                         select(rx_stop_sample, ch_uint<4>(0_d), ch_uint<4>(3_d)),
                                                         ch_uint<4>(0_d)))));
        rx_state->next = select(!io().rst_n, ch_uint<4>(0_d), rx_next_state);
        // 分频计数：起始位 / 数据位 / 停止位阶段计数，到点归零
        rx_div->next = select(!io().rst_n, ch_uint<16>(0_d),
                              select(rx_detect, ch_uint<16>(0_d),
                                     select(rx_is_idle, rx_div,
                                            select(select(rx_start_sample, ch_bool(true),
                                                          select(rx_data_sample, ch_bool(true),
                                                                 select(rx_stop_sample, ch_bool(true),
                                                                        ch_bool(false)))),
                                                   ch_uint<16>(0_d),
                                                   rx_div + ch_uint<16>(1_d)))));

        // 收到完整帧 → 写入 RX_DATA 并置 rx_valid；停止位错误 → 置帧错误标志
        rx_data_reg->next = select(!io().rst_n, ch_uint<32>(0_d),
                                   select(rx_frame_ok, ch_uint<32>(rx_sr), rx_data_reg));
        // 读走 RX_DATA → 清 rx_valid
        // ⚠️ 必须用 **R 握手**（读事务完成）而不是 AR 握手：地址在 AR 握手时才
        // 被锁存，握手当拍 `ar_addr_reg` 还是旧值，解码会指向错误的寄存器。
        auto rx_read_done = select(r_valid, io().rready, ch_bool(false));
        auto rx_read = select(rx_read_done, read_sel_rx_data, ch_bool(false));
        rx_valid->next = select(!io().rst_n, ch_bool(false),
                                select(rx_frame_ok, ch_bool(true),
                                       select(rx_read, ch_bool(false), rx_valid)));
        rx_frame_err->next = select(!io().rst_n, ch_bool(false),
                                    select(rx_frame_bad, ch_bool(true), rx_frame_err));

        // status_reg **单一赋值点**：bit0 = TX_EMPTY、bit1 = RX_VALID、bit2 = RX_FRAME_ERROR
        auto status_mask = ch_uint<32>(~static_cast<uint32_t>(7));
        auto tx_empty_bit = select(tx_empty, ch_uint<32>(1_d), ch_uint<32>(0_d));
        auto rx_valid_bit = select(rx_valid, ch_uint<32>(2_d), ch_uint<32>(0_d));
        auto rx_err_bit = select(rx_frame_err, ch_uint<32>(4_d), ch_uint<32>(0_d));
        status_reg->next = select(!io().rst_n, ch_uint<32>(0_d),
                                  (status_reg & status_mask) | tx_empty_bit | rx_valid_bit
                                      | rx_err_bit);
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
