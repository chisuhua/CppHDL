#!/usr/bin/env bash
# scripts/verify_rv32i_fw_dump.sh
#
# **RV32I 固件转储器 / ELF 装载器**的判据（`firmware-load-chain` **L4 / L5**）
#
# 判据（逐条可复跑 · 失败即 exit 1）：
#   D0 工具存在（缺 ⇒ **fail-closed**，不是 skip）
#   D1 ELF32（PT_LOAD）⇒ `mem` 输出 = **手推**的 3 个字
#      （`addi x1,x0,42` = 0x02a00093 · `nop` = 0x00000013 · `addi x2,x0,11` = 0x00b00113）
#   D2 `manifest` 携带 **入口** + **基址 / 尺寸** + 「字索引 → 字节地址」映射
#   D3 `.bss`（`p_memsz > p_filesz`）⇒ ⇒ **补零**（尾部全 0）
#   D4 **越界地址** ⇒ ⇒ **exit 3 + 诊断**（**禁**静默截断）
#   D5 `--format vh` 三种输出格式齐备
#   D6 工具 **不链 `examples/**`**（`ldd` / 构建依赖面）
#
# ⚠️ **2026-11-23 · 方案 a 批新增（D7–D12）** —— 此前 D0–D6 对**多区域固件**
#    **结构性失明**（fixture 只有**单段** ELF ⇒ ⇒ 下面 4 个真缺陷全绿）：
#   D7 **双段 ELF** ⇒ `manifest` 的 `regions[]` MUST **同时**含 I-TCM 与 D-TCM
#      （2026-11-23 实测缺陷：旧 `covered` 判据 ⇒ ⇒ 第 2 个区域永远进不去）
#   D8 **入口 ≠ `kPcReset`** ⇒ ⇒ **exit 3 + 诊断**（否则装出来**必砖**）
#   D9 **ELF 含非空 `SHT_RELA`/`SHT_REL`** ⇒ ⇒ **exit 3 + 诊断**
#      （此前是**空循环** ⇒ ⇒ 零判别力 ⇒ ⇒ 与 capability 的 MUST 冲突）
#   D10 **多区域固件 + `--format mem`** ⇒ ⇒ **exit 3 + 诊断**
#      （该格式字流**无地址** ⇒ ⇒ 消费者 MUST NOT 靠猜）
#   D11 `--format mem-split` ⇒ ⇒ 两个**按区域**的镜像 · **行号 = 区域字索引**
#      （空洞**补零** ⇒ ⇒ 索引与地址不错位）
#   D12 **反查**：**单区域**固件 + `--format mem` ⇒ ⇒ 仍然**成功**
#      （证明 D10 不是「一律拒绝」⇒ ⇒ D10 有判别力）
#
# ⚠️ **为什么 ELF 是「手写字节」而不是仓内二进制 fixture**：
#    二进制 fixture 会随工具链版本漂移且**不可审阅**；手写字节 ⇒ ⇒
#    **逐字段可审** + 期望值**手推**（禁抄工具自己的输出）。
#
# ⚠️ **本护栏不需要 verilator / 网络** ⇒ ⇒ 主 job 可跑。
#
# 用法：scripts/verify_rv32i_fw_dump.sh [--self-test]
#   `--self-test` ⇒ ⇒ **投毒自证**：把每条判据的「正」与「反」两面互换
#   ⇒ ⇒ 判定函数 MUST 各自转红（证明判据**有判别力**，不是恒绿）。

set -uo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT" || exit 2

BUILD_DIR="${BUILD_DIR:-$REPO_ROOT/build}"
TOOL="${TOOL:-$BUILD_DIR/tools/rv32i-fw-dump}"
WORK="${WORK:-/tmp/opencode/rv32i_fw_dump}"

red()   { printf '\033[0;31m%s\033[0m\n' "$*"; }
green() { printf '\033[0;32m%s\033[0m\n' "$*"; }
fail()  { red "  ❌ $*"; FAILED=1; }
ok()    { green "  ✅ $*"; }

MODE="check"
for arg in "$@"; do
    case "$arg" in
    --self-test) MODE="self-test" ;;
    -h | --help)
        sed -n '3,40p' "$0"
        exit 0
        ;;
    *)
        echo "用法：$0 [--self-test]" >&2
        exit 2
        ;;
    esac
done

FAILED=0

