#!/usr/bin/env bash
# ============================================================================
# build.sh — 从上游 riscv-tests 重建 rv32ui-p-* 的 40 份镜像
# ============================================================================
#
# 用法：
#   bash build.sh <riscv-tests 源码目录> <riscv32 工具链前缀> [输出目录]
#
#   <riscv-tests 源码目录>   上游 checkout（含 isa/rv32ui/*.S 与 isa/macros/scalar）
#   <riscv32 工具链前缀>     例如 /opt/riscv/bin/riscv32-unknown-elf
#   [输出目录]               默认 = 本脚本同级的 ../images
#
# 产物：<输出目录>/rv32ui-p-<名字>/{itcm.mem,dtcm.mem}
#       —— `rv32i-fw-dump --format mem-split` 的**两个区域字镜像**
#       （行号 = 区域内字索引 ⇒ 空洞已补零）
#
# ⚠️ **需要外部工具链 ⇒ CI 跑不了** ⇒ 本脚本是**可复现性凭证**，不是门禁。
#    门禁 = `test_rv32i_official_suite`（消费已入库的镜像）。
#
# ⚠️ **重建产物 MUST 与入库镜像逐字节一致** ⇒ ⇒ 判据 = 本脚本末尾的
#    `diff -r`（默认开）⇒ ⇒ 漂移即红。
#
# SPDX-License-Identifier: BSD-3-Clause
# ============================================================================
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
RT_SRC="${1:?用法: build.sh <riscv-tests 源码目录> <工具链前缀> [输出目录]}"
PREFIX="${2:?用法: build.sh <riscv-tests 源码目录> <工具链前缀> [输出目录]}"
OUT_DIR="${3:-$HERE/../images}"

GCC="$PREFIX-gcc"
FW_DUMP="${RV32I_FW_DUMP:-$(cd "$HERE/../../../../.." && pwd)/build/tools/rv32i-fw-dump}"

for f in "$GCC" "$FW_DUMP"; do
    [ -x "$f" ] || { echo "FATAL: 不可执行: $f" >&2; exit 2; }
done
[ -d "$RT_SRC/isa/rv32ui" ] || { echo "FATAL: 不是 riscv-tests 源码树: $RT_SRC" >&2; exit 2; }

# ── 被排除的测试（与 PROVENANCE.md §排除项一致）────────────────────────
#   fence_i —— Zifencei 扩展指令（本核 RV32I 无该扩展）
#   ma_data —— **非对齐** load/store（本核车道选择忽略低地址位 ⇒ 非对齐被静默对齐）
EXCLUDE="fence_i ma_data"

# ── 被测的测试清单（= 上游 isa/rv32ui/Makefrag 的 rv32ui_sc_tests）────
TESTS="simple add addi and andi auipc beq bge bgeu blt bltu bne jal jalr
       lb lbu lh lhu lw ld_st lui or ori sb sh sw st_ld sll slli
       slt slti sltiu sltu sra srai srl srli sub xor xori"

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

for t in $TESTS; do
    case " $EXCLUDE " in *" $t "*) echo "SKIP(排除): $t"; continue;; esac
    name="rv32ui-p-$t"
    elf="$WORK/$name.elf"
    "$GCC" -march=rv32i -mabi=ilp32 -static -mcmodel=medany \
           -fvisibility=hidden -nostdlib -nostartfiles \
           -I "$HERE" \
           -I "$RT_SRC/isa/rv32ui" \
           -I "$RT_SRC/isa/macros/scalar" \
           -T "$HERE/link.ld" \
           -o "$elf" "$RT_SRC/isa/rv32ui/$t.S"
    mkdir -p "$OUT_DIR/$name"
    "$FW_DUMP" --in "$elf" --format mem-split --out "$OUT_DIR/$name" >/dev/null
    echo "BUILT: $name"
done

# ── 与入库镜像逐字节比对（重建漂移即红）────────────────────────────────
if [ -d "$HERE/../images" ] && [ "$OUT_DIR" != "$HERE/../images" ]; then
    diff -r "$HERE/../images" "$OUT_DIR" \
        && echo "OK: 重建产物与入库镜像逐字节一致" \
        || { echo "FATAL: 重建产物与入库镜像不一致" >&2; exit 1; }
fi
echo "DONE: $OUT_DIR"
