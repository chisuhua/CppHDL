# Tasks: chlib-conditional-acceptance-title-fix-2026-10

- [ ] 0.1 核实主 spec 的该 Requirement **正文**已写「响亮拒绝（编译期 `static_assert`
      或建图期 `CHFATAL_EXCEPTION`）」（⇒ ⇒ 只有**标题**不准）
- [ ] 0.2 记录改动前的 `openspec validate --specs --strict` **✗ 清单**（基线）
- [ ] 1.1 delta 用 **`RENAMED`**（`FROM:` / `TO:`）⇒ 只改标题 ⇒ 正文与全部
      Scenario **天然不动**（⚠️ `MODIFIED` 改标题会被 CLI 静默不合并 ⇒ R3 已抓到）
- [ ] 1.2 核实改动后的 **✗ 清单**与基线**逐条一致**（⇒ 零回归）
- [ ] 1.3 `ctest -LE perf` 零回归（**零**代码改动 ⇒ 预期零影响）
- [ ] 1.4 归档（**禁** `--skip-specs`）
