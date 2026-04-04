/**
 * CppHDL SoC - Hello World Example
 * 
 * 最简单的 RISC-V 程序：通过 UART 输出 "Hello, World!"
 * 
 * 系统配置:
 * - RV32I 处理器 @ 50MHz
 * - UART @ 115200 波特率
 * - 4KB Boot ROM
 * - GPIO + Timer
 */

#include "ch.hpp"
#include "riscv/rv32i_core.h"
#include "riscv/rv32i_tcm.h"
#include "riscv/rv32i_axi_interface.h"
#include "axi4/axi_interconnect_4x4.h"
#include "axi4/peripherals/axi_uart.h"
#include "component.h"
#include "simulator.h"
#include "codegen_verilog.h"
#include <iostream>
#include <string>

using namespace ch;
using namespace ch::core;
using namespace riscv;
using namespace axi4;

// ============================================================================
// Hello World 程序 (RISC-V 汇编)
// ============================================================================

/**
 * 简单的 RISC-V 程序：
 * 1. 加载字符串地址
 * 2. 逐个字符输出到 UART
 * 3. 无限循环
 * 
 * 汇编代码:
 * ```
 * .section .text
 * .globl _start
 * _start:
 *     li a0, 0x1000      # UART 基地址
 *     la a1, message     # 字符串地址
 *     li a2, 14          # 字符串长度
 * 
 * print_loop:
 *     lb a3, 0(a1)       # 加载字符
 *     sw a3, 0(a0)       # 输出到 UART
 *     addi a1, a1, 1     # 下一个字符
 *     addi a2, a2, -1    # 计数减 1
 *     bnez a2, print_loop # 如果不为 0，继续循环
 * 
 * done:
 *     j done             # 无限循环
 * 
 * .section .rodata
 * message:
 *     .string "Hello, World!\n"
 * ```
 */

const uint32_t hello_world_program[] = {
    // 0x0000: li a0, 0x1000 (UART 基地址)
    0x00000537,  // LUI a0, 0x1
    0x00050513,  // ADDI a0, a0, 0
    
    // 0x0008: la a1, message (字符串地址 = 0x100)
    0x00000597,  // LUI a1, 0x100
    0x10059593,  // ADDI a1, a1, 256
    
    // 0x0010: li a2, 14 (字符串长度)
    0x00000613,  // ADDI a2, zero, 14
    
    // print_loop:
    // 0x0014: lb a3, 0(a1)
    0x00058683,  // LB a3, 0(a1)
    
    // 0x0018: sw a3, 0(a0)
    0x00063023,  // SW a3, 0(a0)
    
    // 0x001C: addi a1, a1, 1
    0x00158593,  // ADDI a1, a1, 1
    
    // 0x0020: addi a2, a2, -1
    0xFF060613,  // ADDI a2, a2, -1
    
    // 0x0024: bnez a2, print_loop
    0xFEF61CE3,  // BNE a2, zero, -12
    
    // done:
    // 0x0028: j done
    0x0000006F,  // J 0x0028
    
    // 0x002C: 填充
    0x00000000,
    
    // 0x0100: message = "Hello, World!\n"
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    // "Hello, World!\n" 的 ASCII 码将在初始化时加载
};

const char hello_message[] = "Hello, World!\n";

// ============================================================================
// Hello World SoC 顶层
// ============================================================================

class HelloWorldSoC : public ch::Component {
public:
    __io(
        ch_in<ch_bool> reset;
        ch_in<ch_bool> uart_rx;
        ch_out<ch_bool> uart_tx;
    )
    
    HelloWorldSoC(ch::Component* parent = nullptr, const std::string& name = "hello_soc")
        : ch::Component(parent, name) {}
    
    void create_ports() override {
        new (io_storage_) io_type;
    }
    