echo "── D0：工具存在（fail-closed）"
if [ ! -x "$TOOL" ]; then
    red "❌ D0 工具缺失：$TOOL（先构建：cmake --build $BUILD_DIR --target rv32i-fw-dump）"
    exit 2
fi
ok "D0 工具存在：$TOOL"

rm -rf "$WORK"; mkdir -p "$WORK"

# ── 生成 3 个 ELF（**手写字节** · 逐字段可审）──────────────────────────────
# ELF32 LE: e_ident(16) e_type(2) e_machine(2) e_version(4) e_entry(4)
#           e_phoff(4) e_shoff(4) e_flags(4) e_ehsize(2) e_phentsize(2)
#           e_phnum(2) e_shentsize(2) e_shnum(2) e_shstrndx(2)  = 52 字节
# PHDR(32): p_type(4) p_offset(4) p_vaddr(4) p_paddr(4) p_filesz(4)
#           p_memsz(4) p_flags(4) p_align(4)
python3 - "$WORK" <<'PY'
import struct, sys
out = sys.argv[1]

def elf32(entry, segs, sections=()):
    """segs: [(paddr, data_bytes, memsz)]
       sections: [(sh_type, sh_size, payload_bytes)] —— 可选（造 SHT_RELA 用的）"""
    ehsize, phentsize, shentsize = 52, 32, 40
    off = ehsize + phentsize * len(segs)
    phdrs, body = b'', b''
    for paddr, data, memsz in segs:
        phdrs += struct.pack('<IIIIIIII', 1, off, paddr, paddr,
                             len(data), memsz, 5, 4)
        body += data
        off += len(data)
    # ── section header（可选）──
    # SHDR(40): sh_name(4) sh_type(4) sh_flags(4) sh_addr(4) sh_offset(4)
    #           sh_size(4) sh_link(4) sh_info(4) sh_addralign(4) sh_entsize(4)
    shoff, shnum, shdrs = 0, 0, b''
    if sections:
        for sh_type, sh_size, payload in sections:
            shdrs += struct.pack('<IIIIIIIIII', 0, sh_type, 0, 0,
                                 off, sh_size, 0, 0, 4, 0)
            body += payload
            off += len(payload)
        shnum = len(sections)
        shoff = off
    eh  = b'\x7fELF' + bytes([1, 1, 1, 0]) + b'\x00' * 8          # e_ident
    eh += struct.pack('<HHIIIIIHHHHHH', 2, 243, 1, entry,          # ET_EXEC · EM_RISCV
                      ehsize, shoff, 0, ehsize, phentsize, len(segs),
                      shentsize, shnum, 0)
    return eh + phdrs + body + shdrs

def enc_i(rd, rs1, funct3, imm, opcode):
    return ((imm & 0xFFF) << 20) | (rs1 << 15) | (funct3 << 12) | (rd << 7) | opcode

# 手推的 3 条指令
w = [enc_i(1, 0, 0, 42, 0x13),      # addi x1, x0, 42   = 0x02a00093
     enc_i(0, 0, 0, 0, 0x13),       # nop               = 0x00000013
     enc_i(2, 0, 0, 11, 0x13)]      # addi x2, x0, 11   = 0x00b00113
data = b''.join(struct.pack('<I', x) for x in w)

open(f"{out}/fw_ok.elf", 'wb').write(elf32(0x80000000, [(0x80000000, data, len(data))]))
open(f"{out}/fw_bss.elf", 'wb').write(elf32(0x80000000, [(0x80000000, data, len(data) + 64)]))
open(f"{out}/fw_oob.elf", 'wb').write(elf32(0x90000000, [(0x90000000, data, len(data))]))

# ── **双段 ELF**（I-TCM 的 .text + D-TCM 的 .data）────────────────────────
# ⚠️ 期望值**手推**（禁抄工具输出）：
#   lw   x1, 0x100(x0)  = I 型 · imm=0x100 · rs1=x0 · funct3=010 · rd=x1
#                        = (0x100<<20)|(0<<15)|(2<<12)|(1<<7)|0b0000011
#                        = 0x10002083
#   addi x2, x1, 1      = (1<<20)|(1<<15)|(0<<12)|(2<<7)|0b0010011 = 0x00108113
#   sw   x2, 0x104(x0)  = S 型 · imm[11:5]=8 · imm[4:0]=4 · rs2=x2 · rs1=x0
#                        = (8<<25)|(2<<20)|(0<<15)|(2<<12)|(4<<7)|0b0100011
#                        = 0x10202223
#   jal  x0, 0          = J 型 · imm=0 · rd=x0 = 0x0000006f
def enc_s(imm, rs2, rs1, funct3, op):
    return (((imm >> 5) & 0x7F) << 25) | ((rs2 & 31) << 20) | ((rs1 & 31) << 15) \
        | ((funct3 & 7) << 12) | ((imm & 0x1F) << 7) | op

