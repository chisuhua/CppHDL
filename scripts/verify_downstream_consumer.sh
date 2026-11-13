#!/usr/bin/env bash
# scripts/verify_downstream_consumer.sh
#
# Guardrail: the INSTALLED upstream artifact MUST still compile a DOWNSTREAM
# consumer TU — including the by-value-member-of-a-virtual-dtor-base shape.
#
# WHY this needs a guardrail (issue #28):
#
#   Every upstream test compiles against the SOURCE TREE include set
#   (`-I include -I include/core -I include/ast ...`). A downstream project
#   compiles against `cmake --install` output, in its own build system, with a
#   single `<prefix>/include/cpp-hdl` root. Those are different worlds:
#     - ADR-067 broke the downstream shape (`FsmBase : PluginBase` holding the
#       FSM by value) and every upstream test stayed GREEN, because upstream
#       never builds a downstream-shaped TU;
#     - the installed prefix is NOT single-directory self-sufficient
#       (`ast/ast_nodes.h` does `#include "lnodeimpl.h"`, which installs into
#       `core/`) => the 5 include dirs below mirror the upstream
#       `CH_INCLUDE_DIRS` contract (CMakeLists.txt:116-123).
#
# Rules:
#   R1  the fixture exists (`tests/downstream-consumer/{main.cpp,CMakeLists.txt}`)
#       => missing fixture is FAILURE, never a skip (fail-closed)
#   R2  `cmake --install` into a TEMPORARY prefix succeeds
#       (=> never pollutes the working tree)
#   R3  the prefix carries what a consumer needs
#       (`include/cpp-hdl/chlib/state_machine.h` + `lib/libcpphdl.a`)
#   R4  the independent CMake project configures AND builds
#   R5  the consumer binary RUNS (exit 0) — a link-only check is not enough
#   R6  the fixture covers the three holding shapes
#       (local variable / unique_ptr / by-value member of a virtual-dtor base)
#
# Self-test (`--self-test`) proves discriminating power:
#   M1 poison the INSTALLED copy (revert the composable destructor to
#      `noexcept(false)`) => the consumer build MUST fail (this is exactly the
#      issue #28 breakage, reproduced from the installed artifact)
#   M2 an empty fixture directory => R1 MUST go red (fail-closed)
#
# Exit codes: 0 = pass, 1 = violation, 2 = usage error.
#
# Usage:
#   scripts/verify_downstream_consumer.sh [--self-test]
#
# Environment:
#   BUILD_DIR                  upstream build directory (default: <repo>/build)
#   DOWNSTREAM_FIXTURE_DIR     fixture directory override (self-test hook only)

set -uo pipefail

REPO_ROOT="$(git rev-parse --show-toplevel 2>/dev/null || pwd)"
cd "$REPO_ROOT" || exit 2

BUILD_DIR="${BUILD_DIR:-$REPO_ROOT/build}"
FIXTURE_DIR="$REPO_ROOT/tests/downstream-consumer"
# ⚠️ **编译器 MUST 与上游 build 一致**（2026-11-13 修 CI 红 · RC-3 根因）：
#   本护栏原来用 `c++` ⇒ ⇒ 而 CI 里上游 build 用的是 `/usr/local/bin/g++`
#   （Install g++ 步骤建的 symlink → **g++-13**）⇒ ⇒ `c++` 却是 `/usr/bin/c++`
#   = **g++-11** ⇒ ⇒ **g++-11 不支持 C++23** ⇒ ⇒ 下游工程 configure **通过**
#   （"Detecting CXX compiler ABI info - done"）但 **build 挂**
#   ⇒ ⇒ 失败与被测代码**无关**（纯编译器口径不一致）。
#   ⇒ ⇒ 正解 = **从上游的 CMakeCache 读 `CMAKE_CXX_COMPILER`** ⇒ ⇒ 恒等于上游
#   ⇒ ⇒ （`CXX` 环境变量仍可显式覆盖 ⇒ ⇒ 便于排查）。
CXX_BIN="${CXX:-}"
if [ -z "$CXX_BIN" ] && [ -f "$BUILD_DIR/CMakeCache.txt" ]; then
    CXX_BIN="$(sed -n 's/^CMAKE_CXX_COMPILER:[^=]*=//p' "$BUILD_DIR/CMakeCache.txt" 2>/dev/null | head -1)"
