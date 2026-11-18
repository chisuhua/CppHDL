/**
 * AXI4-Lite Example —— **由 `.verif` 验证源驱动**（`rtl-verification-single-source` · ADR-074 D1）
 *
 * ⚠️ **本文件不含任何激励 / 断言 / 期望值**：它们全在
 *    `examples/axi4/tb_axi4_lite_example.verif`（**唯一语义来源**）⇒ ⇒
 *    IR 解释器（本文件）与 Verilog 渲染器（生成的 `.sv` TB）由**同一份 IR** 派生
 *    ⇒ ⇒ 两侧断言**不可能**漂移。
 *
 * ⚠️ **验证源路径 = 编译期绝对路径**：仓内三个面的 CWD 各不相同
 *    （`run_all_ported_tests.sh` → `build/` · CTest → `build/tests` · 护栏 → 仓根）
 *    ⇒ ⇒ 相对路径必失败 ⇒ ⇒ 用 CMake 注入的 `CPPHDL_SOURCE_DIR`。
 *
 * ⚠️ **两种模式**（`emit-cli-into-example` · 发射入口并入 example）：
 *    · **默认**（无参）= **验证模式**：读 `.verif` → 驱动 → 断言 → `RESULT:` 行
 *    · **`--emit-rtl <dir>`** = **发射模式**：剥离该标记 ⇒ 其余 argv 透传库层 `run_emit_cli`
 *    ⇒ ⇒ 两种模式**建不同的根**（验证模式 `ch_device` 的默认根 / 发射模式显式 `"top"`）
 *    ⇒ ⇒ 后者与原独立薄壳**逐字一致**（产物因此逐字节相同）
 */

#include "ch.hpp"
#include "axi4_lite_example_top.h"
#include "component.h"
#include "simulator.h"
#include "rtl_emit/cli.h"
#include "rtl_emit/manifest.h"
#include "rtl_emit/verif.h"

#include <cstddef>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace {

/// 相对仓根的验证源路径（**唯一**的路径字面量）
constexpr const char *kVerifRelPath = "/examples/axi4/tb_axi4_lite_example.verif";

/// 发射模式的根组件名（**MUST 与原独立薄壳逐字一致** ⇒ 产物逐字节相同）
constexpr const char *kEmitRootName = "top";

/// 发射模式的 design factory（与 `run_emit_cli` 的 `DesignFactory` 同型）
std::unique_ptr<ch::Component> make_emit_design(ch::core::context *) {
    return std::make_unique<axi4::example::AxiLiteTop>(nullptr, kEmitRootName);
}

} // namespace

int main(int argc, char **argv) {
    using namespace ch;

    // ── 0) 模式判定：**位置无关**剥离 `--emit-rtl`（其余 argv 原序保留）──────────
    bool emit_mode = false;
    std::vector<std::string> rest;
    rest.reserve(static_cast<std::size_t>(argc > 0 ? argc - 1 : 0));
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--emit-rtl") {
            emit_mode = true;
            continue;
        }
        rest.push_back(a);
    }

    if (emit_mode) {
        // ── 发射模式 fail-closed：缺 `--out <dir>` ⇒ exit 2 ──────────────────
        //   ⚠️ MUST NOT 落到库层默认 `rtl_out/`（那会写进 CWD）
        bool has_out = false;
        for (std::size_t i = 0; i + 1 < rest.size(); ++i) {
            if (rest[i] == "--out") {
                has_out = true;
                break;
            }
        }
        if (!has_out) {
            std::cerr << "FATAL: --emit-rtl 缺 --out <dir>（拒绝回落到默认 rtl_out/）\n";
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

    // ── 1) 扫描 + 生成期求值（fail-closed ⇒ ⇒ 不打印结论行）────────────────
    std::ifstream in(verif_path);
    if (!in) {
        std::cerr << "FATAL: 打不开验证源：" << verif_path << "\n";
        return 1;
    }
    rtl_emit::VerifProgram prog;
    rtl_emit::VerifParseError perr;
    if (!rtl_emit::parse_verif(in, prog, perr)) {
        std::cerr << "FATAL: 验证源解析失败（" << verif_path << ":" << perr.line << "）："
                  << perr.message << "\n";
        return 1;
    }

    // ── 2) 建图 + 端口清单（`port_table[]` = 端口面的唯一权威）────────────
    ch::ch_device<axi4::example::AxiLiteTop> dev;
    const auto manifest = rtl_emit::build_rtl_manifest(dev.instance(), dev.context());
    if (!manifest.diagnostics.empty()) {
        for (const auto &d : manifest.diagnostics) {
            std::cerr << "FATAL: manifest 诊断：" << d.detail << "\n";
        }
        return 1;
    }

    // ── 3) IR 解释器（驱动 `Simulator`）───────────────────────────────────
    ch::Simulator sim(dev.context());
    const rtl_emit::VerifRunOptions opts{};
    const auto res = rtl_emit::run_verif(prog, sim, dev.context(), manifest, opts);

    // ── 4) 逐条结论：FAIL 进 stderr ──────────────────────────────────────
    for (const auto &line : res.failure_lines) {
        std::cerr << line << "\n";
    }

    // ── 5) fail-closed：执行失败 ⇒ 不打印结论行 ──────────────────────────
    if (!res.ok) {
        std::cerr << "FATAL: 执行失败：" << res.error << "\n";
        return 1;
    }

    // ── 6) 条数自洽：运行期条数 MUST == 静态条数（防「0 条断言被当绿」）──
    const std::size_t expected = rtl_emit::verif_check_count(prog);
    if (res.checks != expected) {
        std::cerr << "FATAL: 条数不等 —— 运行期 " << res.checks << " ≠ 静态 " << expected << "\n";
        return 1;
    }

    // ── 7) 结论行（协议：十进制 · 单行 · 无 ANSI）────────────────────────
    std::cout << "RESULT: checks=" << res.checks << " fails=" << res.fails << "\n";
    return res.fails == 0 ? 0 : 1;
}