def enc_j(imm, rd, op):
    return (((imm >> 20) & 1) << 31) | (((imm >> 1) & 0x3FF) << 21) \
        | (((imm >> 11) & 1) << 20) | (((imm >> 12) & 0xFF) << 12) | ((rd & 31) << 7) | op

text4 = b''.join(struct.pack('<I', x) for x in [
    enc_i(1, 0, 2, 0x100, 0x03),     # lw   x1, 0x100(x0)
    enc_i(2, 1, 0, 1, 0x13),         # addi x2, x1, 1
    enc_s(0x104, 2, 0, 2, 0x23),     # sw   x2, 0x104(x0)
    enc_j(0, 0, 0x6F),               # jal  x0, 0
])
data1 = struct.pack('<I', 0x5A)      # `.data` 的字 = 0x5A（手推）
# ⚠️ `.data` 落在 D-TCM 的**区域内偏移 0x100** ⇒ ⇒ 字索引 = 0x100/4 = **64**
open(f"{out}/fw_two.elf", 'wb').write(elf32(
    0x80000000, [(0x80000000, text4, len(text4)), (0x80010100, data1, len(data1))]))

# ⚠️ **入口 ≠ kPcReset**（= 0x80000000）⇒ ⇒ MUST 被拒绝（否则装出来必砖）
open(f"{out}/fw_badentry.elf", 'wb').write(elf32(
    0x80010000, [(0x80000000, data, len(data))]))

# ⚠️ **含非空 SHT_RELA(4) 节** ⇒ ⇒ MUST 被拒绝（此前是空循环 ⇒ 零判别力）
open(f"{out}/fw_rela.elf", 'wb').write(elf32(
    0x80000000, [(0x80000000, data, len(data))],
    sections=[(4, 24, b'\x00' * 24)]))
# ⚠️ **同一 ELF，但 RELA 段的 size = 0** ⇒ ⇒ MUST 被接受（自检用：证明 D9 的
#    拒绝**确实**由「非空重定位表」引起 ⇒ ⇒ D9 有判别力）
open(f"{out}/fw_rela_empty.elf", 'wb').write(elf32(
    0x80000000, [(0x80000000, data, len(data))],
    sections=[(4, 0, b'')]))
PY
[ -f "$WORK/fw_ok.elf" ] || { red "❌ 无法生成 ELF fixture"; exit 2; }

echo "── D1：ELF（PT_LOAD）⇒ mem 输出 = **手推**的 3 个字"
if "$TOOL" --in "$WORK/fw_ok.elf" --format mem --out "$WORK/ok.mem" >/dev/null 2>&1; then
    want=$'02a00093\n00000013\n00b00113'
    got="$(cat "$WORK/ok.mem")"
    if [ "$got" = "$want" ]; then
        ok "D1 mem = 3 个**手推**字（0x02a00093 / 0x00000013 / 0x00b00113）"
    else
        fail "D1 mem 内容不符 ⇒ ⇒ 期望 [$want] · 实得 [$got]"
    fi
else
    fail "D1 ELF → mem 失败"
fi

echo "── D2：manifest 携带 入口 + 基址/尺寸 + 字索引→字节地址 映射"
if "$TOOL" --in "$WORK/fw_ok.elf" --format manifest --out "$WORK/ok.json" >/dev/null 2>&1; then
    m="$(cat "$WORK/ok.json")"
    d2=0
    # 入口 = 0x80000000 = 2147483648
    printf '%s' "$m" | grep -q '"entry": 2147483648' || { fail "D2 缺入口（entry=2147483648）"; d2=1; }
    # 基址 2147483648 · 尺寸 16384（= 1<<(12+2) ⇒ **派生**，不是手写的 64 KB）
    printf '%s' "$m" | grep -q '"base": 2147483648' || { fail "D2 缺基址"; d2=1; }
    printf '%s' "$m" | grep -q '"size": 16384' || { fail "D2 尺寸不是 16384（16 KB ⇒ 派生自 kTcmAddrWidth）"; d2=1; }
    # 字索引→字节地址：第 2 个字的 addr = base + 4
    printf '%s' "$m" | grep -q '"addr": 2147483652' || { fail "D2 缺「字索引 → 字节地址」映射（base+4）"; d2=1; }
    [ "$d2" -eq 0 ] && ok "D2 manifest 齐（entry / base / size=16KB / 逐字 addr）"
