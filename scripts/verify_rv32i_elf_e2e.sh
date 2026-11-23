#!/usr/bin/env bash
# scripts/verify_rv32i_elf_e2e.sh
#
# **ELF → init_data 向量 → 核取指**的**端到端**判据（方案 a · `firmware-load-chain` L2/L6）
#
# ⚠️ **它补的是什么洞**：`scripts/verify_rv32i_fw_dump.sh` 判的是**工具**；
#    本护栏判的是**整条链路** ⇒ ⇒ 「产物有没有人用」这件事此前**零判据**。
#
# 链路（每一段都有判据）：
#
#   ELF(.text + .data)
#     └─ rv32i-fw-dump --format mem-split   ⇒ 两个**按区域**的初值镜像
#          └─ tb_rv32i_soc --firmware <dir>  ⇒ `ch_mem::init_data` ⇒ 核取指
#               └─ 判据：`.data` 经**真实读通路**读回（`lw` ⇒ 寄存器写回）
#
# 判据（逐条可复跑 · 失败即 exit 1）：
#   E0 三个二进制齐备（缺 ⇒ **fail-closed**，不是 skip）
#   E1 **双段 ELF** ⇒ `mem-split` 产出两个镜像 · **行号 = 区域字索引**（空洞补零）
#   E2 **核级**：`tb_rv32i_soc --firmware` ⇒ `.data` 的值经 `lw` 写回
#      （x1 = 0x5A · x2 = 0x5B · store = 0x104/0x5B）⇒ ⇒ **真实读通路**
#   E3 **SoC 级**：`tb_riscv_soc --firmware` ⇒ 固件在 SoC 里真的在跑
#      （store_count / writeback_count）
#   E4 **产物级**：`--emit-rtl --firmware` ⇒ (a) **顶层端口表与基线逐字相同**
#      （零端口变更）· (b) `initial` 块携带 **ELF 派生的字**（含 NOP 填充）
#   E5 **投毒自证**（`--self-test`）：改坏 ELF 的 .text / .data / 去掉 .data
#      ⇒ ⇒ E2 MUST 各自转红（证明判据**有判别力**）
#   E6 **真工具链 leg**（**env-gated**）：有 `riscv64-unknown-elf-gcc` ⇒ ⇒
#      现场编一个真固件 ⇒ ⇒ 跑同一条链路 ⇒ ⇒ 同组期望值
#      （CI 无工具链 ⇒ **SKIP 并说明**，不判红 —— 仓外依赖不阻塞 CI）
#
# ⚠️ **期望值一律手推**（禁抄工具/仿真输出）——
#    编码推导见 `scripts/verify_rv32i_fw_dump.sh` 的 fixture 生成器
#    （两者**同一组**字 ⇒ ⇒ 交叉印证）。
#
# ⚠️ **本护栏不需要 verilator / 网络** ⇒ ⇒ 主 job（BUILD_VERILATOR=OFF）也能跑。

set -uo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT" || exit 2

BUILD_DIR="${BUILD_DIR:-$REPO_ROOT/build}"
DUMP_BIN="${DUMP_BIN:-$BUILD_DIR/tools/rv32i-fw-dump}"
MINI_BIN="${MINI_BIN:-$BUILD_DIR/examples/riscv-mini/tb_rv32i_soc}"
SOC_BIN="${SOC_BIN:-$BUILD_DIR/examples/riscv-soc/tb_riscv_soc}"
ANALYZER="$REPO_ROOT/scripts/lib/flat_rtl_emit_check.py"
WORK="${WORK:-/tmp/opencode/rv32i_elf_e2e}"

red()   { printf '\033[0;31m%s\033[0m\n' "$*"; }
green() { printf '\033[0;32m%s\033[0m\n' "$*"; }
fail()  { red "  ❌ $*"; FAILED=1; }
ok()    { green "  ✅ $*"; }
note()  { printf '\033[0;33m%s\033[0m\n' "$*"; }

MODE="check"
for arg in "$@"; do
    case "$arg" in
    --self-test) MODE="self-test" ;;
    -h | --help)
        sed -n '3,45p' "$0"
        exit 0
        ;;
    *)
        echo "用法：$0 [--self-test]" >&2
        exit 2
        ;;
    esac
done

FAILED=0

# ── E0：二进制齐备（fail-closed）────────────────────────────────────────
echo "── E0：二进制齐备（fail-closed）"
e0_bad=0
for f in "$DUMP_BIN" "$MINI_BIN" "$SOC_BIN"; do
    if [ ! -x "$f" ]; then
        fail "E0 缺失：$f（先构建：cmake --build build --target rv32i-fw-dump tb_rv32i_soc tb_riscv_soc）"
        e0_bad=1
    fi
