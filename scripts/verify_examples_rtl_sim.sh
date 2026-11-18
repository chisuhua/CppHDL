#!/usr/bin/env bash
# scripts/verify_examples_rtl_sim.sh
#
# **首批 examples 的独立 Verilog 仿真护栏**（`examples-rtl-sim-coverage` **E5**）。
#
# 判据（逐项，缺一即红）：
#   ⓪ **登记数守恒** —— 数据行数 MUST == **三桶之和**
#      （`REGISTERED_TARGETS` + `REGISTERED_SOC` + `REGISTERED_HIER`）
#      ⇒ ⇒ 防「删一行 ⇒ 静默缩面」（与仓内计数锚点同型）
#   ① **`status` 参与判定** —— `pending` 行**红**（除非 `--allow-pending` 且未超预算）
#      ⇒ ⇒ ⚠️ 旧实现**从不读 status** ⇒ `pending` 行只要有 TB 能打 `RESULT` 就转绿
#      （那是「缺失 = 失败」的静默旁路，已修）
#   ② TB 文件存在
#   ③ 发射器存在且能产出 `top.v`
#   ④ TB **能编译**
#   ⑤ TB **跑过**且退出码 0（**带 timeout** —— 挂死 TB 会拖死 CI job）
#   ⑥ 断言**非空且达到下界** —— 运行时证据 `RESULT: checks=<N> fails=0`
#      且 **N >= `min_checks`**（⚠️ 只判 N>0 太弱：删到 1 条断言仍绿）
#   ⑧ **四件套判据**（ADR-072 D2 · T13 / AC8）—— manifest 的**产品端口表**
#      MUST == 该单元产物端口表（**逐字逐序**）⇒ ⇒ 缺 manifest / 缺 `port_table[]`
#      一律 fail-closed（schema v1 清单没有该字段 ⇒ **不**静默回退旧算法）
#   ⑧' **层次化形态判据**（`kind=hier`）—— 产物 MUST 含 **≥ 2** 个 module
#      且含根 module `top` ⇒ ⇒ 否则「层次化」已静默退化成扁平
#      （⚠️ ⑧ 对这条**结构性失明**：退化后端口表**照样相同**）
#   ⑦ `--self-test`：**18 条**投毒**各自**必红 + **2 条**必绿（2026-11-18 · 计数会漂，
#      以本注释与末尾 PASS 行为准）。其中 ⑧ 的**正负成对**两向（篡改 `port_table`
#      一个名字 / 产物多插一个下沉端口）+ ⑧' 的**两向**（`kind=hier` 压成单 module ·
#      `kind=soc` 压成单 module ⇒ 2026-11-18 起 ⑧' 也覆盖 soc）
#
# ⚠️ **禁**自研 Verilog 静态分析器判「断言非空」
#   （草案 metis 订正：仓内那个自研 C 子集分析器在第 4/5/6 轮对抗审稿里
#     **全部** BLOCKER 出在它身上 ⇒ ⇒ 同一类东西**不**再做第二套）
#
# 用法：
#   scripts/verify_examples_rtl_sim.sh                     # 严格（CI 用）
#   scripts/verify_examples_rtl_sim.sh --list              # 只列清单
#   scripts/verify_examples_rtl_sim.sh --self-test         # 18 条投毒 + 2 条必绿自检
#   scripts/verify_examples_rtl_sim.sh --allow-pending --budget=6
#        # 允许 `pending` 的行数 ≤ 6（**E6 接 CI 的过渡形态**：
#        #  预算 MUST 随进度**单调下降** ⇒ ⇒ 忘更新就从「预算超额」变红）

set -uo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT"

BUILD_DIR="${BUILD_DIR:-build}"
# ⚠️ 可覆盖（`--self-test` 用临时清单 + 临时 TB 树）
LIST="${RTL_SIM_LIST:-$REPO_ROOT/.rddf/checks/examples-rtl-sim-targets.tsv}"
WORK_ROOT="${RTL_SIM_WORK:-${TMPDIR:-/tmp/opencode}/examples_rtl_sim}"
TB_TIMEOUT="${TB_TIMEOUT:-120}"

MODE="check"
ALLOW_PENDING=0
PENDING_BUDGET=-1
for arg in "$@"; do
    case "$arg" in
    --list) MODE="list" ;;
    --self-test) MODE="self-test" ;;
    --allow-pending) ALLOW_PENDING=1 ;;
    --budget=*) PENDING_BUDGET="${arg#--budget=}" ;;
    -h | --help)
        sed -n '3,40p' "$0"
        exit 0
        ;;
    *)
        echo "用法：$0 [--list|--self-test|--allow-pending --budget=<N>]" >&2
        exit 2
        ;;
    esac
done

FAILED=0
CHECKED=0
PENDING_SEEN=0

fail() { echo "  [0;31m❌ $*[0m"; FAILED=$((FAILED + 1)); }
ok() { echo "  [0;32m✅ $*[0m"; }
info() { echo "  ── $*"; }

# ── 前置：清单与工具 ────────────────────────────────────────────────
if [ ! -f "$LIST" ]; then
    echo "::error::清单缺失：$LIST" >&2
    exit 2
fi

find_verilator() {
    if [ -x "$BUILD_DIR/verilator-install/bin/verilator" ]; then
        echo "$BUILD_DIR/verilator-install/bin/verilator"
        return 0
    fi
    command -v verilator 2>/dev/null && return 0
    return 1
}
VERILATOR="$(find_verilator || true)"

data_rows() { grep -vE '^[[:space:]]*(#|$)' "$LIST"; }

# ── 列号 MUST 从 `#COLUMNS:` 表头推导（清单自己的规则）─────────────────
# ⚠️ 2026-11-17（`rtl-verification-single-source`）：旧实现按**位置**绑定
#   （`read -r a b c … note`）⇒ ⇒ 清单中间插入一列会**静默错列**（末列吸收剩余字段）
#   ⇒ ⇒ 「看起来还在跑，实际读错了列」⇒ ⇒ 这正是清单自己的 `#COLUMNS:` 规则要防的。
#   ⇒ ⇒ 现在：列名 → 列号（1-based），全部消费方按**名字**取字段。
COLS=""
load_columns() {
    local line need
    line="$(grep -m1 '^#COLUMNS:' "$LIST" || true)"
    if [ -z "$line" ]; then
        echo "::error::清单缺 \`#COLUMNS:\` 表头 ⇒ fail-closed（无法推导列号）" >&2
        return 1
    fi
    COLS="$(printf '%s' "${line#*#COLUMNS:}" | tr -s '[:space:]' ' ')"
    for need in target kind source component emitter emitter_args tb status generated \
        min_checks scenarios spec_checks note; do
        case " $COLS " in
        *" $need "*) ;;
        *)
            echo "::error::\`#COLUMNS\` 缺列 \`$need\`（实际：$COLS）" >&2
            return 1
            ;;
        esac
    done
    local -a _cols_arr=()
    read -r -a _cols_arr <<<"$COLS"
    NCOLS="${#_cols_arr[@]}"
    return 0
}

colidx() { # $1 = 列名 ⇒ 输出 1-based 列号（未命中 ⇒ 返回 1）
    local i=0 c
    for c in $COLS; do
        i=$((i + 1))
        if [ "$c" = "$1" ]; then
            echo "$i"
            return 0
        fi
    done
    return 1
}

load_columns || exit 2
C_TARGET="$(colidx target)"
C_KIND="$(colidx kind)"
C_SRC="$(colidx source)"
C_COMP="$(colidx component)"
C_EMITTER="$(colidx emitter)"
C_EMITTER_ARGS="$(colidx emitter_args)"
C_TB="$(colidx tb)"
C_STATUS="$(colidx status)"
C_GENERATED="$(colidx generated)"
C_MINCHECKS="$(colidx min_checks)"
C_SCENARIOS="$(colidx scenarios)"
C_SPEC="$(colidx spec_checks)"
C_NOTE="$(colidx note)"

# 取一行（tab 分隔）里的第 N 列（1-based）
fieldn() { # $1 = 整行；$2 = 列号
    printf '%s' "$1" | awk -F'\t' -v n="$2" '{print $n}'
}

# 把一行拆进 `F[]`（供 check_one 按列名取字段）
split_row() { # $1 = 整行 ⇒ 拆进 F[]；**字段数 != NCOLS ⇒ 具名 fail（return 2）**
    IFS=$'\t' read -r -a F <<<"$1"
    if [ "${#F[@]}" -ne "${NCOLS:-0}" ]; then
        echo "::error::清单行字段数 ${#F[@]} ≠ #COLUMNS 词数 ${NCOLS:-0}" >&2
        echo "::error::  行：$1" >&2
        echo "::error::  ⇒ 空单元 MUST 写 \`-\`（无参哨兵）" >&2
        return 2
    fi
    return 0
}

