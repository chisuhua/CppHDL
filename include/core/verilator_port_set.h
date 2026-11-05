// include/core/verilator_port_set.h
//
// **显式端口集合**（ADR-058 §2.3 E1）：Verilator backend 的「只增不改」入口
// 消费的端口清单 —— value-only，无 IR 依赖。
//
// ⚠️ **为什么单独一个头**：
//   · `hierarchy_verilog.h`（emitter）与 `core/verilator_backend.h`（simulation）
//     都要用它 ⇒ ⇒ 放在任一侧都会让另一侧反向依赖；
//   · 本头**不** include `component.h` / `<expected>` ⇒ 平台面轻（`verilator_backend.h`
//     在 macOS / Windows 上 MUST 仍可 `#include`）。
//
// ⚠️ **集合 MUST 由 emitter 产物派生**（`ch::backend_port_set(...)`）⇒ 调用方无从
//    手写/手工列举（否则与 emitter 真相脱钩 ⇒ 表错则两错相抵）。

#pragma once

#include <cstdint>
#include <string>

namespace ch {

/// 一个端口在「外部生成的 Verilog」里的**显式描述**。
struct VerilatorPortSpec {
    std::uint32_t node_id = 0;   ///< ctx 私有节点编号（**同一 ctx** ⇒ 与 data_map 的键一致）
    std::string port_name;       ///< 产物里的**顶层端口名**（= ctx 级全局名，逐字相同）
    std::uint32_t bitwidth = 1;  ///< 位宽（`>64` ⇒ 后端合法拒绝 · clock/reset 恒 1）
    bool is_input = false;       ///< `true` = input；`false` = output
    /// 时序基础设施标记（`true` ⇒ 该端口是 clock / reset；两者互斥）
    bool is_clock = false;
    bool is_reset = false;
};

}   // namespace ch
