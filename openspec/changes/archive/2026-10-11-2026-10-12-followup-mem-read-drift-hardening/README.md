# 2026-10-12-followup-mem-read-drift-hardening

钉死 Issue #27 的 4 项跟踪任务为代码级回归防护：

- **P0** Golden Verilog 回归测试（fence against aread→`always_ff` regression）
- **P1** Same-cycle write+read race 测试（fence Simulator pre-write semantic）
- **P2** `examples/riscv-mini/src/branch_predictor_v2.h` 5 处 `.aread()` 内联注释（doc parity）
- **P3** Issue #27 body 编辑（移除冗余 changelog 项）

Issue: https://github.com/chisuhua/CppHDL/issues/27
Target spec: `openspec/specs/mem-read-timing/spec.md` (追加 R5 回归覆盖契约)
