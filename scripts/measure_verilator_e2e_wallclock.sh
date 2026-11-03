#!/usr/bin/env bash
# 护栏/工具：**实测** `ctest -L verilator_e2e -LE perf` 的墙钟（warm vs cold）
#
# ⚠️ **为什么要有它**（2026-11-03）：
#   `pr-verilator-fast` 这个 CI job 的 `timeout-minutes: 45` 是**硬门** ⇒ ⇒
#   「这个套件要跑多久」必须是**实测**而不是估计（仓内历史教训：成本模型
#   「10.66 s/pair」在轻 DUT 上被实测打到 2.1 s/pair，在重 DUT 上又打到 12.6 s/pair）。
#
# ⚠️ **口径**（与 CI 完全一致）：
#   · 命令 = `ctest --output-on-failure --no-tests=error -L verilator_e2e -LE perf -I <shard>,,2`
#     （`-I <start>,,<stride>` 是「区间起点 + 步长」；写成 `-I 0,2` 会是**区间 0..2** ⇒ 只选 2 个测试）
#   · CI 用**两个并行 job**（`strategy.matrix.shard: [1,2]`）⇒ ⇒ **`timeout-minutes` 是每 job 的预算**
#     ⇒ ⇒ 比较时必须用 **max(shard1, shard2)**，不是两片之和。
#   · cold = `HOME` 指向空目录（⇒ `~/.cache/cpphdl/verilator` 为空 ⇒ 每个 DUT 现场编译 `libVtop.so`）
#
# ⚠️ **测量纪律**：
#   · **测量期间不得有其它构建 / 测试在跑**（否则数字是上界，不是真值）；
#   · cold 的 per-test 编译耗时是 CI 的**真实风险面**（缓存一旦未命中就会翻红）
#     ⇒ ⇒ 本脚本顺带打印「每个测试的 cold 耗时 / 它的 TIMEOUT」⇒ 占比 > 50% 即为**欠定档**。
#
# 用法：
#   scripts/measure_verilator_e2e_wallclock.sh            # warm（真实缓存）
#   scripts/measure_verilator_e2e_wallclock.sh --cold      # cold（空 HOME）
#   scripts/measure_verilator_e2e_wallclock.sh --both      # 两轮都跑
set -u

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${REPO_ROOT}/build"
OUT_DIR="${TMPDIR:-/tmp}/cpphdl-wallclock"
mkdir -p "$OUT_DIR"

if [ ! -d "$BUILD_DIR" ]; then
    echo "ERROR: ${BUILD_DIR} 不存在 ⇒ 先 cmake -B build && cmake --build build" >&2
    exit 2
fi

MODE="${1:---warm}"

run_shards() {
    local label="$1" home_prefix="$2"
    local total=0
    for s in 1 2; do
        local start end rc wall log
        start=$(date +%s)
        log="${OUT_DIR}/${label}_shard${s}.log"
        if [ "$home_prefix" = "cold" ]; then
            local coldhome="${OUT_DIR}/coldhome"
            rm -rf "$coldhome"; mkdir -p "$coldhome"
            HOME="$coldhome" ctest --test-dir "$BUILD_DIR" --output-on-failure \
                --no-tests=error -L verilator_e2e -LE perf -I "${s},,2" > "$log" 2>&1
        else
            ctest --test-dir "$BUILD_DIR" --output-on-failure \
                --no-tests=error -L verilator_e2e -LE perf -I "${s},,2" > "$log" 2>&1
        fi
        rc=$?
        end=$(date +%s)
        wall=$((end - start))
        total=$((total + wall))
        echo "${label} shard=${s} rc=${rc} wall=${wall}s (log: ${log})"
    done
    echo "${label} TOTAL(两片之和，仅供参考) = ${total}s"
}

print_timeout_ratios() {
    local label="$1"
    python3 - "$label" "$BUILD_DIR" "$OUT_DIR" <<'PY'
import json, re, subprocess, sys
label, build_dir, out_dir = sys.argv[1], sys.argv[2], sys.argv[3]
try:
    d = json.loads(subprocess.run(["ctest", "--test-dir", build_dir, "--show-only=json-v1",
                                   "-L", "verilator_e2e"], capture_output=True, text=True).stdout)
except Exception as exc:                                  # pragma: no cover
    print(f"  （无法读取 ctest 属性：{exc}）")
    sys.exit(0)
timeouts = {}
for t in d.get("tests", []):
    for p in t.get("properties", []):
        if p["name"] == "TIMEOUT":
            timeouts[t["name"]] = float(p["value"])
cold = {}
for shard in (1, 2):
    try:
        for line in open(f"{out_dir}/{label}_shard{shard}.log", errors="replace"):
            m = re.match(r"\s*\d+/\d+ Test\s+#\d+:\s+(\S+)\s+\.+\s*(?:Passed|\*\*\*\w+)\s+([\d.]+) sec", line)
            if m:
                cold[m.group(1)] = float(m.group(2))
    except OSError:
        pass
rows = sorted(((cold[n] / timeouts[n], n, cold[n], timeouts[n])
                for n in cold if n in timeouts and timeouts[n] > 0), reverse=True)
print(f"  {label}: cold 耗时 / TIMEOUT 占比（降序，前 8）")
for ratio, name, c, t in rows[:8]:
    flag = "  <-- 欠定档（<2× 余量）" if ratio > 0.5 else ""
    print(f"    {name:45s} {c:8.1f}s / {t:7.0f}s = {ratio*100:5.1f}%{flag}")
PY
}

case "$MODE" in
    --warm) run_shards warm warm ;;
    --cold) run_shards cold cold; print_timeout_ratios cold ;;
    --both) run_shards warm warm; run_shards cold cold; print_timeout_ratios cold ;;
    *) echo "用法: $0 [--warm|--cold|--both]" >&2; exit 2 ;;
esac
