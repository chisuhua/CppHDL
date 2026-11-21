/**
 * PWM Example —— **由 `.verif` 验证源驱动**（ADR-074 D1 / D2 · ADR-076）
 *
 * ⚠️ **本文件不含任何激励 / 断言 / 期望值**：它们全在
 *   `examples/spinalhdl-ported/pwm/tb_pwm_example.verif`（**唯一语义来源**）
 *   ⇒ ⇒ IR 解释器（本文件）与 Verilog 渲染器（生成的 `.sv` TB）由**同一份 IR** 派生
 *   ⇒ ⇒ 两侧断言**不可能**漂移。
 *
 * ⚠️ **验证源路径 = 编译期绝对路径**：仓内三个面的 CWD 各不相同
 *   （`run_all_ported_tests.sh` → `build/` · CTest → `build/tests` · 护栏 → 仓根）
 *   ⇒ ⇒ 相对路径必失败 ⇒ ⇒ 用 CMake 注入的 `CPPHDL_SOURCE_DIR`。
 *
 * ⚠️ **两种模式**：
 *   · **默认**（无参）= **验证模式**：读 `.verif` → 驱动 → 断言 → `RESULT:` 行
 *   · **`--emit-rtl <dir>`** = **发射模式**：剥离该标记 ⇒ 其余 argv 透传库层
 *   `run_emit_cli`（产物端口名 = `top_<C++ 成员名>`）
 */

#include "ch.hpp"
#include "component.h"
#include "pwm_top.h"
#include "simulator.h"

#include "rtl_emit/cli.h"
#include "rtl_emit/verif.h"

#include <cstddef>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace {

/// 相对仓根的验证源路径（**唯一**的路径字面量）
constexpr const char *kVerifRelPath = "/examples/spinalhdl-ported/pwm/tb_pwm_example.verif";

/// 发射模式的根组件名（单一真相源 = 设计头的常量）
constexpr const char *kEmitRootName = pwm_example::kPwmRootName;

/// 发射模式的 design factory（与 `run_emit_cli` 的 `DesignFactory` 同型）
std::unique_ptr<ch::Component> make_emit_design(ch::core::context *) {
    return std::make_unique<pwm_example::PwmTop>(nullptr, kEmitRootName);
}

} // namespace

int main(int argc, char **argv) {
    using namespace ch;

    // ── 0) 模式判定：位置无关剥离 `--emit-rtl`（其余 argv 原序保留）──────────
    std::vector<std::string> rest;
    rest.reserve(static_cast<std::size_t>(argc > 0 ? argc - 1 : 0));
    for (int i = 1; i < argc; ++i) {
        rest.emplace_back(argv[i]);
    }
    std::vector<std::string> mode_flags;
    rest = rtl_emit::strip_mode_flags(rest, {"--emit-rtl"}, &mode_flags);

    if (!mode_flags.empty()) {
        // ── 发射模式 fail-closed：`--out` 缺失 / 取值形态非法 ⇒ exit 2 ───────────
        std::string error;
        if (!rtl_emit::require_out_dir(rest, nullptr, &error)) {
            std::cerr << "FATAL: --emit-rtl " << error << "（拒绝回落到默认 rtl_out/）\n";
            return 2;
        }
        return rtl_emit::run_emit_cli(rest, make_emit_design);
    }

    // ── 验证模式 fail-closed：收到**任何**未知参数 ⇒ exit 2 ──────────────────
    if (!rest.empty()) {
        std::cerr << "FATAL: 未知参数（默认模式只接受无参）：" << rest[0] << "\n";
        return 2;
    }

    const std::string verif_path = std::string(CPPHDL_SOURCE_DIR) + kVerifRelPath;

    // ── 1) 建图（`port_table[]` = 端口面的唯一权威）────────────────────────
    ch::ch_device<pwm_example::PwmTop> dev;

    // ── 2) 解析 → 端口清单 → IR 解释器 → 运行期条数 == 静态条数（公共部分）
    const auto r = rtl_emit::run_verif_file(verif_path, dev.instance(), dev.context());

    // ── 3) 逐条结论：FAIL 进 stderr ──────────────────────────────────────
    for (const auto &line : r.run.failure_lines) {
        std::cerr << line << "\n";
    }

    // ── 4) fail-closed：任一步失败（解析 / 清单 / 执行 / 条数）⇒ 不打印结论行
    if (!r.ok) {
        std::cerr << "FATAL: " << r.error << "\n";
        return 1;
    }

    // ── 5) 结论行（协议：十进制 · 单行 · 无 ANSI）────────────────────────
    std::cout << "RESULT: checks=" << r.run.checks << " fails=" << r.run.fails << "\n";
    return r.run.fails == 0 ? 0 : 1;
}
