// examples/axi4/axi_soc_example_top.h
//
// AXI4-Lite SoC 示例的**顶层组件**与**测试主设备**（从
// `examples/axi4/axi_soc_example.cpp` 抽出 · `rtl-verification-single-source` C1）。
//
// ⚠️ **`AxiSocTop` 的 12 个端口 MUST 具名**（ADR-075 E1）⇒ ⇒ 验证源与 TB 生成
//   都按名寻址。⚠️ **`AxiLiteTestMaster` 的 23 个端口本次不具名**（它是**成层后的
//   子 module 端口**，不属产品顶层 ⇒ ⇒ 具名化属另一类产品可见变更 ⇒ ⇒ 不在本 change）。
//
// ⚠️ **住址变更**（2026-11-18 · `example-file-boundary-2026-11` A1/A2）：本头**从
//   `include/axi4/` 搬到 `examples/axi4/`** ⇒ ⇒ 理由 = 它是 **example 专属设计**
//   （库 `include/` 不装 example 设计）⇒ ⇒ 公开面移除登记 =
//   `.rddf/checks/public-api-surface.md` 的「已移除 / 已改名的公开符号」章。
//   ⇒ ⇒ 消费者**只有** `examples/**`（`tests/**` 不反向 include examples —— 护栏 C3）。

#pragma once

#include "axi4/axi4_lite_matrix.h"
#include "axi4/axi4_lite_slave.h"
#include "axi4/peripherals/axi_gpio.h"
#include "axi4/peripherals/axi_uart.h"
#include "ch.hpp"
#include "component.h"

#include <memory>
#include <string>

namespace axi4 {
namespace example {

using namespace ch::core; // ch_uint / ch_bool / ch_reg / select / 0_d 等

// ============================================================================
// 测试主设备 (简化版)
// ============================================================================

class AxiLiteTestMaster : public ch::Component {
public:
    __io(
        ch_in<ch_bool> start;
        ch_in<ch_bool> write;
        ch_in<ch_uint<32>> addr;
        ch_in<ch_uint<32>> wdata;
        // 从设备复位 (低电平有效) —— 透传给 gpio / uart
        ch_in<ch_bool> rst_n;
        ch_out<ch_uint<32>> rdata;
        ch_out<ch_bool> done;

        // AXI 主设备接口 (简化)
        ch_out<ch_uint<32>> awaddr;
        ch_out<ch_bool> awvalid;
        ch_in<ch_bool> awready;

        ch_out<ch_uint<32>> wdata_axi;
        ch_out<ch_bool> wvalid;
        ch_in<ch_bool> wready;

        ch_in<ch_uint<2>> bresp;
        ch_in<ch_bool> bvalid;
        ch_out<ch_bool> bready;

        ch_out<ch_uint<32>> araddr;
        ch_out<ch_bool> arvalid;
        ch_in<ch_bool> arready;

        ch_in<ch_uint<32>> rdata_axi;
        ch_in<ch_uint<2>> rresp;
        ch_in<ch_bool> rvalid;
        ch_out<ch_bool> rready;
    )

    AxiLiteTestMaster(ch::Component *parent = nullptr,
                      const std::string &name = "test_master")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        // 状态：0=idle, 1=write, 2=read
        ch_reg<ch_uint<2>> state(0_d);
        // 各通道的握手完成标志 —— AXI4-Lite 允许 AW 与 W **相隔任意周期**
        // （slave 侧是相位分离的），因此 master 的 AWVALID / WVALID 必须
        // 各自保持到对应握手完成，不能只拉一拍。
        ch_reg<ch_bool> aw_done(ch_bool(false));
        ch_reg<ch_bool> w_done(ch_bool(false));
        ch_reg<ch_bool> ar_done(ch_bool(false));
        // 读数据在 R 握手时捕获（slave 的 RDATA 在后续事务中可能变化）
        ch_reg<ch_uint<32>> rdata_reg(0_d);
        // 完成脉冲（注册一拍）—— 组合单拍脉冲在读事务上必然丢失：
        // 读完成那一拍 state 与 read_done 同拍归位（见下方 state->next），
        // 组合派生的 done 随之消失，而测试台是在 tick() **之后**采样 done。
        // 写事务只是"侥幸"可见（完成拍 state 仍为 1）。
        ch_reg<ch_bool> done_reg(ch_bool(false));

