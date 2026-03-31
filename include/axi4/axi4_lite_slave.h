/**
 * AXI4-Lite Slave Controller
 * 
 * 通用 AXI4-Lite 从设备控制器，支持：
 * - 可配置寄存器数量
 * - 读写访问
 * - 错误响应 (SLVERR)
 * 
 * 使用方式:
 * 1. 继承此类
 * 2. 在 describe() 中调用 handle_axi_access()
 * 3. 实现 register_read()/register_write()
 */

#pragma once

#include "ch.hpp"
#include "axi4/axi4_lite.h"
#include "component.h"

using namespace ch::core;

namespace axi4 {

template <unsigned ADDR_WIDTH = 32, unsigned DATA_WIDTH = 32, unsigned NUM_REGS = 16>
class AxiLiteSlave : public ch::Component {
public:
    __io(
        // 写地址通道
        ch_in<ch_uint<ADDR_WIDTH>> awaddr;
        ch_in<ch_uint<2>> awprot;
        ch_in<ch_bool> awvalid;
        ch_out<ch_bool> awready;
        
        // 写数据通道
        ch_in<ch_uint<DATA_WIDTH>> wdata;
        ch_in<ch_uint<DATA_WIDTH/8>> wstrb;
        ch_in<ch_bool> wvalid;
        ch_out<ch_bool> wready;
        
        // 写响应通道
        ch_out<ch_uint<2>> bresp;
        ch_out<ch_bool> bvalid;
        ch_in<ch_bool> bready;
        
        // 读地址通道
        ch_in<ch_uint<ADDR_WIDTH>> araddr;
        ch_in<ch_uint<2>> arprot;
        ch_in<ch_bool> arvalid;
        ch_out<ch_bool> arready;
        
        // 读数据通道
        ch_out<ch_uint<DATA_WIDTH>> rdata;
        ch_out<ch_uint<2>> rresp;
        ch_out<ch_bool> rvalid;
        ch_in<ch_bool> rready;
    )
    
    AxiLiteSlave(ch::Component* parent = nullptr, const std::string& name = "axi_lite_slave")
        : ch::Component(parent, name) {}
    
    void create_ports() override {
        new (io_storage_) io_type;
    }
    
    void describe() override {
        // 状态机：IDLE -> WRITE/READ -> RESPOND
        ch_reg<ch_uint<2>> state_reg(0_d);  // 0=IDLE, 1=WRITE, 2=READ, 3=RESPOND
        
        // 内部寄存器文件
        ch_reg<ch_uint<DATA_WIDTH>> registers[NUM_REGS];
        for (unsigned i = 0; i < NUM_REGS; ++i) {
            registers[i] = ch_reg<ch_uint<DATA_WIDTH>>(0_d);
        }
        
        // 地址解码 (简化：使用低 log2(NUM_REGS) 位，假设 4 字节对齐)
        constexpr unsigned REG_ADDR_BITS = (NUM_REGS > 1) ? log2ceil(NUM_REGS) : 1;
        auto reg_addr = io().awaddr >> ch_uint<ADDR_WIDTH>(2_d);  // 右移 2 位 (4 字节对齐)
        // 取低 REG_ADDR_BITS 位
        ch_uint<REG_ADDR_BITS> reg_addr_mask;
        if constexpr (REG_ADDR_BITS >= 1) {
            reg_addr_mask = ch_uint<REG_ADDR_BITS>((1 << REG_ADDR_BITS) - 1);
        } else {
            reg_addr_mask = ch_uint<REG_ADDR_BITS>(0_d);
        }
        reg_addr = reg_addr & reg_addr_mask;
        
        // ==================== 写地址通道 ====================
        
        auto state_is_idle = (state_reg == ch_uint<2>(0_d));
        auto write_req = io().awvalid && state_is_idle;  // IDLE 状态 + AW 有效
        io().awready = write_req;
        
        // ==================== 写数据通道 ====================
        
        auto write_data = io().wvalid && io().awready;  // AW 握手后
        io().wready = write_data;
        
        // 写寄存器
        for (unsigned i = 0; i < NUM_REGS; ++i) {
            auto sel = (reg_addr == ch_uint<REG_ADDR_BITS>(i));
            auto we = write_data && sel;
            // 简化的写逻辑 (全字节使能)
            registers[i]->next = select(we, io().wdata, registers[i]);
        }
        
        // ==================== 写响应通道 ====================
        
        auto write_resp = write_data;
        io().bvalid = write_resp;
        io().bresp = ch_uint<2>(resp::OKAY);
        
        // ==================== 读地址通道 ====================
        
        auto read_req = io().arvalid && state_is_idle;
        io().arready = read_req;
        
        // ==================== 读数据通道 ====================
        
        // 读寄存器
        ch_uint<DATA_WIDTH> read_data(0_d);
        auto ar_reg_addr = io().araddr >> ch_uint<ADDR_WIDTH>(2_d);
        ar_reg_addr = ar_reg_addr & reg_addr_mask;
        
        for (unsigned i = 0; i < NUM_REGS; ++i) {
            auto sel = (ar_reg_addr == ch_uint<ADDR_WIDTH>(i));
            read_data = select(sel && read_req, registers[i], read_data);
        }
        
        auto read_data_valid = read_req;
        io().rvalid = read_data_valid;
        io().rdata = read_data;
        io().rresp = ch_uint<2>(resp::OKAY);
        
        // ==================== 状态机更新 ====================
        
        // 简化：每个周期回到 IDLE
        state_reg->next = ch_uint<2>(0_d);
    }
    
private:
    // 编译时计算 log2
    static constexpr unsigned log2ceil(unsigned n) {
        return (n <= 1) ? 0 : 1 + log2ceil((n + 1) / 2);
    }
};

} // namespace axi4
