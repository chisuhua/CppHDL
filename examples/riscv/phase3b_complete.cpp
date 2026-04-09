/**
 * Phase 3B Complete Example
 * 
 * 集成验证 Phase 3B 所有组件:
 * - RV32I 处理器核心
 * - 指令存储器 (I-TCM)
 * - 数据存储器 (D-TCM)
 * 
 * 系统架构:
 * ```
 *                  ┌─────────────────┐
 *                  │   RV32I Core    │
 *                  │                 │
 *                  │  ┌───────────┐  │
 *                  │  │  Decoder  │  │
 *                  │  ├───────────┤  │
 *                  │  │    ALU    │  │
 *                  │  ├───────────┤  │
 *                  │  │ Reg File  │  │
 *                  │  └───────────┘  │
 *                  └─────────────────┘
 *                         │
 *         ┌───────────────┼───────────────┐
 *         │               │               │
 *         ▼               ▼               ▼
 * ┌──────────────┐ ┌──────────────┐ ┌──────────────┐
 * │   I-TCM      │ │   D-TCM      │ │   AXI4       │
 * │  64KB        │ │  64KB        │ │  Interconnect│
 * │  0x0000_0000 │ │  0x8000_0000 │ │              │
 * └──────────────┘ └──────────────┘ └──────────────┘
 * ```
 * 
 * 测试程序 (示例):
 * ```assembly
 * 0x00000000: addi x1, x0, 10     # x1 = 10
 * 0x00000004: addi x2, x0, 20     # x2 = 20
 * 0x00000008: add  x3, x1, x2     # x3 = x1 + x2 = 30
 * 0x0000000C: sw   x3, 0(x0)      # 存储到内存 0x80000000
 * 0x00000010: lw   x4, 0(x0)      # 从内存加载
 * 0x00000014: jal  x1, loop       # 跳转
 * loop:
 * 0x00000018: addi x0, x0, 0      # nop
 * ```
 */

#include "ch.hpp"
#include "../riscv-mini/src/rv32i_core.h"
#include "../riscv-mini/src/i_tcm.h"
#include "../riscv-mini/src/d_tcm.h"
#include "component.h"
#include "simulator.h"
#include "codegen_verilog.h"
#include <iostream>

using namespace ch;
using namespace ch::core;
using namespace riscv;

// ============================================================================
// Phase 3B 顶层系统集成
// ============================================================================

class Phase3BTop : public ch::Component {
public:
    __io(
        // 测试控制
        ch_in<ch_bool>      start;
        ch_in<ch_bool>      step;
        ch_out<ch_bool>     done;
        ch_out<ch_uint<32>> exit_code;
        
        // 调试输出
        ch_out<ch_uint<32>> pc_out;
        ch_out<ch_uint<32>> reg_x1;
        ch_out<ch_uint<32>> reg_x3;
    )
    
    Phase3BTop(ch::Component* parent = nullptr, const std::string& name = "phase3b_top")
        : ch::Component(parent, name) {}
    
    void create_ports() override {
        new (io_storage_) io_type;
    }
    
