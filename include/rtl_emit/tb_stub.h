// include/rtl_emit/tb_stub.h
//
// **测试台骨架生成**（`examples-rtl-sim-coverage` **E2**）—— 只发**骨架**，
// **不含**激励与断言（语义不可自动生成）。
//
// ⚠️ **为什么只发骨架**：激励与期望值是**领域知识**（UART 的波特率、AXI 的握手
//   时序、PWM 的占空比表 ……）⇒ ⇒ 自动生成只能产出「编译通过但什么都没验」的 TB
//   ⇒ ⇒ 那正是仓内被反复治过的病（「TB 变成只编译不跑」/「空断言」）。
//   ⇒ ⇒ 本生成器负责**容易写错的部分**（端口清单、方向、宽度、时钟复位、VCD），
//   由**人**补**有语义的部分**（激励 + 断言）。
//
// ⚠️ **期望值 MUST 有出处登记**（E3 的组织约定）：骨架会在激励/断言位置留下
//   `// TODO:` 标记 ⇒ ⇒ 补完的 TB **必须**在头部写明期望值来源
//   （协议规范 / 手推推导 / C++ 侧断言的位置）。

#pragma once

#include "rtl_emit/manifest.h"

#include <iosfwd>
#include <string>

namespace ch {
namespace rtl_emit {

/// 生成 TB 骨架的选项。
struct TbStubOptions {
    /// 被测 module 名（= 产物里的 `module` 名；扁平 = `top`）
    std::string dut_module = "top";
    /// TB 的 module 名（默认 `tb_<dut_module>_stub`）
    std::string tb_module;
    /// 生成文件里 `// TODO:` 的归属提示（例：`counter_simple_example`）
    std::string example_hint;
    /// 时钟半周期（ns）—— 默认 5（100 MHz）
    unsigned clock_half_period_ns = 5;
    /// 复位拍数 —— 默认 4
    unsigned reset_cycles = 4;
};

/// 生成 TB 骨架（**SystemVerilog**）到 `out`。
///
/// 产物结构（顺序固定 ⇒ ⇒ 确定性）：
///   ① 文件头（生成器标识 + **不含时间戳** ⇒ ⇒ 可重复生成）
///   ② **端口清单注释块**（来自 manifest：name / dir / width / role / owner / cpp_member）
///   ③ 端口信号声明（输入按 `role` tie-off 到 0 · 输出接 `logic`）
///   ④ DUT 实例化（**按 manifest 顺序**逐端口连线 ⇒ ⇒ 与产物端口表一致）
///   ⑤ 时钟 / 复位驱动骨架
///   ⑥ VCD 开关（`ifdef DUMP_VCD`）
///   ⑦ **留空**的激励 / 断言位置（`// TODO:` + 断言收尾约定）
///   ⑧ `endmodule`
///
/// ⚠️ **断言收尾约定**（E5 的运行护栏据此判「跑过且通过」）：
///   补完的 TB **MUST** 在结尾打印 `RESULT: checks=<N> fails=0`
///   ⇒ ⇒ 护栏按该行判红绿（**不靠**退出码 —— Verilator 的 `$finish` 恒 0）。
void write_tb_stub(std::ostream &out, const RtlManifest &manifest,
                   const TbStubOptions &options);

} // namespace rtl_emit
} // namespace ch