# ⓪ 登记数守恒（**三桶** · **fail-closed**：没有锚点行也算红）
# ⚠️ **三桶互斥**（2026-11-16 · `soc-rtl-product-and-tb-2026-11` T6）：
#   · `kind ∈ {target, dut}` 的行数 == `REGISTERED_TARGETS`（**首批 10** 的守卫）
#   · `kind = soc` 的行数 == `REGISTERED_SOC`（**SoC 层独立计数** ⇒ 不混进「首批 10」）
#   · `kind = hier` 的行数 == `REGISTERED_HIER`（**层次化形态独立计数** ⇒ 同理不混进）
#     ⇒ ⇒ 2026-11-17 新增：层次化产物（诉求 ⑤「层次化输出示例」）进产品面 ⇒ ⇒
#     它是**同一设计的另一种发射形态** ⇒ ⇒ 独立桶（不改「首批 10」的口径）
#   · 数据行总数 == 三桶之和；且每行的 `kind` MUST ∈ {target, dut, soc, hier}（未知 ⇒ 红）
check_registration() {
    local registered rec_soc rec_hier actual n_td n_soc n_hier n_unknown
    registered="$(grep -oE '^#[[:space:]]*REGISTERED_TARGETS:[[:space:]]*[0-9]+' "$LIST" | grep -oE '[0-9]+$' || true)"
    rec_soc="$(grep -oE '^#[[:space:]]*REGISTERED_SOC:[[:space:]]*[0-9]+' "$LIST" | grep -oE '[0-9]+$' || true)"
    rec_hier="$(grep -oE '^#[[:space:]]*REGISTERED_HIER:[[:space:]]*[0-9]+' "$LIST" | grep -oE '[0-9]+$' || true)"
    actual="$(data_rows | wc -l | tr -d ' ')"
    if [ -z "$registered" ]; then
        fail "⓪ 清单里没有 \`REGISTERED_TARGETS\` 锚点 ⇒ ⇒ 无法检测「删行缩面」"
        return 1
    fi
    if [ -z "$rec_soc" ]; then
        fail "⓪ 清单里没有 \`REGISTERED_SOC\` 锚点 ⇒ ⇒ 无法检测「SoC 行被删」"
        return 1
    fi
    if [ -z "$rec_hier" ]; then
        fail "⓪ 清单里没有 \`REGISTERED_HIER\` 锚点 ⇒ ⇒ 无法检测「层次化行被删」"
        return 1
    fi
    # ⚠️ 列号**从 `#COLUMNS` 推导**（禁位置绑定）
    n_td="$(data_rows | awk -F'\t' -v k="$C_KIND" '$k=="target" || $k=="dut"' | wc -l | tr -d ' ')"
    n_soc="$(data_rows | awk -F'\t' -v k="$C_KIND" '$k=="soc"' | wc -l | tr -d ' ')"
    n_hier="$(data_rows | awk -F'\t' -v k="$C_KIND" '$k=="hier"' | wc -l | tr -d ' ')"
    n_unknown="$(data_rows | awk -F'\t' -v k="$C_KIND" '$k!="target" && $k!="dut" && $k!="soc" && $k!="hier"' | wc -l | tr -d ' ')"
    if [ "$n_unknown" -ne 0 ]; then
        fail "⓪ 有 $n_unknown 行的 \`kind\` 不属于已知集合 {target, dut, soc, hier}"
        data_rows | awk -F'\t' -v k="$C_KIND" -v n1="$C_TARGET" '$k!="target" && $k!="dut" && $k!="soc" && $k!="hier" {print "      ::error::未知 kind ⇒ " $n1 " (kind=" $k ")"}'
        return 1
    fi
    if [ "$n_td" -ne "$registered" ]; then
        fail "⓪ 首批桶不守恒：kind∈{target,dut} 有 $n_td 行 ≠ REGISTERED_TARGETS=$registered"
        return 1
    fi
    if [ "$n_soc" -ne "$rec_soc" ]; then
        fail "⓪ SoC 桶不守恒：kind=soc 有 $n_soc 行 ≠ REGISTERED_SOC=$rec_soc"
        return 1
    fi
    if [ "$n_hier" -ne "$rec_hier" ]; then
        fail "⓪ 层次化桶不守恒：kind=hier 有 $n_hier 行 ≠ REGISTERED_HIER=$rec_hier"
        return 1
    fi
    if [ "$actual" -ne $((n_td + n_soc + n_hier)) ]; then
        fail "⓪ 三桶之和不等于数据行总数（$n_td + $n_soc + $n_hier ≠ $actual）"
        return 1
    fi
    if [ "$actual" -eq 0 ]; then
        echo "::error::清单数据行为 0 ⇒ ⇒ fail-closed（禁恒绿）" >&2
        exit 2
    fi
    return 0
}

if [ "$MODE" = "list" ]; then
    echo "── 首批 examples 的 RTL 仿真清单（$LIST）"
    printf "  %-30s %-8s %-9s %-5s %-6s %-6s %-6s %s\n" \
        "target" "kind" "status" "gen" "minck" "scen" "spec" "tb"
    while IFS= read -r row; do
        split_row "$row"
        printf "  %-30s %-8s %-9s %-5s %-6s %-6s %-6s %s\n" \
            "${F[$((C_TARGET - 1))]}" "${F[$((C_KIND - 1))]}" "${F[$((C_STATUS - 1))]}" \
            "${F[$((C_GENERATED - 1))]}" "${F[$((C_MINCHECKS - 1))]}" \
            "${F[$((C_SCENARIOS - 1))]}" "${F[$((C_SPEC - 1))]}" "${F[$((C_TB - 1))]}"
    done < <(data_rows)
    echo
    echo "  （status=pending ⇒ ⇒ 尚未补齐期望源 ⇒ ⇒ **护栏判红**）"
    exit 0
fi

check_registration || true

if [ -z "$VERILATOR" ]; then
    echo "::error::找不到 verilator（缺 $BUILD_DIR/verilator-install/bin/verilator 且 PATH 里没有）⇒ fail-closed" >&2
    exit 2
fi

mkdir -p "$WORK_ROOT"

