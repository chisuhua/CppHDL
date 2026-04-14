/**
 * @file rv32i_soc.h
 * @brief RV32I SoC 完整集成 - 整合所有组件构成完整系统
 * 
 * 集成组件:
 * - Rv32iPipeline: 5级流水线 (含 CSR + ExceptionUnit + HazardUnit)
 * - Clint: 定时器/计数器 (Machine Timer Interrupt)
 * - AddressDecoder: MMIO 地址解码器
 * - UartMmio: UART 外设 (TX 输出)
 * - InstrTCM: 指令存储器 (64KB)
 * - DataTCM: 数据存储器 (64KB)
 * 
 * 内存映射:
 * | 区域   | 基地址          | 大小   | 说明         |
 * |--------|-----------------|--------|--------------|
 * | I-TCM  | 0x20000000      | 64KB   | 指令存储     |
 * | D-TCM  | 0x20010000      | 64KB   | 数据存储     |
 * | CLINT  | 0x40000000      | 4KB    | 定时器       |
 * | UART   | 0x40001000      | 4KB    | 串口         |
 * | GPIO   | 0x40002000      | 4KB    | 通用IO       |
 * 
 * 启动配置:
 * - PC 起始地址: 0x20000000 (I-TCM 基地址)
 * - mstatus 复位值: 0x00000080 (MPIE=1)
 * - mtvec 复位值: 0x20000100 (陷阱处理入口)
 * 
 * Tick 序列:
 * 1. Clint mtime++ (计数器递增)
 * 2. Pipeline tick() (流水线执行)
 * 3. AddressDecoder 地址解码
 * 4. 数据路由到正确的外设
 * 
 * 作者: SoC Integration Team
 * 日期: 2026-04-14
 */

#pragma once

#include "ch.hpp"
#include "component.h"
#include "chlib.h"

#include "rv32i_pipeline.h"
#include "rv32i_csr.h"
#include "rv32i_exception.h"
#include "hazard_unit.h"
#include "address_decoder.h"
#include "rv32i_tcm.h"
#include "axi4/peripherals/axi_uart.h"

using namespace ch::core;

namespace riscv {

/**
 * @brief RV32I SoC 顶层集成类
 * 
 * 整合所有 RISC-V 组件构成完整片上系统
 */
class Rv32iSoc : public ch::Component {
public:
    __io(
        // ====================================================================
        // 系统控制
        // ====================================================================
        ch_in<ch_bool>  rst;                // 复位信号
        ch_in<ch_bool>  clk;                // 时钟信号
        
        // ====================================================================
        // UART 输出 (用于 Hello World 等调试输出)
        // ====================================================================
        ch_out<ch_bool>     uart_tx_char_ready;  // UART 发送就绪
        ch_out<ch_uint<8>>  uart_tx_char;        // UART 发送字符
        
        // ====================================================================
        // 调试端口
        // ====================================================================
        ch_out<ch_uint<32>> debug_pc;       // 调试: 当前 PC
        ch_out<ch_bool>     debug_halted;   // 调试: 处理器暂停状态
    )
    
    Rv32iSoc(ch::Component* parent = nullptr, const std::string& name = "rv32i_soc")
        : ch::Component(parent, name) {}
    
    void create_ports() override {
        new (io_storage_) io_type;
    }
    
