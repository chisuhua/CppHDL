# Tasks: 登记 `test_verif_wait_level` 为单后端条目

## 1. 规格登记（唯一交付物）

- [x] 1.1 delta spec：`specs/dual-backend-test-coverage/spec.md` →
  `## MODIFIED Requirements` 的「现有 9 个 verilator 测试 MUST 保留原状」
  （**逐字保留**原文 + 追加新增登记段；标题「9 个」不变）
- [x] 1.2 登记内容说明：分类依据（两条执行路径 vs 两个 eval backend）· MUST 保留原状 ·
  MUST 带 `verilator_e2e` 标签 · **不**增加 parity 两个锚点

## 2. 验证

- [x] 2.1 `openspec validate dual-backend-register-verif-wait-level-2026-11 --strict`
- [x] 2.2 `scripts/verify_openspec_delta.sh`（archive 前的实际门）
- [x] 2.3 `scripts/verify_dual_backend_parity.sh` ⇒ **零 VIOLATION**（归档合并主 spec 后）
- [x] 2.4 `scripts/verify_test_counts.sh` ⇒ 两个锚点仍与实测一致（本 change 不增删测试）

## 3. 归档

- [x] 3.1 `openspec archive dual-backend-register-verif-wait-level-2026-11 --yes`
  （**不带** `--skip-specs` ⇒ delta MUST 合并进主 spec）
- [x] 3.2 归档后复核：主 spec 含该登记段 · `verify_dual_backend_parity.sh` 绿