# ── 单项判定（供 check 与 self-test 共用）────────────────────────────
# 返回 0 = 通过；非 0 = 失败（FAILED 已累加）
check_one() {
    local tgt="$1" kind="$2" src="$3" comp="$4" emitter="$5" tb="$6" status="$7"
    local gen="$8" minck="$9" scen="${10}" spec="${11}" note="${12}" eargs="${13}"
    # ⚠️ `emitter_args` = 空格分隔的 token 序列；`-` 哨兵 ⇒ 展开为空
    if [ "$eargs" = "-" ]; then eargs=""; fi
    CHECKED=$((CHECKED + 1))
    info "$tgt（$kind · $emitter · $tb）"

    # ① status 参与判定
    if [ "$status" = "pending" ]; then
        PENDING_SEEN=$((PENDING_SEEN + 1))
        if [ "$ALLOW_PENDING" -eq 0 ]; then
            fail "① status=pending ⇒ ⇒ 未补齐期望源（缺失 = 失败）"
            return 1
        fi
        # ⚠️ **放行 = 真的跳过**（**不**假绿：它仍然「未验证」）
        #   ⇒ ⇒ 让 E6 能提前接 CI 的唯一办法，代价是「只剩预算这一道防线」
        #   ⇒ ⇒ 预算 MUST 随进度**单调下降**（见 --budget 检查）
        info "  （status=pending · 已放行但**未验证** · pending 计数 $PENDING_SEEN）"
        return 0
    elif [ "$status" != "verified" ]; then
        fail "① 未知 status=$status（只允许 verified / pending）"
        return 1
    fi

    # ② TB 存在
    # ⚠️ `RTL_SIM_TB_OVERRIDE` **仅供 `--self-test`**（生产路径恒空）：
    #   让「TB 形态 / 残留」类判据的判别力自证**不必改动仓内 TB**。
    local tb_path="${RTL_SIM_TB_OVERRIDE:-$REPO_ROOT/$tb}"
    if [ ! -f "$tb_path" ]; then
        fail "② TB 文件缺失：$tb（缺失 = 失败，不跳过）"
        return 1
    fi

    # ②' **期望源出处**（P1-8）：TB 头部 MUST 有 `EXPECT-SRC:` 指向**登记文档**，
    #    且 **MUST NOT** 指向被测 `.cpp`（那是**转录**实现、不是推导 ⇒ ⇒ 循环论证）
    if ! grep -qE '^// EXPECT-SRC:' "$tb_path"; then
        fail "②' TB 头部缺 \`EXPECT-SRC:\`（期望值出处 MUST 登记）"
        return 1
    fi
    if grep -qE '^// EXPECT-SRC:.*\.cpp' "$tb_path"; then
        fail "②' \`EXPECT-SRC\` 指向 \`.cpp\` ⇒ ⇒ 转录实现 = 循环论证"
        return 1
    fi
    # ②′ (2) **锚点存在性**（`soc-rtl-product-and-tb-2026-11` T6 · AC7）：
    #   形如 `<file>#<anchor>` ⇒ **文件**与**锚点**都必须真的存在
    #   （否则「登记在册」只是注释里的一句话 ⇒ 指针可指向空气 ⇒ 判据不可追溯）。
    local esrc esrc_file esrc_anchor
    esrc="$(grep -m1 -E '^// EXPECT-SRC:' "$tb_path" | sed 's|^// EXPECT-SRC:[[:space:]]*||' | tr -d '\r')"
    if printf '%s' "$esrc" | grep -q '#'; then
        esrc_file="${esrc%%#*}"
        esrc_anchor="${esrc##*#}"
        if [ ! -f "$REPO_ROOT/$esrc_file" ]; then
            fail "②' \`EXPECT-SRC\` 指向的文件不存在：$esrc_file"
            return 1
        fi
        if ! grep -qF "{#${esrc_anchor}}" "$REPO_ROOT/$esrc_file"; then
            fail "②' \`EXPECT-SRC\` 的锚点不存在：$esrc_file#${esrc_anchor}"
            return 1
        fi
    fi

    # ②'' **spec_checks 软检查**（不失败，只告警）：
    #   「verified 但 spec_checks < 3」= 只有 impl-contract 断言
    #   ⇒ ⇒ 它只能**防漂移**，不能**证明符合外部规范**
    #   ⇒ ⇒ 不硬判是因为 `gpio_example` 的上游偏离**待裁决**（不能因裁决未定就阻塞）
    if [ "$status" = "verified" ] && [ "$spec" -lt 3 ]; then
        echo "  [0;33m⚠️  ②'' spec_checks=$spec < 3 ⇒ ⇒ impl-contract only（只能防漂移，不能证明符合规范）[0m"
    fi

    # ── ⑥⁗ **生成覆盖度三方一致**（`rtl-verification-single-source` · 2026-11-17）──
    #   三方：① 清单的 `generated` 列 ② TB 内**生成器标记**是否存在 ③ `.verif` 是否存在
    #   ⇒ ⇒ 任一侧单独变化 MUST 转红（这是「手写 TB 逃生口」的**唯一机械抓手**）。
    #   ⚠️ 约定：`.verif` 路径 = TB 路径的扩展名换成 `.verif`（同 stem）⇒ ⇒ 不新增列。
    local verif_path="${REPO_ROOT}/${tb%.sv}.verif"
    case "$gen" in
    yes)
        if ! grep -qF '@generated by cpphdl rtl_emit::verif' "$tb_path"; then
            fail "⑥⁗ generated=yes 但 TB 内**没有生成器标记**（应改回生成 / 或把本列改成 no 并写明原因）"
            return 1
        fi
        if [ ! -f "$verif_path" ]; then
            fail "⑥⁗ generated=yes 但验证源缺失：${verif_path#"$REPO_ROOT"/}"
            return 1
        fi
        ;;
    no)
        if [ -f "$verif_path" ]; then
            fail "⑥⁗ generated=no 但验证源存在（${verif_path#"$REPO_ROOT"/}）⇒ ⇒ 应改为生成"
            return 1
        fi
        # `note` MUST 含**非占位符**原因（长度下限 + 不得是纯占位符）
        case "$note" in
        *TODO* | *TBD* | *"待补"* | "")
            fail "⑥⁗ generated=no 的 \`note\` 是占位符（MUST 写明具体原因）"
            return 1
            ;;
        esac
        if [ "${#note}" -lt 20 ]; then
            fail "⑥⁗ generated=no 的 \`note\` 过短（${#note} 字符）⇒ ⇒ 不是具体原因"
            return 1
        fi
        ;;
    *)
        fail "⑥⁗ 未知 generated=$gen（只允许 yes / no）"
        return 1
        ;;
    esac
    ok "⑥⁗ 生成覆盖度三方一致（generated=$gen）"

    # ── ⑥'' / ⑥‴ 仅对**由验证源生成**的行生效 ──────────────────────────
    if [ "$gen" = "yes" ]; then
        # ⑥'' **期望源锚点 ↔ 验证源 scenario 双向 1:1**（粒度 = scenario）
        #   ⚠️ **本条只判「键集合」** ⇒ ⇒ 数值一致性 MUST NOT 被它的通过所承诺
        #   （数值由 ⑥ 的三方等式承担）。
        #   ⚠️ `.md` 的锚点载体 = `## … {#anchor}` 段落内的 `#### scenario: <名>` 行
        local vif="${RTL_SIM_VERIF:-$verif_path}"
        local md_path="${RTL_SIM_MD:-$REPO_ROOT/${esrc%%#*}}"
        if [ ! -f "$vif" ]; then
            fail "⑥'' 验证源缺失：$vif"
            return 1
        fi
        if [ ! -f "$md_path" ]; then
            fail "⑥'' 期望源文档缺失：$md_path"
            return 1
        fi
        local md_scen vf_scen md_checks
        md_scen="$(awk -v a="{#${esrc_anchor}}" '
            index($0, a) {inside = 1; next}
            inside && /^## / {inside = 0}
            inside && /^#### scenario: / {sub(/^#### scenario: /, ""); print}
        ' "$md_path" | sed 's/[[:space:]]*$//' | sort -u)"
        # ⚠️ **三方等式的第一边**（**人**登记的条数 ⇒ ⇒ 禁由生成器输出回填）
        md_checks="$(awk -v a="{#${esrc_anchor}}" '
            index($0, a) {inside = 1; next}
            inside && /^## / {inside = 0}
            inside && /^VERIF-CHECKS: / {sub(/^VERIF-CHECKS: /, ""); print; exit}
        ' "$md_path" | tr -d "[:space:]")"
        if [ -z "$md_checks" ]; then
            fail "⑥'' 期望源 \`${esrc}\` 段落里没有 \`VERIF-CHECKS:\` 行（三方等式缺第一边）"
            return 1
        fi
        vf_scen="$(grep -E '^@scenario ' "$vif" | sed 's/^@scenario[[:space:]]*//' |
            sed 's/[[:space:]]*$//' | sort -u)"
        if [ -z "$md_scen" ]; then
            fail "⑥'' 期望源 \`${esrc}\` 段落里没有 \`#### scenario:\` 锚点（双向键集合无从比对）"
            return 1
        fi
        if [ "$md_scen" != "$vf_scen" ]; then
            printf '%s\n' "$md_scen" | sed 's/^/      md: /'
            printf '%s\n' "$vf_scen" | sed 's/^/      vf: /'
            fail "⑥'' 期望源锚点集合 ≠ 验证源 scenario 集合（双向 1:1 破裂）"
            return 1
        fi
        ok "⑥'' 期望源锚点 == 验证源 scenario（$(printf '%s\n' "$vf_scen" | wc -l | tr -d ' ') 个）"

        # ⑥‴ **TB 无未完成残留**
        #   ⚠️ 只判「无 TODO」时**完全未生成**的产物会因「没有 TODO」而蒙混
        #   ⇒ ⇒ 两条 MUST 同时具备：① `// TODO:` 计数 == 0 ② 场景数 ≥ 1
        if grep -qE '//[[:space:]]*TODO:' "$tb_path"; then
            fail "⑥‴ TB 内仍有 \`// TODO:\` 残留（生成物 MUST 无未完成标记）"
            return 1
        fi
        local n_scen
        n_scen="$(grep -cE '^@scenario ' "$vif" || true)"
        if [ "$n_scen" -lt 1 ]; then
            fail "⑥‴ 验证源场景数为 0 ⇒ ⇒ 只连端口、无断言的产物不得通过"
            return 1
        fi
        if [ "$n_scen" -ne "$scen" ]; then
            fail "⑥‴ 验证源场景数 $n_scen ≠ 清单登记 scenarios=$scen"
            return 1
        fi
        # 每个断言行 MUST 带 `check_id`（形如 `<scenario>#<n>`）
        local bad_ids
        bad_ids="$(grep -oE '"[^"]*CHECK [^"]*"' "$tb_path" |
            grep -vE 'CHECK [A-Za-z0-9_]+#[0-9]+ (PASS|FAIL)' || true)"
        if [ -n "$bad_ids" ]; then
            printf '%s\n' "$bad_ids" | sed 's/^/      /'
            fail "⑥‴ TB 里有断言行**不带 check_id**（形如 \`CHECK <scenario>#<n> PASS|FAIL\`）"
            return 1
        fi
        ok "⑥‴ TB 无 TODO 残留 · 场景 $n_scen 个 · 每行断言都带 check_id"
    fi

    # ③ 发射器
    local emitter_bin=""
    while IFS= read -r cand; do
        if [ -x "$cand" ]; then
            emitter_bin="$cand"
            break
        fi
    done < <(find "$BUILD_DIR" -type f -name "$emitter" 2>/dev/null | sort)
    if [ -z "$emitter_bin" ]; then
        fail "③ 发射器缺失：$emitter（先构建：cmake --build $BUILD_DIR --target $emitter）"
        return 1
    fi
    local work="$WORK_ROOT/$tgt"
    rm -rf "$work"
    mkdir -p "$work"
    # ⚠️ **用卫生产物**（`--emit-hygiene`）：它**不带** blanket `lint_off`
    #   ⇒ ⇒ 任何宽度扩展/截断都会真的报出来
    #   ⇒ ⇒ 而默认产物用文件顶部的 `lint_off WIDTHEXPAND/WIDTHTRUNC` 把
    #   **23 条**（RV32I 实测）宽度问题盖住 ⇒ ⇒ 「宽度错位只 warning ⇒ 绿」
    #   正是 B2 想避免的缺口（metis 指出的「静态门失效」形态）。
    #   ⇒ ⇒ 实测：9 个 example 的卫生产物全部 `--lint-only` 零告警零错误。
    # ⚠️ `$eargs` **不加引号**（要按空格拆成多个 token）⇒ ⇒ 无参时为空（不传多余 token）
    if ! "$emitter_bin" $eargs --out "$work" --emit-hygiene >"$work/emit.log" 2>&1; then
        fail "③ 发射失败（见 $work/emit.log）"
        return 1
    fi
    local dut_v="$work/top.v"
    [ -f "$dut_v" ] || {
        fail "③ 产物缺失：$dut_v"
        return 1
    }

    # ── ⑧ **四件套判据**（ADR-072 **D2** · T13 / AC8）────────────────────
    #   「产品单元 = 四件套」 ⇒ ⇒ 本判据是它的**可执行载体**：
    #     · ① manifest 存在
    #     · ② 产物存在                                  ← ③
    #     · ③ 独立 TB 可跑且 `RESULT: checks=<N> fails=0` ← ⑤ + ⑥
    #     · ④ tsv 有行（`status` 合规）                  ← ⓪ + ①
    #   ⇒ ⇒ **本项只负责它们之间最硬的那条耦合**：manifest 的**产品端口表**
    #   MUST == 该单元 Verilog 产物的端口表（**逐字逐序**）。
    #   ⚠️ 为什么必须**逐字逐序**（不是集合）：下游 TB 按**名字**连线、按**位置**
    #     写断言，顺序错 = 「都对但对错位」⇒ ⇒ 只有顺序判据能抓住。
    #   ⚠️ 唯一权威 = `port_table[]`（schema v2）⇒ ⇒ 缺该字段 **fail-closed**
    #      （schema v1 清单**没有**它 ⇒ ⇒ 不能静默回退到旧算法）。
    local man="$work/manifest.json"
    if [ ! -f "$man" ]; then
        fail "⑧ manifest 缺失：$man（四件套的第 ① 件）"
        return 1
    fi

    # ⚠️ **仅供 `--self-test` 的判别力自证**（生产路径恒空）：给 ⑧ 的两个方向
    #    各投一次毒 ⇒ ⇒ MUST 转红。「删掉 manifest ⇒ 必红」只证明判据**会打开
    #    文件**，不证明它**区分对错** ⇒ ⇒ 所以这里投的是「名字错 1 个」与
    #    「产物多出 1 个下沉端口」。
    case "${RTL_SIM_TAMPER:-}" in
    port_table_name)
        python3 - "$man" <<'PY'
