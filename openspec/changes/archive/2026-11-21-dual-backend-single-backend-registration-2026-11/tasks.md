# tasks — `dual-backend-single-backend-registration-2026-11`

| # | 任务 | 落点 | 判据 | 状态 |
|---|---|---|---|---|
| A1 | delta：复制该 Requirement 的**完整正文** + 追加 dated 登记段（`tests/test_verif_history_primitives.cpp`） | `specs/dual-backend-test-coverage/spec.md` | AC5 | ☑ |
| A2 | `openspec validate --strict` exit 0 | — | AC4 | ☑ |
| A3 | `openspec archive <name> --yes`（**不**带 `--skip-specs`）⇒ 主 spec 合并 | `openspec/specs/dual-backend-test-coverage/spec.md` | AC5 | ☐ |
| A4 | 护栏：`verify_dual_backend_parity.sh` exit 0 + `--self-test` exit 0 | — | AC1 · AC2 | ☐ |
| A5 | 锚点复核：`verify_test_counts.sh` PASS（三锚点逐字不变） | 根 `AGENTS.md` §NOTES | AC3 | ☐ |
| A6 | openspec 护栏：`verify_openspec_delta.sh` + `verify_openspec_purpose.sh` PASS | — | AC4 | ☐ |
