// include/rtl_emit/verif.h
//
// **RTL 验证的单一真相源**（`rtl-verification-single-source` · ADR-074 D1/D2）
//
// 链路：`.verif` 验证源 --扫描器--> **IR**（唯一语义载体）--> ① IR 解释器（驱动
// `Simulator`）· ② Verilog 渲染器（生成 `.sv` TB）。
//
// ⚠️ **值在生成期全部求值成字面量**（`REPEAT` 在扫描期展开、`_i` 在扫描期绑定）
//   ⇒ ⇒ IR 里**没有表达式** ⇒ ⇒ 两个执行后端**不需要各自解释表达式** ⇒ ⇒
//   语义真的只有一份（ADR-074 D1 的核心）。
//
// ⚠️ **时序锚点（IR 层唯一确定）**：驱动发生在「沿后采样窗」内（不消耗拍）；
//   观测发生在「最近一次结算之后」⇒ ⇒ Verilog 侧统一 `@(posedge clk); #1;`
//   ⇒ ⇒ **禁 `#0`**（inactive region ⇒ 非阻塞赋值未结算 ⇒ 两后端必然分叉）。
//
// ⚠️ **fail-closed**：集合外构造、表达式文法外token、名字未命中/多义 ⇒ 一律报错。

#pragma once

#include "rtl_emit/manifest.h"

#include <cstddef>
#include <cstdint>
#include <iosfwd>
#include <string>
#include <vector>

namespace ch {
class Simulator;
namespace core {
class context;
}
namespace rtl_emit {

/// `WAIT` 的**目标电平**（ADR-074 §7.2 D2′ · 2026-11-20 dated 订正）
///
/// ⚠️ **不得复用 `VerifOp::value`**（那是 `Reset` 的 0/1 电平）—— 二者**默认值语义相反**
/// （`value` 默认 0 · 而 `WAIT` 缺省 = **High / 非零**）⇒ ⇒ 复用会让缺省写法被读成 `low`。
/// ⚠️ **`High` ≡ 非零**，**不是**「N 位全 1」（二者仅 N = 1 时等价）⇒ ⇒ 判定是**零值测试**
/// ⇒ ⇒ 不需要端口位宽。
enum class WaitLevel : std::uint8_t {
    High = 0, // 值 != 0（**≡ 缺省**）
    Low = 1,  // 值 == 0
};

/// IR 原语（**封闭集** · ADR-074 D2）
enum class VerifOpKind {
    Set,          // SET <port> <expr>
    Tick,         // TICK <n>
    Settle,       // SETTLE
    Wait,         // WAIT <port> [<high|low>] <max>
    Expect,       // EXPECT <port> <expr>
    ExpectStable, // EXPECT_STABLE <port> <n>
    Observe,      // OBSERVE <port>
    Reset,        // RESET <port> <high|low> [cycles]
};

/// IR 的一条语句（**值已求值为字面量**）
struct VerifOp {
    VerifOpKind kind{VerifOpKind::Tick};
    std::string port;
    std::uint64_t value = 0;  // Set 的值 / Expect 的期望值 / Reset 的电平（0|1）
    std::uint32_t count = 0;  // Tick 拍数 / Wait 上限 / ExpectStable 拍数 / Reset 复位拍数
    std::string check_id;     // Expect / Wait 的逐条结果标识（渲染期输出 `RESULT: check <id>`）
    WaitLevel level{WaitLevel::High}; // Wait 的目标电平（缺省 = High ≡ 非零 ⇒ 旧写法零影响）
};

struct VerifScenario {
    std::string name;
    std::string anchor; // `.md` 锚点名（双向 1:1 判据用）
    std::vector<VerifOp> ops;
};

struct VerifProgram {
    std::string tb_module;
    std::string expect_src; // `<path>#<anchor>`
    std::vector<VerifScenario> scenarios;
};

struct VerifParseError {
    std::size_t line = 0;
    std::string message;
};

/// 扫描 + 生成期求值。失败 ⇒ `false` + `err`（**MUST NOT** 静默跳过任何一行）。
bool parse_verif(std::istream &in, VerifProgram &out, VerifParseError &err);

/// 静态展开后的**运行期断言条数**（= `EXPECT` + `WAIT` 的条数）
std::size_t verif_check_count(const VerifProgram &program);
std::size_t verif_scenario_count(const VerifProgram &program);

// ── ① IR 解释器 ────────────────────────────────────────────────────────

struct VerifRunOptions {
    bool verbose = false;
    /// 执行前施加**框架复位脉冲**（`default_reset`，与渲染式 TB 的 `write_tb_reset_pulse`
    /// 同构）⇒ ⇒ 两后端从**同一初始状态**起跑（ADR-074 D1 的「语义只有一份」前提）。
    bool apply_frame_reset = true;
};

struct VerifRunResult {
    bool ok = false;
    std::string error; // 解析 / 端口绑定失败（fail-closed）
    std::size_t checks = 0;
    std::size_t fails = 0;
    /// **逐条结论**（格式与渲染式 TB 的 `$display` **逐字一致**：`CHECK <id> PASS|FAIL`）
    /// ⇒ ⇒ 两后端的结论**可直接比对**（ADR-074 D6 的「同结论」判据载体）。
    std::vector<std::string> check_lines;
    /// 失败子集（`check_lines` 里 FAIL 的那些）
    std::vector<std::string> failure_lines;
};

/// 驱动 `Simulator` 执行 IR。`manifest` 用于「DSL 标识符 → 端口名」，`ctx` 用于
/// 「端口名 → node_id」⇒ ⇒ 端口未具名时本条会 fail-closed（这正是 ADR-075 的前置）。
VerifRunResult run_verif(const VerifProgram &program, ch::Simulator &sim,
                         ch::core::context *ctx, const RtlManifest &manifest,
                         const VerifRunOptions &options = {});

// ── ② Verilog 渲染器 ───────────────────────────────────────────────────

struct TbRenderOptions {
    std::string tb_module;
    std::string dut_module = "top";
    std::string expect_src; // TB 头部 `EXPECT-SRC:`
    std::string example_hint;
    unsigned clock_half_period_ns = 5;
    unsigned reset_cycles = 4;
};

/// 渲染完整 `.sv` TB（端口连线来自 `manifest.port_table[]` ⇒ ⇒ **无手写映射表**）。
/// 失败 ⇒ `false` + `error`（端口未解析 / 无时钟却用到 `TICK` / `WAIT` ⇒ fail-closed）。
bool render_tb(std::ostream &out, const VerifProgram &program,
               const RtlManifest &manifest, const TbRenderOptions &options,
               std::string &error);

} // namespace rtl_emit
} // namespace ch