import json, pathlib, sys
p = pathlib.Path(sys.argv[1])
m = json.loads(p.read_text(encoding="utf-8"))
for e in m["port_table"]:
    if e["dir"] == "output":
        e["name"] = e["name"] + "_tampered"
        break
p.write_text(json.dumps(m, ensure_ascii=False, indent=2), encoding="utf-8")
PY
        ;;
    product_extra_port)
        python3 - "$dut_v" <<'PY'
import pathlib, re, sys
p = pathlib.Path(sys.argv[1])
t = p.read_text(encoding="utf-8")
t2 = re.sub(r"(module\s+\w+\s*\(\n)", r"\1    output [7:0] top_ghost_descendant_out,\n", t, count=1)
assert t2 != t, "投毒失败：找不到 module 头"
p.write_text(t2, encoding="utf-8")
PY
        ;;
    flatten_hier|flatten_soc)
        # ⚠️ **投毒「层次化退化成扁平」**（2026-11-17 · 层次化产品面）
        #   ⇒ 裁掉除根 module 外的**全部** module 定义 ⇒ ⇒ 产物只剩 1 个 module
        #   ⇒ ⇒ ⑧'（「层次化产物 MUST ≥ 2 个 module」）MUST 转红
        #   ⇒ ⇒ 裁剪复用**同一个分析器**（`flat_rtl_emit_check.py`）⇒ ⇒ 不自研解析器
        python3 "$REPO_ROOT/scripts/lib/flat_rtl_emit_check.py" \
            --strip-modules-except top "$dut_v" >"$dut_v.flat" 2>/dev/null
        if [ -s "$dut_v.flat" ]; then
            mv "$dut_v.flat" "$dut_v"
        else
            rm -f "$dut_v.flat"
        fi
        ;;
    esac

    local f8_out f8_rc
    # ⚠️ **层次化产物（`kind ∈ {soc, hier}`）是单文件多 module，且子 module 在前** ⇒ ⇒
    #    MUST 显式点名**根 module**（`top`）；扁平产物只有 `top` 一个 ⇒ 默认即可。
    local f8_mod=()
    if [ "$kind" = "soc" ] || [ "$kind" = "hier" ]; then
        f8_mod=(--module top)
    fi
    f8_out="$(python3 "$REPO_ROOT/scripts/lib/flat_rtl_emit_check.py" \
        --check-top-ports "$dut_v" "$man" "${f8_mod[@]}" 2>&1)"
    f8_rc=$?
    if [ "$f8_rc" -ne 0 ]; then
        printf '%s\n' "$f8_out" | sed 's/^/      /'
        fail "⑧ manifest 的 port_table[] 与产物端口表**不一致**（逐字逐序）"
        return 1
    fi
    ok "⑧ 四件套：manifest port_table[] == 产物端口表（逐字逐序）"

    # ── ⑧'' **TB MUST 可判定为「生成物」**（`generated=yes` 的行）──────────
    #   ⚠️ 两条 MUST **同时**满足 —— 只判「无未命名」时，「什么都没连」也会通过：
    #     ① 文件内 MUST NOT 出现 `<根module>_unnamed_*` 形态的字面量
    #     ② DUT 实例连线数 MUST == `port_table[]` 条目数（且逐条端口名 ∈ port_table）
    # ── AC2：**顶层端口身份**的 1:1 join（**所有行**）────────────────────────
    #   `port_table[].name` ↔ `ports[].cpp_member`（key = name）· 顶层端口 MUST 具名
    #   ⇒ ⇒ 判据面 = `role ∉ {clock, reset}` 且 `is_top_io == true`
    if ! python3 "$REPO_ROOT/scripts/lib/verif_tb_shape_check.py" \
        "$tb_path" "$man" join; then
        fail "⑧⁗ 顶层端口身份不是 1:1（未具名 / cpp_member 不唯一 / 集合不一致）"
        return 1
    fi
    ok "⑧⁗ 顶层端口身份 1:1（具名 · cpp_member 唯一 · 与 port_table 集合相等）"

    if [ "$gen" = "yes" ]; then
        if grep -qE "_unnamed_(input|output)" "$tb_path"; then
            fail "⑧'' TB 内出现 \`top_unnamed_*\` 字面量 ⇒ ⇒ 手写映射表残留"
            return 1
        fi
        if ! python3 "$REPO_ROOT/scripts/lib/verif_tb_shape_check.py" \
            "$tb_path" "$man" full; then
            fail "⑧'' TB 不是「按 manifest 生成」的形态（见上）"
            return 1
        fi
        ok "⑧'' TB 可判定为生成物（无 unnamed 字面量 · 连线 == port_table）"
    fi

    # ── ⑥' **生成物新鲜度**（`generated=yes` 的行）────────────────────────
    #   **逐字节重现**：对同一验证源 + 同一生成器重新生成 ⇒ MUST 与仓内文件**逐字节相同**
    #   ⇒ ⇒ 违反 ⇒ 「人改生成物忘改源」⇒ 红。
    #   ⚠️ **由生成器自身 bug 导致的「两侧同错」不受本条保护** ⇒ ⇒ 那由 ⑥ 的三方等式承担。
    if [ "$gen" = "yes" ]; then
        local regen="$work/regen"
        rm -rf "$regen"
        mkdir -p "$regen"
        if ! "$emitter_bin" $eargs --out "$regen" --emit-hygiene \
            --emit-tb-verif "$verif_path" --tb-verif-out "$regen/regen.sv" \
            >"$regen/emit.log" 2>&1; then
            fail "⑥' 重新生成 TB 失败（见 $regen/emit.log）"
            return 1
        fi
        if [ "${RTL_SIM_TAMPER:-}" = "regen_drift" ]; then
            printf '// tampered\n' >>"$regen/regen.sv"
        fi
        # ⚠️ 比对对象 = **仓内规范路径**（`$REPO_ROOT/$tb`）⇒ ⇒ 自检的 `TB_OVERRIDE`
        #   只影响「编译 / 运行」那一侧 ⇒ ⇒ 「刻意失败注入」类投毒不会被新鲜度先抓走
        local canonical_tb="$REPO_ROOT/$tb"
        if ! cmp -s "$regen/regen.sv" "$canonical_tb"; then
            cmp "$regen/regen.sv" "$canonical_tb" 2>&1 | sed 's/^/      /'
            fail "⑥' 生成物与验证源不一致（仓内 TB 被手改 / 验证源已更新但未重新生成）"
            return 1
        fi
        ok "⑥' 生成物新鲜度：逐字节重现 ✓"
    fi

    # ── ⑧' **层次化形态判据**（`kind ∈ {hier, soc}` 生效 · 2026-11-17 · soc 于 2026-11-18 扩展）──
    #   ⚠️ **为什么必须有这条**：⑧ 只验**端口表** ⇒ ⇒ 若层次化**退化成扁平**
    #   （发射器的 `--hierarchical` 失效 / 未来重构掉了层次化分支）⇒ ⇒
    #   端口表**照样相同** ⇒ ⇒ ⑧ 仍绿 ⇒ ⇒ 护栏对「形态退化」**结构性失明**。
    #   ⇒ ⇒ MUST 额外断言「产物里 ≥ 2 个 module 且含根 module `top`」⇒ ⇒ 退化即红。
    #   ⚠️ **MUST 跑在编译之前**：压扁后的产物**不可编译**（根 module 的实例语句
    #   仍引用被裁掉的子 module ⇒ `MODMISSING`）⇒ ⇒ 否则红在「编译失败」而不是
    #   「形态不对」⇒ ⇒ 判别力自证会「红了但不是期望的判据」。
    # ⚠️ **2026-11-18**（`example-file-boundary-2026-11` F7 · 蓝图 §4.3.2 的后置条件
    #   + §7 **R12 的闭口判据**）：条件从 `kind=hier` 扩到 **`kind ∈ {hier, soc}`**
    #   ⇒ ⇒ 否则「SoC 发射入口退回扁平」这条回归路径**无任何常驻判据**
    #   （⑧ 只比端口表 ⇒ 退化后照样相同）；④″ 的 `--check-products` 会把 `$work`
    #   覆写成层次化 ⇒ ⑤ 编译的仍是层次化 ⇒ ⇒ 整条链**全绿**。
    if [ "$kind" = "hier" ] || [ "$kind" = "soc" ]; then
        local mods n_mods
        mods="$(python3 "$REPO_ROOT/scripts/lib/flat_rtl_emit_check.py" \
            --modules "$dut_v" 2>&1)"
        n_mods="$(printf '%s\n' "$mods" | grep -c '[A-Za-z_]' || true)"
        if [ "$n_mods" -lt 2 ]; then
            fail "⑧' 层次化产物只有 $n_mods 个 module（应 ≥ 2）⇒ ⇒ 层次化形态已退化成扁平"
            return 1
        fi
        if ! printf '%s\n' "$mods" | grep -qx "top"; then
            fail "⑧' 层次化产物里没有根 module 'top'（实际：$(printf '%s' "$mods" | tr '\n' ' '))"
            return 1
        fi
        ok "⑧' 层次化形态成立（module 数 $n_mods · 根 = top）"
    fi

    # ── ④' **产物级**结构判据（`memory-timing-contract` T1 · D-TCM 读口）─────
    #   ⚠️ 为什么必须是**产物级**：C++ 侧的 `sread(...)` 只证明「源码这么写」
    #   ⇒ ⇒ 产物里它到底落成了**寄存器**（1 拍）还是**组合 mux**（0 拍），
    #   只有**产物**能回答 ⇒ ⇒ 这正是 T1「先红」的那条判据。
    #   ⚠️ **正反两面**（根 AGENTS.md 的护栏纪律）：
    #     · **正**：load 通路的读口 MUST 是**寄存器**（`always_ff` ⇒ 1 拍）
    #             ⇒ ⇒ 否则 load 数据会同拍可见 ⇒ ⇒ MEM 级采样到**上一拍**的字
    #     · **反**：RMW（字节合并）通路的读口 MUST **仍是组合**（`assign`）
    #             ⇒ ⇒ 否则字节合并拿不到**同拍**的旧字 ⇒ ⇒ 写回的是**已合并**的字
    #               （自我覆盖）⇒ ⇒ `sb`/`sh` 会**丢字节**
    #   ⚠️ 适用范围 = **发射 Rv32iPipelineTop 的 target**（按 `component` 列判定，
    #   **不是**按 target 名 ⇒ ⇒ 将来新增同组件的 target 也会被覆盖）。
    if [ "$comp" = "Rv32iPipelineTop" ]; then
        f9_bad=0
        dut_v="$work/top.v"
        if ! grep -qE 'always_ff.*Read port top_dtcm_mem_sread_data_proxy' "$dut_v"; then
            fail "④' D-TCM 的 load 读口**不是寄存器**（无 always_ff 的 sread 读口）⇒ ⇒ 读延迟退化为 0 拍"
            f9_bad=1
        fi
        if grep -qE '^ *assign top_dtcm_mem_sread_data_proxy *= *top_dtcm_dtcm_memory\[' "$dut_v"; then
            fail "④' D-TCM 的 load 读口是**组合 mux**（assign 内存数组）⇒ ⇒ T1 的 1 拍延迟失效"
            f9_bad=1
        fi
        if ! grep -qE '^ *assign top_dtcm_mem_aread_data_proxy *= *top_dtcm_dtcm_memory\[' "$dut_v"; then
            fail "④' D-TCM 的 RMW 读口**不是组合读** ⇒ ⇒ 字节合并自我覆盖 ⇒ ⇒ sb/sh 丢字节"
            f9_bad=1
        fi
        if [ "$f9_bad" -eq 0 ]; then
            ok "④' 产物读口结构正确（load 口 = always_ff 寄存器 · RMW 口 = 组合 assign）"
        fi
    fi

    # ── ④″ **SoC 层产物级判据**（`soc-rtl-product-and-tb-2026-11` T6 · ADR-073 J-1/J-2/J-3）──
    #   `kind=soc` 的行 MUST 通过 `--check-products` ⇒ ⇒ 它是
    #   **AC4（lint）的执行者** + module 数（J-2）+ 顶层端口面（J-1）的**唯一执行者**
    #   （R1–R3 只遍历 `verify_hierarchical_rtl.sh` 的主产物目录 ⇒ ⇒ 独立目录**没人 lint**）。
    #   ⚠️ 产物目录 = 本行的 `$work`（**已隔离**）⇒ ⇒ 不污染主产物目录的「恰好 3 个 .v」。
    if [ "$kind" = "soc" ]; then
        # ⚠️ **本调用故意不读 `emitter_args`**（change `emit-cli-into-example` 的 A6）：
        #   `--check-products` 是 SoC 行专属硬编码开关 ⇒ ⇒ 将来 `kind=soc` 行需要额外
        #   参数时会被**静默忽略**（已知残留）
        if ! "$emitter_bin" --out "$work" --emit-hygiene --check-products \
                >"$work/check-products.log" 2>&1; then
            fail "④″ SoC 产物级判据失败（见 $work/check-products.log）"
            return 1
        fi
        ok "④″ SoC 产物级判据通过（module 集 · 顶层端口面 · lint）"
    fi

    # ④⑤ 编译 + 缓存（key 用**内容**哈希；写入**原子**）
    local tb_name ver_version flags tb_hash dut_hash cache_key cache_dir sim_bin
    tb_name="$(basename "$tb" .sv)"
    ver_version="$("$VERILATOR" --version 2>/dev/null | head -1)"
    # ⚠️ **不用 `-Wno-fatal`**：仓内 `verify_hierarchical_rtl.sh` 的 R1 明文
    #   「禁 `-Wno-fatal`，它让 warning 不影响退出码 ⇒ 门失效」
    #   ⇒ ⇒ 本护栏同理：TB 与 DUT 的连接宽度错位、缺引脚等 MUST 致命。
    #   ⚠️ `--x-assign unique` 保留（**但它不能制造 X** —— 实测 Verilator 把 X
    #     优化成已知值 ⇒ ⇒ TB 里的 `$isunknown` 在本 runner 下**恒 false**
    #     ⇒ ⇒ 它是**装饰性**断言，真正的 X 判据见 E1 ⑤ 的内容判据）。
    #   ⚠️ **刻意**保持**默认** output-split 行为（**不**加 `--output-split 0`）：
    #     大模型会**自动** split（阈值 20000 statements）⇒ ⇒ 走 PCH 路径 ⇒ ⇒
    #     本护栏因此**真实覆盖**「自动 split ⇒ PCH」这条编译路径（工具链缺陷面）。
    #     ⚠️ 这条路径曾因 vendored verilator 的 **CMake** 构建缺 `CFG_CXXFLAGS_PCH_I`
    #     （上游 autoconf 在 `configure.ac:704-712` 定义）而**必失败**
    #     （`g++: error: Vtb_*__pch.h.fast: linker input file not found`）
    #     ⇒ ⇒ 已由 `verilator-pch-toolchain-fix-2026-11` 在**仓内 CMake 层**注入修好
    #     ⇒ ⇒ 机械判据 = `scripts/verify_verilator_pch.sh`（构建期指纹 + 强制 split 编译
    #     + 负向对照）⇒ ⇒ **别再把绕行加回来**（那会让缺陷重新变成潜伏 CI 红）。
    flags="--binary --timing -CFLAGS -fcoroutines --x-assign unique"
    tb_hash="$(sha1sum "$tb_path" | cut -d' ' -f1)"
    dut_hash="$(sha1sum "$dut_v" | cut -d' ' -f1)"
    # ⚠️ key MUST 含：产物内容 + TB 内容 + flags + verilator 版本 + **TB 名**
    #   + **target 名**（两个 target 共用同名 TB 时不得互相命中）
    #   + **`uname -m`**（`--binary` 产物是本地 ABI ⇒ 跨架构共享会拿到不可执行文件）
    cache_key="$(printf '%s\0%s\0%s\0%s\0%s\0%s\0%s' "$dut_hash" "$tb_hash" "$flags" \
        "$ver_version" "$tb_name" "$tgt" "$(uname -m)" | sha1sum | cut -d' ' -f1)"
    cache_dir="$HOME/.cache/cpphdl/rtl_sim/$cache_key"
    sim_bin="$work/obj/sim"
    mkdir -p "$work/obj"
    if [ -x "$cache_dir/sim" ]; then
        cp "$cache_dir/sim" "$sim_bin"
        ok "$tgt：命中缓存（$cache_key）"
    else
        if ! "$VERILATOR" $flags --top-module "$tb_name" \
                -Mdir "$work/obj" "$tb_path" "$dut_v" -o sim >"$work/compile.log" 2>&1; then
            fail "④ TB 编译失败（见 $work/compile.log）"
            return 1
        fi
        mkdir -p "$cache_dir"
        # ⚠️ **原子写**：先写 `.tmp` 再 `mv`（并发 CI job 同 key 时不得读到半截文件）
        cp "$sim_bin" "$cache_dir/sim.tmp.$$" 2>/dev/null && \
            mv -f "$cache_dir/sim.tmp.$$" "$cache_dir/sim" 2>/dev/null || true
    fi

    # ⑤ 运行（**timeout** —— 挂死 TB 不得拖死 job）
    if ! timeout "$TB_TIMEOUT" "$sim_bin" >"$work/run.log" 2>&1; then
        # ⚠️ **打印失败的 check_id**（TB 以 `$fatal` 收尾 ⇒ 退出码非零 ⇒ 先红在 ⑤）
        #   ⇒ ⇒ 不打印就只剩「仿真失败」四个字 ⇒ ⇒ 判别力自证看不到「同一失败」
        grep -E 'CHECK .* FAIL' "$work/run.log" 2>/dev/null | head -20 | sed 's/^/      /'
        fail "⑤ 仿真失败或超时（${TB_TIMEOUT}s）（见 $work/run.log）"
        return 1
    fi

    # ⑥ 断言非空 **且达到下界**
    local line checks fails
    line="$(grep -Eo 'RESULT: checks=[0-9]+ fails=[0-9]+' "$work/run.log" | tail -1)"
    if [ -z "$line" ]; then
        fail "⑥ 输出里没有 \`RESULT: checks=<N> fails=0\` 行（TB 没按约定收尾）"
        return 1
    fi
    checks="$(printf '%s' "$line" | sed -E 's/.*checks=([0-9]+).*/\1/')"
    fails="$(printf '%s' "$line" | sed -E 's/.*fails=([0-9]+).*/\1/')"
    if [ "$fails" -ne 0 ]; then
        # ⚠️ **打印失败的 check_id**（判别力自证要看「同一失败」⇒ ⇒ 必可定位到条）
        grep -E 'CHECK .* FAIL' "$work/run.log" 2>/dev/null | head -20 | sed 's/^/      /'
        fail "⑥ 断言失败数非 0（$line）"
        return 1
    fi
    if [ "$checks" -le 0 ]; then
        fail "⑥ 断言数为 0（checks=0）⇒ ⇒ TB 只编译不跑"
        return 1
    fi
    # ⚠️ 2026-11-17 起 **等式**（不是下界）：`.md` 登记 == `.verif` 静态展开 == 运行期
    #   ⇒ ⇒ 下界只防「删断言」；等式连「**生成器少发断言**」也一起抓（下界抓不到）。
    if [ "$minck" -gt 0 ] && [ "$checks" -ne "$minck" ]; then
        fail "⑥ 断言数 $checks ≠ 登记值 min_checks=$minck ⇒ ⇒ 三方等式破裂（等式，不是下界）"
        return 1
    fi
    # ⚠️ **三方等式的第一边**：期望源（`.md`）里**人**登记的条数
    #   ⇒ ⇒ 清单的 `min_checks` 若只是抄了生成器的输出 ⇒ ⇒ 两侧同时变 ⇒ ⇒ 恒绿
    if [ "$gen" = "yes" ] && [ -n "${md_checks:-}" ] && [ "$md_checks" -ne "$checks" ]; then
        fail "⑥ 断言数 $checks ≠ 期望源登记的 VERIF-CHECKS=$md_checks ⇒ ⇒ 三方等式破裂"
        return 1
    fi
    if [ "$status" = "verified" ] && [ "$minck" -le 0 ]; then
        fail "⑥ status=verified 但 min_checks 未登记（=0）⇒ ⇒ 无法检测断言被删"
        return 1
    fi
    # ── ⑨ **C++ 侧（解释器路径）条数相等**（`axi4-verif-driven-cpp` · ADR-074 D1）──
    #   作用域 = **note 列含 `interp-entry` 的行**（**用 note 约定 ⇒ 不新增列**）
    #   ⇒ ⇒ 按 `target` 列在 `$BUILD_DIR/examples/**` 找可执行文件（**找不到 ⇒ fail-closed**）
    #   ⇒ ⇒ 运行 ⇒ 断言 stdout 含 `RESULT: checks=<min_checks> fails=0`
    #   ⚠️ 「两后端一致」不是正确性证据 ⇒ ⇒ 本判据只判**条数相等 + 零失败**，不判语义
    if printf '%s' "$note" | grep -q 'interp-entry'; then
        local ibin=""
        while IFS= read -r cand; do ibin="$cand"; break; done \
            < <(find "$BUILD_DIR/examples" -type f -name "$tgt" 2>/dev/null | sort)
        if [ -z "$ibin" ]; then
            fail "⑨ 解释器入口缺失：$tgt（先构建：cmake --build $BUILD_DIR --target $tgt）"
            return 1
        fi
        local iout irc
        iout="$("$ibin" 2>/dev/null)"; irc=$?
        if [ "$irc" -ne 0 ]; then
            fail "⑨ 解释器路径非零退出（exit=$irc）：$tgt"
            return 1
        fi
        if ! printf '%s\n' "$iout" | grep -qx "RESULT: checks=${minck} fails=0"; then
            fail "⑨ 解释器条数不等或缺结论行：期望 'RESULT: checks=${minck} fails=0'"
            return 1
        fi
        ok "⑨ 解释器条数相等（checks=${minck} fails=0）"
    fi

    # ── F1b：`source` 列的路径 MUST 存在于磁盘（该列原「读了不判」）─────────
    if [ -n "$src" ] && [ ! -e "$src" ]; then
        fail "F1b source 列路径不存在：$src"
        return 1
    fi

    ok "$tgt：$line（scenarios=$scen · spec_checks=$spec）"
    return 0
}