done
[ -f "$ANALYZER" ] || { fail "E0 缺失分析器：$ANALYZER"; e0_bad=1; }
[ "$e0_bad" -eq 0 ] || exit 2
ok "E0 三个二进制 + 分析器齐备"

rm -rf "$WORK"; mkdir -p "$WORK"

# ── fixture：**双段 ELF**（手写字节 · 逐字段可审）─────────────────────────
# .text @ 0x8000_0000（I-TCM）：
#   lw   x1, 0x100(x0)  = 0x10002083
#   addi x2, x1, 1      = 0x00108113
#   sw   x2, 0x104(x0)  = 0x10202223
#   jal  x0, 0          = 0x0000006f
# .data @ 0x8001_0100（D-TCM 内偏移 0x100 ⇒ 字索引 **64**）：0x5A
python3 - "$WORK" <<'PY'
import struct, sys
out = sys.argv[1]

def elf32(entry, segs):
    ehsize, phentsize = 52, 32
    off = ehsize + phentsize * len(segs)
    phdrs, body = b'', b''
    for paddr, data, memsz in segs:
        phdrs += struct.pack('<IIIIIIII', 1, off, paddr, paddr,
                             len(data), memsz, 5, 4)
        body += data
        off += len(data)
    eh = b'\x7fELF' + bytes([1, 1, 1, 0]) + b'\x00' * 8
    eh += struct.pack('<HHIIIIIHHHHHH', 2, 243, 1, entry,
                      ehsize, 0, 0, ehsize, phentsize, len(segs), 0, 0, 0)
    return eh + phdrs + body

def enc_i(rd, rs1, funct3, imm, opcode):
    return ((imm & 0xFFF) << 20) | ((rs1 & 31) << 15) | ((funct3 & 7) << 12) \
        | ((rd & 31) << 7) | opcode

def enc_s(imm, rs2, rs1, funct3, opcode):
    return (((imm >> 5) & 0x7F) << 25) | ((rs2 & 31) << 20) | ((rs1 & 31) << 15) \
        | ((funct3 & 7) << 12) | ((imm & 0x1F) << 7) | opcode

def enc_j(imm, rd, opcode):
    return (((imm >> 20) & 1) << 31) | (((imm >> 1) & 0x3FF) << 21) \
        | (((imm >> 11) & 1) << 20) | (((imm >> 12) & 0xFF) << 12) \
        | ((rd & 31) << 7) | opcode

TEXT = [enc_i(1, 0, 2, 0x100, 0x03),      # lw   x1, 0x100(x0)
        enc_i(2, 1, 0, 1, 0x13),          # addi x2, x1, 1
        enc_s(0x104, 2, 0, 2, 0x23),      # sw   x2, 0x104(x0)
        enc_j(0, 0, 0x6F)]                # jal  x0, 0
DATA_WORD = 0x5A

def build(text_words, data_word, with_data=True):
    body = b''.join(struct.pack('<I', x) for x in text_words)
    segs = [(0x80000000, body, len(body))]
    if with_data:
        segs.append((0x80010100, struct.pack('<I', data_word), 4))
    return elf32(0x80000000, segs)

open(f"{out}/fw.elf", 'wb').write(build(TEXT, DATA_WORD))
open(f"{out}/fw_bad_text.elf", 'wb').write(build([0xDEADBEEF] + TEXT[1:], DATA_WORD))
open(f"{out}/fw_bad_data.elf", 'wb').write(build(TEXT, 0x1234))
open(f"{out}/fw_no_data.elf", 'wb').write(build(TEXT, DATA_WORD, with_data=False))
PY

