// examples/spinalhdl-ported/counter/counter_simple_top.h
//
// Counter Simple 示例的**顶层组件**（三角色分离：设计定义 `*_top.h` → 激励驱动
// `tb_*.cpp` → 验证源 `tb_*.{verif,sv}`）。
//
// ⚠️ **端口 MUST 具名**（ADR-075 E1）：框架无反射 ⇒ ⇒ 未具名端口的
//   `manifest.cpp_member` 只能是 `unnamed_input` 占位 ⇒ ⇒ 验证源无法按名寻址。
//
// ⚠️ **端口面（1 根 · 根自有）**：`value`（out · 8 bit）⇒ ⇒ 加框架 2 根 = 产物端口表 3 行。
//
// ⚠️ **上游参考（SpinalHDL Scala 原文 · 期望值的推导出处）**：
//   ```scala
//   val counterReg = RegInit(U(0, 8 bits))
//   counterReg := counterReg + 1
//   io.value := counterReg
//   ```
//   ⇒ ⇒ 语义：**复位值 0 · 每拍 +1 · 模 256 回绕 · 输出 = 寄存器**。

#pragma once

#include "ch.hpp"
#include "component.h"
#include "module.h"

#include <string>

namespace counter_simple_example {

using namespace ch::core;

/// **根组件名**（单一真相源：发射 factory 与验证模式的根共用此常量）
inline constexpr const char *kCounterSimpleRootName = "top";

/// 自由运行 8 位计数器（无输入控制）
template <unsigned N = 8>
class Counter : public ch::Component {
  public:
    __io(ch_out<ch_uint<N>> value{"value"})

    Counter(ch::Component *parent = nullptr, const std::string &name = "counter")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        ch_reg<ch_uint<N>> counter_reg(0_d);
        counter_reg->next = counter_reg + 1_d;
        io().value = counter_reg;
    }
};

/// 顶层组件（**具名端口** ⇒ ⇒ 产物端口名 = `top_<C++ 成员名>`）
class CounterSimpleTop : public ch::Component {
  public:
    __io(ch_out<ch_uint<8>> value{"value"})

    CounterSimpleTop(ch::Component *parent = nullptr, const std::string &name = "counter_simple_top")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        ch::ch_module<Counter<8>> counter1{"counter1"};
        io().value <<= counter1.io().value;
    }
};

} // namespace counter_simple_example
