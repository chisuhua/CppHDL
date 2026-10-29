#!/usr/bin/env bash
# scripts/verify_fsm_next_wiring.sh
#
# Guardrail: an `on_active` closure MUST NOT write a register's `next` and
# MUST NOT drive an IO port.
#
# WHY this is a silent breakage (zero compile error, zero test failure):
#
#   ch_state_machine::build() executes EVERY state's `on_active` closure once,
#   at describe time, so that the `transition_when` calls inside them can be
#   registered into the next-state select tree
#   (include/chlib/state_machine.h -> build()).
#
#   => A register written from inside a closure is written N times, and
#      `ch_reg::next` is LAST-WINS (`regimpl::set_next` == `set_src(0, next)`,
#      include/ast/ast_nodes.h:47-54) => only the LAST state to be emitted
#      survives.
#   => An output driven from inside a closure is likewise overwritten
#      (`ch_out::operator=` == `set_src(0, ...)`) => the last emitter wins.
#
#   The typical symptom is silent: the "load the data" branch of the FIRST
#   state never takes effect, because a LATER state's assignment overwrote
#   it. Two real occurrences were found in this repo (2026-10-29):
#     - examples/spinalhdl-ported/uart/uart_tx.cpp  (the FSM never worked)
#     - tests/test_verilator_complex_e2e.cpp        (the SPI fixture never
#       worked; its assertions were parity-only => same defect on both
#       backends => parity was green)
#
# Two rules:
#   R1: an `on_active` closure MUST NOT contain `<reg>->next = ...`
#   R2: an `on_active` closure MUST NOT assign to an IO port
#       (`io().<port> = ...` or `io().<port> <<= ...`; comparisons are fine)
#
# The contract lives in capability `chlib-state-machine-semantics`.
#
# Exit codes: 0 = no violation, 1 = at least one violation.
#
# Usage:
#   scripts/verify_fsm_next_wiring.sh [--self-test] [<scan-root>]
#
# ⚠️ PORTABILITY: bash + python3 only (no GNU-only `sed -i`; the fixtures are
#    written with `cat` heredocs) => identical on ubuntu-22.04 / macos-14.
#
# ⚠️ The analyser is a hand-rolled C/C++ lexer and therefore NOT fully
#    closable: it can only see DIRECT writes inside the closure body (a write
#    performed by a called helper function is invisible). Its direction is
#    "under-report, never false-positive" (except for `parse-error`, which is
#    fail-closed by design). See scripts/lib/fsm_wiring.py for the exact
#    capability boundary.

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd -P)"
cd "$REPO_ROOT"

CHECK_ROOT="$REPO_ROOT"
if [ "${1:-}" != "--self-test" ] && [ -n "${1:-}" ]; then
    CHECK_ROOT="$1"
fi

RED=$'\033[0;31m'
GREEN=$'\033[0;32m'
NC=$'\033[0m'

# ---------------------------------------------------------------------------
# run_guard <scan-root>：跑共享分析器（scripts/lib/fsm_wiring.py）
#   argv[1] = 扫描根目录；argv[2] = 仓库根（用于定位 lib）
#   退出码：0 = 无违规；1 = 至少一处违规
# ---------------------------------------------------------------------------
run_guard() {
    python3 - "$1" "$REPO_ROOT" <<'PY' || return 1
import os
import sys

scan_root = os.path.abspath(sys.argv[1])
repo_root = os.path.abspath(sys.argv[2])
sys.path.insert(0, os.path.join(repo_root, "scripts", "lib"))
import fsm_wiring  # noqa: E402

violations = []
scanned = 0
for path in fsm_wiring.iter_source_files(scan_root):
    scanned += 1
    try:
        violations.extend(fsm_wiring.analyze_file(path))
    except OSError as exc:
        print("VIOLATION %s: 读取失败 ⇒ fail-closed (%s)" % (path, exc),
              file=sys.stderr)
        sys.exit(1)

rel_root = os.path.relpath(scan_root, repo_root)
if rel_root == ".":
    rel_root = ""

for v in violations:
    p = v.path
    if rel_root and p.startswith(scan_root + os.sep):
        p = os.path.join(rel_root, os.path.relpath(p, scan_root))
    print("VIOLATION %s:%d: %s: %s" % (p, v.line, v.kind, v.snippet),
          file=sys.stderr)

if violations:
    print("%d violation(s) in %d scanned file(s)" % (len(violations), scanned),
          file=sys.stderr)
    sys.exit(1)
print("  OK：扫描 %d 个源文件，未发现 on_active 闭包内的寄存器/IO 写入" % scanned)
PY
}

