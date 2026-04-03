/**
 * @file d_tcm.h
 * @brief RISC-V 紧耦合数据存储器 (Data TCM)
 * 
 * 规格:
 * - 容量：64KB (可配置)
 * - 位宽：32-bit
 * - 接口：AXI4 从设备
 * - 访问：单周期读/写
 * - 地址范围：0x8000_0000 - 0x8000_FFFF (默认)
 * - 字节使能支持
 * 
 * AXI4 接口特性:
 * - 支持读/写突发 (INCR)
 * - 支持字节掩码 (WSTRB)
 * - 数据位宽：32-bit
 * - 地址位宽：32-bit
 */

#pragma once

#include "ch.hpp"
#include "component.h"
#include "axi4/axi4_full.h"

using namespace ch::core;

namespace riscv {

template <size_t SIZE_KB = 64, size_t BASE_ADDR = 0x80000000>
class DTcm : public ch::Component {
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
        
        // AXI4 写地址通道
        ch_in<ch_uint<32>>  axi_awaddr;
        ch_in<ch_uint<4>>   axi_awid;
        ch_in<ch_uint<8>>   axi_awlen;
        ch_in<ch_uint<3>>   axi_awsize;
        ch_in<ch_uint<2>>   axi_awburst;
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
        
        // 调试端口
        ch_in<ch_bool>      debug_enable;
        ch_in<ch_bool>      debug_write;
        ch_in<ch_uint<ADDR_WIDTH>> debug_addr;
        ch_in<ch_uint<DATA_WIDTH>> debug_wdata;
        ch_in<ch_uint<4>>   debug_strb;
        ch_out<ch_uint<DATA_WIDTH>> debug_rdata;
    )
    
    DTcm(ch::Component* parent = nullptr, const std::string& name = "d_tcm")
        : ch::Component(parent, name) {}
    
    void create_ports() override {
        new (io_storage_) io_type;
    }
    
