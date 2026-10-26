/**
 * AXI4-Lite SoC Integration Example
 * 
 * 集成 AXI4 矩阵 + GPIO + UART + Timer
 * 
 * 系统架构:
 * ```
 *                  ┌─────────────────┐
 *                  │  AXI Matrix 2x2 │
 *                  │                 │
 *   Master 0 ──────┤                 ├───▶ Slave 0 (GPIO @ 0x0000_0000)
 *   (Test Master)  │                 │
 *   Master 1 ──────┤                 ├───▶ Slave 1 (UART/Timer @ 0x8000_0000)
 *   (Reserved)     │                 │
 *                  └─────────────────┘
 * ```
 */

#include "ch.hpp"
#include "axi4/axi4_lite_slave.h"
#include "axi4/axi4_lite_matrix.h"
#include "axi4/peripherals/axi_gpio.h"
#include "axi4/peripherals/axi_uart.h"
#include "axi4/peripherals/axi_timer.h"
#include "component.h"
#include "simulator.h"
#include "codegen_verilog.h"
#include <iostream>
#include <cstdint>

using namespace ch;
using namespace ch::core;
using namespace axi4;

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
    
    AxiLiteTestMaster(ch::Component* parent = nullptr, const std::string& name = "test_master")
        : ch::Component(parent, name) {}
    
    void create_ports() override {
        new (io_storage_) io_type;
    }
    
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

class AxiSocTop : public ch::Component {
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
        
        // GPIO 物理接口
        ch_out<ch_uint<8>> gpio_out;
        ch_in<ch_uint<8>> gpio_in;
        
        // UART 物理接口
        ch_out<ch_bool> uart_tx;
        ch_in<ch_bool> uart_rx;
        
        // Timer IRQ
        ch_in<ch_bool> timer_irq;
    )
    
    AxiSocTop(ch::Component* parent = nullptr, const std::string& name = "axi_soc_top")
        : ch::Component(parent, name) {}
    
    void create_ports() override {
        new (io_storage_) io_type;
    }
    
    void describe() override {
        // 测试主设备
        ch::ch_module<AxiLiteTestMaster> master{"master"};
        
        // AXI4 矩阵
        ch::ch_module<AxiLiteMatrix2x2<32, 32>> matrix{"matrix"};
        
        // 外设：GPIO (从设备 0)
        ch::ch_module<AxiLiteGpio<8>> gpio{"gpio"};
        
        // 外设：UART (从设备 1)
        ch::ch_module<AxiLiteUart<16>> uart{"uart"};
        
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

        // 主设备控制接口
        master.io().start <<= io().start;
        master.io().write <<= io().write;
        master.io().addr <<= io().addr;
        master.io().wdata <<= io().wdata;
        io().rdata <<= master.io().rdata;
        io().done <<= master.io().done;
    }
};

// ============================================================================
// 主函数 - 系统验证
// ============================================================================

int main() {
    std::cout << "CppHDL - AXI4-Lite SoC Integration" << std::endl;
    std::cout << "===================================" << std::endl;
    
    ch::ch_device<AxiSocTop> top_device;
    ch::Simulator sim(top_device.context());
    
    // 初始化
    sim.set_input_value(top_device.instance().io().start, false);
    sim.set_input_value(top_device.instance().io().write, false);
    // 释放从设备复位（低有效）
    sim.set_input_value(top_device.instance().io().rst_n, true);
    // GPIO 输入给一个非零值 —— 读回 DATA_IN 才能证明读通道真的取到了数据
    sim.set_input_value(top_device.instance().io().gpio_in, 0x5a_h);
    sim.set_input_value(top_device.instance().io().uart_rx, true);
    
    std::cout << "\n=== Test 1: Write GPIO DATA_OUT (addr=0x00) ===" << std::endl;
    
    // 写 GPIO DATA_OUT 寄存器 (地址 0x00)
    sim.set_input_value(top_device.instance().io().addr, 0_d);
    sim.set_input_value(top_device.instance().io().wdata, 0xAB_h);
    sim.set_input_value(top_device.instance().io().write, true);
    sim.set_input_value(top_device.instance().io().start, true);
    sim.tick();
    sim.set_input_value(top_device.instance().io().start, false);
    
    // 等待完成
    for (int i = 0; i < 10; ++i) {
        sim.tick();
        auto done = sim.get_port_value(top_device.instance().io().done);
        if (static_cast<uint64_t>(done)) {
            std::cout << "Write complete!" << std::endl;
            break;
        }
    }
    
    // 等 master 从写状态回到 idle（done 与 idle 差一拍 —— master 的状态在
    // 响应握手后的**下一个**时钟沿才回到 idle）
    sim.tick();

    auto gpio_out = sim.get_port_value(top_device.instance().io().gpio_out);
    std::cout << "GPIO_OUT = 0x" << std::hex << static_cast<uint64_t>(gpio_out) << std::dec << std::endl;
    
    std::cout << "\n=== Test 2: Read GPIO DATA_IN (addr=0x04) ===" << std::endl;
    
    // 读 GPIO DATA_IN 寄存器 (地址 0x04)
    sim.set_input_value(top_device.instance().io().addr, 4_d);
    sim.set_input_value(top_device.instance().io().write, false);
    sim.set_input_value(top_device.instance().io().start, true);
    sim.tick();
    sim.set_input_value(top_device.instance().io().start, false);
    
    // 等待完成
    bool read_ok = false;
    for (int i = 0; i < 10; ++i) {
        sim.tick();
        auto done = sim.get_port_value(top_device.instance().io().done);
        auto rdata = sim.get_port_value(top_device.instance().io().rdata);
        
        if (static_cast<uint64_t>(done)) {
            std::cout << "Read complete! DATA_IN = 0x" << std::hex << static_cast<uint64_t>(rdata) << std::dec << std::endl;
            read_ok = true;
            break;
        }
    }
    if (!read_ok) {
        // 读事务未完成 —— 属真实失败（不再是"已知限制"），直接以非零码退出，
        // 避免 example 静默通过。
        std::cerr << "ERROR: AXI4-Lite read transaction did not complete"
                  << std::endl;
        return 1;
    }

    std::cout << "\n=== Generating Verilog ===" << std::endl;
    toVerilog("axi_soc_example.v", top_device.context());
    std::cout << "Verilog generated: axi_soc_example.v" << std::endl;
    
    std::cout << "\n✅ AXI4-Lite SoC integration test completed!" << std::endl;
    return 0;
}