fi
CXX_BIN="${CXX_BIN:-c++}"

WORK="$(mktemp -d)"
FAILURES=()

fail() { FAILURES+=("$1"); }
cleanup() { rm -rf "$WORK"; return 0; }
trap cleanup EXIT

# run_consumer <fixture-dir> <prefix> <work-subdir>
#   => configures + builds + runs the independent project; 0 on success
run_consumer() {
    local fixture="$1" prefix="$2" workdir="$3"
    rm -rf "$workdir"
    mkdir -p "$workdir/src"
    cp "$fixture/main.cpp" "$fixture/CMakeLists.txt" "$workdir/src/" || return 1
    if ! cmake -S "$workdir/src" -B "$workdir/build" \
        -DCPPHDL_PREFIX="$prefix" \
        -DCMAKE_CXX_COMPILER="$CXX_BIN" > "$workdir/configure.log" 2>&1; then
        return 1
    fi
    if ! cmake --build "$workdir/build" -j 2 > "$workdir/build.log" 2>&1; then
        return 1
    fi
    "$workdir/build/consumer" > "$workdir/run.log" 2>&1
}

install_prefix() { # <prefix>
    cmake --install "$BUILD_DIR" --prefix "$1" > "$WORK/install.log" 2>&1
}

check_r1_fixture() {
    local bad=0 f
    for f in main.cpp CMakeLists.txt; do
        if [ ! -f "$FIXTURE_DIR/$f" ]; then
            fail "R1 fixture 缺失：$FIXTURE_DIR/$f（fail-closed：缺 fixture 即失败，不跳过）"
            bad=1
        fi
    done
    return $bad
}

check_r6_shapes() {
    local bad=0
    local f="$FIXTURE_DIR/main.cpp"
    [ -f "$f" ] || return 1
    grep -q "形态 ①：局部变量" "$f" || { fail "R6 fixture 缺持有形态 ①（局部变量）"; bad=1; }
    grep -q "形态 ②：unique_ptr" "$f" || { fail "R6 fixture 缺持有形态 ②（unique_ptr）"; bad=1; }
    grep -q "形态 ③：按值成员" "$f" || { fail "R6 fixture 缺持有形态 ③（按值成员 + 虚析构基类）"; bad=1; }
    return $bad
}

check_all() {
    local bad=0
    # re-resolve the fixture dir (the self-test overrides it per-invocation)
    FIXTURE_DIR="${DOWNSTREAM_FIXTURE_DIR:-$REPO_ROOT/tests/downstream-consumer}"
    check_r1_fixture || bad=1
    check_r6_shapes || bad=1
    [ "$bad" -eq 0 ] || return 1

    # R2: install into a temporary prefix
    if [ ! -d "$BUILD_DIR" ]; then
        fail "R2 上游 build 目录不存在：$BUILD_DIR（先 cmake -B build && cmake --build build）"
        return 1
    fi
    if ! install_prefix "$WORK/prefix"; then
        fail "R2 cmake --install 失败（见 $WORK/install.log）"
        return 1
    fi

    # R3: the prefix carries headers + the static library
    if [ ! -f "$WORK/prefix/include/cpp-hdl/chlib/state_machine.h" ]; then
        fail "R3 prefix 缺 include/cpp-hdl/chlib/state_machine.h"
        bad=1
    fi
    if [ ! -f "$WORK/prefix/lib/libcpphdl.a" ]; then
        fail "R3 prefix 缺 lib/libcpphdl.a"
        bad=1
    fi
    [ "$bad" -eq 0 ] || return 1

    # R4 + R5: independent project builds and the binary runs
    if ! run_consumer "$FIXTURE_DIR" "$WORK/prefix" "$WORK/consumer"; then
        # ⚠️ **必须把失败日志的尾部打进输出**（2026-11-13 修 CI 可诊断性）：
        #   原来只报「见 $WORK/consumer/*.log」⇒ ⇒ 那路径在 CI runner 的
        #   `/tmp` 里 ⇒ ⇒ **job 结束即丢** ⇒ ⇒ 失败**无法定位**（实测：
        #   2026-11-12 的 CI run 里这条红只留了一行「见 /tmp/.../*.log」
        #   ⇒ ⇒ 拿不到真实原因）⇒ ⇒ ⇒ 这里补上 configure / build / run
        #   **三个日志的尾部**（各 15 行）⇒ ⇒ 下一轮 CI 就能自证根因。
        fail "R4/R5 下游视角工程 configure/build/run 失败（见 $WORK/consumer/*.log）"
        for lg in configure build run; do
            if [ -f "$WORK/consumer/$lg.log" ]; then
                printf '  ── %s.log (tail 15) ──\n' "$lg" >&2
                tail -15 "$WORK/consumer/$lg.log" | sed 's/^/    /' >&2
            fi
        done
        return 1
    fi
    return 0
}