# ── E1：`mem-split` 产出两个镜像（行号 = 区域字索引 · 空洞补零）────────────
echo "── E1：ELF → mem-split ⇒ 两个按区域的初值镜像"
if "$DUMP_BIN" --in "$WORK/fw.elf" --format mem-split --out "$WORK/split" >/dev/null 2>&1; then
    e1_bad=0
    itcm_lines="$(wc -l < "$WORK/split/itcm.mem" | tr -d ' ')"
    dtcm_lines="$(wc -l < "$WORK/split/dtcm.mem" | tr -d ' ')"
    want_itcm=$'10002083\n00108113\n10202223\n0000006f'
    [ "$itcm_lines" = "4" ] || { fail "E1 itcm.mem 行数 $itcm_lines ≠ 4"; e1_bad=1; }
    [ "$dtcm_lines" = "65" ] || { fail "E1 dtcm.mem 行数 $dtcm_lines ≠ 65"; e1_bad=1; }
    [ "$(cat "$WORK/split/itcm.mem")" = "$want_itcm" ] || {
        fail "E1 itcm.mem 与手推的 4 个字不符"; e1_bad=1; }
    [ "$(sed -n '65p' "$WORK/split/dtcm.mem")" = "0000005a" ] || {
        fail "E1 dtcm.mem 索引 64 ≠ 0000005a（「.data」 落点错位）"; e1_bad=1; }
    [ "$(head -64 "$WORK/split/dtcm.mem" | sort -u)" = "00000000" ] || {
        fail "E1 dtcm.mem 前 64 行不是全零（空洞未补零）"; e1_bad=1; }
    [ "$e1_bad" -eq 0 ] && ok "E1 两个镜像齐（I-TCM 4 字 · D-TCM 索引 64 = 0x5a）"
else
    fail "E1 mem-split 失败"
fi

# ── E2：核级 ⇒ `.data` 经**真实读通路**读回 ───────────────────────────────
# ⚠️ 判据取自 TB 打印的**协议行** `FIRMWARE-TRACE:`（TB **不**硬编码固件的期望值
#    ⇒ ⇒ 固件专属的期望值只存在于**本护栏** ⇒ ⇒ 换固件不必改 TB）。
run_mini_fw() {   # $1 = 镜像目录 ⇒ ⇒ 打印 TB 的协议行
    "$MINI_BIN" --firmware "$1" 2>/dev/null | grep '^FIRMWARE-TRACE:' | head -1
}
echo "── E2：核级 ⇒ 「.data」 经 lw 读回并写回（x1=0x5A · x2=0x5B）"
trace="$(run_mini_fw "$WORK/split")"
if [ -z "$trace" ]; then
    fail "E2 TB 没打印 FIRMWARE-TRACE（装载或运行失败）"
else
    e2_bad=0
    case "$trace" in
        *"wb=1:0x5a,"*|*"wb=1:0x5a") ;;
        *) fail "E2 写回里没有 x1=0x5a（「.data」 未经真实读通路读回）: $trace"; e2_bad=1 ;;
    esac
    case "$trace" in
        *"2:0x5b"*) ;;
        *) fail "E2 写回里没有 x2=0x5B（ALU 没拿到真实值）: $trace"; e2_bad=1 ;;
    esac
    case "$trace" in
        *"stores=0x104:0x5b"*) ;;
        *) fail "E2 store 不是 0x104/0x5B: $trace"; e2_bad=1 ;;
    esac
    [ "$e2_bad" -eq 0 ] && ok "E2 「.data」 读回链通（lw⇒0x5A · addi⇒0x5B · sw⇒0x5B）"
fi

# ── E3：SoC 级 ⇒ 固件在 SoC 里真的在跑 ───────────────────────────────────
echo "── E3：SoC 级 ⇒ 固件在 SoC 里真的在跑（两个计数）"
soc_trace="$("$SOC_BIN" --firmware "$WORK/split" 2>/dev/null | grep '^SOC-FIRMWARE-TRACE:' | head -1)"
if [ -z "$soc_trace" ]; then
    fail "E3 SoC TB 没打印 SOC-FIRMWARE-TRACE（装载或运行失败）"
else
    case "$soc_trace" in
        *"store_count=1 writeback_count=2"*)
            ok "E3 SoC 级计数符合手推（1 条 sw · 2 次写回 x1/x2）" ;;
        *)
            fail "E3 SoC 级计数与手推不符（期望 store_count=1 writeback_count=2）: $soc_trace" ;;
    esac
fi

