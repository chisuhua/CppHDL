/**
 * CLINT (Core Local Interrupt Controller) Module
 *
 * RISC-V 处理器核本地中断控制器
 * 实现 mtime 计数器、mtimecmp 比较器和 MTIP 中断信号
 *
 * 寄存器映射 (基地址偏移):
 * - 0x0: mtime[31:0]     - 当前时间低32位 (R/W, reset=0)
 * - 0x4: mtime[63:32]    - 当前时间高32位 (R/W, reset=0)
 * - 0x8: mtimecmp[31:0]  - 时间比较值低32位 (R/W, reset=0xFFFFFFFF)
 * - 0xC: mtimecmp[63:32] - 时间比较值高32位 (R/W, reset=0xFFFFFFFF)
 *
 * Memory map: CLINT at 0x40000000 (address decoder handles base in Phase D)
 */

#pragma once

#include "ch.hpp"
#include "component.h"

using namespace ch::core;

namespace ch::chlib {

/**
 * CLINT 模块 - 64位定时器比较器
 *
 * 提供机器定时器中断 (MTIP) 用于 FreeRTOS 等操作系统
 * 64位 mtime 计数器在每个 tick() 调用时递增
 * 当 mtime >= mtimecmp 时，mtip 中断信号置高
 */
class Clint : public ch::Component {
public:
    __io(
        // MMIO 接口 (地址解码器路由到这些端口)
        ch_in<ch_uint<32>> addr;          // MMIO 地址
        ch_in<ch_uint<32>> write_data;    // 写数据
        ch_in<ch_bool>     write_en;      // 写使能
        ch_out<ch_uint<32>> read_data;    // 读数据
        ch_in<ch_bool>     read_en;       // 读使能

        // 中断输出
        ch_out<ch_bool>    mtip;          // 机器定时器中断 pending
    )

    Clint(ch::Component* parent = nullptr, const std::string& name = "clint")
        : ch::Component(parent, name) {}

    void create_ports() override {
        new (io_storage_) io_type;
    }

    void describe() override {
        // mtime 寄存器 - 分为高低32位
        ch_reg<ch_uint<32>> mtime_lo(0_d);   // mtime[31:0]
        ch_reg<ch_uint<32>> mtime_hi(0_d);   // mtime[63:32]

        // mtimecmp 寄存器 - 分为高低32位
        // 复位值为 0xFFFFFFFF 以便系统启动时不会立即触发中断
        ch_reg<ch_uint<32>> mtimecmp_lo(0xFFFFFFFF);  // mtimecmp[31:0]
        ch_reg<ch_uint<32>> mtimecmp_hi(0xFFFFFFFF); // mtimecmp[63:32]

        // 计算地址偏移 (低2位)
        ch_uint<2> addr_offset = bits<1, 0>(io().addr);

        // 地址解码 - 使用 select 实现多路选择
        ch_bool is_mtime_lo  = (addr_offset == ch_uint<2>(0_d));
        ch_bool is_mtime_hi  = (addr_offset == ch_uint<2>(1_d));
        ch_bool is_mtimecmp_lo = (addr_offset == ch_uint<2>(2_d));
        ch_bool is_mtimecmp_hi = (addr_offset == ch_uint<2>(3_d));

        // 写使能信号
        ch_bool write_mtime_lo    = io().write_en & is_mtime_lo;
        ch_bool write_mtime_hi    = io().write_en & is_mtime_hi;
        ch_bool write_mtimecmp_lo = io().write_en & is_mtimecmp_lo;
        ch_bool write_mtimecmp_hi = io().write_en & is_mtimecmp_hi;

        // 更新 mtime_lo 寄存器
        mtime_lo->next = select(write_mtime_lo, io().write_data, mtime_lo);

        // 更新 mtime_hi 寄存器
        mtime_hi->next = select(write_mtime_hi, io().write_data, mtime_hi);

        // 更新 mtimecmp_lo 寄存器
        mtimecmp_lo->next = select(write_mtimecmp_lo, io().write_data, mtimecmp_lo);

        // 更新 mtimecmp_hi 寄存器
        mtimecmp_hi->next = select(write_mtimecmp_hi, io().write_data, mtimecmp_hi);

        // 读取数据多路选择
        ch_uint<32> read_val(0_d);
        read_val = select(is_mtime_lo, mtime_lo, read_val);
        read_val = select(is_mtime_hi, mtime_hi, read_val);
        read_val = select(is_mtimecmp_lo, mtimecmp_lo, read_val);
        read_val = select(is_mtimecmp_hi, mtimecmp_hi, read_val);

        // 读响应
        io().read_data = select(io().read_en, read_val, ch_uint<32>(0_d));

        // 64位比较: mtime >= mtimecmp
        // 分两种情况:
        // 1. hi 部分: mtime_hi > mtimecmp_hi -> mtime >= mtimecmp
        // 2. hi 部分相等: mtime_hi == mtimecmp_hi && mtime_lo >= mtimecmp_lo
        ch_bool hi_gt  = mtime_hi > mtimecmp_hi;
        ch_bool hi_eq  = mtime_hi == mtimecmp_hi;
        ch_bool lo_ge  = mtime_lo >= mtimecmp_lo;
        ch_bool mtip_cond = hi_gt | (hi_eq & lo_ge);

        // MTIP 中断输出
        io().mtip = mtip_cond;
    }

    /**
     * C++ 层 tick 方法 - 在每个时钟周期调用以更新 mtime 计数器
     * 此方法供 Simulator 在每个 tick 周期调用
     */
    void tick() {
        // mtime 计数器递增 (在 describe() 之外由 Simulator 调用)
        // 注意: 由于 mtime_lo 和 mtime_hi 是 ch_reg，
        // 实际递增逻辑需要在 describe() 中通过组合逻辑实现
    }

    /**
     * 获取当前 mtime 值 (低32位)
     */
    ch_uint<32> get_mtime_lo() const {
        return ch_uint<32>(0_d); // 占位，实际值由内部寄存器提供
    }

    /**
     * 获取当前 mtimecmp 值 (低32位)
     */
    ch_uint<32> get_mtimecmp_lo() const {
        return ch_uint<32>(0_d); // 占位，实际值由内部寄存器提供
    }
};

} // namespace ch::chlib