run_quietly() {
    local saved=("${FAILURES[@]+"${FAILURES[@]}"}")
    FAILURES=()
    check_all
    local rc=0
    [ "${#FAILURES[@]}" -gt 0 ] && rc=1
    FAILURES=("${saved[@]+"${saved[@]}"}")
    return $rc
}

self_test() {
    echo "self-test: M1/M2 ..."

    # R2/R3/R4/R5 must be green first (otherwise the poisons prove nothing)
    if ! check_all; then
        echo "❌ self-test: the unpoisoned run is already RED — cannot measure discriminating power" >&2
        for f in "${FAILURES[@]}"; do echo "   - $f" >&2; done
        return 1
    fi
    # keep the good prefix from the successful run, then poison a COPY
    cp -a "$WORK/prefix" "$WORK/prefix_poison"
    local installed="$WORK/prefix_poison/include/cpp-hdl/chlib/state_machine.h"
    if ! grep -q 'noexcept(kDetectAtDtor ? false : true)' "$installed"; then
        echo "❌ self-test: M1 poisoning target not found in the installed header" >&2
        return 1
    fi
    sed -e 's/noexcept(kDetectAtDtor ? false : true)/noexcept(false)/' \
        "$installed" > "$WORK/state_machine_poisoned.h"
    mv "$WORK/state_machine_poisoned.h" "$installed"
    if run_consumer "$FIXTURE_DIR" "$WORK/prefix_poison" "$WORK/consumer_poison"; then
        echo "❌ self-test: M1 (revert the composable destructor in the INSTALLED copy) MUST break the consumer build" >&2
        return 1
    fi

    # M2: empty fixture directory => R1 MUST go red (fail-closed, never a skip)
    mkdir -p "$WORK/empty_fixture"
    if DOWNSTREAM_FIXTURE_DIR="$WORK/empty_fixture" run_quietly; then
        echo "❌ self-test: M2 (empty fixture) MUST make R1 red" >&2
        return 1
    fi

    echo "✅ self-test: M1/M2 each turned the guard RED (installed-artifact breakage reproduced; missing fixture fails closed)"
    return 0
}

case "${1:-}" in
    --self-test)
        self_test
        exit $?
        ;;
    "")
        ;;
    *)
        echo "usage: $0 [--self-test]" >&2
        exit 2
        ;;
esac

check_all

if [ "${#FAILURES[@]}" -gt 0 ]; then
    echo "❌ verify_downstream_consumer: ${#FAILURES[@]} violation(s)" >&2
    for f in "${FAILURES[@]}"; do
        echo "   - $f" >&2
    done
    exit 1
fi

echo "✅ verify_downstream_consumer: PASS (R1 fixture · R2 install→临时 prefix · R3 产物齐备 · R4/R5 独立工程 build+run · R6 三种持有形态)"
exit 0
