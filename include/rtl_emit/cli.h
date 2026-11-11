// include/rtl_emit/cli.h
//
// RTL 发射的**薄 CLI**（`rtl-emit-product-tooling` **C1**）。
//
// ⚠️ **形态 = 「库 + 薄 CLI」**：用户在自己的可执行文件里**登记**自己的设计
//   ⇒ ⇒ 调用 `run_emit_cli()` ⇒ ⇒ 退出码 / 失败语义**全仓一致**
//   ⇒ ⇒ 本仓的 `tools/cpphdl-emit-rtl` 只是**一个使用者**（+ 自测载体）。
//
// ⚠️ **本仓二进制 MUST NOT 链 `examples/**`**（C1 硬要求）⇒ ⇒
//   `tools/cpphdl-emit-rtl` 用**自己的**小设计 ⇒ ⇒ 工具**不依赖** examples。
//
// ⚠️ **退出码（fail-closed）**：
//   · **0** = 成功（manifest + 产物都写出，且**无诊断**）
//   · **1** = 运行失败（发射/写盘失败，或**存在诊断** ⇒ ⇒ 悬空输入是**缺陷**
//     ⇒ ⇒ 绝不让它静默通过）
//   · **2** = **用法错误**（参数错 / 文件打不开 / 设计注册失败）
//   ⇒ ⇒ **任何异常都不得返回 0**（fail-closed ⇒ ⇒ 不静默跳过）。

#pragma once

#include "component.h"
#include "core/context.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace ch {
namespace rtl_emit {

/// CLI 选项。
struct CliOptions {
    /// 输出目录（产物与 manifest 的默认落点）
    std::string out_dir = "rtl_out";
    /// manifest 落盘路径（空 ⇒ `<out_dir>/manifest.json`）
    std::string manifest_path;
    /// Verilog 落盘路径（空 ⇒ `<out_dir>/top.v`）
    std::string verilog_path;
    /// 是否发射 Verilog（默认 true）
    bool emit_verilog = true;
    /// 是否写 manifest（默认 true）
    bool emit_manifest = true;
    /// 有诊断时**仍然**返回 0（默认 false ⇒ ⇒ 诊断 ⇒ 退出码 1）
    bool allow_diagnostics = false;
    /// ⚠️ **产物卫生**开关（C6 · **默认 off** ⇒ ⇒ 默认路径**产物逐字节不变**）
    /// ⇒ ⇒ 打开它 = **显式的**产物形态变更（批次表 ★3：层次化产物逐字节不变
    /// 那条 AC **只在该开关默认 off 时成立**）
    bool emit_hygiene = false;
    /// ⚠️ **测试台骨架**开关（`examples-rtl-sim-coverage` **E2**）—— 默认 **off**
    /// ⇒ ⇒ 打开后额外产出 `<out_dir>/tb_<module>_stub.sv`（**只发骨架**：
    /// 端口连线 + 时钟/复位 + VCD 开关；激励与断言留 `// TODO:`）
    bool emit_tb_stub = false;
    /// TB 骨架的 module 名（空 ⇒ `tb_<dut_module>_stub`）
    std::string tb_stub_module;
    /// TB 骨架里 `// TODO:` 的归属提示（空 ⇒ `<example>`）
    std::string tb_stub_hint;
    /// TB 骨架的时钟半周期（ns，默认 5 = 100 MHz）
    unsigned tb_clock_half_period_ns = 5;
    /// 打印用法
    bool help = false;
};

/// 解析命令行参数。
/// @return false ⇒ 参数非法（**fail-closed** ⇒ 调用方 MUST 退出码 2）
bool parse_cli_options(const std::vector<std::string> &args, CliOptions &out,
                       std::string &error);

/// 打印用法（到 stdout）。
void print_cli_usage(const char *program_name);

/// 设计注册回调：在给定的 context 里造出**根组件**。
///
/// ⚠️ 返回 `nullptr` ⇒ ⇒ 调用方 MUST 退出码 2（fail-closed）。
using DesignFactory = std::function<std::unique_ptr<Component>(core::context *)>;

/// 跑一次完整发射（解析参数 → 建 ctx → 注册设计 → 写 manifest → 发射产物）。
///
/// @return 退出码（0 / 1 / 2，见文件头）
int run_emit_cli(const std::vector<std::string> &args, const DesignFactory &factory);

}   // namespace rtl_emit
}   // namespace ch
