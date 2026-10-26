#!/usr/bin/env bash
# scripts/lib/assertion_quality.sh
#
# **共享库**：`assertion quality` 护栏的**分析器**与 **fail-closed 判定**。
#
# ⚠️ 为什么抽成 lib（2026-10-26，change `example-assertion-quality-2026-10` 的 T1.2）：
#    两个消费者 MUST 用**同一份**实现 ——
#      ① `scripts/verify_example_assertion_quality.sh`（护栏本体）
#      ② `scripts/verify_assertion_quality_fixtures.sh`（fixture 自检 = 规格）
#    若各写一份 ⇒ 分析器会**漂移**（C1 连续三轮对抗审稿的 BLOCKER 全部出在
#    「两处对同一机制的两种理解」上）⇒ 抽 lib 让「护栏行为」与「规格」不可能分叉。
#
# 用法（**source**，不是执行）：
#     . "$(dirname "$0")/lib/assertion_quality.sh"
#
# ⚠️ 本文件**不产生任何副作用**：source 时不设全局变量、不建临时文件、不改
#    shell 选项、不 `cd`、不 `exit` ⇒ 可安全 source。
#    （函数内部的临时变量一律 `local` ⇒ 调用后也不污染调用方的环境。）
#
# 提供：
#   analyze_source <file>          —— 静态分析器；输出 `A`/`S`/`C`/`E` 四类行
#   aq_analyze_status <out>         —— fail-closed 状态：`empty` | `no_end_marker` | `ok`
#   aq_violation_reason <src> <out> —— C4b 的 fail-closed 违规理由（空串 = 无违规）