else
    fail "D2 manifest 生成失败"
fi

echo "── D3：.bss（p_memsz > p_filesz）⇒ 尾部补零"
if "$TOOL" --in "$WORK/fw_bss.elf" --format mem --out "$WORK/bss.mem" >/dev/null 2>&1; then
    n="$(wc -l < "$WORK/bss.mem")"
    tail_zeros="$(tail -16 "$WORK/bss.mem" | sort -u)"
    if [ "$n" -eq 19 ] && [ "$tail_zeros" = "00000000" ]; then
        ok "D3 .bss 补零（19 字 = 3 字代码 + 16 字零）"
    else
        fail "D3 .bss 补零不符（行数=$n · 尾部唯一值=[$tail_zeros]）"
    fi
else
    fail "D3 .bss ELF 装载失败"
fi

echo "── D4：越界地址 ⇒ exit 3 + 诊断（禁静默截断）"
"$TOOL" --in "$WORK/fw_oob.elf" --format mem --out "$WORK/oob.mem" >/dev/null 2>"$WORK/oob.err"
rc=$?
if [ "$rc" -eq 3 ] && grep -q '之外' "$WORK/oob.err"; then
    ok "D4 越界被拒绝（exit 3 + 诊断）"
else
    fail "D4 越界**没有**被拒绝（exit=$rc · 期望 3）⇒ ⇒ 截断会让固件装到错误的位置"
fi

echo "── D5：三种输出格式齐备"
d5=0
for f in mem vh manifest; do
    "$TOOL" --in "$WORK/fw_ok.elf" --format "$f" --out "$WORK/fmt.$f" >/dev/null 2>&1 \
        || { fail "D5 --format $f 失败"; d5=1; }
    [ -s "$WORK/fmt.$f" ] || { fail "D5 --format $f 产物为空"; d5=1; }
done
[ "$d5" -eq 0 ] && ok "D5 mem / vh / manifest 三种格式均产出非空"

echo "── D6：工具不链 examples/**（C1 纪律）"
if ldd "$TOOL" 2>/dev/null | grep -q 'examples'; then
    fail "D6 工具链到了 examples/** ⇒ ⇒ 违反 C1"
else
    ok "D6 工具未链 examples/**（构建面独立）"
fi

# ── 判定函数（**单独成函数** ⇒ ⇒ 自检能把「被投毒的一面」直接喂进来）───────

# check_two_regions <manifest> ⇒ 0 = I-TCM + D-TCM **两区域齐** / 1 = 缺
check_two_regions() {
    printf '%s' "$1" | grep -q '"base": 2147483648' || return 1   # 0x8000_0000
    printf '%s' "$1" | grep -q '"base": 2147549184' || return 1   # 0x8001_0000
    return 0
}

# check_memsplit <dir> ⇒ 0 = 齐 / 1 = 缺
#   ⚠️ 期望值**手推**：I-TCM 4 个字（编码推导见 fixture 生成器）·
#     D-TCM 的字索引 **64**（= 区域内偏移 0x100 ÷ 4）= 0x5a · 前 64 行全零
check_memsplit() {
    local dir="$1"
    local itcm_lines dtcm_lines
    itcm_lines="$(wc -l < "$dir/itcm.mem" | tr -d ' ')"
    dtcm_lines="$(wc -l < "$dir/dtcm.mem" | tr -d ' ')"
    [ "$itcm_lines" = "4" ] || { echo "    itcm.mem 行数 $itcm_lines ≠ 4"; return 1; }
    [ "$dtcm_lines" = "65" ] || { echo "    dtcm.mem 行数 $dtcm_lines ≠ 65"; return 1; }
    local want_itcm
    want_itcm=$'10002083\n00108113\n10202223\n0000006f'
    [ "$(cat "$dir/itcm.mem")" = "$want_itcm" ] || {
        echo "    itcm.mem 内容与手推的 4 个字不符"; return 1; }
    [ "$(sed -n '65p' "$dir/dtcm.mem")" = "0000005a" ] || {
        echo "    dtcm.mem 索引 64 ≠ 0000005a（`.data` 的落点错位）"; return 1; }
    [ "$(head -64 "$dir/dtcm.mem" | sort -u)" = "00000000" ] || {
        echo "    dtcm.mem 前 64 行不是全零（空洞未补零 ⇒ ⇒ 索引会错位）"; return 1; }
    return 0
}