# ---------------------------------------------------------------------------
# --self-test：断言本护栏的每条判据**各自有判别力**
# （fixture 集 = 规格；护栏与自检共用 scripts/lib/fsm_wiring.py 的同一份实现）
# ---------------------------------------------------------------------------
if [ "${1:-}" = "--self-test" ]; then
    echo "=== verify_fsm_next_wiring: self-test ==="
    if ! command -v python3 >/dev/null 2>&1; then
        echo "${RED}❌ self-test: 缺少 python3 ⇒ 无法自测${NC}" >&2
        exit 1
    fi
    WORK="$(mktemp -d)"
    trap 'rm -rf "$WORK"' EXIT

    fx() { mkdir -p "$WORK/$1"; cat > "$WORK/$1/sample.cpp"; }

    # --- 1. 干净：闭包内**只**写 transition_when（`io().start` 作操作数）---
    fx fx_clean <<'EOF'
#include "ch.hpp"
class M : public ch::Component {
public:
    __io(ch_in<ch_bool> start; ch_out<ch_bool> busy;)
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override {
        chlib::ch_state_machine<S, 2> sm;
        auto start_sig = io().start;
        sm.state(S::A).on_active([&] {
            sm.transition_when(start_sig, S::B);
        });
        sm.set_entry(S::A);
        sm.build();
        io().busy = select(sm.is_in(S::B), ch_bool(true), ch_bool(false));
    }
};
EOF

    # --- 2. 闭包内写寄存器 next（R1）---
    fx fx_reg_next <<'EOF'
#include "ch.hpp"
class M : public ch::Component {
public:
    __io(ch_in<ch_bool> start; ch_out<ch_bool> busy;)
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override {
        ch_reg<ch_uint<8>> counter(0_d);
        chlib::ch_state_machine<S, 2> sm;
        sm.state(S::A).on_active([&] {
            counter->next = ch_uint<8>(1_d);
        });
        sm.set_entry(S::A);
        sm.build();
    }
};
EOF

    # --- 3. 闭包内驱动输出（R2，`=`）---
    fx fx_io_assign <<'EOF'
#include "ch.hpp"
class M : public ch::Component {
public:
    __io(ch_in<ch_bool> start; ch_out<ch_bool> busy;)
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override {
        chlib::ch_state_machine<S, 2> sm;
        sm.state(S::A).on_active([&] {
            io().busy = ch_bool(true);
        });
        sm.set_entry(S::A);
        sm.build();
    }
};
EOF

    # --- 4. 闭包内驱动输出（R2，`<<=`）---
    fx fx_io_assign_shl <<'EOF'
#include "ch.hpp"
class M : public ch::Component {
public:
    __io(ch_in<ch_bool> start; ch_out<ch_bool> busy;)
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override {
        chlib::ch_state_machine<S, 2> sm;
        sm.state(S::A).on_active([&] {
            io().busy <<= ch_bool(true);
        });
        sm.set_entry(S::A);
        sm.build();
    }
};
EOF

    # --- 5. 闭包**外**写寄存器 next（合法 ⇒ PASS）---
    fx fx_outside <<'EOF'
#include "ch.hpp"
class M : public ch::Component {
public:
    __io(ch_in<ch_bool> start; ch_out<ch_bool> busy;)
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override {
        ch_reg<ch_uint<8>> counter(0_d);
        chlib::ch_state_machine<S, 2> sm;
        sm.state(S::A).on_active([&] {
            sm.transition_when(io().start, S::B);
        });
        sm.set_entry(S::A);
        sm.build();
        counter->next = ch_uint<8>(1_d);
    }
};
EOF

    # --- 6. 闭包内把 IO 当**操作数**比较（合法 ⇒ PASS）---
    fx fx_compare <<'EOF'
#include "ch.hpp"
class M : public ch::Component {
public:
    __io(ch_in<ch_uint<8>> x; ch_out<ch_bool> busy;)
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override {
        chlib::ch_state_machine<S, 2> sm;
        sm.state(S::A).on_active([&] {
            sm.transition_when(io().x == ch_uint<8>(3_d), S::B);
        });
        sm.set_entry(S::A);
        sm.build();
    }
};
EOF

    # --- 7. 注释里的 `->next =`（合法 ⇒ PASS）---
    fx fx_comment <<'EOF'
#include "ch.hpp"
class M : public ch::Component {
public:
    __io(ch_in<ch_bool> start; ch_out<ch_bool> busy;)
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override {
        chlib::ch_state_machine<S, 2> sm;
        sm.state(S::A).on_active([&] {
            // counter->next = ch_uint<8>(1_d);
            /* io().busy = ch_bool(true); */
            sm.transition_when(io().start, S::B);
        });
        sm.set_entry(S::A);
        sm.build();
    }
};
EOF

    # --- 8. 字符串里的 `->next =`（合法 ⇒ PASS）---
    fx fx_string <<'EOF'
