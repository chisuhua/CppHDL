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
 * | I-TCM  | 0x80000000      | 64KB   | i_tcm_cs   |
 * | D-TCM  | 0x80010000      | 64KB   | d_tcm_cs   |
 * | CLINT  | 0x80100000      | 4KB    | clint_cs   |
 * | UART   | 0x80101000      | 4KB    | uart_cs    |
 * | GPIO   | 0x80102000      | 4KB    | gpio_cs    |
 */

#pragma once
#include <cstdint>
#include "ch.hpp"
#include "component.h"

using namespace ch::core;

namespace riscv {

// 地址空间基地址常量 (使用原生 uint32_t 避免 ch_uint constexpr 问题)
// riscv-tests ELF 期望: ITCM=0x80000000, DTCM=0x80010000, peripherals above
constexpr uint32_t ADDR_NULL_BASE  = 0x00000000;
constexpr uint32_t ADDR_ITCM_BASE  = 0x80000000;
constexpr uint32_t ADDR_DTCM_BASE  = 0x80010000;
constexpr uint32_t ADDR_CLINT_BASE = 0x80100000;
constexpr uint32_t ADDR_UART_BASE  = 0x80101000;
constexpr uint32_t ADDR_GPIO_BASE  = 0x80102000;
constexpr uint32_t ADDR_ITCM_SIZE  = 0x00010000;
constexpr uint32_t ADDR_DTCM_SIZE  = 0x00010000;
constexpr uint32_t ADDR_CLINT_SIZE = 0x00001000;
constexpr uint32_t ADDR_UART_SIZE  = 0x00001000;
constexpr uint32_t ADDR_GPIO_SIZE  = 0x00001000;
constexpr uint32_t ADDR_ITCM_END   = ADDR_ITCM_BASE + ADDR_ITCM_SIZE - 1;
constexpr uint32_t ADDR_DTCM_END   = ADDR_DTCM_BASE + ADDR_DTCM_SIZE - 1;
constexpr uint32_t ADDR_CLINT_END  = ADDR_CLINT_BASE + ADDR_CLINT_SIZE - 1;
constexpr uint32_t ADDR_UART_END   = ADDR_UART_BASE + ADDR_UART_SIZE - 1;
constexpr uint32_t ADDR_GPIO_END   = ADDR_GPIO_BASE + ADDR_GPIO_SIZE - 1;

class AddressDecoder : public ch::Component {
public:
    __io(
        ch_in<ch_uint<32>> addr;
        ch_out<ch_bool>    i_tcm_cs;
        ch_out<ch_bool>    d_tcm_cs;
        ch_out<ch_bool>    clint_cs;
        ch_out<ch_bool>    uart_cs;
        ch_out<ch_bool>    gpio_cs;
        ch_out<ch_bool>    error_cs;
        ch_out<ch_uint<32>> local_addr;
    )

    AddressDecoder(ch::Component* parent = nullptr, const std::string& name = "address_decoder")
        : ch::Component(parent, name) {}

    void create_ports() override {
        new (io_storage_) io_type;
    }

    void describe() override {
        ch_uint<32> addr_in = io().addr;

        auto itcm_base  = ch_uint<32>(ADDR_ITCM_BASE);
        auto dtcm_base  = ch_uint<32>(ADDR_DTCM_BASE);
        auto clint_base = ch_uint<32>(ADDR_CLINT_BASE);
        auto uart_base  = ch_uint<32>(ADDR_UART_BASE);
        auto gpio_base  = ch_uint<32>(ADDR_GPIO_BASE);

        auto itcm_end  = ch_uint<32>(ADDR_ITCM_END);
        auto dtcm_end  = ch_uint<32>(ADDR_DTCM_END);
        auto clint_end = ch_uint<32>(ADDR_CLINT_END);
        auto uart_end  = ch_uint<32>(ADDR_UART_END);
        auto gpio_end  = ch_uint<32>(ADDR_GPIO_END);

        ch_bool in_itcm = select(addr_in >= itcm_base, addr_in <= itcm_end, ch_bool(false));
        ch_bool in_dtcm = select(addr_in >= dtcm_base, addr_in <= dtcm_end, ch_bool(false));
        ch_bool in_clint = select(addr_in >= clint_base, addr_in <= clint_end, ch_bool(false));
        ch_bool in_uart = select(addr_in >= uart_base, addr_in <= uart_end, ch_bool(false));
        ch_bool in_gpio = select(addr_in >= gpio_base, addr_in <= gpio_end, ch_bool(false));

        ch_bool in_valid_region = select(in_itcm, ch_bool(true),
                              select(in_dtcm, ch_bool(true),
                              select(in_clint, ch_bool(true),
                              select(in_uart, ch_bool(true),
                              select(in_gpio, ch_bool(true),
                              ch_bool(false))))));

        io().i_tcm_cs = in_itcm;
        io().d_tcm_cs = in_dtcm;
        io().clint_cs = in_clint;
        io().uart_cs = in_uart;
        io().gpio_cs = in_gpio;
        io().error_cs = select(in_valid_region, ch_bool(false), ch_bool(true));

        ch_uint<32> itcm_local = addr_in - itcm_base;
        ch_uint<32> dtcm_local = addr_in - dtcm_base;
        ch_uint<32> clint_local = addr_in - clint_base;
        ch_uint<32> uart_local = addr_in - uart_base;
        ch_uint<32> gpio_local = addr_in - gpio_base;

        io().local_addr = select(in_itcm, itcm_local,
                       select(in_dtcm, dtcm_local,
                       select(in_clint, clint_local,
                       select(in_uart, uart_local,
                       select(in_gpio, gpio_local,
                       ch_uint<32>(0_d))))));
    }
};

} // namespace riscv
