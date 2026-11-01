# Tasks：命名诚实性的可判定条款（B8）

## C1 —— 登记表 + 护栏

- [x] `.rddf/checks/chlib-naming-honesty-registry.md`（**新**）：5 条登记
      （`stream_demux` · `stream_fork` · `stream_arbiter_round_robin` ·
      `sync_fifo` · `stream_fifo`）⇒ 每条含「**声称**的语义」+ 判别性用例
      （tag/macro）+ 登记原因
- [x] `scripts/lib/chlib_naming_honesty.py`（**新**）：
      **R1** 登记 ⊆ 组件全集（穷举 `include/chlib/*.h`）⇒ 登记漂移 ⇒ 红
      **R2** 判别性用例存在 + 仍带其 tag ⇒ 判别力消失 ⇒ 红
      **R3** per-change 义务（最近的 chlib change MUST 用
      `<!-- touched-components: ... -->` 声明它触及的组件 ⇒ 那些组件 MUST 已登记）
- [x] `scripts/verify_chlib_naming_honesty.sh`（**新**）+ `--self-test`
      （3 种投毒：删登记行 · 组件名改成不存在的 · 判别性 tag 改成不存在的）
- [x] `.github/workflows/ci.yml`：**2 处**挂载（主路径 + `--self-test`）

## C2 —— spec 条款

- [x] `chlib-stream-arbitration` 的 **MODIFIED ×1**：
      Requirement 正文补「命名诚实性登记表」段 + **新增** Scenario
      「被改动的组件 MUST 登记并绑定判别性行为用例」
      ⇒ ⚠️ 既有 2 个 Scenario **逐字保留**（`MODIFIED` 是整条替换 ⇒ 漏抄即丢）
- [x] 既有归档 change 的 proposal 补 `<!-- touched-components: ... -->` 标记
      （`stream-demux-fork-ready-semantics-2026-11`）

## 验收

- [x] `scripts/verify_chlib_naming_honesty.sh` PASS + `--self-test` **3/3**
- [x] `openspec validate chlib-naming-honesty-clause-2026-11 --type change --strict` valid
- [x] `scripts/verify_openspec_delta.sh` 4 条全绿
- [x] CI YAML 合法 + 脚本 `bash -n` 通过
- [x] 收口清单第 14 行 → ✅ ⇒ 锚点 `16/1 → 16/0`
- [ ] `ctest -LE perf` 零回归（本 change **零代码改动** ⇒ 只跑护栏即可，
      但仍跑一次以确保工作区整体健康）
- [ ] 归档：**禁用** `--skip-specs`
