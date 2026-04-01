/**
 * @file axi_to_axilite.h
 * @brief AXI4 到 AXI4-Lite 协议转换器
 * 
 * 将 AXI4 全功能协议转换为 AXI4-Lite 精简协议：
 * - 忽略突发传输 (强制长度为 0)
 * - 忽略 ID 标记
 * - 忽略缓存/保护属性
 * - 仅支持 32 位数据宽度
 * 
 * 使用场景:
 * - AXI4 主设备 → AXI4-Lite 从设备
 * - RISC-V 处理器 → 简单外设
 */

#pragma once

#include "ch.hpp"
#include "component.h"

using namespace ch::core;

namespace axi4 {

// ============================================================================
// AXI to AXI-Lite Converter
// ============================================================================

template <
    unsigned ADDR_WIDTH = 32,
    unsigned DATA_WIDTH = 32
>
class AxiToAxiLiteConverter : public ch::Component {
public:
    static_assert(DATA_WIDTH == 32, "AXI4-Lite only supports 32-bit data width");
    
    __io(
        // ==================== AXI4 主设备接口 (输入) ====================
        ch_in<ch_uint<ADDR_WIDTH>> axi_awaddr;
        ch_in<ch_uint<4>> axi_awid;
        ch_in<ch_uint<4>> axi_awlen;
        ch_in<ch_bool> axi_awvalid;
        ch_out<ch_bool> axi_awready;
        
        ch_in<ch_uint<DATA_WIDTH>> axi_wdata;
        ch_in<ch_uint<4>> axi_wstrb;
        ch_in<ch_bool> axi_wlast;
        ch_in<ch_bool> axi_wvalid;
        ch_out<ch_bool> axi_wready;
        
        ch_out<ch_uint<4>> axi_bid;
        ch_out<ch_uint<2>> axi_bresp;
        ch_out<ch_bool> axi_bvalid;
        ch_in<ch_bool> axi_bready;
        
        ch_in<ch_uint<ADDR_WIDTH>> axi_araddr;
        ch_in<ch_uint<4>> axi_arid;
        ch_in<ch_uint<4>> axi_arlen;
        ch_in<ch_bool> axi_arvalid;
        ch_out<ch_bool> axi_arready;
        
        ch_out<ch_uint<DATA_WIDTH>> axi_rdata;
        ch_out<ch_uint<4>> axi_rid;
        ch_out<ch_uint<2>> axi_rresp;
        ch_out<ch_bool> axi_rlast;
        ch_out<ch_bool> axi_rvalid;
        ch_in<ch_bool> axi_rready;
        
        // ==================== AXI4-Lite 从设备接口 (输出) ====================
        ch_out<ch_uint<ADDR_WIDTH>> axil_awaddr;
        ch_out<ch_uint<2>> axil_awprot;
        ch_out<ch_bool> axil_awvalid;
        ch_in<ch_bool> axil_awready;
        
        ch_out<ch_uint<DATA_WIDTH>> axil_wdata;
        ch_out<ch_uint<DATA_WIDTH/8>> axil_wstrb;
        ch_out<ch_bool> axil_wvalid;
        ch_in<ch_bool> axil_wready;
        
        ch_in<ch_uint<2>> axil_bresp;
        ch_in<ch_bool> axil_bvalid;
        ch_out<ch_bool> axil_bready;
        
        ch_out<ch_uint<ADDR_WIDTH>> axil_araddr;
        ch_out<ch_uint<2>> axil_arprot;
        ch_out<ch_bool> axil_arvalid;
        ch_in<ch_bool> axil_arready;
        
        ch_in<ch_uint<DATA_WIDTH>> axil_rdata;
        ch_in<ch_uint<2>> axil_rresp;
        ch_in<ch_bool> axil_rvalid;
        ch_out<ch_bool> axil_rready;
    )
    
    AxiToAxiLiteConverter(ch::Component* parent = nullptr, const std::string& name = "axi_to_axilite")
        : ch::Component(parent, name) {}
    
    void create_ports() override {
        new (io_storage_) io_type;
    }
    
