/**
 * @file i_tcm.h
 * @brief RISC-V 紧耦合指令存储器 (Instruction TCM)
 * 
 * 规格:
 * - 容量：64KB (可配置)
 * - 位宽：32-bit (指令宽度)
 * - 接口：AXI4 从设备
 * - 访问：单周期
 * - 地址范围：0x0000_0000 - 0x0000_FFFF (默认)
 * 
 * AXI4 接口特性:
 * - 支持读突发 (INCR)
 * - 固定 burst 长度：1 (单指令) 或 4 (预取)
 * - 数据位宽：32-bit
 * - 地址位宽：32-bit
 */

#pragma once

#include "ch.hpp"
#include "component.h"
#include "axi4/axi4_full.h"

using namespace ch::core;

namespace riscv {

template <size_t SIZE_KB = 64, size_t BASE_ADDR = 0x00000000>
class ITcm : public ch::Component {
public:
    static constexpr size_t SIZE_BYTES = SIZE_KB * 1024;
    static constexpr size_t ADDR_WIDTH = ch::log2_ceil(SIZE_BYTES);
    static constexpr size_t DATA_WIDTH = 32;
    
    __io(
        // AXI4 读地址通道
        ch_in<ch_uint<32>>  axi_araddr;
        ch_in<ch_uint<4>>   axi_arid;
        ch_in<ch_uint<8>>   axi_arlen;
        ch_in<ch_uint<3>>   axi_arsize;
        ch_in<ch_uint<2>>   axi_arburst;
        ch_out<ch_bool>     axi_arready;
        
        // AXI4 读数据通道
        ch_out<ch_uint<32>> axi_rdata;
        ch_out<ch_uint<4>>  axi_rid;
        ch_out<ch_bool>     axi_rlast;
        ch_out<ch_uint<2>>  axi_rresp;
        ch_out<ch_bool>     axi_rvalid;
        ch_in<ch_bool>      axi_rready;
        
        // AXI4 写地址通道 (不支持写入，始终返回 SLVERR)
        ch_in<ch_uint<32>>  axi_awaddr;
        ch_out<ch_bool>     axi_awready;
        
        // AXI4 写数据通道
        ch_in<ch_uint<32>>  axi_wdata;
        ch_in<ch_uint<4>>   axi_wstrb;
        ch_in<ch_bool>      axi_wlast;
        ch_out<ch_bool>     axi_wready;
        
        // AXI4 写响应通道
        ch_out<ch_uint<4>>  axi_bid;
        ch_out<ch_uint<2>>  axi_bresp;
        ch_out<ch_bool>     axi_bvalid;
        ch_in<ch_bool>      axi_bready;
        
        // 控制
        ch_in<ch_bool>      clk;
        ch_in<ch_bool>      rst;
        
        // 调试端口 (可选)
        ch_in<ch_bool>      debug_enable;
        ch_in<ch_uint<ADDR_WIDTH>> debug_addr;
        ch_out<ch_uint<DATA_WIDTH>> debug_rdata;
    )
    
    ITcm(ch::Component* parent = nullptr, const std::string& name = "i_tcm")
        : ch::Component(parent, name) {}
    
    void create_ports() override {
        new (io_storage_) io_type;
    }
    
