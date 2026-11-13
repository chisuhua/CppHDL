#!/usr/bin/env bash
# ─────────────────────────────────────────────────────────────────────────────
# probe_r21_hier_granularity.sh —— **R21 规模侦察**（只读 · 不产出仓库文件）
#
# 问题：**给 RV32I 声明「成层」⇒ 会不会撞分析层的 fail-closed？**
# 手段：`Granularity::ForceLayer`（总开关 = 把所有组件当成层）⇒ 发射
#       ⇒ 打印 `ok` / `error` / 端口行数 ⇒ 由此估**立项范围**。
#
# ⚠️ 只读：产物写到 `$TMPDIR/opencode/r21_probe.v`（**不入仓**）⇒ 本脚本不改任何源文件。
# ⚠️ 需要 `build/libcpphdl.a`（先 `cmake --build build`）。
# 退出码：0 = 侦察完成（无论 ok/fail-closed）· 2 = 基础设施缺失（fail-closed）
# ---------------------------------------------------------------------------
set -uo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
LIB="$REPO_ROOT/build/libcpphdl.a"
OUT_DIR="${TMPDIR:-/tmp/opencode}/r21_hier_probe"
mkdir -p "$OUT_DIR"

if [ ! -f "$LIB" ]; then
    echo "❌ 缺 build/libcpphdl.a ⇒ 先 cmake --build build（fail-closed）" >&2
    exit 2
fi

cat > "$OUT_DIR/probe.cpp" <<'CPP'
#include "ch.hpp"
#include "component.h"
#include "device.h"
#include "hierarchy_verilog.h"
#include "rv32i_pipeline_top.h"
#include <cstdio>
int main() {
    ch::HierarchyEmitResult r;
    {
        ch::Component::GranularityScope scope(ch::Granularity::ForceLayer);
        ch::ch_device<Rv32iPipelineTop> dev;
        r = ch::toVerilogHierarchical("PROBE_OUT", dev.instance());
    }
    std::printf("ok=%s\n", r.ok ? "true" : "false");
    std::printf("files=%zu port_rows=%zu warnings=%zu\n",
                r.files.size(), r.port_provenance.size(), r.warnings.size());
    if (!r.ok) {
        std::printf("--- error ---\n%s\n", r.error.c_str());
    }
    return 0;
}
CPP
sed -i "s|PROBE_OUT|$OUT_DIR/r21_probe.v|" "$OUT_DIR/probe.cpp"

INC=(-I "$REPO_ROOT/include" -I "$REPO_ROOT/include/core" -I "$REPO_ROOT/include/ast"
     -I "$REPO_ROOT/include/abstract" -I "$REPO_ROOT/include/utils"
     -I "$REPO_ROOT/include/lnode" -I "$REPO_ROOT/examples/riscv-mini/src")

if ! g++ -std=c++23 "${INC[@]}" -O0 "$OUT_DIR/probe.cpp" "$LIB" -o "$OUT_DIR/probe" 2>"$OUT_DIR/cc.log"; then
    echo "❌ 探针编译失败（见 $OUT_DIR/cc.log）⇒ fail-closed" >&2
    head -12 "$OUT_DIR/cc.log" >&2
    exit 2
fi

"$OUT_DIR/probe" 2>&1 | grep -vE '^\[(INFO|WARN)\]' | sed 's/^/  /'
echo
echo "── 结论摘要 ──"
"$OUT_DIR/probe" 2>/dev/null | grep -cE "^(ok|files)=" >/dev/null || true
case "$("$OUT_DIR/probe" 2>/dev/null | grep -E '^ok=' | head -1)" in
    "ok=true")  echo "  ✅ 成层可发射（未撞 fail-closed）" ;;
    "ok=false") n=$("$OUT_DIR/probe" 2>/dev/null | grep -cE '^  \[[0-9]+\] ')
                echo "  ⚠️ fail-closed：$n 条跨 module 边无法映射（见上）" ;;
    *)          echo "  ⚠️ 探针输出不符合预期 ⇒ fail-closed"; exit 2 ;;
esac
exit 0
