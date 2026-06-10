#!/usr/bin/env bash
#
# init-submodules.sh — convenience wrapper for initializing the Verilator
# git submodule. Run once after a fresh clone, or whenever the submodule
# pointer is bumped in .gitmodules.
#
# Per perf-report-followup spec v4 (commit 4f9eb16).

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${REPO_ROOT}"

if [[ ! -d third_party/verilator/.git ]]; then
    echo "[init-submodules] third_party/verilator is empty or not a submodule."
    echo "[init-submodules] Running: git submodule update --init --recursive"
    git submodule update --init --recursive
else
    echo "[init-submodules] third_party/verilator already initialized."
fi

# Sanity check
if [[ ! -x third_party/verilator/bin/verilator ]]; then
    echo "[init-submodules] ERROR: third_party/verilator/bin/verilator missing." >&2
    echo "[init-submodules] The submodule is broken. Try:" >&2
    echo "[init-submodules]   git submodule deinit -f third_party/verilator" >&2
    echo "[init-submodules]   git submodule update --init --recursive" >&2
    exit 1
fi

echo "[init-submodules] OK: Verilator submodule is ready."
head -1 third_party/verilator/bin/verilator  # Should print: #!/usr/bin/env perl