# ── 主循环（check）──────────────────────────────────────────────────
if [ "$MODE" = "check" ]; then
    while IFS= read -r row; do
        split_row "$row" || exit 2
        check_one "${F[$((C_TARGET - 1))]}" "${F[$((C_KIND - 1))]}" \
            "${F[$((C_SRC - 1))]}" "${F[$((C_COMP - 1))]}" \
            "${F[$((C_EMITTER - 1))]}" "${F[$((C_TB - 1))]}" \
            "${F[$((C_STATUS - 1))]}" "${F[$((C_GENERATED - 1))]}" \
            "${F[$((C_MINCHECKS - 1))]}" "${F[$((C_SCENARIOS - 1))]}" \
            "${F[$((C_SPEC - 1))]}" "${F[$((C_NOTE - 1))]}" \
            "${F[$((C_EMITTER_ARGS - 1))]}"
    done < <(data_rows)

    # 预算检查（--allow-pending 时）
    if [ "$ALLOW_PENDING" -eq 1 ]; then
        if [ "$PENDING_BUDGET" -lt 0 ]; then
            fail "预算未给：--allow-pending MUST 配 --budget=<N>（不许无限放行）"
        elif [ "$PENDING_SEEN" -gt "$PENDING_BUDGET" ]; then
            fail "pending 数 $PENDING_SEEN > 预算 $PENDING_BUDGET ⇒ ⇒ 预算 MUST 随进度单调下降"
        else
            ok "pending $PENDING_SEEN ≤ 预算 $PENDING_BUDGET"
        fi
    fi

    echo
    if [ "$FAILED" -ne 0 ]; then
        echo "❌ verify_examples_rtl_sim: $FAILED 项失败（共检查 $CHECKED 项）"
        exit 1
    fi
    echo "✅ verify_examples_rtl_sim: PASS（$CHECKED 项 · 登记数守恒 · status 合规 · TB 存在 · 四件套（manifest 端口表 == 产物端口表）· 编译通过 · 跑通 · 断言 == 登记（等式）· 生成覆盖度三方一致 · 生成物形态/新鲜度/双向键集合/无残留）"
    exit 0
