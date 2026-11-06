# 任务清单：`dual-backend-lock-register-hier-equivalence-2026-11`

> 判据来源：proposal.md §Acceptance · capability delta（`dual-backend-test-coverage` MODIFIED ×1）。

## T1 登记

- [x] 1.1 `dual-backend-test-coverage` 的契约锁定清单**追加**登记
      `tests/test_hierarchy_equivalence.cpp`（单后端 · 层次化 ≡ 扁平 逐拍等价套件）
      ⇒ ⇒ MUST 保留原状（MUST NOT 迁移到 `DUAL_BACKEND_TEST` macro）
      ⇒ ⇒ **标题不动**（「9 个」= reference implementation 数量 ⇒ 仍为 9）
- [x] 1.2 归档（**禁** `--skip-specs`）⇒ 主 spec 的契约锁定清单落地

## T2 零回归

- [x] 2.1 `bash scripts/verify_dual_backend_parity.sh` 退出码 0
- [x] 2.2 `bash scripts/verify_test_counts.sh` 全绿（`VERILATOR_E2E_TOTAL` 锚点）
- [x] 2.3 `openspec validate --specs --strict` 通过

## 验证命令

```bash
cd /workspace/project/CppHDL
bash scripts/verify_dual_backend_parity.sh
bash scripts/verify_test_counts.sh
openspec validate --specs --strict
```
