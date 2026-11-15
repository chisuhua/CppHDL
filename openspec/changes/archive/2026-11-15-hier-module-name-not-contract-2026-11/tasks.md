# Tasks

- [x] 1.1 `rtl-hierarchical-output` 的该 Requirement：`REMOVED` + `ADDED`（标题不再含「MUST 是产物级契约」）
- [x] 1.2 正文改「不是契约」+ 明写裁决来源（用户 · 2026-11-15 · ADR-070 §6 ⑦）
- [x] 1.3 `docs/CHANGELOG.md` 同步同一措辞
- [x] 1.4 归档 + 主 spec grep

## Verify

```bash
openspec validate 2026-11-15-hier-module-name-not-contract-2026-11
bash scripts/verify_openspec_delta.sh 2026-11-15-hier-module-name-not-contract-2026-11
grep -q "不是契约" openspec/specs/rtl-hierarchical-output/spec.md
grep -c "MUST 是产物级契约" openspec/specs/rtl-hierarchical-output/spec.md
```