    void describe() override {
        // ========================================================================
        // 时钟与复位
        // ========================================================================
        ch_var<ch_bool> clk{"clk", false};
        ch_var<ch_bool> rst{"rst", true};
        
        // 简单的时钟生成器
        CH_PROCESS() {
            clk = !clk;
        }
        
        // 复位序列 (3 个周期后释放)
        ch_var<ch_uint<2>> rst_counter{"rst_counter", 0};
        CH_PROCESS(clk) {
            if (rst_counter < 3) {
                rst_counter = rst_counter + 1;
                rst = true;
            } else {
                rst = false;
            }
        }
        
        // ========================================================================
        // RV32I 处理器核心
        // ========================================================================
        ch::ch_module<Rv32iCore> cpu{"cpu"};
        cpu.io().clk = clk;
        cpu.io().rst = rst;
        
        // ========================================================================
        // 指令存储器 (I-TCM)
        // ========================================================================
        ch::ch_module<ITcm<64, 0x00000000>> itcm{"itcm"};
        itcm.io().clk = clk;
        itcm.io().rst = rst;
        itcm.io().debug_enable = false;
        
        // ========================================================================
        // 数据存储器 (D-TCM)
        // ========================================================================
        ch::ch_module<DTcm<64, 0x80000000>> dtcm{"dtcm"};
        dtcm.io().clk = clk;
        dtcm.io().rst = rst;
        dtcm.io().debug_enable = false;
        dtcm.io().debug_write = false;
        
        // ========================================================================
        // AXI4 互连逻辑
        // ========================================================================
        // I-TCM 连接 (指令_fetch)
        itcm.io().axi_araddr = cpu.io().ifetch_addr;
        itcm.io().axi_arid = 0;
        itcm.io().axi_arlen = 0;
        itcm.io().axi_arsize = 2;  // 4 bytes
        itcm.io().axi_arburst = 1;  // INCR
        cpu.io().ifetch_data = itcm.io().axi_rdata;
        cpu.io().ifetch_ready = itcm.io().axi_rvalid;
        itcm.io().axi_rready = true;  // 始终准备接收
        
        // D-TCM 连接 (数据加载/存储)
        dtcm.io().axi_araddr = cpu.io().dmem_addr;
        dtcm.io().axi_awaddr = cpu.io().dmem_addr;
        dtcm.io().axi_wdata = cpu.io().dmem_wdata;
        dtcm.io().axi_wstrb = cpu.io().dmem_strbe;
        dtcm.io().axi_wlast = true;
        
        // 读数据选择器 (简化：直接连接 DTCM)
        cpu.io().dmem_rdata = dtcm.io().axi_rdata;
        cpu.io().dmem_ready = dtcm.io().axi_rvalid | dtcm.io().axi_wready;
        
        // DTCM 控制
        dtcm.io().axi_rready = true;
        dtcm.io().axi_bready = true;
        
        // ========================================================================
        // 测试程序加载 (硬编码示例程序)
        // ========================================================================
        // 在实际使用中，这里应该从文件加载或使用初始化文件
        // 示例程序:
        //   addi x1, x0, 10
        //   addi x2, x0, 20
        //   add  x3, x1, x2
        //   sw   x3, 0(x0)
        //   lw   x4, 0(x0)
        //   jal  x1, loop
        // loop: nop
        
        // I-TCM 初始化 (通过调试端口)
        ch_var<ch_bool> init_done{"init_done", false};
        ch_var<ch_uint<6>> init_addr{"init_addr", 0};
        
        CH_PROCESS(clk, rst) {
            if (rst()) {
                init_done = false;
                init_addr = 0;
                itcm->debug_enable = true;
            } else if (!init_done) {
                // 加载测试程序到 I-TCM
                CH_SWITCH(init_addr) {
                    CH_CASE(0) {
                        // 0x00000000: addi x1, x0, 10
                        // 编码：000000000001_00000_000_00001_0010011
                        itcm.io().debug_addr = 0;
                        itcm.io().debug_wdata = 0x00000083 | (10 << 20);
                        itcm.io().debug_strb = 0b1111;
                        itcm.io().debug_write = true;
                    }
                    CH_CASE(1) {
                        // 0x00000004: addi x2, x0, 20
                        itcm.io().debug_addr = 1;
                        itcm.io().debug_wdata = 0x00000083 | (20 << 20) | (2 << 7);
                        itcm.io().debug_strb = 0b1111;
                        itcm.io().debug_write = true;
                    }
                    CH_CASE(2) {
                        // 0x00000008: add x3, x1, x2
                        // 编码：0000000_00010_00001_000_00011_0110011
                        itcm.io().debug_addr = 2;
                        itcm.io().debug_wdata = 0x00000033 | (1 << 15) | (2 << 20) | (3 << 7);
                        itcm.io().debug_strb = 0b1111;
                        itcm.io().debug_write = true;
                    }
                    CH_CASE(3) {
                        // 0x0000000C: sw x3, 0(x0)
                        // 编码：0000000_00011_00000_010_00000_0100011
                        itcm.io().debug_addr = 3;
                        itcm.io().debug_wdata = 0x00000023 | (3 << 20);
                        itcm.io().debug_strb = 0b1111;
                        itcm.io().debug_write = true;
                    }
                    CH_CASE(4) {
                        // 0x00000010: lw x4, 0(x0)
                        itcm.io().debug_addr = 4;
                        itcm.io().debug_wdata = 0x00000003 | (4 << 7);
                        itcm.io().debug_strb = 0b1111;
                        itcm.io().debug_write = true;
                    }
                    CH_CASE(5) {
                        // 0x00000014: jal x1, 0x00000018 (loop)
                        // 编码：000000000001_00000_000_00001_1101111
                        itcm.io().debug_addr = 5;
                        itcm.io().debug_wdata = 0x0000006F | (1 << 7);
                        itcm.io().debug_strb = 0b1111;
                        itcm.io().debug_write = true;
                    }
                    CH_CASE(6) {
                        // 0x00000018: nop (addi x0, x0, 0)
                        itcm.io().debug_addr = 6;
                        itcm.io().debug_wdata = 0x00000013;
                        itcm.io().debug_strb = 0b1111;
                        itcm.io().debug_write = true;
                    }
                    CH_DEFAULT {
                        init_done = true;
                        itcm.io().debug_enable = false;
                        itcm.io().debug_write = false;
                    }
                }
                
                if (itcm.io().debug_enable) {
                    init_addr = init_addr + 1;
                }
            }
        }
        
        // ========================================================================
        // 状态监控与输出
        // ========================================================================
        done() = start() && (cpu->pc_out > 0x20);  // 简单完成检测
        exit_code() = 0;  // 可扩展为从内存读取结果
        
        // 调试输出
        // pc_out() = cpu->pc;  // 需要添加 PC 输出端口
        // reg_x1() = cpu->regfile_read_data_a;
        // reg_x3() = cpu->regfile_read_data_b;
    }
};

// ============================================================================
// 主函数：仿真入口
// ============================================================================

int main() {
    std::cout << "=== CppHDL Phase 3B: RV32I Core Test ===" << std::endl;
    
    // 创建顶层模块
    Phase3BTop top{nullptr, "phase3b_top"};
    
    // 生成 Verilog
    ch::to_verilog(top, "phase3b_output");
    
    std::cout << "Verilog generation complete!" << std::endl;
    std::cout << "Output directory: phase3b_output/" << std::endl;
    
    return 0;
}
