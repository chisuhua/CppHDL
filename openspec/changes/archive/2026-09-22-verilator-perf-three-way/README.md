# verilator-perf-three-way

修复 `get_literal_str` 内部 mask 缺失导致 TC-07 depth=1000 Verilator "Too many digits" 编译失败 + 添加 ctest 入口 `perf_three_way` (用 `--tc=07 --tc=08` 子集)
