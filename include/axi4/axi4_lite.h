/**
 * AXI4-Lite Protocol Definitions
 * 
 * AXI4-Lite 是 AXI4 的简化版本，用于寄存器访问：
 * - 32 位地址
 * - 32 位数据
 * - 5 个独立通道 (AW, W, B, AR, R)
 * - 无突发传输 (每次 1 个数据)
 */

#pragma once

#include "ch.hpp"

using namespace ch::core;

namespace axi4 {

// ============================================================================
// AXI4-Lite 信号定义
// ============================================================================

/**
 * AXI4-Lite 写地址通道 (AW)
 */
template <unsigned ADDR_WIDTH = 32>
struct AxiLiteAW {
    ch_uint<ADDR_WIDTH> awaddr;   // 写地址
    ch_uint<2> awprot;            // 保护类型
    ch_bool awvalid;              // 地址有效
    ch_bool awready;              // 地址就绪
};

/**
 * AXI4-Lite 写数据通道 (W)
 */
template <unsigned DATA_WIDTH = 32>
struct AxiLiteW {
    ch_uint<DATA_WIDTH> wdata;    // 写数据
    ch_uint<DATA_WIDTH/8> wstrb;  // 写 strobe
    ch_bool wvalid;               // 数据有效
    ch_bool wready;               // 数据就绪
};

/**
 * AXI4-Lite 写响应通道 (B)
 */
template <>
struct AxiLiteW<0> {};  // 特化用于响应

struct AxiLiteB {
    ch_uint<2> bresp;               // 响应类型 (OKAY=00, SLVERR=10)
    ch_bool bvalid;                 // 响应有效
    ch_bool bready;                 // 响应就绪
};

/**
 * AXI4-Lite 读地址通道 (AR)
 */
template <unsigned ADDR_WIDTH = 32>
struct AxiLiteAR {
    ch_uint<ADDR_WIDTH> araddr;   // 读地址
    ch_uint<2> arprot;            // 保护类型
    ch_bool arvalid;              // 地址有效
    ch_bool arready;              // 地址就绪
};

/**
 * AXI4-Lite 读数据通道 (R)
 */
template <unsigned DATA_WIDTH = 32>
struct AxiLiteR {
    ch_uint<DATA_WIDTH> rdata;    // 读数据
    ch_uint<2> rresp;             // 响应类型
    ch_bool rvalid;               // 数据有效
    ch_bool rready;               // 数据就绪
};

// ============================================================================
// AXI4-Lite 从设备接口
// ============================================================================

template <unsigned ADDR_WIDTH = 32, unsigned DATA_WIDTH = 32>
struct AxiLiteSlaveIF {
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
};

// ============================================================================
// 响应编码
// ============================================================================

namespace resp {
    constexpr unsigned OKAY = 0b00;    // 正常访问
    constexpr unsigned EXOKAY = 0b01;  // 独占访问成功
    constexpr unsigned SLVERR = 0b10;  // 从设备错误
    constexpr unsigned DECERR = 0b11;  // 解码错误
}

// ============================================================================
// 保护类型编码
// ============================================================================

namespace prot {
    constexpr unsigned UNPRIVILEGED = 0b000;  // 非特权访问
    constexpr unsigned PRIVILEGED = 0b001;    // 特权访问
    constexpr unsigned SECURE = 0b000;        // 安全访问
    constexpr unsigned NON_SECURE = 0b010;    // 非安全访问
    constexpr unsigned INSTRUCTION = 0b000;   // 指令访问
    constexpr unsigned DATA = 0b100;          // 数据访问
}

} // namespace axi4
