# ADR-028: 已禁用测试处理

**状态**: ✅ 已采纳
**日期**: 2026-05-08
**决策人**: Sisyphus + 用户

---

## 1. 背景

议题 #22 审查 `tests/CMakeLists.txt` 中 7 个被注释掉的测试。

## 2. 禁用测试清单

| 测试 | 原因 | TODO 标签 |
|------|------|-----------|
| `test_branch_predictor` | `ch_reg[]` 数组下标不兼容，需 `switch_()` 多路复用器重构 | L26 |
| `test_cache_pipeline` | `rv32i_pipeline_cache.h` 构造函数/IO/`bits` API 不兼容 | L69 |
| `test_phase4_chlib` | riscv-mini API 兼容性问题 | L80 |
| `test_ch_flow` | `ch_flow <<= ch_out` 运算符不兼容 | L167 |
| `test_rv32i_pipeline` | 编译错误 | L181 |
| `test_mod_width_simple` | 全文注释 | L195 |
| `test_stream_width_adapter` | 全文注释 | L196 |

已有 3 个内存测试（`test_memory_leak/optimization/performance`）被删除（L134-136），表明清理模式已存在。

## 3. 决议

**保留 TODO 注释**。理由：
- TODO 注释记录了已知问题和修复方向，包含足够上下文
- 注释不影响构建流程
- 未来修复时，注释提供了直接的入口点

---

## 4. 变更日志

| 日期 | 版本 | 变更 | 作者 |
|------|------|------|------|
| 2026-05-08 | v1.0 | 初始版本：记录已禁用测试清单，决定保留 TODO 注释 | Sisyphus |

---

**相关链接**:
- `tests/CMakeLists.txt:26-27` — test_branch_predictor
- `tests/CMakeLists.txt:69-71` — test_cache_pipeline
- `tests/CMakeLists.txt:80-81` — test_phase4_chlib
- `tests/CMakeLists.txt:167` — test_ch_flow
- `tests/CMakeLists.txt:181-182` — test_rv32i_pipeline
- `tests/CMakeLists.txt:194-196` — 全文注释的测试
- `docs/adr/ADR-DISCUSSION-PLAN.md` — 议题 #22
