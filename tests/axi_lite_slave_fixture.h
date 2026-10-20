// tests/axi_lite_slave_fixture.h
//
// AXI4-Lite **主机侧**驱动辅助（协议合规 master）。
// 供 tests/test_axi_{uart,timer,gpio}.cpp 共用。
//
// 契约（对齐 fix-axi4-peripheral-rst-n-port-2026-10 确立的规范形态）：
//   * AWVALID 保持到 AW 握手完成
//   * WVALID 保持到 W 握手完成（AW 与 W 可分两周期 —— 相位分离）
//   * BVALID 必须自己出现（不被 BREADY 触发），master 看到后才拉 BREADY
//   * RVALID 保持到 RREADY
//
// 时序模型（CppHDL Simulator）:
//   `sim.tick()` = "组合求值 → 时钟沿更新寄存器 → 再组合求值"。
//   时钟沿采样的是**本次 tick 开头组合求值**出来的值（即当前周期的值），
//   tick 结束后读到的端口值 = **下一周期**的值。
//   ⇒ 组合信号（awready/wready/bvalid/rvalid）只在 tick 之间可观测，
//     且一旦握手完成就会被 busy / 响应寄存器改写。
//   ⇒ 因此本文件用**事件驱动**而非"等 ready 变 1"来同步：
//     保持 VALID 信号并 tick，直到观察到响应信号（BVALID / RVALID）出现。

#pragma once

#include "catch_amalgamated.hpp"
#include "simulator.h"

#include <cstdint>

namespace ch_test::axi_lite {

// 读取端口当前值
template <typename Port>
inline uint64_t pv(ch::Simulator &sim, const Port &port) {
    return static_cast<uint64_t>(sim.get_port_value(port));
}

// 等待组合信号出现（期间保持调用方的输入不变）
template <typename Port>
inline bool wait_for(ch::Simulator &sim, const Port &port, int max_cycles = 32) {
    for (int i = 0; i < max_cycles; ++i) {
        if (pv(sim, port) == 1) {
            return true;
        }
        sim.tick();
    }
    return pv(sim, port) == 1;
}

// AXI4-Lite 写事务（io 级核心）。
//
// separate_aw_w = false: AW 与 W 同周期（master 常见的紧凑写法）
// separate_aw_w = true : AW 与 W 分两周期（AXI4-Lite 允许，用于验证相位分离）
//
// wstrb = 0xF（默认）: 整字写；其他值表示**字节道掩码**（bit0 → byte0 …
// bit3 → byte3）—— AXI4-Lite 要求 master 始终驱动 WSTRB。
//
// ⚠️ 协议合规要点：AWVALID / WVALID 各自**只**保持到对应的握手完成。
// 若 master 在 AW 握手后仍持有 AWVALID，slave 会接受**第二笔** AW 事务
// 并等待它的 WDATA —— 而 master 若此时撤掉 AWVALID，该笔事务永远等不到 WDATA，
// busy 永久卡住（后续所有 AXI 事务阻塞）。
template <typename Io>
inline void write_io(ch::Simulator &sim, Io &io, uint32_t addr, uint32_t data,
                      bool separate_aw_w = false, uint8_t wstrb = 0xF) {
    // --- AW 阶段 ---
    sim.set_input_value(io.awaddr, addr);
    sim.set_input_value(io.awprot, 0);
    sim.set_input_value(io.awvalid, 1);
    sim.set_input_value(io.wdata, data);
    sim.set_input_value(io.wstrb, wstrb);
    sim.set_input_value(io.wvalid, separate_aw_w ? 0 : 1);
    sim.tick();  // AW 握手发生在本次时钟沿
    sim.set_input_value(io.awvalid, 0);  // 握手完成即撤（否则会开出第二笔事务）

    if (separate_aw_w) {
        // AW 与 W 分离：master 在 AW 之后（下一周期）才送 W
        sim.set_input_value(io.wvalid, 1);
        sim.tick();  // W 握手发生在本次时钟沿
    }

    // --- W 阶段 ---
    // 保持 WVALID 直到观察到 BVALID：BVALID 只在 WDATA 被接受之后才拉高，
    // 因此观察到 BVALID 即证明 W 已被接受（且 BVALID 会保持到 BREADY）。
    const bool b_seen = wait_for(sim, io.bvalid);
    REQUIRE(b_seen);
    sim.set_input_value(io.wvalid, 0);

    // --- B 阶段：拉 BREADY 完成写响应握手（并释放 busy）---
    sim.set_input_value(io.bready, 1);
    sim.tick();
    sim.set_input_value(io.bready, 0);
}

// AXI4-Lite 读事务（io 级核心），返回读到的数据
template <typename Io>
inline uint32_t read_io(ch::Simulator &sim, Io &io, uint32_t addr) {
    // --- AR 阶段 ---
    sim.set_input_value(io.araddr, addr);
    sim.set_input_value(io.arprot, 0);
    sim.set_input_value(io.arvalid, 1);
    sim.tick();  // AR 握手发生在本次时钟沿
    sim.set_input_value(io.arvalid, 0);

    // --- R 阶段：RVALID 保持到 RREADY ---
    const bool r_seen = wait_for(sim, io.rvalid);
    REQUIRE(r_seen);
    const uint32_t rdata = static_cast<uint32_t>(pv(sim, io.rdata));

    sim.set_input_value(io.rready, 1);
    sim.tick();  // R 握手 → 释放 busy
    sim.set_input_value(io.rready, 0);
    return rdata;
}

// AXI4-Lite 写事务（device 级）
template <typename Dev>
inline void write(ch::Simulator &sim, Dev &dev, uint32_t addr, uint32_t data,
                  bool separate_aw_w = false, uint8_t wstrb = 0xF) {
    write_io(sim, dev.instance().io(), addr, data, separate_aw_w, wstrb);
}

// AXI4-Lite 读事务（device 级），返回读到的数据
template <typename Dev>
inline uint32_t read(ch::Simulator &sim, Dev &dev, uint32_t addr) {
    return read_io(sim, dev.instance().io(), addr);
}

} // namespace ch_test::axi_lite
