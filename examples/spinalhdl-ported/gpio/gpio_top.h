// examples/spinalhdl-ported/gpio/gpio_top.h
//
// GPIO 示例的**顶层组件**（三角色分离）。
//
// ⚠️ **端口 MUST 具名**（ADR-075 E1）。
// ⚠️ **端口面（5 根 · 根自有）**：`gpio_in[7:0]` · `output_enable` · `interrupt_enable` ·
//   `gpio_out[7:0]` · `interrupt` ⇒ ⇒ 加框架 2 = 产物端口表 7 行。
//
// ⚠️ **语义（impl-contract · 上游 Scala 的「移植简化」形态 · D1 裁决 2026-11-12）**：
//   · 两级同步（`sync_stage1/2`）· **单 bit 边沿检测**（`rise|fall` 归约成 1 位）
//   · `interrupt = interrupt_enable ? (edge_detect != 0) : 0`
//   · `gpio_out = output_enable ? output_reg : 0`，`output_reg->next = gpio_in`（1 拍延迟）
//   ⇒ ⇒ 与上游 Scala（逐位边沿 / 1 级同步 / 端口集不同）**不求一致**（只判 impl-contract）。

#pragma once

#include "ch.hpp"
#include "component.h"
#include "module.h"

#include <string>

namespace gpio_example {

using namespace ch::core;

/// **根组件名**（单一真相源）
inline constexpr const char *kGpioRootName = "top";

/// 8 位 GPIO（输入同步 + 中断 + 输出使能）
class GPIO : public ch::Component {
  public:
    __io(ch_in<ch_uint<8>> gpio_in{"gpio_in"};
         ch_out<ch_uint<8>> gpio_out{"gpio_out"};
         ch_in<ch_bool> output_enable{"output_enable"};
         ch_out<ch_bool> interrupt{"interrupt"};
         ch_in<ch_bool> interrupt_enable{"interrupt_enable"})

    GPIO(ch::Component *parent = nullptr, const std::string &name = "gpio")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        ch_reg<ch_uint<8>> sync_stage1(0_d);
        ch_reg<ch_uint<8>> sync_stage2(0_d);
        sync_stage1->next = io().gpio_in;
        sync_stage2->next = sync_stage1;

        auto rise = io().gpio_in & (~sync_stage2);
        auto fall = (~io().gpio_in) & sync_stage2;
        auto edge_detect = rise | fall;

        io().interrupt = select(io().interrupt_enable, (edge_detect != ch_uint<8>(0_d)),
                                ch_bool(false));

        ch_reg<ch_uint<8>> output_reg(0_d);
        io().gpio_out = select(io().output_enable, output_reg, ch_uint<8>(0_d));
        output_reg->next = io().gpio_in;
    }
};

/// 顶层组件（**具名端口**）
class GpioTop : public ch::Component {
  public:
    __io(ch_in<ch_uint<8>> gpio_in{"gpio_in"};
         ch_out<ch_uint<8>> gpio_out{"gpio_out"};
         ch_in<ch_bool> output_enable{"output_enable"};
         ch_out<ch_bool> interrupt{"interrupt"};
         ch_in<ch_bool> interrupt_enable{"interrupt_enable"})

    GpioTop(ch::Component *parent = nullptr, const std::string &name = "gpio_top")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        ch::ch_module<GPIO> gpio_inst{"gpio_inst"};
        gpio_inst.io().gpio_in <<= io().gpio_in;
        gpio_inst.io().output_enable <<= io().output_enable;
        gpio_inst.io().interrupt_enable <<= io().interrupt_enable;
        io().gpio_out <<= gpio_inst.io().gpio_out;
        io().interrupt <<= gpio_inst.io().interrupt;
    }
};

} // namespace gpio_example
