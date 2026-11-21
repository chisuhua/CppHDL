// examples/spinalhdl-ported/pwm/pwm_top.h
//
// PWM 示例的**顶层组件**（三角色分离：设计定义 `*_top.h` → 激励驱动 `tb_*.cpp`
// → 激励源 `tb_*.{verif,sv}`）。
//
// ⚠️ **端口 MUST 具名**（ADR-075 E1）：框架无反射 ⇒ ⇒ 未具名端口的
//   `manifest.cpp_member` 只能是 `unnamed_input` 占位 ⇒ ⇒ 验证源无法按名寻址。
//
// ⚠️ **端口面（3 根 · 根自有）**：`enable`（in · 1 bit）· `duty_cycle`（in · 8 bit）·
//   `pwm_out`（out · 1 bit）⇒ ⇒ 加框架 2 根（clock/reset）= 产物端口表 5 行。
//
// ⚠️ **上游参考（SpinalHDL Scala 原文 · 期望值的推导出处）**：
//   ```scala
//   val counter = Reg(UInt(8 bits)) init(0)
//   counter := counter + 1
//   io.pwm = (counter < io.value) & io.enable
//   ```
//   ⇒ ⇒ 语义：**`pwm = enable ∧ (counter < duty)`**；counter **自由运行**（模 256）。
//   ⇒ ⇒ ⇒ 因此「高电平段长度 == duty」是 **spec-derived**（不是实现细节）。
//
// ⚠️ **两级同步器不需要**：PWM 的计数器是**自由运行**的 ⇒ ⇒ 它的相位**不可控**
//   ⇒ ⇒ 验证源 MUST 用**事件锚定**（`WAIT`）而不是「按绝对拍号」⇒ ⇒ 见
//   `tb_pwm_example.verif` 的场景 1（锚到 PWM 上升沿 ⇒ ⇒ 段长确定）。

#pragma once

#include "ch.hpp"
#include "chlib/arithmetic.h"
#include "chlib/combinational.h"
#include "component.h"
#include "module.h"

#include <string>

namespace pwm_example {

using namespace ch::core; // ch_uint / ch_bool / ch_in / ch_out / _d 等

/// **根组件名**（单一真相源：发射 factory 与验证模式的根共用此常量）
inline constexpr const char *kPwmRootName = "top";

/// PWM 发生器（8 位占空比 · 自由运行计数器）
template <unsigned WIDTH = 8>
class PWM : public ch::Component {
  public:
    __io(ch_in<ch_bool> enable{"enable"};
         ch_in<ch_uint<WIDTH>> duty_cycle{"duty_cycle"};
         ch_out<ch_bool> pwm_out{"pwm_out"})

    PWM(ch::Component *parent = nullptr, const std::string &name = "pwm")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        // 8 位计数器（0..255 自由运行 ⇒ 模 256 回绕）
        ch_reg<ch_uint<WIDTH>> counter(0_d);
        counter->next = counter + ch_uint<WIDTH>(1_d);

        // `counter < duty_cycle` ⇒ 占空比比较
        const auto less_than = (counter < io().duty_cycle);

        // PWM 输出：使能门控（`enable=0` ⇒ 恒 0）
        io().pwm_out = select(io().enable, less_than, ch_bool(false));
    }
};

/// 顶层组件（**具名端口** ⇒ ⇒ 产物端口名 = `top_<C++ 成员名>`）
class PwmTop : public ch::Component {
  public:
    __io(ch_in<ch_bool> enable{"enable"};
         ch_in<ch_uint<8>> duty_cycle{"duty_cycle"};
         ch_out<ch_bool> pwm_out{"pwm_out"})

    PwmTop(ch::Component *parent = nullptr, const std::string &name = "pwm_top")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        ch::ch_module<PWM<8>> pwm_inst{"pwm_inst"};

        pwm_inst.io().enable <<= io().enable;
        pwm_inst.io().duty_cycle <<= io().duty_cycle;
        io().pwm_out <<= pwm_inst.io().pwm_out;
    }
};

} // namespace pwm_example