# ── D7：双段 ELF ⇒ manifest 的 regions[] MUST **同时**含 I-TCM 与 D-TCM ──────
echo "── D7：双段 ELF ⇒ manifest 登记**两个**区域"
if "$TOOL" --in "$WORK/fw_two.elf" --format manifest --out "$WORK/two.json" >/dev/null 2>&1; then
    if check_two_regions "$(cat "$WORK/two.json")"; then
        ok "D7 manifest 的 regions[] 含 I-TCM + D-TCM（0x80000000 + 0x80010000）"
    else
        fail "D7 manifest 缺区域 ⇒ ⇒ 旧缺陷：第 2 个区域永远进不去（regions[] 与 words[] 自相矛盾）"
        sed 's/^/       /' "$WORK/two.json"
    fi
else
    fail "D7 双段 ELF 装载失败"
fi

# ── D8：入口 ≠ kPcReset ⇒ ⇒ exit 3 + 诊断 ──────────────────────────────────
echo "── D8：入口 ≠ I-TCM base ⇒ exit 3 + 诊断（否则装出来必砖）"
"$TOOL" --in "$WORK/fw_badentry.elf" --format manifest --out "$WORK/be.json" >/dev/null 2>"$WORK/be.err"
rc=$?
if [ "$rc" -eq 3 ] && grep -q '入口' "$WORK/be.err"; then
    ok "D8 入口不符被拒绝（exit 3 + 诊断）"
else
    fail "D8 入口不符**没有**被拒绝（exit=$rc · 期望 3）⇒ ⇒ 装出来的固件无法启动"
fi

# ── D9：含非空 SHT_RELA ⇒ ⇒ exit 3 + 诊断 ─────────────────────────────────
echo "── D9：非空重定位表 ⇒ exit 3 + 诊断（此前是空循环 ⇒ 零判别力）"
"$TOOL" --in "$WORK/fw_rela.elf" --format manifest --out "$WORK/rl.json" >/dev/null 2>"$WORK/rl.err"
rc=$?
if [ "$rc" -eq 3 ] && grep -q '重定位' "$WORK/rl.err"; then
    ok "D9 非空重定位表被拒绝（exit 3 + 诊断）"
else
    fail "D9 非空重定位表**没有**被拒绝（exit=$rc · 期望 3）⇒ ⇒ 静默忽略会产出「看起来装好了、实际地址全错」的固件"
fi

# ── D10：多区域固件 + `--format mem` ⇒ ⇒ exit 3 + 诊断 ─────────────────────
echo "── D10：多区域固件 + --format mem ⇒ exit 3（字流无地址 ⇒ 禁靠猜）"
"$TOOL" --in "$WORK/fw_two.elf" --format mem --out "$WORK/two.mem" >/dev/null 2>"$WORK/two.err"
rc=$?
if [ "$rc" -eq 3 ] && grep -q '多区域' "$WORK/two.err"; then
    ok "D10 多区域 + mem 被拒绝（exit 3 + 诊断）"
else
    fail "D10 多区域 + mem **没有**被拒绝（exit=$rc · 期望 3）⇒ ⇒ `.data` 会被静默装进 I-TCM"
fi

# ── D11：`--format mem-split` ⇒ ⇒ 两个按区域的镜像（行号 = 区域字索引）──────
echo "── D11：--format mem-split ⇒ 两个按区域的镜像（空洞补零）"
if "$TOOL" --in "$WORK/fw_two.elf" --format mem-split --out "$WORK/split" >/dev/null 2>&1; then
    if check_memsplit "$WORK/split"; then
        ok "D11 mem-split 齐（I-TCM 4 字 + D-TCM 索引 64 = 0x5a · 前 64 行零）"
    else
        fail "D11 mem-split 内容/索引不符"
    fi
else
    fail "D11 mem-split 产出失败"
fi

# ── D12：反查 —— **单区域**固件 + `--format mem` ⇒ ⇒ 仍然成功 ───────────────
echo "── D12：反查 ⇒ 单区域固件 + --format mem 仍然成功（D10 不是一律拒绝）"
if "$TOOL" --in "$WORK/fw_ok.elf" --format mem --out "$WORK/ok2.mem" >/dev/null 2>&1; then
    ok "D12 单区域 + mem 仍成功（3 个手推字）"