    void describe() override {
        // ====================================================================
        // 子模块实例化
        // ====================================================================
        
        // 流水线 (含 CSR + ExceptionUnit + HazardUnit)
        ch::ch_module<Rv32iPipeline> pipeline{"pipeline"};
        
        // 地址解码器
        ch::ch_module<AddressDecoder> addr_decoder{"addr_decoder"};
        
        // 指令存储器 (64KB, I-TCM)
        ch::ch_module<InstrTCM<20, 32>> instr_tcm{"instr_tcm"};
        
        // 数据存储器 (64KB, D-TCM)
        ch::ch_module<DataTCM<20, 32>> data_tcm{"data_tcm"};
        
        // CLINT 定时器
        ch::ch_module<ch::chlib::Clint> clint{"clint"};
        
        // UART 外设 (通过 AXI4-Lite)
        ch::ch_module<axi4::AxiLiteUart<16>> uart{"uart"};
        
        // ====================================================================
        // 信号定义 (用于模块间连接)
        // ====================================================================
        
        // 流水线到指令存储器的信号
        ch_var<ch_uint<32>> instr_addr{"instr_addr", 0_d};
        ch_var<ch_uint<32>> instr_data{"instr_data", 0_d};
        ch_var<ch_bool>     instr_ready{"instr_ready", true};
        
        // 流水线到数据存储器的信号
        ch_var<ch_uint<32>> data_addr{"data_addr", 0_d};
        ch_var<ch_uint<32>> data_write_data{"data_write_data", 0_d};
        ch_var<ch_uint<4>>  data_strbe{"data_strbe", 0_d};
        ch_var<ch_bool>     data_write_en{"data_write_en", false};
        ch_var<ch_uint<32>> data_read_data{"data_read_data", 0_d};
        ch_var<ch_bool>     data_ready{"data_ready", true};
        
        // 地址解码器片选信号
        ch_var<ch_bool> i_tcm_cs{"i_tcm_cs", false};
        ch_var<ch_bool> d_tcm_cs{"d_tcm_cs", false};
        ch_var<ch_bool> clint_cs{"clint_cs", false};
        ch_var<ch_bool> uart_cs{"uart_cs", false};
        ch_var<ch_bool> gpio_cs{"gpio_cs", false};
        ch_var<ch_bool> error_cs{"error_cs", false};
        ch_var<ch_uint<32>> local_addr{"local_addr", 0_d};
        
        // CLINT 相关信号
        ch_var<ch_uint<32>> clint_addr{"clint_addr", 0_d};
        ch_var<ch_uint<32>> clint_wdata{"clint_wdata", 0_d};
        ch_var<ch_bool>     clint_write_en{"clint_write_en", false};
        ch_var<ch_uint<32>> clint_rdata{"clint_rdata", 0_d};
        ch_var<ch_bool>     clint_read_en{"clint_read_en", false};
        ch_var<ch_bool>     clint_mtip{"clint_mtip", false};
        
        // UART 相关信号 (AXI4-Lite)
        ch_var<ch_uint<32>> uart_awaddr{"uart_awaddr", 0_d};
        ch_var<ch_uint<2>>  uart_awprot{"uart_awprot", 0_d};
        ch_var<ch_bool>     uart_awvalid{"uart_awvalid", false};
        ch_var<ch_bool>     uart_awready{"uart_awready", false};
        
        ch_var<ch_uint<32>> uart_wdata{"uart_wdata", 0_d};
        ch_var<ch_uint<4>>  uart_wstrb{"uart_wstrb", 0_d};
        ch_var<ch_bool>     uart_wvalid{"uart_wvalid", false};
        ch_var<ch_bool>     uart_wready{"uart_wready", false};
        
        ch_var<ch_uint<2>>  uart_bresp{"uart_bresp", 0_d};
        ch_var<ch_bool>     uart_bvalid{"uart_bvalid", false};
        ch_var<ch_bool>     uart_bready{"uart_bready", false};
        
        ch_var<ch_uint<32>> uart_araddr{"uart_araddr", 0_d};
        ch_var<ch_uint<2>>  uart_arprot{"uart_arprot", 0_d};
        ch_var<ch_bool>     uart_arvalid{"uart_arvalid", false};
        ch_var<ch_bool>     uart_arready{"uart_arready", false};
        
        ch_var<ch_uint<32>> uart_rdata{"uart_rdata", 0_d};
        ch_var<ch_uint<2>>  uart_rresp{"uart_rresp", 0_d};
        ch_var<ch_bool>     uart_rvalid{"uart_rvalid", false};
        ch_var<ch_bool>     uart_rready{"uart_rready", false};
        
        ch_var<ch_bool>     uart_tx{"uart_tx", false};
        ch_var<ch_bool>     uart_irq{"uart_irq", false};
        
        // ====================================================================
        // 流水线连接
        // ====================================================================
        
        // 系统控制
        pipeline.io().rst <<= io().rst;
        pipeline.io().clk <<= io().clk;
        
        // 指令存储器接口 (连接到 I-TCM)
        pipeline.io().instr_addr <<= instr_addr;
        instr_data <<= instr_tcm.io().data;
        pipeline.io().instr_data <<= instr_data;
        pipeline.io().instr_ready <<= instr_ready;
        
        // 数据存储器接口 (连接到 D-TCM/外设)
        pipeline.io().data_addr <<= data_addr;
        pipeline.io().data_write_data <<= data_write_data;
        pipeline.io().data_strbe <<= data_strbe;
        pipeline.io().data_write_en <<= data_write_en;
        pipeline.io().data_read_data <<= data_read_data;
        pipeline.io().data_ready <<= data_ready;
        
        // ====================================================================
        // 地址解码器连接
        // ====================================================================
        
        // 指令地址解码 (用于取指)
        addr_decoder.io().addr <<= instr_addr;
        
        // 数据地址解码 (用于访存)
        addr_decoder.io().addr <<= data_addr;
        
        // 片选信号输出
        i_tcm_cs <<= addr_decoder.io().i_tcm_cs;
        d_tcm_cs <<= addr_decoder.io().d_tcm_cs;
        clint_cs <<= addr_decoder.io().clint_cs;
        uart_cs <<= addr_decoder.io().uart_cs;
        gpio_cs <<= addr_decoder.io().gpio_cs;
        error_cs <<= addr_decoder.io().error_cs;
        local_addr <<= addr_decoder.io().local_addr;
        
        // ====================================================================
        // I-TCM 连接 (指令存储)
        // ====================================================================
        
        // 计算 TCM 内部地址 (32位地址 -> TCM 地址宽度)
        // I-TCM 基地址: 0x20000000, 我们取低 20 位
        auto itcm_offset = instr_addr - ADDR_ITCM_BASE;
        auto itcm_internal_addr = bits<19, 0>(itcm_offset);
        
        // I-TCM 接口连接
        instr_tcm.io().addr <<= itcm_internal_addr;
        instr_tcm.io().write_en <<= ch_bool(false);  // I-TCM 只读
        instr_tcm.io().write_addr <<= ch_uint<20>(0_d);
        instr_tcm.io().write_data <<= ch_uint<32>(0_d);
        
        // ====================================================================
        // D-TCM 连接 (数据存储)
        // ====================================================================
        
        // 计算 TCM 内部地址
        // D-TCM 基地址: 0x20010000
        auto dtcm_offset = data_addr - ADDR_DTCM_BASE;
        auto dtcm_internal_addr = bits<19, 0>(dtcm_offset);
        
        // D-TCM 接口连接 (根据片选信号选择)
        data_tcm.io().addr <<= select(d_tcm_cs, dtcm_internal_addr, ch_uint<20>(0_d));
        data_tcm.io().write <<= select(d_tcm_cs, data_write_en, ch_bool(false));
        data_tcm.io().wdata <<= data_write_data;
        data_tcm.io().valid <<= d_tcm_cs;
        data_ready <<= data_tcm.io().ready;
        data_read_data <<= select(d_tcm_cs, data_tcm.io().rdata, ch_uint<32>(0_d));
        
        // ====================================================================
        // CLINT 连接 (定时器)
        // ====================================================================
        
        // CLINT 地址和数据
        clint_addr <<= select(clint_cs, local_addr, ch_uint<32>(0_d));
        clint_wdata <<= select(clint_cs, data_write_data, ch_uint<32>(0_d));
        clint_write_en <<= clint_cs & data_write_en;
        clint_read_en <<= clint_cs & (~data_write_en);
        
        // CLINT MMIO 接口
        clint.io().addr <<= clint_addr;
        clint.io().write_data <<= clint_wdata;
        clint.io().write_en <<= clint_write_en;
        clint.io().read_en <<= clint_read_en;
        
        // CLINT 读取数据路由 (当访问 CLINT 时)
        data_read_data <<= select(clint_cs, clint.io().read_data, data_read_data);
        
        // CLINT MTIP 中断信号连接到流水线
        clint_mtip <<= clint.io().mtip;
        
        // ====================================================================
        // UART 连接 (AXI4-Lite 接口)
        // ====================================================================
        
        // UART 地址和数据
        auto uart_local_addr = select(uart_cs, local_addr, ch_uint<32>(0_d));
        
        // UART 写地址通道
        uart_awaddr <<= select(uart_cs, data_addr, ch_uint<32>(0_d));
        uart_awprot <<= ch_uint<2>(0_d);
        uart_awvalid <<= uart_cs & data_write_en;
        uart.io().awaddr <<= uart_awaddr;
        uart.io().awprot <<= uart_awprot;
        uart.io().awvalid <<= uart_awvalid;
        uart_awready <<= uart.io().awready;
        
        // UART 写数据通道
        uart_wdata <<= select(uart_cs, data_write_data, ch_uint<32>(0_d));
        uart_wstrb <<= select(uart_cs, data_strbe, ch_uint<4>(0_d));
        uart_wvalid <<= uart_cs & data_write_en;
        uart.io().wdata <<= uart_wdata;
        uart.io().wstrb <<= uart_wstrb;
        uart.io().wvalid <<= uart_wvalid;
        uart_wready <<= uart.io().wready;
        
        // UART 写响应通道
        uart_bready <<= ch_bool(true);  // 总是就绪
        uart_bresp <<= uart.io().bresp;
        uart_bvalid <<= uart.io().bvalid;
        
        // UART 读地址通道
        uart_araddr <<= select(uart_cs, data_addr, ch_uint<32>(0_d));
        uart_arprot <<= ch_uint<2>(0_d);
        uart_arvalid <<= uart_cs & (~data_write_en);
        uart.io().araddr <<= uart_araddr;
        uart.io().arprot <<= uart_arprot;
        uart.io().arvalid <<= uart_arvalid;
        uart_arready <<= uart.io().arready;
        
        // UART 读数据通道
        uart_rready <<= ch_bool(true);  // 总是就绪
        uart_rdata <<= uart.io().rdata;
        uart_rresp <<= uart.io().rresp;
        uart_rvalid <<= uart.io().rvalid;
        
        // UART 读取数据路由
        data_read_data <<= select(uart_cs, uart.io().rdata, data_read_data);
        
        // UART 物理接口
        uart_tx <<= uart.io().uart_tx;
        uart_irq <<= uart.io().irq;
        
        // UART TX 连接到 SoC 输出
        io().uart_tx_char_ready <<= uart_tx;
        io().uart_tx_char <<= bits<7, 0>(uart.io().rdata);  // 仅低8位
        
        // ====================================================================
        // 调试端口连接
        // ====================================================================
        
        // 调试 PC (简化: 使用流水线输出)
        io().debug_pc <<= instr_addr;
        io().debug_halted <<= ch_bool(false);  // 简化: 始终运行
        
        // ====================================================================
        // Boot 配置
        // ====================================================================
        
        // PC 起始地址由流水线 IF 阶段初始化为 0x20000000 (I-TCM 基地址)
        // mstatus 复位值: 0x00000080 (MPIE=1) - 在 CsrBank 中配置
        // mtvec 复位值: 0x20000100 - 在 CsrBank 中配置
    }
    
    /**
     * @brief SoC Tick 方法 - 按正确顺序更新所有组件
     * 
     * Tick 序列:
     * 1. CLINT mtime++ (调用 clint.tick())
     * 2. 流水线 tick (调用 pipeline.tick())
     * 3. 内存访问完成
     */
    void tick() {
        // 注意: 由于 ch::Component 的 tick 机制基于描述而非实际仿真循环
        // 实际的 tick 顺序由 Simulator 控制
        // 这里的方法供参考，实际仿真由 ch::simulator 处理
    }
};

} // namespace riscv