        auto is_idle = (state == ch_uint<2>(0_d));
        auto is_write = (state == ch_uint<2>(1_d));
        auto is_read = (state == ch_uint<2>(2_d));

        auto start_and_write = select(io().start, io().write, ch_bool(false));
        auto not_write = select(io().write, ch_bool(false), ch_bool(true));
        auto start_and_not_write = select(io().start, not_write, ch_bool(false));

        auto write_start = select(start_and_write, is_idle, ch_bool(false));
        auto read_start = select(start_and_not_write, is_idle, ch_bool(false));

        // ---------------- 写通道 ----------------
        // VALID 由**自身状态**驱动 —— 不得用从设备的 ready gate 自己的 valid
        // （AXI 禁止；且 slave 侧是相位分离的，W 会在 AW 之后一拍才被接受）
        auto aw_active = select(write_start, ch_bool(true),
                                 select(is_write, select(aw_done, ch_bool(false), ch_bool(true)), ch_bool(false)));
        auto w_active = select(write_start, ch_bool(true),
                                select(is_write, select(w_done, ch_bool(false), ch_bool(true)), ch_bool(false)));
        io().awvalid = aw_active;
        io().awaddr = io().addr;      // 事务期间保持稳定
        io().wvalid = w_active;
        io().wdata_axi = io().wdata;  // 事务期间保持稳定
        io().bready = is_write;       // 写事务期间恒拉高（B 响应随时可能到）

        auto aw_hs = select(io().awvalid, io().awready, ch_bool(false));
        auto w_hs = select(io().wvalid, io().wready, ch_bool(false));
        auto write_done = select(io().bvalid, is_write, ch_bool(false));  // bvalid && bready

        // ---------------- 读通道 ----------------
        auto ar_active = select(read_start, ch_bool(true),
                                 select(is_read, select(ar_done, ch_bool(false), ch_bool(true)), ch_bool(false)));
        io().arvalid = ar_active;
        io().araddr = io().addr;
        io().rready = is_read;
        auto ar_hs = select(io().arvalid, io().arready, ch_bool(false));
        auto read_done = select(io().rvalid, is_read, ch_bool(false));    // rvalid && rready

        // ---------------- 握手标志 ----------------
        aw_done->next = select(aw_hs, ch_bool(true), select(write_start, ch_bool(false), aw_done));
        w_done->next = select(w_hs, ch_bool(true), select(write_start, ch_bool(false), w_done));
        ar_done->next = select(ar_hs, ch_bool(true), select(read_start, ch_bool(false), ar_done));
        rdata_reg->next = select(read_done, io().rdata_axi, rdata_reg);

        // ---------------- 状态更新 ----------------
        state->next = select(write_start, ch_uint<2>(1_d),
                             select(read_start, ch_uint<2>(2_d),
                                    select(write_done, ch_uint<2>(0_d),
                                           select(read_done, ch_uint<2>(0_d), state))));

        io().rdata = rdata_reg;
        // done 由寄存器打一拍 —— 保证在 tick() 之后仍可被采样到
        done_reg->next = select(write_done, ch_bool(true),
                                select(read_done, ch_bool(true), done_reg));
        io().done = done_reg;
    }
};

// ============================================================================
// 顶层 SoC
// ============================================================================

/// **根组件名**（**单一真相源** · 2026-11-18）—— 发射 factory 与产品自检的
/// owner 基准**共用**此常量 ⇒ ⇒ 消除「三处字面量恰好一致」的巧合。
inline constexpr const char *kSocRootName = "top";

class AxiSocTop : public ch::Component {
public:
    __io(
        ch_in<ch_bool> start{"start"};
        ch_in<ch_bool> write{"write"};
        ch_in<ch_uint<32>> addr{"addr"};
        ch_in<ch_uint<32>> wdata{"wdata"};
        // 从设备复位 (低电平有效) —— 透传给 gpio / uart
        ch_in<ch_bool> rst_n{"rst_n"};
        ch_out<ch_uint<32>> rdata{"rdata"};
        ch_out<ch_bool> done{"done"};

        // GPIO 物理接口
        ch_out<ch_uint<8>> gpio_out{"gpio_out"};
        ch_in<ch_uint<8>> gpio_in{"gpio_in"};

        // UART 物理接口
        ch_out<ch_bool> uart_tx{"uart_tx"};
        ch_in<ch_bool> uart_rx{"uart_rx"};

        // 根自有的 IRQ 输入端口（**零消费** —— DUT 从不读它 · ADR-073 事实⑭）
        ch_in<ch_bool> timer_irq{"timer_irq"};
    )