else
    fail "D12 单区域 + mem 失败 ⇒ ⇒ D10 把合法用法也拒了（判据过宽）"
fi

# ── 投毒自证（--self-test）────────────────────────────────────────────────
# ⚠️ 思路：把每条判据的「正」「反」两面**互换**喂给判定 ⇒ ⇒ 各自必红
#    ⇒ ⇒ 证明判据**有判别力**（不是恒绿出口）。
if [ "$MODE" = "self-test" ]; then
    echo "── S：投毒自证（正反互换 ⇒ ⇒ 各自必红）"

    # S-D7：把 good manifest 的 D-TCM 区域**删掉** ⇒ ⇒ 判定 MUST 转红
    if check_two_regions "$(cat "$WORK/two.json")"; then
        poisoned="$(grep -v '"base": 2147549184' "$WORK/two.json")"
        if check_two_regions "$poisoned"; then
            fail "S-D7 删掉 D-TCM 区域后判定仍绿 ⇒ ⇒ D7 无判别力"
        else
            ok "S-D7 删掉 D-TCM 区域 ⇒ 判定转红 ✓（D7 有判别力）"
        fi
    else
        fail "S-D7 前置条件不成立（good manifest 本就不含两区域）"
    fi

    # S-D8：同一段内容、入口改对 ⇒ ⇒ MUST 被接受（证明 D8 的拒绝**确实**由入口引起）
    if "$TOOL" --in "$WORK/fw_ok.elf" --format manifest --out "$WORK/ok3.json" >/dev/null 2>&1; then
        ok "S-D8 入口改对的同一内容被接受 ✓（D8 的拒绝确实由入口引起）"
    else
        fail "S-D8 入口改对后仍被拒绝 ⇒ ⇒ D8 的拒绝原因判错"
    fi

    # S-D9：RELA 段的 size 改 0 ⇒ ⇒ MUST 被接受（证明 D9 的拒绝**确实**由「非空」引起）
    if "$TOOL" --in "$WORK/fw_rela_empty.elf" --format manifest --out "$WORK/rle.json" >/dev/null 2>&1; then
        ok "S-D9 空重定位表被接受 ✓（D9 的拒绝确实由「非空」引起）"
    else
        fail "S-D9 空重定位表仍被拒绝 ⇒ ⇒ D9 把「无重定位」也拒了（判据过宽）"
    fi

    # S-D10：同一 ELF 换 `--format mem-split` ⇒ ⇒ MUST 被接受
    if "$TOOL" --in "$WORK/fw_two.elf" --format mem-split --out "$WORK/split2" >/dev/null 2>&1; then
        ok "S-D10 同一 ELF 换 mem-split 被接受 ✓（D10 的拒绝确实由「格式 + 多区域」引起）"
    else
        fail "S-D10 同一 ELF 换 mem-split 仍被拒绝 ⇒ ⇒ D10 的拒绝原因判错"
    fi

    # S-D11：把 D-TCM 的值**挪到索引 0** ⇒ ⇒ 判定 MUST 转红（证明「索引 = 区域字索引」）
    bad_split="$WORK/split_bad"
    rm -rf "$bad_split"; mkdir -p "$bad_split"
    cp "$WORK/split/itcm.mem" "$bad_split/itcm.mem"
    { echo "0000005a"; tail -n +2 "$WORK/split/dtcm.mem"; } > "$bad_split/dtcm.mem"
    if check_memsplit "$bad_split"; then
        fail "S-D11 值挪到索引 0 后判定仍绿 ⇒ ⇒ D11 无判别力"
    else
        ok "S-D11 值挪到索引 0 ⇒ 判定转红 ✓（D11 的索引判据有判别力）"
    fi
fi

echo
if [ "$FAILED" -ne 0 ]; then
    red "❌ verify_rv32i_fw_dump: FAIL"
    exit 1
fi
green "✅ verify_rv32i_fw_dump: PASS（D0 工具 · D1 手推字 · D2 manifest · D3 bss 补零 · D4 越界拒绝 · D5 三格式 · D6 无 examples 依赖 · D7 双区域 · D8 入口校验 · D9 重定位拒绝 · D10 mem 多区域拒绝 · D11 mem-split 索引 · D12 反查）"