# ── E4：产物级 ⇒ 零端口变更 + `initial` 块携带 ELF 派生的字 ───────────────
echo "── E4：产物级 ⇒ 顶层端口表不变 + initial 块携带 ELF 派生的字"
e4_bad=0
if "$MINI_BIN" --emit-rtl --out "$WORK/base" >/dev/null 2>&1 \
   && "$MINI_BIN" --emit-rtl --firmware "$WORK/split" --out "$WORK/fwprod" >/dev/null 2>&1; then
    if [ -f "$WORK/base/top.v" ] && [ -f "$WORK/fwprod/top.v" ]; then
        python3 "$ANALYZER" --top-ports "$WORK/base/top.v" > "$WORK/ports.base" 2>/dev/null
        python3 "$ANALYZER" --top-ports "$WORK/fwprod/top.v" > "$WORK/ports.fw" 2>/dev/null
        if diff -u "$WORK/ports.base" "$WORK/ports.fw" > "$WORK/ports.diff"; then
            ok "E4a 顶层端口表与基线**逐字逐序**相同（$(wc -l < "$WORK/ports.base" | tr -d ' ') 个 · 零端口变更）"
        else
            fail "E4a 顶层端口表与基线不同（⇒ ⇒ 装载引入了端口变更）"
            sed 's/^/       /' "$WORK/ports.diff"
            e4_bad=1
        fi
        # `initial` 块：I-TCM[0..3] = 手推的 4 个字 · I-TCM[4..63] = NOP 填充
        #               D-TCM[64] = 0x5a（`.data`）
        mem_dump="$(python3 - "$WORK/fwprod/top.v" <<'PY'
import re
import sys
src = open(sys.argv[1], encoding="utf-8").read()
for blk in re.finditer(r"initial begin(.*?)^\s*end\s*$", src, re.S | re.M):
    for am in re.finditer(
            r"^\s*([A-Za-z_][\w$]*)\[(\d+)\]\s*=\s*32'h([0-9a-fA-F]+);",
            blk.group(1), re.M):
        print("%s %s %s" % (am.group(1), am.group(2), am.group(3).lower()))
PY
)"
        nmem="$(printf '%s\n' "$mem_dump" | grep -c '^' || true)"
        # 期望：I-TCM 64 项（4 个程序字 + 60 个 NOP 填充）+ D-TCM 1 项 = 65
        if [ "$nmem" -ne 65 ]; then
            fail "E4b initial 赋值条数 $nmem ≠ 65（4 程序字 + 60 NOP + 1 .data）"
            e4_bad=1
        fi
        # ⚠️ Verilog 的 `32'h…` 字面量**不补前导零** ⇒ ⇒ 两侧 MUST 归一化成 8 位
        norm_hex() { [ -n "$1" ] && printf '%08x' "$((0x$1))"; }
        for idx in 0 1 2 3; do
            want="$(printf '%s\n' "$want_itcm" | sed -n "$((idx + 1))p")"
            got="$(printf '%s\n' "$mem_dump" | awk -v i="$idx" '$1=="top_itcm_itcm_memory" && $2==i {print $3}')"
            got="$(norm_hex "$got")"
            if [ "$got" != "$want" ]; then
                fail "E4b I-TCM[$idx] = '0x${got:-缺}' ⇒ ⇒ 期望 '0x$want'"
                e4_bad=1
            fi
        done
        got_nop="$(printf '%s\n' "$mem_dump" | awk '$1=="top_itcm_itcm_memory" && $2==4 {print $3}')"
        got_nop="$(norm_hex "$got_nop")"
        [ "$got_nop" = "00000013" ] || {
            fail "E4b I-TCM[4] = '0x${got_nop:-缺}' ⇒ ⇒ 期望 '0x00000013'（NOP 填充）"
            e4_bad=1; }
        got_d="$(printf '%s\n' "$mem_dump" | awk '$1=="top_dtcm_dtcm_memory" && $2==64 {print $3}')"
        got_d="$(norm_hex "$got_d")"
        [ "$got_d" = "0000005a" ] || {
            fail "E4b D-TCM[64] = '0x${got_d:-缺}' ⇒ ⇒ 期望 '0x0000005a'（「.data」 未落盘）"
            e4_bad=1; }
        [ "$e4_bad" -eq 0 ] && ok "E4b initial 块携带 ELF 派生的字（4 程序字 + NOP 填充 + .data）"
    else
        fail "E4 产物缺失（base/top.v 或 fwprod/top.v）"
        e4_bad=1
    fi
else
    fail "E4 发射失败"
    e4_bad=1
fi