fi

# ── ⑦ --self-test：**18 条**投毒各自必红 + **2 条**必绿（2026-11-18 计数）───────────
run_self_test() {
    local st_root="$WORK_ROOT/selftest"
    rm -rf "$st_root"
    mkdir -p "$st_root"
    local golden_tb="$st_root/golden_tb.sv"
    local src_tb="$REPO_ROOT/examples/spinalhdl-ported/counter/tb_counter_simple_example.sv"
    if [ ! -f "$src_tb" ]; then
        echo "::error::self-test 需要 $src_tb 存在" >&2
        exit 2
    fi
    cp "$src_tb" "$golden_tb"

    # ⚠️ `generated` 列（2026-11-17）放在 `status` 之后 —— **中间插列** ⇒ ⇒ 顺带
    #   自证「列号从 `#COLUMNS` 推导」这条规则（按位置绑定的实现会立刻错列）。
    make_list() { # $1 = 目标路径；$2 = TB 路径；$3 = REGISTERED_TARGETS；$4 = status
        # ⚠️ **三行**：
        #   · `target` 行 = 轻量 fixture（counter_simple）⇒ 主判据的载体
        #   · `hier` 行 = **真实的层次化产品单元**（RV32I）⇒ ⑧' 判据的载体
        #   · `tb_axi4_lite_example` 行 = **真的由 `.verif` 生成**的单元
        #     ⇒ ⇒ ⑥'（新鲜度）/ ⑥''（双向键集合）/ ⑥‴（无残留）/ ⑧''（生成物形态）
        #     / ⑥⁗（三方一致）**只有它有可判对象** ⇒ ⇒ 缺它这 5 条判据在自检里恒绿。
        # ⚠️ 锚点：`REGISTERED_TARGETS: $3` · `REGISTERED_SOC: 0` · `REGISTERED_HIER: 1`
        cat >"$1" <<EOF
# self-test 用的临时清单
#COLUMNS: target kind source component emitter emitter_args tb status generated min_checks scenarios spec_checks note
# REGISTERED_TARGETS: $3
# REGISTERED_SOC: 0
# REGISTERED_HIER: 1
selftest_target	target	examples/spinalhdl-ported/counter/counter_simple.cpp	Top	emit_rtl_counter_simple	-	$2	$4	no	768	256	0	self-test fixture（手写 TB · 只作 ①/②/⑤/⑥ 的载体）
selftest_hier	hier	examples/riscv-mini/src/rv32i_pipeline_top.h	Rv32iPipelineTop	emit_rtl_rv32i_soc_hier	-	examples/riscv-mini/tb_rv32i_soc.sv	verified	no	24	6	22	self-test fixture（层次化形态 · ⑧' 的载体 · 手写 TB）
tb_axi4_lite_example	target	examples/axi4/tb_axi4_lite_example.cpp	AxiLiteTop	tb_axi4_lite_example	--emit-rtl	examples/axi4/tb_axi4_lite_example.sv	verified	yes	113	7	33	self-test fixture（**真的由 .verif 生成** ⇒ ⑥'/⑥''/⑥‴/⑧''/⑥⁗ 的载体）· interp-entry
EOF
    }

    local st_list="$st_root/list.tsv"
    local st_tb_rel="examples/spinalhdl-ported/counter/tb_counter_simple_example.sv"

    run_case() { # $1 = 名称；$2 = 期望的错误关键词；$3（可选）= 环境前缀；$4（可选）= 清单
        local name="$1" want="$2" env="${3:-}" list="${4:-$st_list}" out rc
        out="$(env $env RTL_SIM_LIST="$list" RTL_SIM_WORK="$st_root/work" \
            bash "$0" 2>&1)"
        rc=$?
        if [ "$rc" -eq 0 ]; then
            echo "  [0;31m❌ 自检 $name：**仍绿** ⇒ ⇒ 该判据无判别力[0m"
            printf '%s\n' "$out" | tail -30 | sed 's/^/       /'
            FAILED=$((FAILED + 1))
            return 1
        fi
        if ! printf '%s' "$out" | grep -q "$want"; then
            echo "  [0;31m❌ 自检 $name：红了但不是期望的判据（期望含：$want）[0m"
            printf '%s\n' "$out" | tail -30 | sed 's/^/       /'
            FAILED=$((FAILED + 1))
            return 1
        fi
        ok "自检 $name：转红 ✓（$want）"
        return 0
    }

    # **必绿**用例（`emit-cli-into-example` B5）：投毒形态不改变可观测行为 ⇒ MUST 仍绿
    run_case_green() { # $1 = 名称；$2（可选）= 清单
        local name="$1" list="${2:-$st_list}" out rc
        out="$(RTL_SIM_LIST="$list" RTL_SIM_WORK="$st_root/work" bash "$0" 2>&1)"
        rc=$?
        if [ "$rc" -ne 0 ]; then
            echo "  [0;31m❌ 自检 $name：**转红** ⇒ ⇒ 该形态本应不影响行为[0m"
            printf '%s\n' "$out" | tail -30 | sed 's/^/       /'
            FAILED=$((FAILED + 1))
            return 1
        fi
        ok "自检 $name：仍绿 ✓"
        return 0
    }

    # ⚠️ **kind=soc 的独立清单**（2026-11-18 · F7）：不并入上面的共享清单 ——
    #   共享清单会被**每一个**投毒用例跑一遍，而 soc 行会额外触发 ④″
    #   （`--check-products`：lint + 双形态逐拍等价 + verilator 编译）⇒ ⇒ 并入会让
    #   自检时长乘上用例数。⇒ ⇒ 只用一个只含 soc 行的清单跑 **2 次**（绿 + 毒）。
    make_soc_list() { # $1 = 清单路径 ⇒ ⇒ 只含一行 kind=soc
        cat >"$1" <<'SOCEOF'
# self-test 用的临时清单（**只含 soc 行** ⇒ ⇒ ⑧' 对 kind=soc 的载体）
#COLUMNS: target kind source component emitter emitter_args tb status generated min_checks scenarios spec_checks note
# REGISTERED_TARGETS: 0
# REGISTERED_SOC: 1
# REGISTERED_HIER: 0
tb_axi_soc_example	soc	examples/axi4/tb_axi_soc_example.cpp	AxiSocTop	tb_axi_soc_example	--emit-rtl	examples/axi4/tb_axi_soc_example.sv	verified	no	14	4	10	self-test fixture（kind=soc · ⑧' 的载体）
SOCEOF
    }

    # ── 基线：未投毒 ⇒ MUST 绿 ────────────────────────────────────
    cp "$golden_tb" "$REPO_ROOT/$st_tb_rel.st_orig" 2>/dev/null || true
    make_list "$st_list" "$st_tb_rel" 2 verified
    local base_out base_rc
    base_out="$(RTL_SIM_LIST="$st_list" RTL_SIM_WORK="$st_root/work" bash "$0" 2>&1)"
    base_rc=$?
    if [ "$base_rc" -ne 0 ]; then
        echo "  [0;31m❌ 自检基线：未投毒就红 ⇒ ⇒ 自检环境有问题[0m"
        printf '%s\n' "$base_out" | tail -8 | sed 's/^/       /'
        FAILED=$((FAILED + 1))
    else
        ok "自检基线：未投毒 ⇒ 绿 ✓"
    fi

    # ── 投毒 1：删掉 TB 的 RESULT 行 ⇒ ⑥ 必红 ────────────────────
    cp "$golden_tb" "$REPO_ROOT/$st_tb_rel"
    python3 - "$REPO_ROOT/$st_tb_rel" <<'PY'
import pathlib, re, sys
p = pathlib.Path(sys.argv[1])
t = p.read_text(encoding="utf-8")
t = re.sub(r'^\s*\$display\("RESULT:.*\n', '', t, flags=re.M)
p.write_text(t, encoding="utf-8")
PY
    run_case "投毒1（删 RESULT 行）" "没有 \`RESULT:"

    # ── 投毒 2：把 checks 打成 0 ⇒ ⑥ 必红 ────────────────────────
    cp "$golden_tb" "$REPO_ROOT/$st_tb_rel"
    sed -i 's|\$display("RESULT: checks=%0d fails=%0d", checks, fails);|$display("RESULT: checks=0 fails=%0d", fails);|' \
        "$REPO_ROOT/$st_tb_rel"
    run_case "投毒2（checks=0）" "断言数为 0"

    # ── 投毒 3：REGISTERED_TARGETS 与实际行数不符 ⇒ ⓪ 必红 ───────
    cp "$golden_tb" "$REPO_ROOT/$st_tb_rel"
    make_list "$st_list" "$st_tb_rel" 3 verified
    run_case "投毒3（登记数不守恒）" "首批桶不守恒"

    # ── 投毒 4：status=verified 但 TB 缺失 ⇒ ② 必红 ──────────────
    rm -f "$REPO_ROOT/$st_tb_rel"
    make_list "$st_list" "$st_tb_rel" 2 verified
    run_case "投毒4（status=verified 但 TB 缺失）" "TB 文件缺失"

    # ── 投毒 5：status=pending 且未放行 ⇒ ① 必红 ─────────────────
    cp "$golden_tb" "$REPO_ROOT/$st_tb_rel"
    make_list "$st_list" "$st_tb_rel" 2 pending
    run_case "投毒5（pending 未放行）" "status=pending"

    # ── 投毒 6/7：⑧ 的**正负成对**判别力自证（T13 / AC8）───────────
    #   ⚠️ 只投「删 manifest」不算 —— 那只证明判据会打开文件；
    #      MUST 证明它**区分对错** ⇒ ⇒ 两个方向各投一次。
    cp "$golden_tb" "$REPO_ROOT/$st_tb_rel"
    make_list "$st_list" "$st_tb_rel" 2 verified
    run_case "投毒6（篡改 port_table 的一个端口名）" "⑧ manifest 的 port_table" \
        "RTL_SIM_TAMPER=port_table_name"
    run_case "投毒7（产物里多插一个下沉端口）" "⑧ manifest 的 port_table" \
        "RTL_SIM_TAMPER=product_extra_port"

    # ── 投毒 8：⑧' 的判别力自证（**层次化形态退化** · 2026-11-17）──────
    #   ⚠️ 为什么必须单独投：⑧（端口表）对「层次化 → 扁平」**结构性失明**
    #   ⇒ ⇒ 把层次化产物裁成单 module ⇒ ⇒ 端口表**照样相同** ⇒ ⇒ 只有 ⑧' 能抓。
    run_case "投毒8（层次化产物被压成单 module）" "⑧' 层次化产物只有" \
        "RTL_SIM_TAMPER=flatten_hier"

    # ── 投毒 9–13：`generated=yes` 的**五条判据**各自的判别力自证 ──────────
    #   ⚠️ 载体 = 清单里那行**真的由 `.verif` 生成**的 `tb_axi4_lite_example`
    #     ⇒ ⇒ 没有它，⑥'/⑥''/⑥‴/⑧''/⑥⁗ 在自检里**恒绿**（无可判对象）。
    #   ⚠️ 全部投毒都**只动临时产物 / 环境变量 / 临时清单**（不动仓内 `.verif` / `.md`）
    #     ⇒ ⇒ `RTL_SIM_VERIF` / `RTL_SIM_MD` / `RTL_SIM_TB_OVERRIDE` 是为此设的钩子。
    cp "$golden_tb" "$REPO_ROOT/$st_tb_rel"
    local vif_real="$REPO_ROOT/examples/axi4/tb_axi4_lite_example.verif"

    # 投毒 9：重新生成的 TB 与仓内文件逐字节不同 ⇒ ⑥' 新鲜度必红
    make_list "$st_list" "$st_tb_rel" 2 verified
    run_case "投毒9（生成物与验证源不一致）" "生成物与验证源不一致" \
        "RTL_SIM_TAMPER=regen_drift"

    # 投毒 10：登记的 min_checks 与运行期不同 ⇒ ⑥ 等式必红
    make_list "$st_list" "$st_tb_rel" 2 verified
    sed -i 's/\t113\t7\t33\t/\t112\t7\t33\t/' "$st_list"
    run_case "投毒10（min_checks 与运行期不等）" "三方等式破裂"

    # 投毒 11：验证源少一个 scenario（`.md` 未变）⇒ ⑥'' 双向键集合必红
    make_list "$st_list" "$st_tb_rel" 2 verified
    local vif_bad="$st_root/bad.verif"
    awk '
        /^@scenario wstrb_byte_lanes$/ {skip = 1; next}
        skip && /^@scenario / {skip = 0}
        !skip {print}
    ' "$vif_real" >"$vif_bad"
    if ! cmp -s "$vif_real" "$vif_bad"; then
        run_case "投毒11（验证源少一个 scenario）" "双向 1:1 破裂" \
            "RTL_SIM_VERIF=$vif_bad"
    else
        fail "自检 投毒11 无效：没能从验证源里删掉 scenario"
    fi

    # 投毒 12：登记的 `generated` 说 no 但 `.verif` 存在 ⇒ ⑥⁗ 三方一致必红
    make_list "$st_list" "$st_tb_rel" 2 verified
    # ⚠️ 2026-11-18（`emit-cli-into-example`）：插入 `emitter_args` 列 ⇒ 原 8 字段 regex **不再匹配**
    #   ⇒ ⇒ 改为**按列名**构造（用 `colidx` 的同一套 `#COLUMNS` 解析在 awk 里重建）
    awk -F'\t' -v OFS='\t' '
        BEGIN { }
        /^#COLUMNS:/ { n = split($0, c, " "); for (i = 2; i <= n; i++) idx[c[i]] = i - 1; print; next }
        /^#/ || /^$/ { print; next }
        { if ($idx["target"] == "tb_axi4_lite_example") $idx["generated"] = "no"; print }
    ' "$st_list" >"$st_list.tmp" && mv "$st_list.tmp" "$st_list"
    run_case "投毒12（generated=no 却有验证源）" "应改为生成"

    # 投毒 13：TB 里残留 `TOP_unnamed_input` 字面量 ⇒ ⑧'' 生成物形态必红
    make_list "$st_list" "$st_tb_rel" 2 verified
    local tb_bad="$st_root/bad_tb.sv"
    cp "$REPO_ROOT/examples/axi4/tb_axi4_lite_example.sv" "$tb_bad"
    sed -i '0,/^module /s//\/\/ legacy: top_unnamed_input_1\nmodule /' "$tb_bad"
    run_case "投毒13（TB 残留 unnamed 端口字面量）" "手写映射表残留" \
        "RTL_SIM_TB_OVERRIDE=$tb_bad"

    # ── 投毒 14：**刻意失败注入** ⇒ 两后端**同一失败**（AC8 / AC20 的载体）──
    #   ⚠️ 只把一个 `EXPECT` 的期望值改错 ⇒ ⇒ 渲染式 TB 跑出 `fails != 0`
    #     且**失败点的 check_id 逐字可定位** ⇒ ⇒ 与解释器侧（单元测试
    #     `tests/test_verif_single_source.cpp`）报出的**同一条** check_id 对照
    #     ⇒ ⇒ 「同一份 IR ⇒ 同一结论」有了**两端各自的实证**。
    make_list "$st_list" "$st_tb_rel" 2 verified
    local tb_fail="$st_root/failing_tb.sv"
    cp "$REPO_ROOT/examples/axi4/tb_axi4_lite_example.sv" "$tb_fail"
    sed -i "s/if (top_rdata !== 32'd3405691582)/if (top_rdata !== 32'd1)/" "$tb_fail"
    if cmp -s "$tb_fail" "$REPO_ROOT/examples/axi4/tb_axi4_lite_example.sv"; then
        fail "自检 投毒14 无效：没能改掉期望值"
    else
        run_case "投毒14（刻意失败注入 ⇒ 同一 check_id 转红）" \
            "CHECK aw_w_phase_split_and_addr_latch#8 FAIL" \
            "RTL_SIM_TB_OVERRIDE=$tb_fail"
    fi

    # 投毒 15：**只**毒解释器二进制（`min_checks` 不动 ⇒ ⑥ 仍绿）⇒ ⑨ 必红
    #   ⚠️ 不能用「改 `min_checks`」当投毒 —— 那会被 ⑥（三方等式，先跑）抢先命中
    #      ⇒ ⇒ ⑨ 就**永远**没有独立判别力。
    make_list "$st_list" "$st_tb_rel" 2 verified
    local ibin="$BUILD_DIR/examples/axi4/tb_axi4_lite_example"
    if [ -x "$ibin" ]; then
        cp "$ibin" "$st_root/ibin.bak"
        # ⚠️ 2026-11-18（`emit-cli-into-example`）：tb 现在是**双模式**（验证 / --emit-rtl）
        #   ⇒ ⇒ stub MUST **先转发真实二进制**（否则 ③ 拿不到产物 ⇒ 先报「产物缺失」⇒ ⑨ 永远没机会红）
        #   ⇒ ⇒ 再覆盖 stdout 的 RESULT 行（只在默认模式调用时生效 —— 即 ⑨ 那一路）
        printf '#!/bin/sh\n"%s" "$@" >/dev/null 2>&1\necho "RESULT: checks=99 fails=0"\n' \
            "$st_root/ibin.bak" >"$ibin"
        chmod +x "$ibin"
        run_case "投毒15（解释器条数与 min_checks 不等）" "⑨ 解释器条数不等"
        cp "$st_root/ibin.bak" "$ibin"
    else
        fail "自检 投毒15 无效：解释器二进制不存在（$ibin）"
    fi

    # 投毒 16：`source` 列指向不存在的路径 ⇒ F1b 必红
    make_list "$st_list" "$st_tb_rel" 2 verified
    sed -i 's#examples/spinalhdl-ported/counter/counter_simple.cpp#examples/spinalhdl-ported/counter/NOPE.cpp#' "$st_list"
    run_case "投毒16（source 列路径不存在）" "F1b source 列路径不存在"

    # 投毒 17：`emitter_args` 被置为 `-`（钉在 axi4 行）⇒ ③ 必红
    #   （tb 的默认模式收到 `--out` 等未知参数 ⇒ exit 2 ⇒ 发射失败）
    make_list "$st_list" "$st_tb_rel" 2 verified
    awk -F'\t' -v OFS='\t' '
        /^#COLUMNS:/ { n = split($0, c, " "); for (i = 2; i <= n; i++) idx[c[i]] = i - 1; print; next }
        /^#/ || /^$/ { print; next }
        { if ($idx["target"] == "tb_axi4_lite_example") $idx["emitter_args"] = "-"; print }
    ' "$st_list" >"$st_list.tmp" && mv "$st_list.tmp" "$st_list"
    run_case "投毒17（emitter_args 置 - ⇒ 默认模式收到未知参数）" "③ 发射失败"

    # 必绿用例：中间插一列（值非空）+ 同步 #COLUMNS ⇒ 按**名字**取列 ⇒ 仍绿
    make_list "$st_list" "$st_tb_rel" 2 verified
    awk -F'\t' -v OFS='\t' '
        /^#COLUMNS:/ {
            n = split($0, c, " ")
            out = "#COLUMNS:"
            for (i = 2; i <= n; i++) { if (c[i] == "component") out = out " zzz_inserted"; out = out " " c[i] }
            print out; next
        }
        /^#/ || /^$/ { print; next }
        { row = ""; for (i = 1; i <= NF; i++) { if (i == 4) row = row (row == "" ? "" : "\t") "nonempty"; row = row (row == "" ? "" : "\t") $i }; print row }
    ' "$st_list" >"$st_list.tmp" && mv "$st_list.tmp" "$st_list"
    run_case_green "必绿：中间插列（值非空）"

    # ── 投毒 18：**kind=soc 的形态退化**（⑧' 对 soc 的判别力 · 2026-11-18 · F7）──
    #   ⚠️ 与投毒8（flatten_hier）**不同**：那条钉的是 kind=hier 行；本条钉的是
    #   **kind=soc** 行 ⇒ ⇒ 两者各自证明 ⑧' 在两个桶上都有判别力（AC24 的
    #   「soc 负向对照（投毒 ⇒ 必红）」）。
    local st_soc_list="$st_root/list_soc.tsv"
    make_soc_list "$st_soc_list"
    run_case "投毒18（SoC 产物被压成单 module ⇒ ⑧' 对 kind=soc 必红）" \
        "⑧' 层次化产物只有" "RTL_SIM_TAMPER=flatten_soc" "$st_soc_list"
    # 必绿：soc 清单**未投毒** ⇒ MUST 绿（证明 ⑧' 对 kind=soc 不误伤）
    run_case_green "soc 清单未投毒（⑧' 对 kind=soc 不误伤）" "$st_soc_list"

    # ── 还原 ─────────────────────────────────────────────────────
    cp "$golden_tb" "$REPO_ROOT/$st_tb_rel"
    rm -f "$REPO_ROOT/$st_tb_rel.st_orig"
    # 还原后复绿
    make_list "$st_list" "$st_tb_rel" 2 verified
    if RTL_SIM_LIST="$st_list" RTL_SIM_WORK="$st_root/work" bash "$0" >/dev/null 2>&1; then
        ok "自检：还原后复绿 ✓"
    else
        fail "自检：还原后**未**复绿 ⇒ ⇒ 自检污染了仓内 TB"
    fi

    echo
    if [ "$FAILED" -ne 0 ]; then
        echo "❌ verify_examples_rtl_sim --self-test: $FAILED 条投毒未被抓住"
        exit 1
    fi
    echo "✅ verify_examples_rtl_sim --self-test: PASS（基线绿 · 18 条投毒各自转红 · 2 条必绿用例仍绿 · 还原复绿）"
    exit 0
}

if [ "$MODE" = "self-test" ]; then
    run_self_test
fi

echo "::error::未知模式" >&2
exit 2
