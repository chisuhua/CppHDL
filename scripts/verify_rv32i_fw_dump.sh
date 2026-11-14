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
# ⚠️ **为什么 ELF 是「手写字节」而不是仓内二进制 fixture**：
#    二进制 fixture 会随工具链版本漂移且**不可审阅**；手写字节 ⇒ ⇒
#    **逐字段可审** + 期望值**手推**（禁抄工具自己的输出）。
#
# ⚠️ **本护栏不需要 verilator / 网络** ⇒ ⇒ 主 job 可跑。

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

def elf32(entry, segs):
    """segs: [(paddr, data_bytes, memsz)]"""
    ehsize, phentsize = 52, 32
    off = ehsize + phentsize * len(segs)
    phdrs, body = b'', b''
    for paddr, data, memsz in segs:
        phdrs += struct.pack('<IIIIIIII', 1, off, paddr, paddr,
                             len(data), memsz, 5, 4)
        body += data
        off += len(data)
    eh  = b'\x7fELF' + bytes([1, 1, 1, 0]) + b'\x00' * 8          # e_ident
    eh += struct.pack('<HHIIIIIHHHHHH', 2, 243, 1, entry,          # ET_EXEC · EM_RISCV
                      ehsize, 0, 0, ehsize, phentsize, len(segs), 0, 0, 0)
    return eh + phdrs + body

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

echo
if [ "$FAILED" -ne 0 ]; then
    red "❌ verify_rv32i_fw_dump: FAIL"
    exit 1
fi
green "✅ verify_rv32i_fw_dump: PASS（D0 工具 · D1 手推字 · D2 manifest · D3 bss 补零 · D4 越界拒绝 · D5 三格式 · D6 无 examples 依赖）"
