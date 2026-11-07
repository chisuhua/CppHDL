// tests/hier_equivalence.h
//
// **层次化 ≡ 扁平 逐拍等价 harness 的仓内薄包装**（P8 公共化后的 alias 层）。
//
// ⚠️ **历史**：本文件曾是 T3-005 的 harness 本体（内部实现 `ch_test::hier::impl` +
// 薄包装 `ch_test::hier` 两层都在 `tests/`）。P8 把公共面搬到了
// `include/hierarchy_equivalence.h`（命名空间 `ch::hier_eq`）⇒ ⇒ 本文件**降级**为
// 纯 alias 层 ⇒ ⇒ ⇒ 既有的 `ch_test::hier::*` 调用点**零改动**。
//
// ⚠️ **公共 API 面**：`include/hierarchy_equivalence.h`（capability
//   `hierarchical-equivalence-public-helper`）⇒ ⇒ 本文件**不**再是「harness 的家」
//   ⇒ ⇒ ⇒ 它只是**兼容旧调用点的转发层**。
//
// ⚠️ **比较的是两份 Verilog**（扁平 vs 层次化）⇒ ⇒ **不是**「C++ 解释器 vs Verilog」
//   ⇒ ⇒ 双后端 parity 抓不到这一族缺陷（两后端由**同一份 C++** 生成 ⇒ 同构缺陷会被盖章）。
//
// ⚠️ **观测面由调用方手写**（ADR-058 §2.2 ②：`O` MUST 显式声明 · MUST 非空 · MUST 至少含一个
//    **随激励变化**的信号）⇒ ⇒ harness **不猜**观测面（猜 = 回到「全部输出」的空洞判据，
//    且会踩「子输出在层次化顶层不可观测」的盲区 ⇒ ADR-058 §4 风险 6）。
//    ⇒ ⇒ **且** 公共 helper 现在会**机械拒绝**退化观测面（前缀 `no-varying-signal` /
//    `empty-observations` / `empty-stimulus` / `observation-not-visible`）。
//
// ⚠️ **激励施加 MUST 用 `set_input_value(port, <纯整数>)`**：`<<=` 会把顶层端口变成
//    「有内部驱动」⇒ 该端口不再是顶层端口 ⇒ 两侧观测面不一致 ⇒ **假分歧**。

#pragma once

#include "hierarchy_equivalence.h"

namespace ch_test::hier {

using Observation = ch::hier_eq::Observation;
using DrivenInput = ch::hier_eq::DrivenInput;
using ProductSet = ch::hier_eq::ProductSet;
using Result = ch::hier_eq::Result;

/// ① 生成层次化产物 + 派生端口集合。
using ch::hier_eq::emit;

/// ② 逐拍比对（返回 value-only 的结论）。
using ch::hier_eq::compare;

/// 观测信号的工厂（★ 用户 MUST NOT 手填 `node_id`）。
using ch::hier_eq::make_observation;

}  // namespace ch_test::hier
