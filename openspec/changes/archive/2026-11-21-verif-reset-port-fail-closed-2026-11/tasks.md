# tasks — `verif-reset-port-fail-closed-2026-11`

> ⚠️ **无 spec 变更** ⇒ `.openspec.yaml` 已声明 `skip_specs: true` ⇒ 归档用
> `openspec archive verif-reset-port-fail-closed-2026-11 --yes --skip-specs`。

| # | 任务 | 落点 | 判据 | 状态 |
|---|---|---|---|---|
| A1 | 内部共享判定（端口 ident → role，**镜像 Resolver 三条路径**） | `src/rtl_emit/verif_internal.h` | AC4 | ☑ |
| A2 | 解释器侧调用（`Resolver::build` 之后、任何 sim 变更之前） | `src/rtl_emit/verif_run.cpp` | AC1 | ☑ |
| A3 | 渲染器侧调用（**排在端口解析之前**） | `src/rtl_emit/verif_render.cpp` | AC1 | ☑ |
| A4 | 判据载体：两端同结论用例（消息**逐字相等**） | `tests/test_verif_wait_level.cpp` | AC1 · AC2 | ☑ |
| A5 | 变异自证：移除解释器侧 ⇒ 红 · 移除渲染器侧 ⇒ 红 · 还原 ⇒ 绿 | — | AC2 | ☑ |
| A6 | 零破坏复核：既有 `.verif` 零处使用 `RESET` ⇒ `verify_examples_rtl_sim.sh` PASS | — | AC3 | ☑ |
| A7 | 文档：usage guide（`RESET` 边界 + B0 ②）· CHANGELOG · 期望源登记 · 清单 note | 见 proposal | AC5 | ☑ |
| A8 | **归档**（`--skip-specs`） | — | — | ☐ |