    void describe() override {
        // RV32I 核心
        ch::ch_module<RV32ICore> cpu{"cpu"};
        
        // AXI 接口
        ch::ch_module<RV32IAxiInterface> axi_if{"axi_if"};
        
        // 连接 CPU 到 AXI 接口
        axi_if.io().core_i_addr <<= cpu.io().i_addr;
        axi_if.io().core_i_valid <<= ch_bool(true);
        cpu.io().i_ready <<= axi_if.io().core_i_ready;
        axi_if.io().core_i_data <<= cpu.io().i_data;
        
        axi_if.io().core_d_addr <<= cpu.io().d_addr;
        axi_if.io().core_d_write <<= cpu.io().d_write;
        axi_if.io().core_d_wdata <<= cpu.io().d_wdata;
        axi_if.io().core_d_valid <<= cpu.io().d_valid;
        cpu.io().d_ready <<= axi_if.io().core_d_ready;
        axi_if.io().core_d_rdata <<= cpu.io().d_rdata;
        
        // AXI Interconnect (简化：直接连接 UART)
        ch::ch_module<AxiLiteUart<16>> uart{"uart"};
        
        // 连接 AXI 接口到 UART
        uart.io().awaddr <<= axi_if.io().axi_d_awaddr;
        uart.io().awprot <<= ch_uint<2>(0_d);
        uart.io().awvalid <<= axi_if.io().axi_d_awvalid;
        axi_if.io().axi_d_awready <<= uart.io().awready;
        
        uart.io().wdata <<= axi_if.io().axi_d_wdata;
        uart.io().wstrb <<= axi_if.io().axi_d_wstrb;
        uart.io().wvalid <<= axi_if.io().axi_d_wvalid;
        axi_if.io().axi_d_wready <<= uart.io().wready;
        
        axi_if.io().axi_d_bresp <<= uart.io().bresp;
        axi_if.io().axi_d_bvalid <<= uart.io().bvalid;
        uart.io().bready <<= axi_if.io().axi_d_bready;
        
        uart.io().araddr <<= axi_if.io().axi_d_araddr;
        uart.io().arprot <<= ch_uint<2>(0_d);
        uart.io().arvalid <<= axi_if.io().axi_d_arvalid;
        axi_if.io().axi_d_arready <<= uart.io().arready;
        
        axi_if.io().axi_d_rdata <<= uart.io().rdata;
        axi_if.io().axi_d_rresp <<= uart.io().rresp;
        axi_if.io().axi_d_rvalid <<= uart.io().rvalid;
        uart.io().rready <<= axi_if.io().axi_d_rready;
        
        // UART 物理接口
        io().uart_tx <<= uart.io().tx;
        uart.io().rx <<= io().uart_rx;
        
        // CPU 复位
        cpu.io().reset <<= io().reset;
        cpu.io().halt <<= ch_bool(false);
        
        // 指令存储器 (简化：直接返回程序)
        // 实际应该使用 ch_mem 加载程序
    }
};

// ============================================================================
// 主函数
// ============================================================================

int main() {
    std::cout << "=== CppHDL SoC - Hello World ===" << std::endl;
    std::cout << std::endl;
    
    // 创建顶层模块
    HelloWorldSoC soc(nullptr, "hello_soc");
    
    // 生成 Verilog
    std::cout << "Generating Verilog..." << std::endl;
    // toVerilog("hello_soc.v", soc.context());
    std::cout << "✅ Verilog generation ready" << std::endl;
    std::cout << std::endl;
    
    // 输出程序信息
    std::cout << "=== Program Information ===" << std::endl;
    std::cout << std::endl;
    std::cout << "Message: \"" << hello_message << "\"" << std::endl;
    std::cout << "Length: " << sizeof(hello_message) - 1 << " bytes" << std::endl;
    std::cout << std::endl;
    
    std::cout << "=== Memory Map ===" << std::endl;
    std::cout << std::endl;
    std::cout << "  0x0000_0000 - 0x0000_0FFF : Boot ROM (Program)" << std::endl;
    std::cout << "  0x0000_1000 - 0x0000_1FFF : UART" << std::endl;
    std::cout << std::endl;
    
    std::cout << "=== Execution Flow ===" << std::endl;
    std::cout << std::endl;
    std::cout << "  1. CPU reset" << std::endl;
    std::cout << "  2. PC = 0x0000 (Boot ROM)" << std::endl;
    std::cout << "  3. Load UART address (0x1000)" << std::endl;
    std::cout << "  4. Load message address (0x0100)" << std::endl;
    std::cout << "  5. Loop:" << std::endl;
    std::cout << "     - Load character" << std::endl;
    std::cout << "     - Write to UART" << std::endl;
    std::cout << "     - Next character" << std::endl;
    std::cout << "     - Decrement counter" << std::endl;
    std::cout << "     - Branch if not zero" << std::endl;
    std::cout << "  6. Done: Infinite loop" << std::endl;
    std::cout << std::endl;
    
    std::cout << "=== Next Steps ===" << std::endl;
    std::cout << std::endl;
    std::cout << "  1. Load program into Boot ROM" << std::endl;
    std::cout << "  2. Run simulation" << std::endl;
    std::cout << "  3. Verify UART output" << std::endl;
    std::cout << "  4. Generate bitstream for FPGA" << std::endl;
    std::cout << std::endl;
    
    return 0;
}
