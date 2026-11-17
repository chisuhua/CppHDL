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
#   ⑦ `--self-test`：**8 条**投毒**各自**必红（含 ⑧ 的**正负成对**两向：
#      篡改 `port_table` 一个名字 / 产物多插一个下沉端口；以及 ⑧' 的一向：
#      把层次化产物压成单 module）
#
# ⚠️ **禁**自研 Verilog 静态分析器判「断言非空」
#   （草案 metis 订正：仓内那个自研 C 子集分析器在第 4/5/6 轮对抗审稿里
#     **全部** BLOCKER 出在它身上 ⇒ ⇒ 同一类东西**不**再做第二套）
#
# 用法：
#   scripts/verify_examples_rtl_sim.sh                     # 严格（CI 用）
#   scripts/verify_examples_rtl_sim.sh --list              # 只列清单
#   scripts/verify_examples_rtl_sim.sh --self-test         # 4 条投毒自检
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
    n_td="$(data_rows | awk -F'\t' '$2=="target" || $2=="dut"' | wc -l | tr -d ' ')"
    n_soc="$(data_rows | awk -F'\t' '$2=="soc"' | wc -l | tr -d ' ')"
    n_hier="$(data_rows | awk -F'\t' '$2=="hier"' | wc -l | tr -d ' ')"
    n_unknown="$(data_rows | awk -F'\t' '$2!="target" && $2!="dut" && $2!="soc" && $2!="hier"' | wc -l | tr -d ' ')"
    if [ "$n_unknown" -ne 0 ]; then
        fail "⓪ 有 $n_unknown 行的 \`kind\` 不属于已知集合 {target, dut, soc, hier}"
        data_rows | awk -F'\t' '$2!="target" && $2!="dut" && $2!="soc" && $2!="hier" {print "      ::error::未知 kind ⇒ " $1 " (kind=" $2 ")"}'
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
    printf "  %-30s %-8s %-9s %-6s %-6s %-6s %s\n" "target" "kind" "status" "minck" "scen" "spec" "tb"
    while IFS=$'\t' read -r tgt kind src comp emitter tb status minck scen spec note; do
        printf "  %-30s %-8s %-9s %-6s %-6s %-6s %s\n" "$tgt" "$kind" "$status" "$minck" "$scen" "$spec" "$tb"
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
    local minck="$8" scen="$9" spec="${10}" note="${11}"
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
    local tb_path="$REPO_ROOT/$tb"
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
    if ! "$emitter_bin" --out "$work" --emit-hygiene >"$work/emit.log" 2>&1; then
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
    flatten_hier)
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

    # ── ⑧' **层次化形态判据**（`kind=hier` 生效 · 2026-11-17）─────────────
    #   ⚠️ **为什么必须有这条**：⑧ 只验**端口表** ⇒ ⇒ 若层次化**退化成扁平**
    #   （发射器的 `--hierarchical` 失效 / 未来重构掉了层次化分支）⇒ ⇒
    #   端口表**照样相同** ⇒ ⇒ ⑧ 仍绿 ⇒ ⇒ 护栏对「形态退化」**结构性失明**。
    #   ⇒ ⇒ MUST 额外断言「产物里 ≥ 2 个 module 且含根 module `top`」⇒ ⇒ 退化即红。
    #   ⚠️ **MUST 跑在编译之前**：压扁后的产物**不可编译**（根 module 的实例语句
    #   仍引用被裁掉的子 module ⇒ `MODMISSING`）⇒ ⇒ 否则红在「编译失败」而不是
    #   「形态不对」⇒ ⇒ 判别力自证会「红了但不是期望的判据」。
    if [ "$kind" = "hier" ]; then
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
        fail "⑥ 断言失败数非 0（$line）"
        return 1
    fi
    if [ "$checks" -le 0 ]; then
        fail "⑥ 断言数为 0（checks=0）⇒ ⇒ TB 只编译不跑"
        return 1
    fi
    if [ "$minck" -gt 0 ] && [ "$checks" -lt "$minck" ]; then
        fail "⑥ 断言数 $checks < 登记下界 min_checks=$minck ⇒ ⇒ 断言被删（不算缩面？算）"
        return 1
    fi
    if [ "$status" = "verified" ] && [ "$minck" -le 0 ]; then
        fail "⑥ status=verified 但 min_checks 未登记（=0）⇒ ⇒ 无法检测断言被删"
        return 1
    fi
    ok "$tgt：$line（scenarios=$scen · spec_checks=$spec）"
    return 0
}

# ── 主循环（check）──────────────────────────────────────────────────
if [ "$MODE" = "check" ]; then
    while IFS=$'\t' read -r tgt kind src comp emitter tb status minck scen spec note; do
        check_one "$tgt" "$kind" "$src" "$comp" "$emitter" "$tb" "$status" \
            "$minck" "$scen" "$spec" "$note"
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
    echo "✅ verify_examples_rtl_sim: PASS（$CHECKED 项 · 登记数守恒 · status 合规 · TB 存在 · 四件套（manifest 端口表 == 产物端口表）· 编译通过 · 跑通 · 断言 ≥ 下界）"
    exit 0
fi

