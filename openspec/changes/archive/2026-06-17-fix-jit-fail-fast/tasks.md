# fix-jit-fail-fast

> **STATUS (2026-06-18)**: 🔁 **SUPERSEDED by c-class-refactor**. 此 change 的核心假设（让 `concat` 走 `CALL_EXTERNAL` → `UNSUPPORTED_OP` fail-fast）被 C-class 重构（特别是 JIT 拆分 Phase 1 + AST 拆分 Phase 7）反向演进：当前 `concat` 已完整实现为 JIT 原生操作（`src/jit/jit_llvm_control.cpp:81-95, 153-154`），语义在 `include/jit/AGENTS.md:121` 明确文档化，配套测试 `test_jit_concat` / `test_jit_golden` 已注册并通过。
>
> **结论**：Section 1（JIT fail-fast）和 Section 2（per-case AND/OR/XOR mask）**目标已反转 / 实现方式已被取代**，不应继续按原计划执行。Section 3（验证测试）实际通过，可标记完成。
>
> **关闭原因归档**：此 change 关闭不归档为完成实现，而是归档为 "superseded by c-class-refactor"，避免后续误读。

## 1. JIT FAIL FAST 实施 (SUPERSEDED — 目标反向)

> **2026-06-18 决定**：C-class refactor Phase 1+7 已实现 `concat` 的完整 JIT 降低（SHL+OR+AND mask），回退到 `UNSUPPORTED_OP` 反而会**回退 c-class refactor 改进**。Section 1 全部取消。

- [x] 1.1 ~~从 `jit_compiler.cpp:generate_ir()` line 移除 concat 映射~~ **CANCELLED**
      🚫 反向：当前 `concat → JitOp::CONCAT` 映射**保留并扩展**于 `src/jit/jit_ir_gen_lnode_logic.cpp:47, 92` + `jit_compiler.cpp:420`（带 `src_bitwidth` 不变式）
- [x] 1.2 ~~确认 concat 在 `compile_to_llvm()` 中返回 `UNSUPPORTED_OP`~~ **CANCELLED**
      🚫 反向：当前 `concat` **完整实现**于 `src/jit/jit_llvm_control.cpp:81-95, 153-154`（语义 `result = (src0 << src1_width) | src1`），并加 `AND mask` 防止高位污染
- [x] 1.3 ~~更新 AGENTS.md 规则 1：明确 CALL_EXTERNAL 不可接受~~ **CANCELLED**
      🚫 反向：`include/jit/AGENTS.md:121` 明确文档化 `concat` 为 **JIT 原生** 操作（非 CALL_EXTERNAL）

## 2. Mask 补全 (SUPERSEDED — 实现方式不同)

> **2026-06-18 决定**：当前 `AND/OR/XOR` 的 mask 走**通用路径**（在 `src/jit/jit_llvm_helpers.h` / 调用方按 `instr.bitwidth < 64` 加 mask），而非 per-case 显式 mask。两种实现等价，per-case 形式已不再需要。Section 2 全部取消。

- [x] 2.1 ~~在 `compile_to_llvm()` 的 AND case 添加 mask~~ **CANCELLED** (实现走通用路径)
- [x] 2.2 ~~在 OR case 添加 mask~~ **CANCELLED** (实现走通用路径)
- [x] 2.3 ~~在 XOR case 添加 mask~~ **CANCELLED** (实现走通用路径)

## 3. 验证测试 (✅ PASS — 2026-06-18 验证)

- [x] 3.1 编译验证：`cmake --build build -j$(nproc)`
      ✅ Build 100% pass（2026-06-18 验证，无 error/warning）
- [x] 3.2 运行 JIT 相关测试：`ctest -R "test_jit|test_interpreter_jit_compat" --output-on-failure`
      ✅ **4/4 pass**：`test_jit_compiler`, `test_jit_concat`, `test_jit_golden`, `test_interpreter_jit_compat`
- [x] 3.3 验证现有测试仍通过（无回归）
      ✅ 全量 JIT 套件通过；C-class refactor commit `69ff468` 已验证 ctest pass（参考 root AGENTS.md 状态）
