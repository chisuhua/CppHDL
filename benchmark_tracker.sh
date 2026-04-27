#!/bin/bash
# benchmark_tracker.sh - Track performance metrics over time
# Usage: ./benchmark_tracker.sh [--compare]

set -e

RESULTS_FILE="perf_results.csv"
TIMESTAMP=$(date +%Y-%m-%d_%H:%M:%S)
TEMP_FILE=$(mktemp)

cd /workspace/project/CppHDL/build

# Run benchmarks and append to CSV
echo "timestamp,test,params,ticks_per_sec,ns_per_tick,ns_per_node_tick,overhead_pct" > "$TEMP_FILE"

# TC-01: Combinational chain
for depth in 10 100 1000 5000 10000; do
    output=$(timeout 30 ./tests/benchmark/perf_tests --tc=01 2>/dev/null | grep "depth=$depth" || echo "")
    if [ -n "$output" ]; then
        ticks=$(echo "$output" | awk '{print $3}' | tr -d ',')
        ns_tick=$(echo "$output" | awk '{print $5}')
        ns_node=$(echo "$output" | awk '{print $6}')
        echo "$TIMESTAMP,TC-01,depth=$depth,$ticks,$ns_tick,$ns_node,0" >> "$TEMP_FILE"
    fi
done

# TC-02: Sequential registers
for regs in 10 100 1000 5000 10000; do
    output=$(timeout 30 ./tests/benchmark/perf_tests --tc=02 2>/dev/null | grep "regs=$regs" || echo "")
    if [ -n "$output" ]; then
        ticks=$(echo "$output" | awk '{print $3}' | tr -d ',')
        ns_tick=$(echo "$output" | awk '{print $5}')
        ns_node=$(echo "$output" | awk '{print $6}')
        echo "$TIMESTAMP,TC-02,regs=$regs,$ticks,$ns_tick,$ns_node,0" >> "$TEMP_FILE"
    fi
done

# TC-04: Trace overhead
output=$(timeout 30 ./tests/benchmark/perf_tests --tc=04 2>/dev/null | grep "signals=100" || echo "")
if [ -n "$output" ]; then
    ticks=$(echo "$output" | awk '{print $3}' | tr -d ',')
    ns_tick=$(echo "$output" | awk '{print $5}')
    overhead=$(echo "$output" | awk '{print $8}')
    echo "$TIMESTAMP,TC-04,signals=100,$ticks,$ns_tick,0,$overhead" >> "$TEMP_FILE"
fi

# Append to main results file
if [ -f "$RESULTS_FILE" ]; then
    tail -n +2 "$TEMP_FILE" >> "$RESULTS_FILE"
else
    mv "$TEMP_FILE" "$RESULTS_FILE"
fi

echo "Results appended to $RESULTS_FILE"
cat "$RESULTS_FILE"