# ═══════════════════════════════════════════════════════════════════════════
# 源文件静态分析：**活代码提取** + **逐用例断言计数**
#
# 输出三行：
#   `A <n>`            —— 活代码里的可执行断言总数
#   `S <n>`            —— 因条件编译被**跳过**的区段里出现的用例宏数（可见性用）
#   `C <n> <用例名>`   —— 每个用例体内的可执行断言数（每用例一行）
#
# ⚠️ 能力边界（如实声明；第四/五/六轮对抗审稿各纠正过一次，**勿再夸张**）：
#   ① **已覆盖**：整行/行尾/跨行块注释、字符串与字符字面量、
#      宏体（`#define` 行**整行不计**）、词边界（`MYREQUIRE(` / `XCHECK(` 不计）、
#      按**出现次数**计（同一行两条都算）、Catch2 的 throws/nothrow/STATIC 系断言宏。
#      条件编译：`#if <字面常量>` 求值；`#if 0` 区段跳过但**其 `#else` 之后的活代码计入**；
#      `#if 1` 之后的 `#elif` / `#else` **都是死的** ⇒ 跳过（`#elif` 曾漏判，第六轮 BLOCKER-1）；
#      `#if 0 / #elif 1` 的 `#elif` 体**是活的** ⇒ 计入。
#   ② **不覆盖（整组保守跳过）**：`#ifdef` / `#ifndef` / 非常量 `#if` —— **连同其
#      `#elif` / `#else` 一起跳过**（可能不编译 ⇒ 宁可少算）。
#      ⚠️ 第六轮 BLOCKER-2：此前只跳过 `#else` **之前**，恰好把**死**的那一半算进来
#      （`#ifdef X` 在 X 已定义的平台上，`#else` 正是死分支）⇒ 与「不会多算」的声明矛盾。
#      代价：本仓 `test_riscv_tests.cpp` 用 `TEST_CASE_FOR_each(...)` 宏**生成**的
#      41 个用例（宏体里的 `REQUIRE(passed == true)`）不进计数 ⇒ 该文件的
#      锚点是 14（8 个字面用例），不是 55。**这是已知且已声明的局限**。
#      被跳过区段里出现的用例宏由 `S` 计数**点名**（不再完全静默）。
#   ③ **不防「等量替换」**：把 `REQUIRE(pc == X)` 换成 `REQUIRE(1 == 1)` 断言数
#      不变 ⇒ 任何计数守恒都发现不了 ⇒ 由 **code review** 承担。
#      脚本 MUST NOT 声称覆盖它。
#   ④ **逐用例扫描用「子串」匹配用例宏**（2026-10-26 实测确认的局限）：
#      END 块里按 `substr(t, pos, 9) == "TEST_CASE"` 等**子串**找用例起点，
#      而不是「行首的用例宏」⇒ 任何**含该子串的标识符**（如 `MY_TEST_CASE(`）
#      都会吃掉一个名字槽 ⇒ 后续真实用例的名字被误报为 `?`。
#      ⚠️ **计数不受影响**（断言数 / 用例数仍正确）⇒ 只影响违规消息里的**用例名**。
#      已由 fixture `identifier_lookalike.cpp` 钉住（A=2 / E=2 / C=[1,1]），
#      归位条件 = 分析器改为「按登记序取名」或「按行首匹配」。
#
# ⚠️ 为什么「宏体整行不计」（第五轮对抗审稿 BLOCKER-1）：宏体**可能永不展开** ——
#    实测把一个 52 条 `REQUIRE` 的 `#define DEAD_NEVER_EXPANDED()` 追加到被掏空的
#    文件尾，就能把计数从 0 灌回 52 而护栏全绿。宏体里的断言因此 MUST NOT 计入
#    （保守方向：少算 ⇒ 只可能误报，不会漏报）。
#
# ⚠️ 为什么「逐用例」而不只是「逐文件」（第五轮 BLOCKER-2）：文件级下限会被同文件
#    其它用例掩盖 —— 实测往 `test_pipeline_exec.cpp` 追加一个只有 `SUCCEED` 的
#    TEST_CASE，文件级计数不变 ⇒ 护栏全绿，而 delta 里写着「**每个**用例 MUST
#    至少断言一个语义确定的输出」。
# ═══════════════════════════════════════════════════════════════════════════
analyze_source() {
    awk 'BEGIN {
    SQ = "\047"; inbc = 0; depth = 0; pending = ""; live = ""; nc = 0; sk = 0
    ASM = "(REQUIRE_THROWS_MATCHES|CHECK_THROWS_MATCHES|REQUIRE_THROWS_WITH|CHECK_THROWS_WITH|REQUIRE_THROWS_AS|CHECK_THROWS_AS|REQUIRE_NOTHROW|CHECK_NOTHROW|REQUIRE_THROWS|CHECK_THROWS|REQUIRE_FALSE|CHECK_FALSE|STATIC_REQUIRE|STATIC_CHECK|REQUIRE|CHECK)"
}
function total_skip(   i) {
    for (i = 1; i <= depth; i++) if (gskip[i]) return 1
    return 0
}
function eval_cond(c) {
    sub(/[[:space:]]+$/, "", c)
    if (c ~ /^[-+]?[0-9]+$/) return (c + 0 != 0) ? 1 : 0
    return 2
}
# 在任意文本里数「可执行断言」：断言宏 + 词边界（前驱不是标识符字符）
# ⚠️ 词边界实现：**不**用「交替组里的 `^`」（`(^|[^A-Za-z0-9_])`）——
#    POSIX 对「子表达式内的 `^` 是否仍是锚点」**未定义** ⇒ 不同 awk 实现
#    （尤其 BSD/BWK awk = macOS 默认）可能按字面字符处理 ⇒ 行为分叉。
#    改为在串首**垫一个哨兵空格**（空格不是标识符字符 ⇒ 满足词边界），
#    这样边界判定只需 `[^A-Za-z0-9_]`，全部落在 POSIX 明确定义的范围内。
function count_asserts(str,   cnt, s) {
    cnt = 0; s = " " str
    while (match(s, "[^A-Za-z0-9_]" ASM "[[:space:]]*\\(") > 0) {
        cnt++
        s = substr(s, RSTART + RLENGTH)
    }
    return cnt
}
{
    line = $0
    if (pending != "") line = pending line
    if (line ~ /\\[[:space:]]*$/) { sub(/\\[[:space:]]*$/, "", line); pending = line; next }
    pending = ""

    keep = ""; out = ""; i = 1; n = length(line); instr = 0; inchr = 0
    while (i <= n) {
        c = substr(line, i, 1); two = substr(line, i, 2)
        if (inbc) { if (two == "*/") { inbc = 0; i += 2 } else { i++ }; continue }
        if (instr) {
            if (c == "\\") { keep = keep substr(line, i, 2); i += 2; continue }
            if (c == "\"") instr = 0
            keep = keep c; i++; continue
        }
        if (inchr) {
            if (c == "\\") { keep = keep substr(line, i, 2); i += 2; continue }
            if (c == SQ) inchr = 0
            keep = keep c; i++; continue
        }
        if (two == "/*") { inbc = 1; i += 2; continue }
        if (two == "//") break
        if (c == "\"") { instr = 1; keep = keep c; i++; continue }
        if (c == SQ) { inchr = 1; keep = keep c; i++; continue }
        keep = keep c; out = out c; i++
    }

    # ---- 预处理条件状态机（规则见 guardrail 头注释）----
    if (out ~ /^[[:space:]]*#[[:space:]]*define([[:space:]]|$)/) next
    if (out ~ /^[[:space:]]*#[[:space:]]*if[[:space:]]/) {
        c = out; sub(/^[[:space:]]*#[[:space:]]*if[[:space:]]*/, "", c)
        r = eval_cond(c)
        depth++; gres[depth] = r
        gskip[depth] = (r == 1) ? 0 : 1
        gtaken[depth] = (r == 1)
        next
    }
    if (out ~ /^[[:space:]]*#[[:space:]]*(ifdef|ifndef)/) {
        depth++; gres[depth] = 2; gskip[depth] = 1; gtaken[depth] = 0
        next
    }
    if (out ~ /^[[:space:]]*#[[:space:]]*elif/) {
        if (depth > 0) {
            if (gres[depth] == 2 || gtaken[depth]) {
                gskip[depth] = 1
            } else {
                c = out; sub(/^[[:space:]]*#[[:space:]]*elif[[:space:]]*/, "", c)
                r = eval_cond(c)
                if (r == 1) { gskip[depth] = 0; gtaken[depth] = 1 } else { gskip[depth] = 1 }
            }
        }
        next
    }
    if (out ~ /^[[:space:]]*#[[:space:]]*else/) {
        if (depth > 0) gskip[depth] = (gres[depth] == 2 || gtaken[depth]) ? 1 : 0
        next
    }
    if (out ~ /^[[:space:]]*#[[:space:]]*endif/) { if (depth > 0) depth--; next }

    if (total_skip()) {
        # 被跳过的区段里若有用例宏 ⇒ 记一笔（让「不可见的用例」至少**可见**）
        if (keep ~ /^[[:space:]]*(TEST_CASE|TEST_CASE_METHOD|TEMPLATE_TEST_CASE|TEMPLATE_PRODUCT_TEST_CASE|SCENARIO)[[:space:]]*\(/) sk++
        next
    }

    if (keep ~ /^[[:space:]]*(TEST_CASE|TEST_CASE_METHOD|TEMPLATE_TEST_CASE|TEMPLATE_PRODUCT_TEST_CASE|SCENARIO)[[:space:]]*\(/) {
        if (match(keep, /"[^"]*"/)) {
            nm = substr(keep, RSTART + 1, RLENGTH - 2)
            nc++; casenames[nc] = nm
        }
    }
    live = live "\n" out
}
END {
    printf "A %d\n", count_asserts(live)
    printf "S %d\n", sk

    t = live; m = length(t); pos = 1; inbody = 0; bd = 0; cur = ""; cni = 0; body = ""
    while (pos <= m) {
        if (!inbody) {
            if (substr(t, pos, 9) == "TEST_CASE" || substr(t, pos, 10) == "TEMPLATE_T" || substr(t, pos, 8) == "SCENARIO") {
                cni++
                cur = (cni <= nc) ? casenames[cni] : "?"
                pos += 8; continue
            }
            if (substr(t, pos, 1) == "{" && cur != "") { inbody = 1; bd = 1; body = ""; pos++; continue }
            pos++; continue
        }
        ch = substr(t, pos, 1)
        if (ch == "{") { bd++; body = body ch }
        else if (ch == "}") {
            bd--
            if (bd == 0) { printf "C %d %s\n", count_asserts(body), cur; inbody = 0; cur = ""; body = "" }
            else body = body ch
        } else body = body ch
        pos++
    }
    # ⚠️ 结束标记：证明 awk **完整执行到底**（没在中间出错退出）。
    #    调用方据此判 fail-closed（分析器半途失败 ⇒ 无此标记 ⇒ 报违规）。
    printf "E %d\n", nc
}' "$1"
}

# ---------------------------------------------------------------------------
# fail-closed 判定（**单一实现**）：护栏与 fixture 自检共用
# ---------------------------------------------------------------------------
# ⚠️ 为什么要有「状态」概念（第八轮对抗审稿 BLOCKER-1）：
#    分析器可能**半途失败** ⇒ 它的输出不可信 ⇒ MUST 报违规，而不是打出假 OK。
#    失效形态（实测复现）：
#      ① `analyze_source` 返回**空**（awk 完全没跑起来）；
#      ② awk 输出 `A`/`S` 之后**出错退出** ⇒ 输出非空但**没有结束标记 `E`**
#         ⇒ 逐用例扫描根本没跑 ⇒ 原实现会打「0 个字面用例，每个都 ≥ 1 条断言」
#         且 violations 保持 0 ⇒ **假绿**。
#    ⇒ 状态判定 MUST **先于**任何「看起来 OK」的结论。
aq_analyze_status() {
    # $1 = 分析器输出（可能为空 / 可能缺结束标记）
    local _aq_in="$1"
    if [ -z "$_aq_in" ]; then
        printf 'empty\n'
        return 0
    fi
    if ! printf '%s\n' "$_aq_in" | grep -q '^E '; then
        printf 'no_end_marker\n'
        return 0
    fi
    printf 'ok\n'
}

# C4b 的 fail-closed 违规理由（**空串 = 无违规**）。
# ⚠️ 消息文本与护栏原实现**逐字一致**（护栏调用本函数 ⇒ 输出不变）。
#    逐字一致是刻意的：任何消息改动都应先改这里，避免两处漂移。
aq_violation_reason() {
    # $1 = 源文件路径（用于消息）；$2 = 分析器输出
    local _aq_src="$1"
    local _aq_out="$2"
    local _aq_status _aq_ncases _aq_e_nc

    _aq_status="$(aq_analyze_status "$_aq_out")"
    if [ "$_aq_status" = "empty" ]; then
        printf '%s 的断言分析**没有任何输出** —— 分析器失效 MUST 视为违规，不得静默放行\n' "$_aq_src"
        return 0
    fi
    if [ "$_aq_status" = "no_end_marker" ]; then
        printf '%s 的断言分析**未完整执行**（缺少结束标记）—— 分析器中途失败 MUST 视为违规\n' "$_aq_src"
        return 0
    fi
    # ⚠️ 结束标记**只证明 awk 跑到 END**，不证明**逐用例扫描真的产出了 `C` 行**。
    #    失效形态（第九轮对抗审稿 SHOULD_FIX）：源文件里含 raw string
    #    （`R"( ... )"`）⇒ 其中的裸 `{` / `"` 破坏扫描器的 brace-matching
    #    ⇒ 一行 `C` 都没产出 ⇒ 原实现会打「0 个字面用例，每个都 ≥ 1 条断言」并
    #    以 0 违规退出（假绿）。⇒ 逐用例计数 MUST 与 `E` 行携带的字面用例数**相等**。
    _aq_ncases="$(printf '%s\n' "$_aq_out" | grep -c '^C ' || true)"
    _aq_e_nc="$(printf '%s\n' "$_aq_out" | awk '/^E /{print $2; exit}')"
    if [ "$_aq_ncases" != "$_aq_e_nc" ]; then
        printf '%s 的逐用例计数（%s）与分析器报告的字面用例数（%s）不符 —— 逐用例扫描**未完整执行**（如 raw string 破坏 brace-matching）MUST 视为违规\n' \
            "$_aq_src" "$_aq_ncases" "$_aq_e_nc"
        return 0
    fi
    printf ''
}
