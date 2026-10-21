#!/usr/bin/env bash
# scripts/verify_rst_n_wiring.sh
#
# Guard against the **silent** "permanently reset" breakage:
# an unwired `ch_in<ch_bool> rst_n` reads back as 0, which means
# "permanently in reset". The component compiles, the test binary builds,
# the simulator runs without any error - but every register stays at its
# init value (writes never land, reads return 0).
#
# That is a ZERO-COMPILE-ERROR silent breakage, so it must be caught
# statically. Three complementary rules:
#
#   R1 (per test case): a test case that SIMULATES a component with a
#       mandatory `rst_n` port MUST drive it
#       (`sim.set_input_value(io().rst_n, 1)`), either directly or through
#       a file-scope helper that does (e.g. `gpio_init(sim, io)`).
#       Granularity matters: file level is not enough, because a file that
#       drives rst_n for one DUT would mask another DUT in the same file.
#
#   R2 (per file): a file that wraps a component with a mandatory `rst_n`
#       port in a `ch_module` MUST forward the port
#       (`slave.io().rst_n <<= io().rst_n;`) so an outer testbench can
#       drive it.
#
#   R3 (per file): a file that simulates a mandatory-rst_n component MUST
#       contain at least one rst_n drive somewhere (a coarse safety net
#       for constructs R1 cannot see, e.g. a DUT instantiated in a
#       fixture header).
#
# Opt-out: a file that deliberately exercises the UNWIRED (permanently
# reset) behaviour - i.e. the runtime guard itself - MUST carry the
# marker line
#
#     // rst_n_guard: intentionally-unwired
#
# Such a file is exempted from R1/R3 (it is the thing that proves the
# unwired behaviour is a real, observable state) but it is NOT silent:
# the script prints a NOTE naming every exempted file, so a reviewer sees
# the exemption in the CI log.
#
# The exemption is NARROW: besides the marker, the file name must contain
# "rst_n_guard" (only the runtime guard may claim it). A marker dropped
# into an arbitrary test file does NOT exempt that file.
#
# Exit codes: 0 = all sites wired, 1 = at least one missing.
#
# Usage: scripts/verify_rst_n_wiring.sh
#
# Maintenance: COMPONENTS_W_RST_N below MUST stay in sync with the set of
# components that expose a mandatory `ch_in<ch_bool> rst_n` port
# (grep -rln "ch_in<ch_bool> rst_n" include/).
#
# Only bash + awk are used (portable across the ubuntu-22.04 / macos-14
# CI matrix; no python dependency).

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT"

# Components exposing a MANDATORY rst_n port (low active, effective on the
# clock edge). An unwired instance is permanently in reset.
COMPONENTS_W_RST_N="AxiLiteSlave Axi4Slave AxiToAxiLiteConverter AxiLiteGpio AxiLiteUart AxiLiteTimer AxiLitePwm AxiLiteI2c AxiLiteSpi AxiDma"

# Component definitions and non-product trees are excluded from the scan.
scan_files() {
  # NOTE: a test file that only uses DUAL_BACKEND_TEST_PAIR never spells
  # out ch_device (the construction lives inside the macro), so the macro
  # name must be part of the scan filter as well.
  grep -rlnE "ch_device|ch_module|DUAL_BACKEND_TEST" \
    --include='*.cpp' --include='*.h' examples tests src include 2>/dev/null \
  | grep -v "^include/axi4/" \
  | sort -u
}

violations=0

