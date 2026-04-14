/**
 * @file address_decoder.h
 * @brief RISC-V MMIO 地址解码器 - 将 CPU 内存访问路由到正确的外设
 * 
 * 功能:
 * - 解析 CPU 32 位地址
 * - 根据地址范围生成片选信号 (one-hot 编码)
 * - 计算本地地址偏移 (移除基地址)
 * 
 * 内存映射:
 * | 区域   | 基地址          | 大小   | 片选信号   |
 * |--------|-----------------|--------|------------|
 * | NULL   | 0x00000000      | 512MB  | error_cs   |
 * | I-TCM  | 0x20000000      | 64KB   | i_tcm_cs   |
 * | D-TCM  | 0x20010000      | 64KB   | d_tcm_cs   |
 * | CLINT  | 0x40000000      | 4KB    | clint_cs   |
 * | UART   | 0x40001000      | 4KB    | uart_cs    |
 * | GPIO   | 0x40002000      | 4KB    | gpio_cs    |
 */

#pragma once

#include "ch.hpp"
#include "component.h"

using namespace ch::core;

namespace riscv {

// 地址空间基地址常量
constexpr ch_uint<32> ADDR_NULL_BASE  = ch_uint<32>(0x00000000_d);
constexpr ch_uint<32> ADDR_ITCM_BASE  = ch_uint<32>(0x20000000_d);
constexpr ch_uint<32> ADDR_DTCM_BASE  = ch_uint<32>(0x20010000_d);
constexpr ch_uint<32> ADDR_CLINT_BASE = ch_uint<32>(0x40000000_d);
constexpr ch_uint<32> ADDR_UART_BASE  = ch_uint<32>(0x40001000_d);
constexpr ch_uint<32> ADDR_GPIO_BASE  = ch_uint<32>(0x40002000_d);

// 地址空间大小常量
constexpr ch_uint<32> ADDR_ITCM_SIZE  = ch_uint<32>(0x00010000_d);   // 64KB
constexpr ch_uint<32> ADDR_DTCM_SIZE  = ch_uint<32>(0x00010000_d);   // 64KB
constexpr ch_uint<32> ADDR_CLINT_SIZE = ch_uint<32>(0x00001000_d);   // 4KB
constexpr ch_uint<32> ADDR_UART_SIZE  = ch_uint<32>(0x00001000_d);   // 4KB
constexpr ch_uint<32> ADDR_GPIO_SIZE  = ch_uint<32>(0x00001000_d);   // 4KB

// 地址空间上限 (基地址 + 大小 - 1)
constexpr ch_uint<32> ADDR_ITCM_END  = ADDR_ITCM_BASE + ch_uint<32>(ADDR_ITCM_SIZE - 1_d);
constexpr ch_uint<32> ADDR_DTCM_END  = ADDR_DTCM_BASE + ch_uint<32>(ADDR_DTCM_SIZE - 1_d);
constexpr ch_uint<32> ADDR_CLINT_END = ADDR_CLINT_BASE + ch_uint<32>(ADDR_CLINT_SIZE - 1_d);
constexpr ch_uint<32> ADDR_UART_END  = ADDR_UART_BASE + ch_uint<32>(ADDR_UART_SIZE - 1_d);
constexpr ch_uint<32> ADDR_GPIO_END  = ADDR_GPIO_BASE + ch_uint<32>(ADDR_GPIO_SIZE - 1_d);

class AddressDecoder : public ch::Component {
public:
    __io(
        ch_in<ch_uint<32>> addr;           // CPU 地址输入
        
        // 片选输出 (one-hot 编码: 同时只有一个为高)
        ch_out<ch_bool>    i_tcm_cs;       // I-TCM 片选
        ch_out<ch_bool>    d_tcm_cs;       // D-TCM 片选
        ch_out<ch_bool>    clint_cs;       // CLINT 片选
        ch_out<ch_bool>    uart_cs;        // UART 片选
        ch_out<ch_bool>    gpio_cs;        // GPIO 片选
        ch_out<ch_bool>    error_cs;       // 无效地址
        
        // 本地地址偏移 (相对于选中区域的基地址)
        ch_out<ch_uint<32>> local_addr;    // 区域内的偏移地址
    )
    
    AddressDecoder(ch::Component* parent = nullptr, const std::string& name = "address_decoder")
        : ch::Component(parent, name) {}
    
    void create_ports() override {
        new (io_storage_) io_type;
    }
    
    void describe() override {
        ch_uint<32> addr_in = io().addr;
        
        // 计算各区域的地址范围满足条件
        // 使用 select() 实现: condition ? true_value : false_value
        // addr >= base && addr <= end 等价于:
        // select(addr >= base, select(addr <= end, true, false), false)
        
        // I-TCM 区域检测: [0x20000000, 0x2000FFFF]
        ch_bool in_itcm = select(
            addr_in >= ADDR_ITCM_BASE,
            addr_in <= ADDR_ITCM_END,
            ch_bool(false)
        );
        
        // D-TCM 区域检测: [0x20010000, 0x2001FFFF]
        ch_bool in_dtcm = select(
            addr_in >= ADDR_DTCM_BASE,
            addr_in <= ADDR_DTCM_END,
            ch_bool(false)
        );
        
        // CLINT 区域检测: [0x40000000, 0x40000FFF]
        ch_bool in_clint = select(
            addr_in >= ADDR_CLINT_BASE,
            addr_in <= ADDR_CLINT_END,
            ch_bool(false)
        );
        
        // UART 区域检测: [0x40001000, 0x40001FFF]
        ch_bool in_uart = select(
            addr_in >= ADDR_UART_BASE,
            addr_in <= ADDR_UART_END,
            ch_bool(false)
        );
        
        // GPIO 区域检测: [0x40002000, 0x40002FFF]
        ch_bool in_gpio = select(
            addr_in >= ADDR_GPIO_BASE,
            addr_in <= ADDR_GPIO_END,
            ch_bool(false)
        );
        
        // 无效地址: 不在任何有效区域内
        ch_bool in_valid_region = select(in_itcm, ch_bool(true),
                              select(in_dtcm, ch_bool(true),
                              select(in_clint, ch_bool(true),
                              select(in_uart, ch_bool(true),
                              select(in_gpio, ch_bool(true),
                              ch_bool(false))))));
        
        ch_bool is_error = select(in_valid_region, ch_bool(false), ch_bool(true));
        
        // One-hot 片选输出: 只有匹配区域的片选为高
        io().i_tcm_cs = in_itcm;
        io().d_tcm_cs = in_dtcm;
        io().clint_cs = in_clint;
        io().uart_cs = in_uart;
        io().gpio_cs = in_gpio;
        io().error_cs = is_error;
        
        // 本地地址计算: addr - base (仅在对应区域内有效)
        // 当地址无效时，local_addr 为 0
        ch_uint<32> itcm_local = addr_in - ADDR_ITCM_BASE;
        ch_uint<32> dtcm_local = addr_in - ADDR_DTCM_BASE;
        ch_uint<32> clint_local = addr_in - ADDR_CLINT_BASE;
        ch_uint<32> uart_local = addr_in - ADDR_UART_BASE;
        ch_uint<32> gpio_local = addr_in - ADDR_GPIO_BASE;
        
        // 选择对应区域的本地地址
        io().local_addr = select(in_itcm, itcm_local,
                       select(in_dtcm, dtcm_local,
                       select(in_clint, clint_local,
                       select(in_uart, uart_local,
                       select(in_gpio, gpio_local,
                       ch_uint<32>(0_d))))));
    }
};

} // namespace riscv
