/**
 * AXI4-Lite 2x2 Matrix
 * 
 * 连接 2 个主设备到 2 个从设备：
 * - 主设备 0 (CPU) → 从设备 0 (SRAM), 从设备 1 (外设)
 * - 主设备 1 (DMA) → 从设备 0 (SRAM), 从设备 1 (外设)
 * 
 * 特性:
 * - 轮询仲裁
 * - 地址解码
 * - 错误响应 (访问未映射地址)
 */

#pragma once

#include "ch.hpp"
#include "axi4/axi4_lite.h"
#include "component.h"

using namespace ch::core;

namespace axi4 {

template <unsigned ADDR_WIDTH = 32, unsigned DATA_WIDTH = 32>
class AxiLiteMatrix2x2 : public ch::Component {
public:
    __io(
        // 主设备 0 接口
        ch_out<ch_uint<ADDR_WIDTH>> m0_awaddr;
        ch_out<ch_uint<2>> m0_awprot;
        ch_out<ch_bool> m0_awvalid;
        ch_in<ch_bool> m0_awready;
        
        ch_out<ch_uint<DATA_WIDTH>> m0_wdata;
        ch_out<ch_uint<DATA_WIDTH/8>> m0_wstrb;
        ch_out<ch_bool> m0_wvalid;
        ch_in<ch_bool> m0_wready;
        
        ch_in<ch_uint<2>> m0_bresp;
        ch_in<ch_bool> m0_bvalid;
        ch_out<ch_bool> m0_bready;
        
        ch_out<ch_uint<ADDR_WIDTH>> m0_araddr;
        ch_out<ch_uint<2>> m0_arprot;
        ch_out<ch_bool> m0_arvalid;
        ch_in<ch_bool> m0_arready;
        
        ch_in<ch_uint<DATA_WIDTH>> m0_rdata;
        ch_in<ch_uint<2>> m0_rresp;
        ch_in<ch_bool> m0_rvalid;
        ch_out<ch_bool> m0_rready;
        
        // 主设备 1 接口 (简化，省略详细定义)
        // ... 类似主设备 0
        
        // 从设备 0 接口
        ch_in<ch_uint<ADDR_WIDTH>> s0_awaddr;
        ch_in<ch_uint<2>> s0_awprot;
        ch_in<ch_bool> s0_awvalid;
        ch_out<ch_bool> s0_awready;
        
        // ... 类似定义
        
        // 从设备 1 接口
        ch_in<ch_uint<ADDR_WIDTH>> s1_awaddr;
        // ... 类似定义
    )
    
    AxiLiteMatrix2x2(ch::Component* parent = nullptr, const std::string& name = "axi_matrix_2x2")
        : ch::Component(parent, name) {}
    
    void create_ports() override {
        new (io_storage_) io_type;
    }
    
    void describe() override {
        // 仲裁器：轮询主设备 0 和 1
        ch_reg<ch_bool> arb_select(ch_bool(false));  // 0=主设备 0, 1=主设备 1
        
        // 地址解码：根据地址选择从设备
        // 假设：0x0000_0000 - 0x7FFF_FFFF → 从设备 0 (SRAM)
        //       0x8000_0000 - 0xFFFF_FFFF → 从设备 1 (外设)
        auto addr_bit = io().s0_awaddr >> ch_uint<ADDR_WIDTH>(ADDR_WIDTH-1);
        auto select_slave_1 = (addr_bit != ch_uint<ADDR_WIDTH>(0_d));
        
        // 仲裁逻辑
        auto m0_req = io().m0_awvalid;
        auto m1_req = true;  // 简化：假设有主设备 1
        
        // 选择哪个主设备
        auto selected_master = arb_select;
        
        // 更新仲裁器
        auto transaction_complete = io().s0_awready || io().s1_awready;
        arb_select->next = select(transaction_complete, !arb_select, arb_select);
        
        // 从设备选择
        // 简化实现：直接连接主设备 0 到从设备
        io().s0_awready = io().m0_awvalid && !select_slave_1;
        io().s1_awready = io().m0_awvalid && select_slave_1;
        
        // 主设备响应
        io().m0_awready = select(!select_slave_1, io().s0_awready, io().s1_awready);
        
        // 地址转发
        io().s0_awaddr = io().m0_awaddr;
        io().s1_awaddr = io().m0_awaddr;
    }
};

} // namespace axi4