    void describe() override {
        // ========================================================================
        // 指令存储器数组
        // ========================================================================
        ch_array<ch_uint<DATA_WIDTH>, SIZE_BYTES / 4> mem{"mem"};
        
        // ========================================================================
        // AXI4 状态机
        // ========================================================================
        enum class ReadState : uint2_t {
            IDLE,
            READ,
            BURST
        };
        
        ch_var<ReadState> read_state{"read_state", ReadState::IDLE};
        ch_var<ch_uint<8>> burst_counter{"burst_counter", 0};
        ch_var<ch_uint<32>> current_addr{"current_addr", 0};
        ch_var<ch_uint<4>> current_id{"current_id", 0};
        
        // ========================================================================
        // 读地址通道处理
        // ========================================================================
        CH_PROCESS(clk, rst) {
            if (rst()) {
                axi_arready() = false;
                read_state = ReadState::IDLE;
                burst_counter = 0;
            } else {
                CH_SWITCH(read_state) {
                    CH_CASE(static_cast<uint32_t>(ReadState::IDLE)) {
                        // 准备接收新请求
                        axi_arready() = true;
                        
                        if (axi_arready() && axi_rready()) {
                            // 地址有效且主设备准备好接收数据
                            current_addr = axi_araddr();
                            current_id = axi_arid();
                            burst_counter = axi_arlen();
                            
                            if (axi_arlen() > 0) {
                                read_state = ReadState::BURST;
                            } else {
                                read_state = ReadState::READ;
                            }
                            axi_arready() = false;
                        }
                    }
                    
                    CH_CASE(static_cast<uint32_t>(ReadState::READ)) {
                        // 单周期读
                        axi_rvalid() = true;
                        axi_rid() = current_id;
                        axi_rlast() = true;
                        axi_rresp() = 0;  // OKAY
                        
                        // 地址解码
                        ch_var<ch_uint<ADDR_WIDTH>> tcm_addr{"tcm_addr"};
                        tcm_addr = (current_addr - BASE_ADDR).slice<ADDR_WIDTH + 1, 2>();
                        
                        // 边界检查
                        if ((current_addr >= BASE_ADDR) && (current_addr < BASE_ADDR + SIZE_BYTES)) {
                            axi_rdata() = mem[tcm_addr];
                        } else {
                            axi_rdata() = 0;
                            axi_rresp() = 2;  // SLVERR
                        }
                        
                        if (axi_rready()) {
                            read_state = ReadState::IDLE;
                            axi_rvalid() = false;
                        }
                    }
                    
                    CH_CASE(static_cast<uint32_t>(ReadState::BURST)) {
                        // 突发读
                        axi_rvalid() = true;
                        axi_rid() = current_id;
                        axi_rresp() = 0;  // OKAY
                        
                        // 地址解码
                        ch_var<ch_uint<ADDR_WIDTH>> tcm_addr{"tcm_addr"};
                        tcm_addr = (current_addr - BASE_ADDR).slice<ADDR_WIDTH + 1, 2>();
                        
                        // 边界检查
                        if ((current_addr >= BASE_ADDR) && (current_addr < BASE_ADDR + SIZE_BYTES)) {
                            axi_rdata() = mem[tcm_addr];
                        } else {
                            axi_rdata() = 0;
                            axi_rresp() = 2;  // SLVERR
                        }
                        
                        // 最后一个传输
                        axi_rlast() = (burst_counter == 0);
                        
                        if (axi_rready()) {
                            if (burst_counter == 0) {
                                read_state = ReadState::IDLE;
                                axi_rvalid() = false;
                            } else {
                                burst_counter = burst_counter - 1;
                                current_addr = current_addr + 4;  // 下一个 32-bit 字
                            }
                        }
                    }
                }
            }
        }
        
        // ========================================================================
        // 写地址通道 (始终返回 SLVERR，ITCM 不可写)
        // ========================================================================
        CH_PROCESS(clk, rst) {
            if (rst()) {
                axi_awready() = false;
            } else {
                // 始终准备接收但返回错误
                axi_awready() = true;
            }
        }
        
        // ========================================================================
        // 写数据通道 (忽略数据)
        // ========================================================================
        CH_PROCESS(clk, rst) {
            if (rst()) {
                axi_wready() = false;
            } else {
                axi_wready() = true;
            }
        }
        
        // ========================================================================
        // 写响应通道 (始终返回 SLVERR)
        // ========================================================================
        CH_PROCESS(clk, rst) {
            if (rst()) {
                axi_bvalid() = false;
                axi_bresp() = 0;
            } else if (axi_wready() && axi_wlast()) {
                axi_bvalid() = true;
                axi_bid() = 0;
                axi_bresp() = 2;  // SLVERR - 写操作不允许
                
                if (axi_bready()) {
                    axi_bvalid() = false;
                }
            }
        }
        
        // ========================================================================
        // 调试端口
        // ========================================================================
        CH_PROCESS(clk) {
            if (debug_enable()) {
                debug_rdata() = mem[debug_addr()];
            } else {
                debug_rdata() = 0;
            }
        }
        
        // ========================================================================
        // 初始化 (可选，通过文件加载或硬编码)
        // ========================================================================
        // 实际使用时可通过 $readmemh 或类似机制加载固件
        // 这里保持空存储器，由外部初始化
    }
};

} // namespace riscv