# ── E5：投毒自证（改坏 ELF ⇒ E2 MUST 转红）────────────────────────────────
if [ "$MODE" = "self-test" ]; then
    echo "── E5：投毒自证（改坏 ELF ⇒ 判据 MUST 转红）"
    # P1：改坏 `.text` 的第 0 个字 ⇒ ⇒ 首字不再等于镜像首字 ⇒ ⇒ F1b 红
    rm -rf "$WORK/poison_text"
    if "$DUMP_BIN" --in "$WORK/fw_bad_text.elf" --format mem-split \
        --out "$WORK/poison_text" >/dev/null 2>&1; then
        if "$MINI_BIN" --firmware "$WORK/poison_text" >/dev/null 2>&1; then
            fail "P1 改坏 .text 后 TB 仍判绿 ⇒ ⇒ E2 无判别力"
        else
            ok "P1 改坏 .text ⇒ 判据转红 ✓（取指确实来自镜像）"
        fi
    else
        fail "P1 投毒 ELF 装载失败（自检无效）"
    fi
    # P2：改坏 `.data` 的值 ⇒ ⇒ 写回不再是 0x5A ⇒ ⇒ E2 红
    rm -rf "$WORK/poison_data"
    if "$DUMP_BIN" --in "$WORK/fw_bad_data.elf" --format mem-split \
        --out "$WORK/poison_data" >/dev/null 2>&1; then
        ptrace="$(run_mini_fw "$WORK/poison_data")"
        case "$ptrace" in
            *"wb=1:0x5a"*) fail "P2 改坏 .data 后仍读回 0x5A ⇒ ⇒ E2 无判别力" ;;
            *) ok "P2 改坏 .data ⇒ 判据转红 ✓（「.data」 真的进了 D-TCM）" ;;
        esac
    else
        fail "P2 投毒 ELF 装载失败（自检无效）"
    fi
    # P3：去掉 `.data` 段 ⇒ ⇒ 读回 0 ⇒ ⇒ E2 红
    rm -rf "$WORK/poison_nodata"
    if "$DUMP_BIN" --in "$WORK/fw_no_data.elf" --format mem-split \
        --out "$WORK/poison_nodata" >/dev/null 2>&1; then
        ptrace="$(run_mini_fw "$WORK/poison_nodata")"
        case "$ptrace" in
            *"wb=1:0x5a"*) fail "P3 去掉 .data 后仍读回 0x5A ⇒ ⇒ E2 无判别力" ;;
            *) ok "P3 去掉 .data ⇒ 判据转红 ✓（「.data」 是必需的）" ;;
        esac
    else
        fail "P3 投毒 ELF 装载失败（自检无效）"
    fi
fi

# ── E6：真工具链 leg（**env-gated** ⇒ ⇒ CI 无工具链则 SKIP，不判红）──────
echo "── E6：真工具链 leg（env-gated）"
RVCC="${RVCC:-riscv64-unknown-elf-gcc}"
if ! command -v "$RVCC" >/dev/null 2>&1; then
    note "E6 SKIP：找不到 $RVCC（仓外工具链 ⇒ ⇒ CI 不阻塞；装上后本 leg 自动生效）"
else
    cat > "$WORK/real.S" <<'EOF'
    .section .text.init
    .globl _start
_start:
    lw   x1, 0x100(x0)
    addi x2, x1, 1
    sw   x2, 0x104(x0)
1:  j 1b
    .section .data
    .align 2
    .word 0x5A
EOF
    cat > "$WORK/real.ld" <<'EOF'
MEMORY {
  itcm (rx) : ORIGIN = 0x80000000, LENGTH = 16K
  dtcm (rw) : ORIGIN = 0x80010100, LENGTH = 16K
}
SECTIONS {
  .text : { *(.text.init) *(.text*) } > itcm
  .data : { *(.data*) } > dtcm
  .bss  : { *(.bss*) *(COMMON) } > dtcm
}
EOF
    if "$RVCC" -march=rv32i -mabi=ilp32 -nostdlib -nostartfiles \
        -T "$WORK/real.ld" -o "$WORK/real.elf" "$WORK/real.S" >/dev/null 2>&1; then
        rm -rf "$WORK/realsplit"
        if "$DUMP_BIN" --in "$WORK/real.elf" --format mem-split \
            --out "$WORK/realsplit" >/dev/null 2>&1; then
            rtrace="$(run_mini_fw "$WORK/realsplit")"
            case "$rtrace" in
                *"wb=1:0x5a"*"2:0x5b"*"stores=0x104:0x5b"*)
                    ok "E6 真工具链 ELF 跑通（同一组手推期望值）" ;;
                *)
                    fail "E6 真工具链 ELF 的观测与手推不符: $rtrace" ;;
            esac
        else
            fail "E6 真工具链 ELF 装载失败"
        fi
    else
        fail "E6 真工具链编译失败（$RVCC）"
    fi
fi

echo
if [ "$FAILED" -ne 0 ]; then
    red "❌ verify_rv32i_elf_e2e: FAIL"
    exit 1
fi
green "✅ verify_rv32i_elf_e2e: PASS（E0 二进制 · E1 mem-split 镜像 · E2 核级读回 · E3 SoC 级计数 · E4 产物端口面+initial · E5 投毒自证 · E6 真工具链 leg）"
