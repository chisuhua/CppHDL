// examples/spinalhdl-ported/counter/counter_top.h
//
// Counter 示例的**顶层组件**（三角色分离：设计定义 `*_top.h` → 激励驱动
// `tb_*.cpp` → 验证源 `tb_*.{verif,sv}`）。
//
// ⚠️ **端口 MUST 具名**（ADR-075 E1）：框架无反射 ⇒ ⇒ 未具名端口的
//   `manifest.cpp_member` 只能是 `unnamed_input` 占位 ⇒ ⇒ 验证源无法按名寻址。
//
// ⚠️ **端口面（3 根 · 根自有）**：`enable`（in · 1 bit）· `clear`（in · 1 bit）·
//   `value`（out · 8 bit）⇒ ⇒ 加框架 2 根 = 产物端口表 5 行。
//
// ⚠️ **上游参考（SpinalHDL Scala 原文 · 期望值的推导出处）**：
//   ```scala
//   val counterReg = RegInit(U(0, 8 bits))
//   when(io.clear) {
//     counterReg := 0
//   } elsewhen(io.enable) {
//     counterReg := counterReg + 1
//   }
//   io.value := counterReg
//   ```
//   ⇒ ⇒ 语义：**clear 优先**（**同步**优先级，不是异步清零）；否则 enable ⇒ +1；
//   否则**保持**。

#pragma once

#include "ch.hpp"
#include "component.h"
#include "module.h"

#include <string>

namespace counter_example {

using namespace ch::core;

/// **根组件名**（单一真相源：发射 factory 与验证模式的根共用此常量）
inline constexpr const char *kCounterRootName = "top";

/// 带 enable / clear 的 8 位计数器
template <unsigned N = 8>
class Counter : public ch::Component {
  public:
    __io(ch_in<ch_bool> enable{"enable"};
         ch_in<ch_bool> clear{"clear"};
         ch_out<ch_uint<N>> value{"value"})

    Counter(ch::Component *parent = nullptr, const std::string &name = "counter")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        ch_reg<ch_uint<N>> counter_reg(0_d);
        counter_reg->next = select(io().clear,
                                   ch_uint<N>(0_d),
                                   select(io().enable,
                                          counter_reg + ch_uint<N>(1_d),
                                          counter_reg));
        io().value = counter_reg;
    }
};

/// 顶层组件（**具名端口** ⇒ ⇒ 产物端口名 = `top_<C++ 成员名>`）
class CounterTop : public ch::Component {
  public:
    __io(ch_in<ch_bool> enable{"enable"};
         ch_in<ch_bool> clear{"clear"};
         ch_out<ch_uint<8>> value{"value"})

    CounterTop(ch::Component *parent = nullptr, const std::string &name = "counter_top")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        ch::ch_module<Counter<8>> counter1{"counter1"};
        counter1.io().enable <<= io().enable;
        counter1.io().clear <<= io().clear;
        io().value <<= counter1.io().value;
    }
};

} // namespace counter_example