    void describe() override {
        // ========================================================================
        // 状态机定义
        // ========================================================================
        
        // 写状态机：0=IDLE, 1=AW, 2=W, 3=B
        ch_reg<ch_uint<2>> write_state(0_d);
        
        // 读状态机：0=IDLE, 1=AR, 2=R
        ch_reg<ch_uint<2>> read_state(0_d);
        
        // ========================================================================
        // AXI4 → AXI4-Lite 地址转换
        // ========================================================================
        
        // AXI4-Lite 地址 = AXI4 地址 (直接传递)
        io().axil_awaddr = io().axi_awaddr;
        io().axil_araddr = io().axi_araddr;
        
        // 保护属性 (默认值)
        io().axil_awprot = ch_uint<2>(0_d);  // 非安全，非特权，数据访问
        io().axil_arprot = ch_uint<2>(0_d);
        
        // ========================================================================
        // 写地址通道 (AW)
        // ========================================================================
        
        auto is_write_idle = (write_state == ch_uint<2>(0_d));
        auto is_write_aw = (write_state == ch_uint<2>(1_d));
        
        // AXI4 AW → AXI4-Lite AW
        io().axil_awvalid = select(is_write_idle, io().axi_awvalid, ch_bool(false));
        io().axi_awready = select(is_write_idle, io().axil_awready, ch_bool(false));
        
        // 状态转换：IDLE → AW
        auto next_write_state = select(is_write_idle,
                                        select(io().axi_awvalid, ch_uint<2>(1_d), ch_uint<2>(0_d)),
                                        select(is_write_aw,
                                            select(io().axil_awready, ch_uint<2>(2_d), ch_uint<2>(1_d)),
                                            select(io().axi_bready, ch_uint<2>(0_d), ch_uint<2>(3_d))));
        write_state->next = next_write_state;
        
        // ========================================================================
        // 写数据通道 (W)
        // ========================================================================
        
        // AXI4 W → AXI4-Lite W (直接传递)
        io().axil_wdata = io().axi_wdata;
        io().axil_wstrb = io().axi_wstrb;
        io().axil_wvalid = select(is_write_aw, io().axi_wvalid, ch_bool(false));
        io().axi_wready = select(is_write_aw, io().axil_wready, ch_bool(false));
        
        // ========================================================================
        // 写响应通道 (B)
        // ========================================================================
        
        // AXI4-Lite B → AXI4 B
        io().axi_bid = ch_uint<4>(0_d);  // 忽略 ID
        io().axi_bresp = io().axil_bresp;
        io().axi_bvalid = io().axil_bvalid;
        io().axil_bready = io().axi_bready;
        
        // ========================================================================
        // 读地址通道 (AR)
        // ========================================================================
        
        auto is_read_idle = (read_state == ch_uint<2>(0_d));
        auto is_read_ar = (read_state == ch_uint<2>(1_d));
        
        // AXI4 AR → AXI4-Lite AR
        io().axil_arvalid = select(is_read_idle, io().axi_arvalid, ch_bool(false));
        io().axi_arready = select(is_read_idle, io().axil_arready, ch_bool(false));
        
        // 状态转换：IDLE → AR → R
        auto next_read_state = select(is_read_idle,
                                       select(io().axi_arvalid, ch_uint<2>(1_d), ch_uint<2>(0_d)),
                                       select(is_read_ar,
                                           select(io().axil_arready, ch_uint<2>(2_d), ch_uint<2>(1_d)),
                                           select(io().axi_rready && io().axi_rvalid,
                                               ch_uint<2>(0_d), ch_uint<2>(2_d))));
        read_state->next = next_read_state;
        
        // ========================================================================
        // 读数据通道 (R)
        // ========================================================================
        
        // AXI4-Lite R → AXI4 R
        io().axi_rdata = io().axil_rdata;
        io().axi_rid = ch_uint<4>(0_d);  // 忽略 ID
        io().axi_rresp = io().axil_rresp;
        io().axi_rlast = ch_bool(true);  // 单次传输
        io().axi_rvalid = io().axil_rvalid;
        io().axil_rready = io().axi_rready;
    }
};

// ============================================================================
// AXI4-Lite to AXI4 Adapter (反向转换)
// ============================================================================

template <
    unsigned ADDR_WIDTH = 32,
    unsigned DATA_WIDTH = 32,
    unsigned ID_WIDTH = 4
>
class AxiLiteToAxiAdapter : public ch::Component {
public:
    __io(
        // ==================== AXI4-Lite 主设备接口 (输入) ====================
        ch_in<ch_uint<ADDR_WIDTH>> axil_awaddr;
        ch_in<ch_uint<2>> axil_awprot;
        ch_in<ch_bool> axil_awvalid;
        ch_out<ch_bool> axil_awready;
        
        ch_in<ch_uint<DATA_WIDTH>> axil_wdata;
        ch_in<ch_uint<DATA_WIDTH/8>> axil_wstrb;
        ch_in<ch_bool> axil_wvalid;
        ch_out<ch_bool> axil_wready;
        
        ch_out<ch_uint<2>> axil_bresp;
        ch_out<ch_bool> axil_bvalid;
        ch_in<ch_bool> axil_bready;
        
        ch_in<ch_uint<ADDR_WIDTH>> axil_araddr;
        ch_in<ch_uint<2>> axil_arprot;
        ch_in<ch_bool> axil_arvalid;
        ch_out<ch_bool> axil_arready;
        
        ch_out<ch_uint<DATA_WIDTH>> axil_rdata;
        ch_out<ch_uint<2>> axil_rresp;
        ch_out<ch_bool> axil_rvalid;
        ch_in<ch_bool> axil_rready;
        
        // ==================== AXI4 从设备接口 (输出) ====================
        ch_out<ch_uint<ADDR_WIDTH>> axi_awaddr;
        ch_out<ch_uint<ID_WIDTH>> axi_awid;
        ch_out<ch_uint<4>> axi_awlen;
        ch_out<ch_uint<3>> axi_awsize;
        ch_out<ch_uint<2>> axi_awburst;
        ch_out<ch_bool> axi_awvalid;
        ch_in<ch_bool> axi_awready;
        
        ch_out<ch_uint<DATA_WIDTH>> axi_wdata;
        ch_out<ch_uint<ID_WIDTH>> axi_wid;
        ch_out<ch_uint<DATA_WIDTH/8>> axi_wstrb;
        ch_out<ch_bool> axi_wlast;
        ch_out<ch_bool> axi_wvalid;
        ch_in<ch_bool> axi_wready;
        
        ch_in<ch_uint<ID_WIDTH>> axi_bid;
        ch_in<ch_uint<2>> axi_bresp;
        ch_in<ch_bool> axi_bvalid;
        ch_out<ch_bool> axi_bready;
        
        ch_out<ch_uint<ADDR_WIDTH>> axi_araddr;
        ch_out<ch_uint<ID_WIDTH>> axi_arid;
        ch_out<ch_uint<4>> axi_arlen;
        ch_out<ch_uint<3>> axi_arsize;
        ch_out<ch_uint<2>> axi_arburst;
        ch_out<ch_bool> axi_arvalid;
        ch_in<ch_bool> axi_arready;
        
        ch_in<ch_uint<DATA_WIDTH>> axi_rdata;
        ch_in<ch_uint<ID_WIDTH>> axi_rid;
        ch_in<ch_uint<2>> axi_rresp;
        ch_in<ch_bool> axi_rlast;
        ch_in<ch_bool> axi_rvalid;
        ch_out<ch_bool> axi_rready;
    )
    