#include "ch.hpp"
class M : public ch::Component {
public:
    __io(ch_in<ch_bool> start; ch_out<ch_bool> busy;)
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override {
        chlib::ch_state_machine<S, 2> sm;
        sm.state(S::A).on_active([&] {
            const char *s = "counter->next = 1;";
            (void)s;
            sm.transition_when(io().start, S::B);
        });
        sm.set_entry(S::A);
        sm.build();
    }
};
EOF

    # --- 9. 闭包内**嵌套** lambda，其内有 `->next`（仍在闭包体内 ⇒ 违规）---
    fx fx_nested_lambda <<'EOF'
#include "ch.hpp"
class M : public ch::Component {
public:
    __io(ch_in<ch_bool> start; ch_out<ch_bool> busy;)
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override {
        ch_reg<ch_uint<8>> counter(0_d);
        chlib::ch_state_machine<S, 2> sm;
        sm.state(S::A).on_active([&] {
            auto helper = [&]() { counter->next = ch_uint<8>(1_d); };
            helper();
        });
        sm.set_entry(S::A);
        sm.build();
    }
};
EOF

    # --- 10. 花括号不配平（fail-closed ⇒ 违规）---
    fx fx_unbalanced <<'EOF'
#include "ch.hpp"
class M : public ch::Component {
public:
    __io(ch_in<ch_bool> start; ch_out<ch_bool> busy;)
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override {
        chlib::ch_state_machine<S, 2> sm;
        sm.state(S::A).on_active([&] {
            if (true) {
            sm.transition_when(io().start, S::B);
        });
        sm.set_entry(S::A);
        sm.build();
    }
};
EOF

    # --- 11. `on_active` 的**成员函数定义**（非调用 ⇒ PASS）---
    fx fx_member_def <<'EOF'
#include "ch.hpp"
struct state_def {
    void *active_action = nullptr;
    state_def &on_active(void *action) {
        active_action = action;
        return *this;
    }
};
EOF

    # --- 12. 无参数列表的 lambda（`on_active([&]{ ... })`）⇒ PASS ---
    fx fx_no_parens_lambda <<'EOF'
#include "ch.hpp"
class M : public ch::Component {
public:
    __io(ch_in<ch_bool> start; ch_out<ch_bool> busy;)
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override {
        chlib::ch_state_machine<S, 2> sm;
        sm.state(S::A).on_active([&]{
            sm.transition_when(io().start, S::B);
        });
        sm.set_entry(S::A);
        sm.build();
    }
};
EOF

    fails=0
    # expect <case> <期望exit> <输出必须含> <输出不得含>
    expect() {
        local name="$1" want_rc="$2" want="$3" reject="$4"
        local out rc
        set +e
        out="$(run_guard "$WORK/$name" 2>&1)"
        rc=$?
        set -e
        local ok=1
        if [ "$rc" != "$want_rc" ]; then
            ok=0
            echo "  ✗ $name: 退出码 $rc，期望 $want_rc"
        fi
        if [ -n "$want" ] && ! printf '%s' "$out" | grep -qF "$want"; then
            ok=0
            echo "  ✗ $name: 输出缺少 '$want'"
            printf '%s\n' "$out" | sed 's/^/      /' | head -5
        fi
        if [ -n "$reject" ] && printf '%s' "$out" | grep -qF "$reject"; then
            ok=0
            echo "  ✗ $name: 输出不应含 '$reject'"
        fi
        if [ "$ok" = "1" ]; then
            echo "  ✓ $name"
        else
            fails=$((fails + 1))
        fi
    }

    expect fx_clean          0 ""              ""
    expect fx_reg_next       1 "reg-next-in-closure" ""
    expect fx_io_assign      1 "io-assign-in-closure" ""
    expect fx_io_assign_shl  1 "io-assign-in-closure" ""
    expect fx_outside        0 ""              ""
    expect fx_compare        0 ""              ""
    expect fx_comment        0 ""              ""
    expect fx_string         0 ""              ""
    expect fx_nested_lambda  1 "reg-next-in-closure" ""
    expect fx_unbalanced     1 "parse-error"   ""
    expect fx_member_def     0 ""              ""
    expect fx_no_parens_lambda 0 ""            ""

    if [ "$fails" -ne 0 ]; then
        echo "${RED}❌ verify_fsm_next_wiring self-test: $fails 个 fixture 失败${NC}" >&2
        exit 1
    fi
    echo "${GREEN}✅ verify_fsm_next_wiring self-test: 12/12 fixture 全绿${NC}"
    exit 0
fi

# ---------------------------------------------------------------------------
# 主检查路径
# ---------------------------------------------------------------------------
if ! command -v python3 >/dev/null 2>&1; then
    echo "${RED}❌ verify_fsm_next_wiring: 缺少 python3 ⇒ 无法检查${NC}" >&2
    exit 1
fi

if run_guard "$CHECK_ROOT"; then
    echo "${GREEN}✅ verify_fsm_next_wiring: PASS${NC}"
    exit 0
fi
echo "${RED}❌ verify_fsm_next_wiring: 有违规${NC}" >&2
exit 1