for f in $(scan_files); do
  # ---- R1 + R3 -----------------------------------------------------------
  base=$(basename "$f")
  out=$(awk -v comps="$COMPONENTS_W_RST_N" -v base="$base" '
    function has_comp(s,   i, c, n, arr) {
        n = split(comps, arr, " ")
        for (i = 1; i <= n; i++) {
            c = arr[i]
            if (s ~ ("[^A-Za-z0-9_]" c "[^A-Za-z0-9_]")) return 1
        }
        return 0
    }
    # An alias stands for a mandatory-rst_n component, e.g.
    #   using Full32 = axi4::Axi4Slave<32, 32, 4, 16, 16>;
    #   ch::ch_device<Full32> dev;
    function has_comp_or_alias(s,   a) {
        if (has_comp(s)) return 1
        for (a in alias_of) {
            if (s ~ ("[^A-Za-z0-9_]" a "[^A-Za-z0-9_]")) return 1
        }
        return 0
    }
    # A file-scope helper that drives rst_n counts as driving for every
    # test case that calls it (e.g. `gpio_init(sim, io);`).
    function calls_driver(s,   d) {
        for (d in driver) {
            if (s ~ ("[^A-Za-z0-9_]" d "[ \t]*\\(")) return 1
        }
        return 0
    }
    function flush_block(   has_dev, has_sim, has_drive) {
        if (block == "") return
        # a DUAL_BACKEND_TEST_PAIR block always builds a ch_device
        # (the construction lives inside the macro, not in the source)
        has_dev   = (block ~ /ch_device|ch_module/) || (block_is_macro == 1)
        # a test case simulates when it constructs a Simulator, or when it
        # uses the DUAL_BACKEND_TEST_PAIR macro (which builds one)
        has_sim   = (block ~ /Simulator[ \t]+[A-Za-z_][A-Za-z0-9_]*[ \t]*\(/) || (block_is_macro == 1)
        has_drive = (block ~ /set_input_value\([^;]*rst_n/) || calls_driver(block)
        if (!optout && has_comp_or_alias(block) && has_dev && has_sim && !has_drive) {
            printf("VIOLATION\t%s\t%s\n", FILENAME, tcname)
        }
    }
    { lines[NR] = $0 }
    END {
        # the whole file, and the explicit opt-out marker
        whole = ""
        for (i = 1; i <= NR; i++) whole = whole lines[i] "\n"
        optout = (whole ~ /rst_n_guard: intentionally-unwired/) && (base ~ /rst_n_guard/)
        # pass 1: collect aliases of mandatory-rst_n components
        for (i = 1; i <= NR; i++) {
            if (lines[i] ~ /using[ \t]+[A-Za-z_][A-Za-z0-9_]*[ \t]*=/) {
                l = lines[i]
                sub(/^[ \t]*using[ \t]+/, "", l)
                a = l
                sub(/[ \t=].*$/, "", a)
                if (has_comp(lines[i])) alias_of[a] = 1
            }
        }
        # pass 2: file-scope functions whose body drives rst_n
        i = 1
        while (i <= NR) {
            l = lines[i]
            if (l ~ /^(TEST_CASE|DUAL_BACKEND_TEST)/) { i++; continue }
            if (l ~ /^[A-Za-z_][A-Za-z0-9_:<>,&*[:space:]]*[A-Za-z_][A-Za-z0-9_]*[[:space:]]*\(/) {
                tmp = l
                sub(/[[:space:]]*\([^\(]*$/, "", tmp)
                m = split(tmp, w, /[^A-Za-z0-9_]+/)
                fname = w[m]
                body = ""
                j = i
                while (j <= NR) {
                    body = body lines[j] "\n"
                    if (j > i && lines[j] ~ /^\}/) break
                    j++
                }
                if (body ~ /set_input_value\([^;]*rst_n/) driver[fname] = 1
                i = j + 1
                continue
            }
            i++
        }
        # pass 3: walk the test-case blocks
        for (i = 1; i <= NR; i++) {
            if (lines[i] ~ /^(TEST_CASE|DUAL_BACKEND_TEST_PAIR|DUAL_BACKEND_TEST)[ \t]*\(/) {
                flush_block()
                block_is_macro = (lines[i] ~ /^DUAL_BACKEND_TEST/) ? 1 : 0
                tcname = lines[i]
                sub(/^[ \t]*/, "", tcname)
                if (length(tcname) > 90) tcname = substr(tcname, 1, 90) "..."
                block = lines[i] "\n"
                continue
            }
            if (block != "") block = block lines[i] "\n"
        }
        flush_block()
        # R3: the file as a whole must contain a drive somewhere
        w_comp   = has_comp_or_alias(whole)
        w_dev    = (whole ~ /ch_device|ch_module|DUAL_BACKEND_TEST/)
        w_sim    = (whole ~ /Simulator[ \t]+[A-Za-z_][A-Za-z0-9_]*[ \t]*\(/)
        w_drive  = (whole ~ /set_input_value\([^;]*rst_n/)
        if (!optout && w_comp && w_dev && w_sim && !w_drive) {
            printf("VIOLATION_FILE\t%s\t-\n", FILENAME)
        }
        if (optout) {
            printf("EXEMPT\t%s\t-\n", FILENAME)
        }
    }
  ' "$f")
  if [ -n "$out" ]; then
    while IFS=$'\t' read -r kind file tc; do
      if [ "$kind" = "EXEMPT" ]; then
        echo "::notice file=$file exempted from the rst_n wiring check"
        echo "        (marker: rst_n_guard: intentionally-unwired - this file exercises the"
        echo "         UNWIRED = permanently-reset behaviour on purpose)"
        continue
      fi
      if [ "$kind" = "VIOLATION_FILE" ]; then
        echo "::error file=$file"
        echo "        This file simulates a component with a mandatory 'rst_n' port but never"
        echo "        drives it anywhere. An unwired ch_in<ch_bool> rst_n reads back as 0 ="
        echo "        PERMANENTLY IN RESET: the design compiles and simulates without any error,"
        echo "        but every register stays at its init value (writes never land, reads"
        echo "        return 0) - the whole file is silently vacuous."
        echo "        Fix: sim.set_input_value(io().rst_n, 1);"
      else
        echo "::error file=$file test_case=$tc"
        echo "        This test case SIMULATES a component with a mandatory 'rst_n' port but never"
        echo "        drives it. An unwired ch_in<ch_bool> rst_n reads back as 0 = PERMANENTLY IN"
        echo "        RESET: the design compiles and simulates without any error, but every register"
        echo "        stays at its init value (writes never land, reads return 0) - the test case"
        echo "        is silently vacuous."
        echo "        Fix: sim.set_input_value(io().rst_n, 1);"
        echo "        NOTE: a mere mention of 'rst_n' in a comment does NOT satisfy this check."
      fi
      violations=$((violations + 1))
    done <<< "$out"
  fi

  # ---- R2: per file, wrapper forwarding ---------------------------------
  if grep -qE "ch_module" "$f" && \
     grep -qE "AxiLiteSlave|Axi4Slave|AxiToAxiLiteConverter|AxiLiteGpio|AxiLiteUart|AxiLiteTimer|AxiLitePwm|AxiLiteI2c|AxiLiteSpi|AxiDma" "$f" && \
     ! grep -qE "rst_n[ \t]*<<" "$f"; then
    echo "::error file=$f"
    echo "        This file wraps a component with a mandatory 'rst_n' port but never forwards"
    echo "        it. A wrapper MUST pass the port through (slave.io().rst_n <<= io().rst_n;)"
    echo "        so that an outer testbench can drive it - otherwise the inner component is"
    echo "        permanently in reset (unwired ch_in<ch_bool> reads back as 0)."
    violations=$((violations + 1))
  fi
done

if [ "$violations" -gt 0 ]; then
  echo ""
  echo "::error $violations site(s) leave a mandatory 'rst_n' unwired"
  exit 1
fi

echo "rst_n wiring OK (every simulated DUT drives rst_n; every wrapper forwards it) ✓"
exit 0