# ── ⑦ --self-test：4 条投毒**各自**必红 ─────────────────────────────
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

    make_list() { # $1 = 目标路径；$2 = TB 路径；$3 = REGISTERED_TARGETS；$4 = status
        # ⚠️ **两行**（`kind=target` + `kind=hier`）：
        #   · `target` 行 = 轻量 fixture（counter_simple）⇒ 主判据的载体
        #   · `hier` 行 = **真实的层次化产品单元**（RV32I）⇒ ⑧' 判据的载体
        #     ⇒ ⇒ 它的必要性：⑧' 的判别力自证（投毒 8「压扁」）必须有一个
        #     **真会产出多 module** 的单元 ⇒ 否则投毒落空 ⇒ 判据恒绿。
        # ⚠️ 锚点：`REGISTERED_TARGETS: $3` · `REGISTERED_SOC: 0` · `REGISTERED_HIER: 1`
        cat >"$1" <<EOF
# self-test 用的临时清单
# REGISTERED_TARGETS: $3
# REGISTERED_SOC: 0
# REGISTERED_HIER: 1
selftest_target	target	examples/spinalhdl-ported/counter/counter_simple.cpp	Top	emit_rtl_counter_simple	$2	$4	1	256	0	self-test
selftest_hier	hier	examples/riscv-mini/src/rv32i_pipeline_top.h	Rv32iPipelineTop	emit_rtl_rv32i_soc_hier	examples/riscv-mini/tb_rv32i_soc.sv	verified	24	6	22	self-test-hier
EOF
    }

    local st_list="$st_root/list.tsv"
    local st_tb_rel="examples/spinalhdl-ported/counter/tb_counter_simple_example.sv"

    run_case() { # $1 = 名称；$2 = 期望的错误关键词；$3（可选）= 环境前缀
        local name="$1" want="$2" env="${3:-}" out rc
        out="$(env $env RTL_SIM_LIST="$st_list" RTL_SIM_WORK="$st_root/work" \
            bash "$0" 2>&1)"
        rc=$?
        if [ "$rc" -eq 0 ]; then
            echo "  [0;31m❌ 自检 $name：**仍绿** ⇒ ⇒ 该判据无判别力[0m"
            printf '%s\n' "$out" | tail -6 | sed 's/^/       /'
            FAILED=$((FAILED + 1))
            return 1
        fi
        if ! printf '%s' "$out" | grep -q "$want"; then
            echo "  [0;31m❌ 自检 $name：红了但不是期望的判据（期望含：$want）[0m"
            printf '%s\n' "$out" | tail -6 | sed 's/^/       /'
            FAILED=$((FAILED + 1))
            return 1
        fi
        ok "自检 $name：转红 ✓（$want）"
        return 0
    }

    # ── 基线：未投毒 ⇒ MUST 绿 ────────────────────────────────────
    cp "$golden_tb" "$REPO_ROOT/$st_tb_rel.st_orig" 2>/dev/null || true
    make_list "$st_list" "$st_tb_rel" 1 verified
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
    make_list "$st_list" "$st_tb_rel" 2 verified
    run_case "投毒3（登记数不守恒）" "首批桶不守恒"

    # ── 投毒 4：status=verified 但 TB 缺失 ⇒ ② 必红 ──────────────
    rm -f "$REPO_ROOT/$st_tb_rel"
    make_list "$st_list" "$st_tb_rel" 1 verified
    run_case "投毒4（status=verified 但 TB 缺失）" "TB 文件缺失"

    # ── 投毒 5：status=pending 且未放行 ⇒ ① 必红 ─────────────────
    cp "$golden_tb" "$REPO_ROOT/$st_tb_rel"
    make_list "$st_list" "$st_tb_rel" 1 pending
    run_case "投毒5（pending 未放行）" "status=pending"

    # ── 投毒 6/7：⑧ 的**正负成对**判别力自证（T13 / AC8）───────────
    #   ⚠️ 只投「删 manifest」不算 —— 那只证明判据会打开文件；
    #      MUST 证明它**区分对错** ⇒ ⇒ 两个方向各投一次。
    cp "$golden_tb" "$REPO_ROOT/$st_tb_rel"
    make_list "$st_list" "$st_tb_rel" 1 verified
    run_case "投毒6（篡改 port_table 的一个端口名）" "⑧ manifest 的 port_table" \
        "RTL_SIM_TAMPER=port_table_name"
    run_case "投毒7（产物里多插一个下沉端口）" "⑧ manifest 的 port_table" \
        "RTL_SIM_TAMPER=product_extra_port"

    # ── 投毒 8：⑧' 的判别力自证（**层次化形态退化** · 2026-11-17）──────
    #   ⚠️ 为什么必须单独投：⑧（端口表）对「层次化 → 扁平」**结构性失明**
    #   ⇒ ⇒ 把层次化产物裁成单 module ⇒ ⇒ 端口表**照样相同** ⇒ ⇒ 只有 ⑧' 能抓。
    run_case "投毒8（层次化产物被压成单 module）" "⑧' 层次化产物只有" \
        "RTL_SIM_TAMPER=flatten_hier"

    # ── 还原 ─────────────────────────────────────────────────────
    cp "$golden_tb" "$REPO_ROOT/$st_tb_rel"
    rm -f "$REPO_ROOT/$st_tb_rel.st_orig"
    # 还原后复绿
    make_list "$st_list" "$st_tb_rel" 1 verified
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
    echo "✅ verify_examples_rtl_sim --self-test: PASS（基线绿 · 8 条投毒各自转红 · 还原复绿）"
    exit 0
}

if [ "$MODE" = "self-test" ]; then
    run_self_test
fi

echo "::error::未知模式" >&2
exit 2