    AxiLiteToAxiAdapter(ch::Component* parent = nullptr, const std::string& name = "axil_to_axi")
        : ch::Component(parent, name) {}
    
    void create_ports() override {
        new (io_storage_) io_type;
    }
    
    void describe() override {
        // ========================================================================
        // AXI4-Lite → AXI4 转换
        // ========================================================================
        
        // 地址直接传递
        io().axi_awaddr = io().axil_awaddr;
        io().axi_araddr = io().axil_araddr;
        
        // 设置 AXI4 固定参数 (单次传输)
        io().axi_awid = ch_uint<ID_WIDTH>(0_d);
        io().axi_awlen = ch_uint<4>(0_d);     // 单次传输 (LEN=0)
        io().axi_awsize = ch_uint<3>(2_d);    // 4 字节
        io().axi_awburst = ch_uint<2>(1_d);   // INCR
        
        io().axi_wid = ch_uint<ID_WIDTH>(0_d);
        io().axi_wlast = ch_bool(true);       // 单次传输
        
        io().axi_arid = ch_uint<ID_WIDTH>(0_d);
        io().axi_arlen = ch_uint<4>(0_d);
        io().axi_arsize = ch_uint<3>(2_d);
        io().axi_arburst = ch_uint<2>(1_d);
        
        // 数据和控制信号直接传递
        io().axi_wdata = io().axil_wdata;
        io().axi_wstrb = io().axil_wstrb;
        
        // 握手信号直接传递
        io().axi_awvalid = io().axil_awvalid;
        io().axil_awready = io().axi_awready;
        
        io().axi_wvalid = io().axil_wvalid;
        io().axil_wready = io().axi_wready;
        
        io().axil_bresp = io().axi_bresp;
        io().axil_bvalid = io().axi_bvalid;
        io().axi_bready = io().axil_bready;
        
        io().axi_arvalid = io().axil_arvalid;
        io().axil_arready = io().axi_arready;
        
        io().axil_rdata = io().axi_rdata;
        io().axil_rresp = io().axi_rresp;
        io().axil_rvalid = io().axi_rvalid;
        io().axi_rready = io().axil_rready;
    }
};

} // namespace axi4
