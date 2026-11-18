// include/rtl_emit/product_check.h
//
// **产物级自检的产品无关规则**（`example-file-boundary-2026-11` **F2** · 蓝图 §4.3.1）。
//
// ⚠️ **规则 vs 值**（蓝图 §4.3.1 的归属维度）：
//   · **规则**（产品无关的判定逻辑 ⇒ 住本头）= `product_modules` · `tamper_hier` ·
//    `check_products` 的 4 项判据与**编排**
//   · **值**（某个产品的期望 ⇒ 留调用方）= 期望 module 集 · 期望顶层端口数 ·
//    激励面 · 观测面 · 变异目标 · lint 的 `--top-module` 名 · 最小变化观测量阈值
//
// ⚠️ **库层 MUST NOT 内置产品值**（蓝图 §4.3.1）：
//   · MUST NOT 出现字面根名（`"top"`）—— 根名**可配** ⇒ ⇒ 硬编码即错 ⇒ ⇒
//    任何根名不同的产品会把根自有点误算成后代未驱动 ⇒ **假红**
//   · MUST NOT 内置 `>= 2` 之类阈值 —— 一切阈值**参数传入**
//   ⇒ ⇒ owner 基准由**建出来的根组件**派生（`Component::name()`）⇒ ⇒ 单一真相源；
//    lint 的 `--top-module` 名由调用方传入（它是**产物**的 module 名，
//    **不等于**设计根名 —— 见 `ch::HierarchyEmitOptions::top_module_name`）。
//
// ⚠️ **编排归属**（蓝图 §4.3.1 钉死）：4 项判据的**顺序** · `failures` 累加 ·
//   `eq/` 目录产出 · `CHECK 1..4` 与 `PRODUCTS-CHECK` 结论行 · 退出码 0/1
//   ⇒ ⇒ **全部在本实现内** ⇒ ⇒ 调用方**不得**另写一份编排。
//
// ⚠️ **lint 的执行属平台细节**：本头**不**跑进程（POSIX `fork`/`exec` 不是可移植 API）
//   ⇒ ⇒ 调用方提供一个 `lint` 回调（跑 linter ⇒ 返回「是否干净」）⇒ ⇒
//   本层负责**判定与计数**，调用方负责**工具发现与执行**
//   （蓝图 §4.3.1：`find_verilator` 属「环境依赖 ⇒ 不下沉」）。
//   ⇒ ⇒ **回调缺失 ⇒ 按失败计**（fail-closed）⇒ ⇒ 「找不到 linter」不再是常态。

#pragma once

#include "component.h"
#include "core/context.h"
#include "hierarchy_equivalence.h"
#include "hierarchy_verilog.h"
#include "rtl_emit/cli.h"
#include "rtl_emit/manifest.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <set>
#include <string>
#include <vector>

namespace ch {
namespace rtl_emit {

/// 产物里出现的 module 名集合（**从产物派生** ⇒ 禁手写）。
///
/// 判定口径 = 每行**行首**是 `module ` ⇒ 取其后第一个空白前的标识符。
std::set<std::string> product_modules(const std::string &verilog_path);

/// **变异自证**（判别力自证）：把层次化产物里「某条被观测信号的顶层端口连接」取反
/// ⇒ ⇒ 产出一份篡改副本 ⇒ ⇒ 再比对一次 ⇒ ⇒ 若**仍相等** ⇒ ⇒ 判据恒绿 ⇒ 必须判红。
///
/// @param hier_path    层次化产物路径（读它 ⇒ 写 `<path>.tampered`）
/// @param signal_map   层次化发射给出的信号映射表（`ch::SignalMapEntry`）
/// @param observations 被观测的顶层输出（**目标由它决定** ⇒ ⇒ 属「值」侧）
/// @param top_module   顶层 module 名（用于定位 `assign <port> = ...` 段）
/// @return 篡改副本路径；**空串** = 找不到可取反的连接（⇒ 判据无判别力 ⇒ 调用方须判红）
std::string tamper_hier(const std::string &hier_path,
                        const std::vector<ch::SignalMapEntry> &signal_map,
                        const std::vector<ch::hier_eq::Observation> &observations,
                        const std::string &top_module);

/// 产品级自检的**值**（全部由调用方给 ⇒ 库层无字面根名 / 无阈值）。
struct ProductCheckSpec {
    /// 被检查的产物（**层级化**产物）路径 —— CHECK 1 与 CHECK 3 都读它。
    std::string verilog_path;
    /// 工作目录 —— `eq/`（双形态比对的产物与篡改副本）写在这里。
    std::string work_dir;
    /// 期望的 module 集（**值**）—— CHECK 1 判「集合相等」。
    std::vector<std::string> expected_modules;
    /// 期望的顶层端口数（**值**）—— CHECK 2 判「端口数相等」。
    std::size_t expected_top_ports = 0;
    /// 最小「随激励变化的观测量」个数（**值**）—— CHECK 4 判「变化量 >= 此值」。
    std::size_t min_varying_observations = 0;
    /// lint 的 `--top-module` 名（**值**）—— 它是**产物**的顶层 module 名
    /// （默认 `ch::HierarchyEmitOptions::top_module_name`）⇒ ⇒ **不等于**设计根名。
    std::string lint_top_module;
    /// 跑 linter（`--lint-only`）⇒ 返回「是否干净」—— **调用方执行**（平台细节）。
    /// ⚠️ **缺失 ⇒ CHECK 3 按失败计**（fail-closed）⇒ ⇒ 不得静默跳过。
    std::function<bool(const std::string &verilog_path)> lint;
};

/// 产品专属的**激励面**（**值** ⇒ 留调用方）。
struct ProductCheckHooks {
    /// 顶层受驱动输入（顺序 MUST 与 `make_stimulus` 每行的取值顺序**逐字一致**）。
    std::function<std::vector<hier_eq::DrivenInput>(Component &)> make_inputs;
    /// 顶层被观测输出（★ 只能取**根**的 `ch_out`）。
    std::function<std::vector<hier_eq::Observation>(Component &)> make_observations;
    /// 逐拍激励（每行一拍 ⇒ ⇒ MUST 非空）。
    std::function<std::vector<std::vector<std::uint64_t>>()> make_stimulus;
};

/// 跑 4 项产物级自检（**规则 + 编排**）并打印结论行。
///
/// 4 项（顺序固定）：
///   ① 产物 module 集 == `spec.expected_modules`
///   ② 顶层端口数 == `spec.expected_top_ports` **且** 后代未驱动端口 == 0
///   ③ lint 干净（`spec.lint` 回调）
///   ④ 双形态逐拍等价 + 变异自证（篡改必须被抓住）
///
/// @param factory 设计工厂（与 `run_emit_cli` 的 `DesignFactory` 同型）—— 本层用它
///                **重建设计**（CHECK 2 的端口面与 CHECK 4 的比对都需要它）。
/// @return **0 = 全部通过；1 = 有失败项**（★ 只这两种 ⇒ 用法错误也按失败计 ⇒ fail-closed）
int check_products(const ProductCheckSpec &spec, const ProductCheckHooks &hooks,
                   const DesignFactory &factory);

}   // namespace rtl_emit
}   // namespace ch
