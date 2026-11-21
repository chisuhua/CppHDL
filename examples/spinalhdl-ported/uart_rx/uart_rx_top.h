// examples/spinalhdl-ported/uart_rx/uart_rx_top.h
//
// UART RX 示例的**顶层组件**（三角色分离：设计定义 `*_top.h` → 激励驱动
// `tb_*.cpp` → 验证源 `tb_*.{verif,sv}`）。
//
// ⚠️ **端口 MUST 具名**（ADR-075 E1）。
//
// ⚠️ **端口面（4 根 · 根自有）**：`rx`（in · 1）· `prescale`（in · 16）·
//   `data`（out · 8）· `valid`（out · 1）⇒ ⇒ 加框架 2 = 产物端口表 6 行。
//
// ⚠️ **两个已修缺陷的判据出处**（2026-11-12 · D6 / D7）：
//   · **D6** 采样点 MUST 按 `prescale` 缩放（`div == prescale/2`）⇒ 不是硬编码 `div == 4`；
//   · **D7** 线上 **LSB first** ⇒ 首个采样位 MUST 落在 `bit0`（右移 + 新位进 MSB）。
//   ⇒ ⇒ 判据 = `tb_uart_rx_example.verif` 的采样相位夹逼 + 非回文字节（0x96 / 0x69 / 0x4B）。

#pragma once

#include "ch.hpp"
#include "component.h"
#include "module.h"

#include <string>

namespace uart_rx_example {

using namespace ch::core;

/// **根组件名**（单一真相源：发射 factory 与验证模式的根共用此常量）
inline constexpr const char *kUartRxRootName = "top";

/// UART 接收器（8N1 · 中点采样 · `prescale` 可配）
class UartRx : public ch::Component {
  public:
    __io(ch_in<ch_bool> rx{"rx"};
         ch_in<ch_uint<16>> prescale{"prescale"};
         ch_out<ch_uint<8>> data{"data"};
         ch_out<ch_bool> valid{"valid"})

    UartRx(ch::Component *parent = nullptr, const std::string &name = "uart_rx")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        ch_reg<ch_bool> idle(ch_bool(true));
        ch_reg<ch_uint<8>> shift(0_d);
        ch_reg<ch_uint<4>> cnt(0_d);
        ch_reg<ch_uint<16>> div(0_d);
        ch_reg<ch_bool> rx_d(ch_bool(true));

        rx_d->next = io().rx;

        // 起始位检测：下降沿（rx_d 高 且 rx 低）
        auto start = idle & rx_d & (io().rx == ch_bool(false));

        // 采样点：**每位中间**（按 prescale 缩放 · D6）
        ch_uint<16> prescale_v = io().prescale;
        auto sample_point = (div == (prescale_v >> ch_uint<16>(1_d)));

        auto div_done = (div == io().prescale);
        div->next = select(start, ch_uint<16>(0_d),
                           select(div_done, ch_uint<16>(0_d), div + ch_uint<16>(1_d)));

        auto cnt_done = (cnt == ch_uint<4>(8_d));
        auto cnt_inc = cnt + ch_uint<4>(1_d);
        auto not_idle = (idle == ch_bool(false));

        auto sample = not_idle & sample_point;
        auto sample_and_not_done = sample & (!cnt_done);
        auto next_cnt = select(start, ch_uint<4>(0_d),
                               select(sample_and_not_done, cnt_inc, cnt));
        cnt->next = next_cnt;

        // 移位：**右移 + 新位进 MSB**（D7 ⇒ 首个采样位落在 bit0）
        auto rx_bit = select(io().rx, ch_uint<8>(1_d), ch_uint<8>(0_d));
        auto shifted = shift >> ch_uint<8>(1_d);
        auto new_shift = shifted | (rx_bit << ch_uint<8>(7_d));
        auto cnt_ge_1 = (cnt != ch_uint<4>(0_d));
        shift->next = select(start, ch_uint<8>(0_d),
                             select(sample & cnt_ge_1, new_shift, shift));

        auto done = sample & cnt_done;
        idle->next = select(start, ch_bool(false), select(done, ch_bool(true), idle));

        io().data = shift;

        // `valid` 延迟一周期（单拍脉冲）
        ch_reg<ch_bool> valid_reg(ch_bool(false));
        valid_reg->next = done;
        io().valid = valid_reg;
    }
};

/// 顶层组件（**具名端口** ⇒ ⇒ 产物端口名 = `top_<C++ 成员名>`）
class UartRxTop : public ch::Component {
  public:
    __io(ch_in<ch_bool> rx{"rx"};
         ch_in<ch_uint<16>> prescale{"prescale"};
         ch_out<ch_uint<8>> data{"data"};
         ch_out<ch_bool> valid{"valid"})

    UartRxTop(ch::Component *parent = nullptr, const std::string &name = "uart_rx_top")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        ch::ch_module<UartRx> u{"u"};
        u.io().rx <<= io().rx;
        u.io().prescale <<= io().prescale;
        io().data <<= u.io().data;
        io().valid <<= u.io().valid;
    }
};

} // namespace uart_rx_example