    void describe() override {
        // ========================================================================
        // 数据存储器数组 (字节可寻址)
        // ========================================================================
        ch_array<ch_uint<8>, SIZE_BYTES> mem_bytes{"mem_bytes"};
        
        // ========================================================================
        // AXI4 读状态机
        // ========================================================================
        enum class ReadState : uint2_t {
            IDLE,
            READ,
            BURST
        };
        
        ch_var<ReadState> read_state{"read_state", ReadState::IDLE};
        ch_var<ch_uint<8>> read_burst_counter{"read_burst_counter", 0};
        ch_var<ch_uint<32>> read_addr{"read_addr", 0};
        ch_var<ch_uint<4>> read_id{"read_id", 0};
        
        // ========================================================================
        // AXI4 写状态机
        // ========================================================================
        enum class WriteState : uint2_t {
            IDLE,
            WRITE,
            RESPOND
        };
        
        ch_var<WriteState> write_state{"write_state", WriteState::IDLE};
        ch_var<ch_uint<8>> write_burst_counter{"write_burst_counter", 0};
        ch_var<ch_uint<32>> write_addr{"write_addr", 0};
        ch_var<ch_uint<4>> write_id{"write_id", 0};
        
        // ========================================================================
        // 读地址通道处理
        // ========================================================================
        CH_PROCESS(clk, rst) {
            if (rst()) {
                axi_arready() = false;
                axi_rvalid() = false;
                read_state = ReadState::IDLE;
            } else {
                CH_SWITCH(read_state) {
                    CH_CASE(static_cast<uint32_t>(ReadState::IDLE)) {
                        axi_arready() = true;
                        
                        if (axi_arready()) {
                            read_addr = axi_araddr();
                            read_id = axi_arid();
                            read_burst_counter = axi_arlen();
                            
                            if (axi_arlen() > 0) {
                                read_state = ReadState::BURST;
                            } else {
                                read_state = ReadState::READ;
                            }
                            axi_arready() = false;
                        }
                    }
                    
                    CH_CASE(static_cast<uint32_t>(ReadState::READ)) {
                        axi_rvalid() = true;
                        axi_rid() = read_id;
                        axi_rlast() = true;
                        axi_rresp() = 0;  // OKAY
                        
                        // 地址解码和边界检查
                        if ((read_addr >= BASE_ADDR) && (read_addr < BASE_ADDR + SIZE_BYTES)) {
                            ch_var<ch_uint<ADDR_WIDTH>> offset{"offset"};
                            offset = read_addr - BASE_ADDR;
                            
                            // 字节加载并组装为 32-bit
                            ch_var<ch_uint<32>> data{"data"};
                            data = ch_cast<ch_uint<32>>(mem_bytes[offset]).concat(
                                mem_bytes[offset + 1]).concat(
                                mem_bytes[offset + 2]).concat(
                                mem_bytes[offset + 3]);
                            axi_rdata() = data;
                        } else {
                            axi_rdata() = 0;
                            axi_rresp() = 3;  // DECERR
                        }
                        
                        if (axi_rready()) {
                            read_state = ReadState::IDLE;
                            axi_rvalid() = false;
                        }
                    }
                    
                    CH_CASE(static_cast<uint32_t>(ReadState::BURST)) {
                        axi_rvalid() = true;
                        axi_rid() = read_id;
                        axi_rresp() = 0;  // OKAY
                        axi_rlast() = (read_burst_counter == 0);
                        
                        // 地址解码
                        if ((read_addr >= BASE_ADDR) && (read_addr < BASE_ADDR + SIZE_BYTES)) {
                            ch_var<ch_uint<ADDR_WIDTH>> offset{"offset"};
                            offset = read_addr - BASE_ADDR;
                            
                            ch_var<ch_uint<32>> data{"data"};
                            data = ch_cast<ch_uint<32>>(mem_bytes[offset]).concat(
                                mem_bytes[offset + 1]).concat(
                                mem_bytes[offset + 2]).concat(
                                mem_bytes[offset + 3]);
                            axi_rdata() = data;
                        } else {
                            axi_rdata() = 0;
                            axi_rresp() = 3;  // DECERR
                        }
                        
                        if (axi_rready()) {
                            if (read_burst_counter == 0) {
                                read_state = ReadState::IDLE;
                                axi_rvalid() = false;
                            } else {
                                read_burst_counter = read_burst_counter - 1;
                                read_addr = read_addr + 4;
                            }
                        }
                    }
                }
            }
        }
        
        // ========================================================================
        // 写地址通道处理
        // ========================================================================
        CH_PROCESS(clk, rst) {
            if (rst()) {
                axi_awready() = false;
                write_state = WriteState::IDLE;
            } else {
                CH_SWITCH(write_state) {
                    CH_CASE(static_cast<uint32_t>(WriteState::IDLE)) {
                        axi_awready() = true;
                        
                        if (axi_awready()) {
                            write_addr = axi_awaddr();
                            write_id = axi_awid();
                            write_burst_counter = axi_awlen();
                            write_state = WriteState::WRITE;
                            axi_awready() = false;
                        }
                    }
                    
                    CH_CASE(static_cast<uint32_t>(WriteState::WRITE)) {
                        axi_wready() = true;
                        
                        if (axi_wready() && axi_wlast()) {
                            write_state = WriteState::RESPOND;
                            axi_wready() = false;
                        }
                    }
                    
                    CH_CASE(static_cast<uint32_t>(WriteState::RESPOND)) {
                        axi_bvalid() = true;
                        axi_bid() = write_id;
                        axi_bresp() = 0;  // OKAY
                        
                        if (axi_bready()) {
                            write_state = WriteState::IDLE;
                            axi_bvalid() = false;
                        }
                    }
                }
            }
        }
        
        // ========================================================================
        // 写数据通道处理 (实际写入存储器)
        // ========================================================================
        CH_PROCESS(clk, rst) {
            if (rst()) {
                // 复位时不清零存储器，保持内容
            } else if ((write_state == WriteState::WRITE) && axi_wready()) {
                // 边界检查
                if ((write_addr >= BASE_ADDR) && (write_addr < BASE_ADDR + SIZE_BYTES)) {
                    ch_var<ch_uint<ADDR_WIDTH>> offset{"offset"};
                    offset = write_addr - BASE_ADDR;
                    
                    // 字节写入 (根据 WSTRB)
                    CH_IF(axi_wstrb()[0]) {
                        mem_bytes[offset] = axi_wdata().slice<7, 0>();
                    }
                    CH_IF(axi_wstrb()[1]) {
                        mem_bytes[offset + 1] = axi_wdata().slice<15, 8>();
                    }
                    CH_IF(axi_wstrb()[2]) {
                        mem_bytes[offset + 2] = axi_wdata().slice<23, 16>();
                    }
                    CH_IF(axi_wstrb()[3]) {
                        mem_bytes[offset + 3] = axi_wdata().slice<31, 24>();
                    }
                }
                
                // 地址递增 (为下一个 burst 准备)
                write_addr = write_addr + 4;
            }
        }
        
        // ========================================================================
        // 调试端口
        // ========================================================================
        CH_PROCESS(clk) {
            if (debug_enable()) {
                if (debug_write()) {
                    // 调试写入
                    CH_IF(debug_strb()[0]) {
                        mem_bytes[debug_addr()] = debug_wdata().slice<7, 0>();
                    }
                    CH_IF(debug_strb()[1]) {
                        mem_bytes[debug_addr() + 1] = debug_wdata().slice<15, 8>();
                    }
                    CH_IF(debug_strb()[2]) {
                        mem_bytes[debug_addr() + 2] = debug_wdata().slice<23, 16>();
                    }
                    CH_IF(debug_strb()[3]) {
                        mem_bytes[debug_addr() + 3] = debug_wdata().slice<31, 24>();
                    }
                }
                
                // 调试读取
                debug_rdata() = ch_cast<ch_uint<32>>(mem_bytes[debug_addr()]).concat(
                    mem_bytes[debug_addr() + 1]).concat(
                    mem_bytes[debug_addr() + 2]).concat(
                    mem_bytes[debug_addr() + 3]);
            } else {
                debug_rdata() = 0;
            }
        }
    }
};

} // namespace riscv