    AxiSocTop(ch::Component *parent = nullptr, const std::string &name = "axi_soc_top")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        // 测试主设备
        // ⚠️ **成层声明用档 ③**（实例处给模块类型名）—— 4 个 IP 均无类级 CH_HIERARCHY_LAYER
        // ⇒ ⇒ 模块类型名**一次定死**（ADR-073 T1）⇒ ⇒ 改名会改结构判据（J-2 的期望 module 数）
        ch::ch_layer<AxiLiteTestMaster> master{"master", "AxiLiteTestMaster"};

        // AXI4 矩阵
        ch::ch_layer<AxiLiteMatrix2x2<32, 32>> matrix{"matrix", "AxiLiteMatrix2x2<32,32>"};

        // 外设：GPIO (从设备 0)
        ch::ch_layer<AxiLiteGpio<8>> gpio{"gpio", "AxiLiteGpio<8>"};

        // 外设：UART (从设备 1)
        ch::ch_layer<AxiLiteUart<16>> uart{"uart", "AxiLiteUart<16>"};

        // 连接主设备到矩阵
        matrix.io().m0_awaddr <<= master.io().awaddr;
        matrix.io().m0_awprot <<= ch_uint<2>(0_d);
        matrix.io().m0_awvalid <<= master.io().awvalid;
        master.io().awready <<= matrix.io().m0_awready;

        matrix.io().m0_wdata <<= master.io().wdata_axi;
        matrix.io().m0_wstrb <<= ch_uint<4>(0xF_h);
        matrix.io().m0_wvalid <<= master.io().wvalid;
        master.io().wready <<= matrix.io().m0_wready;

        master.io().bresp <<= matrix.io().m0_bresp;
        master.io().bvalid <<= matrix.io().m0_bvalid;
        matrix.io().m0_bready <<= master.io().bready;

        matrix.io().m0_araddr <<= master.io().araddr;
        matrix.io().m0_arprot <<= ch_uint<2>(0_d);
        matrix.io().m0_arvalid <<= master.io().arvalid;
        master.io().arready <<= matrix.io().m0_arready;

        master.io().rdata_axi <<= matrix.io().m0_rdata;
        master.io().rresp <<= matrix.io().m0_rresp;
        master.io().rvalid <<= matrix.io().m0_rvalid;
        matrix.io().m0_rready <<= master.io().rready;

        // 连接矩阵到 GPIO (从设备 0)
        gpio.io().awaddr <<= matrix.io().s0_awaddr;
        gpio.io().awprot <<= matrix.io().s0_awprot;
        gpio.io().awvalid <<= matrix.io().s0_awvalid;
        matrix.io().s0_awready <<= gpio.io().awready;

        gpio.io().wdata <<= matrix.io().s0_wdata;
        gpio.io().wstrb <<= matrix.io().s0_wstrb;
        gpio.io().wvalid <<= matrix.io().s0_wvalid;
        matrix.io().s0_wready <<= gpio.io().wready;

        matrix.io().s0_bresp <<= gpio.io().bresp;
        matrix.io().s0_bvalid <<= gpio.io().bvalid;
        gpio.io().bready <<= matrix.io().s0_bready;

        gpio.io().araddr <<= matrix.io().s0_araddr;
        gpio.io().arprot <<= matrix.io().s0_arprot;
        gpio.io().arvalid <<= matrix.io().s0_arvalid;
        matrix.io().s0_arready <<= gpio.io().arready;

        matrix.io().s0_rdata <<= gpio.io().rdata;
        matrix.io().s0_rresp <<= gpio.io().rresp;
        matrix.io().s0_rvalid <<= gpio.io().rvalid;
        gpio.io().rready <<= matrix.io().s0_rready;

