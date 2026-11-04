// include/hierarchy_verilog.h
//
// **层次化 RTL 输出**的公开入口（ADR-058 §2.3：独立新入口，既有 `toVerilog()` 一字不动）。
//
// 产物形态：**一个文件、多个 module**（子 module 在前、根 module 在后）
// ⇒ ⇒ `verilator --top-module <root_module>` 可直接吃。
//
// ⚠️ 判据纪律（本段最重要的一条）：**校验 MUST 在第一次 I/O 之前完成**
//    ⇒ ⇒ 失败时**不留下任何新文件**；写出走「临时文件 → 成功后 rename」。
// ⚠️ 失败 MUST **可被调用方观测**（返回 `HierarchyEmitResult`）⇒ MUST NOT 只打日志。
//    既有的 `toVerilog()` 用 `catch(...)` + 日志（那是既有行为，不改）；新入口不用它。

#pragma once

#include "component.h"

#include <string>
#include <vector>

namespace ch {

/// 跨层非端口边的处理策略（ADR-060 §2.1 / §2.2）。
enum class HierarchyPortPolicy {
    /// 合成隐式端口 + `CHWARN`（默认）
    SynthesizeWarn,
    /// 合成隐式端口 + `CHWARN`，但**已列为拒绝**（保留枚举值 ⇒ 与 ADR-060 的线性枚举一致）
    SynthesizeFatal,
    /// 拒绝（抛）
    Reject,
};

/// 悬空子输入的处理策略（ADR-060 §2.2）。
enum class HierarchyUndrivenPolicy {
    /// 逐级冒泡成顶层 input + `CHWARN`（默认）
    Bubble,
    /// 拒绝
    Reject,
};

struct HierarchyEmitOptions {
    HierarchyPortPolicy port_policy = HierarchyPortPolicy::SynthesizeWarn;
    HierarchyUndrivenPolicy undriven_policy = HierarchyUndrivenPolicy::Bubble;
    /// 根 module 的名字（默认 `top`，与扁平产物同名 ⇒ 便于 A/B 比对）
    std::string top_module_name = "top";
};

struct HierarchyEmitResult {
    bool ok = false;
    /// 失败原因（`ok == false` 时非空；MUST NOT 只打日志）
    std::string error;
    /// 写出的文件（`ok == true` 时恰一个）
    std::vector<std::string> files;
    /// 分析期告警（隐式端口 / 冒泡 / 归属空洞 ……）
    std::vector<std::string> warnings;
};

/// 把 `root` 所在的层次发射成 Verilog（多 module）。
///
/// ⚠️ 前置校验与既有 `toVerilog()` 相同的那几条（已 build / 已 `add_child` / 成层组件
/// 不自驱动 `ch_in` / 跨模块 `ch_mem`）⇒ ⇒ 任何一条不满足 ⇒ `ok=false` + `error`
/// 且**不产生任何文件**。
HierarchyEmitResult toVerilogHierarchical(const std::string &out_path, Component &root,
                                          const HierarchyEmitOptions &options = {});

}   // namespace ch
