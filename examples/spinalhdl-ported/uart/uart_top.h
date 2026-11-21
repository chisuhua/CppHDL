// examples/spinalhdl-ported/uart/uart_top.h
//
// UART TX 示例的**顶层组件**（三角色分离：设计定义 `*_top.h` → 激励驱动
// `tb_*.cpp` → 验证源 `tb_*.{verif,sv}`）。
//
// ⚠️ **端口 MUST 具名**（ADR-075 E1）。
//
// ⚠️ **端口面（4 根 · 根自有）**：`frame_start`（in · 1）· `data`（in · 8）·
//   `uart_tx`（out · 1）· `busy`（out · 1）⇒ ⇒ 加框架 2 = 产物端口表 6 行。
//
// ⚠️ **本模块的写法必须遵守 3 条承重约束**（详见
//   `openspec/changes/archive/2026-10-29-uart-tx-fsm-live-logic-2026-10/design.md`）：
//  ① `if (ch_bool)` / 三元 `? :` 在**建图期**求值 ⇒ 条件逻辑 MUST 用
//     `transition_when` / `select`；
//  ② `reg->next` 是 **last-wins** ⇒ **每个寄存器 MUST 只有一个写入点**；
//  ③ `ch_out` 的 `<<=` 也是覆盖 ⇒ **每个输出 MUST 只驱动一次**。

#pragma once

#include "ch.hpp"
#include "chlib/state_machine.h"
#include "component.h"
#include "module.h"

#include <cstdint>
#include <string>

namespace uart_tx_example {

using namespace ch::core;
using namespace chlib;

/// **根组件名**（单一真相源：发射 factory 与验证模式的根共用此常量）
inline constexpr const char *kUartTxRootName = "top";

enum class UartTxState : std::uint8_t {
    IDLE,  // 空闲
    START, // 起始位（0）
    DATA,  // 8 个数据位（LSB first）
    STOP,  // 停止位（1）
};

/// UART 发送器（固定位宽 434 拍/位 · 8N1）
class UartTx : public ch::Component {
  public:
    __io(ch_in<ch_bool> frame_start{"frame_start"};
         ch_in<ch_uint<8>> data{"data"};
         ch_out<ch_bool> uart_tx{"uart_tx"};
         ch_out<ch_bool> busy{"busy"})

    UartTx(ch::Component *parent = nullptr, const std::string &name = "uart_tx")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        ch_reg<ch_uint<16>> baud_counter(0_d);
        ch_reg<ch_uint<3>> bit_counter(0_d);
        ch_reg<ch_uint<9>> shift_reg(511_d); // 空闲 = 全 1

        ch_state_machine<UartTxState, 4> sm;

        // `frame_start` 经寄存器中转（端口无 `ch_bool` 转换）⇒ 延后 1 拍
        ch_reg<ch_bool> start_latch(ch_bool(false));
        start_latch->next = io().frame_start;
        ch_bool frame_start = start_latch;

        ch_uint<8> data_v = io().data;
        ch_bool baud_done = (baud_counter == ch_uint<16>(433_d));
        ch_bool bit_last = (bit_counter == ch_uint<3>(7_d));

        sm.state(UartTxState::IDLE).on_active([&] {
            sm.transition_when(frame_start, UartTxState::START);
        });
        sm.state(UartTxState::START).on_active([&] {
            sm.transition_when(baud_done, UartTxState::DATA);
        });
        sm.state(UartTxState::DATA).on_active([&] {
            // 与 `baud_done` 合取 ⇒ 第 8 位也满一个波特周期
            sm.transition_when(bit_last & baud_done, UartTxState::STOP);
        });
        sm.state(UartTxState::STOP).on_active([&] {
            sm.transition_when(baud_done, UartTxState::IDLE);
        });

        sm.set_entry(UartTxState::IDLE);
        sm.build();

        // ── 每个寄存器/输出各**一个**写入点（约束 ② / ③）──────────────────
        sm.contrib(baud_counter, UartTxState::START,
                   select(baud_done, ch_uint<16>(0_d), baud_counter + ch_uint<16>(1_d)));
        sm.contrib(baud_counter, UartTxState::DATA,
                   select(baud_done, ch_uint<16>(0_d), baud_counter + ch_uint<16>(1_d)));
        sm.contrib(baud_counter, UartTxState::STOP,
                   select(baud_done, ch_uint<16>(0_d), baud_counter + ch_uint<16>(1_d)));
        baud_counter->next = sm.resolve_hold(baud_counter);

        sm.contrib(bit_counter, UartTxState::DATA,
                   select(baud_done,
                          select(bit_last, ch_uint<3>(0_d), bit_counter + ch_uint<3>(1_d)),
                          bit_counter));
        sm.contrib(bit_counter, UartTxState::START,
                   select(baud_done, ch_uint<3>(0_d), bit_counter));
        bit_counter->next = sm.resolve_hold(bit_counter);

        sm.contrib(shift_reg, UartTxState::DATA,
                   select(baud_done, (shift_reg >> ch_uint<9>(1_d)) | ch_uint<9>(1_d), shift_reg));
        sm.contrib(shift_reg, UartTxState::STOP,
                   select(baud_done, ch_uint<9>(511_d), shift_reg));
        sm.contrib(shift_reg, UartTxState::IDLE,
                   select(frame_start, (data_v << ch_uint<9>(1_d)) | ch_uint<9>(0_d), shift_reg));
        shift_reg->next = sm.resolve_hold(shift_reg);

        sm.contrib(io().uart_tx, UartTxState::IDLE, ch_bool(true));
        sm.contrib(io().uart_tx, UartTxState::START, ch_bool(false));
        sm.contrib(io().uart_tx, UartTxState::DATA,
                   ((shift_reg >> ch_uint<9>(1_d)) & ch_uint<9>(1_d)) != ch_uint<9>(0_d));
        sm.contrib(io().uart_tx, UartTxState::STOP, ch_bool(true));
        io().uart_tx <<= sm.resolve_or(io().uart_tx, ch_bool(true));

        sm.contrib(io().busy, UartTxState::IDLE, ch_bool(false));
        sm.contrib(io().busy, UartTxState::START, ch_bool(true));
        sm.contrib(io().busy, UartTxState::DATA, ch_bool(true));
        sm.contrib(io().busy, UartTxState::STOP, ch_bool(false));
        io().busy <<= sm.resolve_or(io().busy, ch_bool(false));
    }
};

/// 顶层组件（**具名端口** ⇒ ⇒ 产物端口名 = `top_<C++ 成员名>`）
class UartTxTop : public ch::Component {
  public:
    __io(ch_in<ch_bool> frame_start{"frame_start"};
         ch_in<ch_uint<8>> data{"data"};
         ch_out<ch_bool> uart_tx{"uart_tx"};
         ch_out<ch_bool> busy{"busy"})

    UartTxTop(ch::Component *parent = nullptr, const std::string &name = "uart_tx_top")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        ch::ch_module<UartTx> uart_tx_inst{"uart_tx_inst"};
        uart_tx_inst.io().frame_start <<= io().frame_start;
        uart_tx_inst.io().data <<= io().data;
        io().uart_tx <<= uart_tx_inst.io().uart_tx;
        io().busy <<= uart_tx_inst.io().busy;
    }
};

} // namespace uart_tx_example
