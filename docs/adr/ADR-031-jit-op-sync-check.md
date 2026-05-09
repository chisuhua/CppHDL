# ADR-031: JIT `ch_op` ↔ `JitOp` 编译时同步检查

**状态**: ✅ 已采纳（已执行）
**日期**: 2026-05-08
**决策人**: Sisyphus + 用户（Oracle 分析参考）

---

## 1. 背景

议题 #25 解决 `ch_op`（`lnodeimpl.h:41-75`）、`JitOp`（`jit_ir.h:10-24`）、`generate_ir()`（`jit_compiler.cpp:291-312`）三者间的同步问题。

### 1.1 问题

新增 `ch_op` 值时，开发者可能忘记在 `generate_ir()` 中添加映射。旧的 `default: jit_op = CALL_EXTERNAL` 使编译器无法检测遗漏。

### 1.2 Oracle 方案分析

| # | 方案 | 保证级别 | 工作量 |
|---|------|---------|--------|
| 1 🥇 | **`-Wswitch-enum` pragma + 显式 external 列** | ✅ 编译时 | **<1h** |
| 2 | X-macro 单一真实源 | ✅ 编译时 | 1-2d |
| 3 | constexpr 映射 + static_assert | ⚠️ 仅计数 | 中 |
| 4 | 运行时测试 | ❌ 运行时 | 短 |

---

## 2. 决议

**采用方案 1**：在 `generate_ir()` switch 周围添加 `-Wswitch-enum` pragma，删除 `default:`，将所有 14 个外部操作显式列出。

### 2.1 变更

| 文件 | 变更 |
|------|------|
| `src/jit/jit_compiler.cpp:291` | 添加 `#pragma GCC diagnostic push` / `error "-Wswitch-enum"` |
| `src/jit/jit_compiler.cpp:311` | 删除 `default: jit_op = JitOp::CALL_EXTERNAL; break;` |
| `src/jit/jit_compiler.cpp:311-327` | 显式列出 14 个外部操作: `div, sshr, neg, bits_update, sext, zext, mux, and_reduce, or_reduce, xor_reduce, rotate_l, rotate_r, popcount, assign` |

### 2.2 效果

- 新增 `ch_op` 后，必须添加到 switch 的 native 或 external 块，否则**编译失败**
- 所有 14 个当前 `CALL_EXTERNAL` 操作现在是**明确的决策**而非默认回退
- pragma 作用域仅限于 `generate_ir()` switch，不影响其他文件

---

## 3. 变更日志

| 日期 | 版本 | 变更 | 作者 |
|------|------|------|------|
| 2026-05-08 | v1.0 | 初始版本：在 generate_ir() 中添加 -Wswitch-enum 编译时完整性检查 | Sisyphus |

---

**相关链接**:
- `src/jit/jit_compiler.cpp:289-332` — generate_ir() switch（变更位置）
- `include/core/lnodeimpl.h:41-75` — ch_op 枚举
- `include/jit/jit_ir.h:10-24` — JitOp 枚举
- `include/jit/AGENTS.md` — 规则 1（同步要求文档化）
- `docs/adr/ADR-021-jit-op-coverage-strategy.md` — FAIL FAST 决议
- `docs/adr/ADR-DISCUSSION-PLAN.md` — 议题 #25