        // GPIO 物理接口
        io().gpio_out <<= gpio.io().gpio_out;
        gpio.io().gpio_in <<= io().gpio_in;

        // 连接矩阵从设备 1 口 (s1_*) 到 UART（AXI4-Lite 从设备）
        // —— 此前 s1_* 零连线 ⇒ uart 不在总线上（不可达）；接通后 uart 可经矩阵访问。
        // 请求侧 11 根（矩阵 ch_out → uart ch_in）
        uart.io().awaddr <<= matrix.io().s1_awaddr;
        uart.io().awprot <<= matrix.io().s1_awprot;
        uart.io().awvalid <<= matrix.io().s1_awvalid;
        uart.io().wdata <<= matrix.io().s1_wdata;
        uart.io().wstrb <<= matrix.io().s1_wstrb;
        uart.io().wvalid <<= matrix.io().s1_wvalid;
        uart.io().bready <<= matrix.io().s1_bready;
        uart.io().araddr <<= matrix.io().s1_araddr;
        uart.io().arprot <<= matrix.io().s1_arprot;
        uart.io().arvalid <<= matrix.io().s1_arvalid;
        uart.io().rready <<= matrix.io().s1_rready;
        // 响应侧 8 根（uart ch_out → 矩阵 ch_in）
        matrix.io().s1_awready <<= uart.io().awready;
        matrix.io().s1_wready <<= uart.io().wready;
        matrix.io().s1_bresp <<= uart.io().bresp;
        matrix.io().s1_bvalid <<= uart.io().bvalid;
        matrix.io().s1_arready <<= uart.io().arready;
        matrix.io().s1_rdata <<= uart.io().rdata;
        matrix.io().s1_rresp <<= uart.io().rresp;
        matrix.io().s1_rvalid <<= uart.io().rvalid;

        // 主设备 1 接口未使用 —— tie-off
        // （惯用法照 `examples/axi4/phase3a_top.h` 的 `describe()`：握手信号接常量）
        matrix.io().m1_awvalid <<= ch_bool(false);
        matrix.io().m1_wvalid <<= ch_bool(false);
        matrix.io().m1_bready <<= ch_bool(false);
        matrix.io().m1_arvalid <<= ch_bool(false);
        matrix.io().m1_rready <<= ch_bool(false);
        // ⚠️ payload 也 MUST tie —— 否则它们仍是**未驱动的后代输入** ⇒ ⇒ 发射 fail-closed
        matrix.io().m1_awaddr <<= ch_uint<32>(0_d);
        matrix.io().m1_awprot <<= ch_uint<2>(0_d);
        matrix.io().m1_wdata <<= ch_uint<32>(0_d);
        matrix.io().m1_wstrb <<= ch_uint<4>(0_d);
        matrix.io().m1_araddr <<= ch_uint<32>(0_d);
        matrix.io().m1_arprot <<= ch_uint<2>(0_d);

        // UART 物理接口
        io().uart_tx <<= uart.io().uart_tx;
        uart.io().uart_rx <<= io().uart_rx;

        // 从设备复位 (低电平有效) —— 由顶层 rst_n 透传
        // （不接则从设备会被永久按住复位：未驱动的 ch_in 读出为 0）
        gpio.io().rst_n <<= io().rst_n;
        uart.io().rst_n <<= io().rst_n;
        // 互连层是 rst_n 链路的**中间节点**：MUST 透传父级复位源
        // （MUST NOT tie 常量 —— 那会永久掩盖复位路径）
        matrix.io().rst_n <<= io().rst_n;
        // 主设备的 rst_n 同样透传（此前声明但零驱动 ⇒ 未驱动后代输入）
        // ⚠️ AxiLiteTestMaster::describe() **不消费** rst_n ⇒ 纯发射前置，无行为影响
        master.io().rst_n <<= io().rst_n;

        // 主设备控制接口
        master.io().start <<= io().start;
        master.io().write <<= io().write;
        master.io().addr <<= io().addr;
        master.io().wdata <<= io().wdata;
        io().rdata <<= master.io().rdata;
        io().done <<= master.io().done;
    }
};

} // namespace example
} // namespace axi4
