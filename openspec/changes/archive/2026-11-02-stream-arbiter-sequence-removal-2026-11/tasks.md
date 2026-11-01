# Tasks：删除 `stream_arbiter_sequence`（B5）

## C1 —— 删除组件 + 消费者迁移 + 回归禁令

- [x] `include/chlib/stream_arbiter.h`：删除 `StreamArbiterSequenceResult` +
      `stream_arbiter_sequence`（≈ 92 行）+ 文件头注释去掉 "and sequential"
- [x] `tests/chlib/test_stream.cpp`：反压用例 variant 1
      `stream_arbiter_sequence` → `stream_arbiter_priority`（覆盖面不减）
- [x] `tests/test_stream_arbiter.cpp`：3 个 sequence 用例
      ⇒ **移植 2 个**到 `priority`（basic 补 `payload` 行为断言 + no-valid）
      ⇒ **删 1 个**（"wraps around" —— 断言的是**不存在**的行为，零判别力）
- [x] `scripts/lib/stream_domain_closure.py`：新增 **R12**（两面判据）
- [x] 判别力验证（手动探针）：把 `stream_arbiter_sequence` 写回
      `stream_arbiter.h` ⇒ R12 转红；还原 ⇒ 复绿

## C2 —— spec 订正

- [x] `chlib-stream-arbitration` 的 **MODIFIED ×1**：
      ⇒ Scenario 1（`固定优先级仲裁器 MUST NOT 声称轮转`）改为检查
      `stream_arbiter_priority` + 声明 `sequence` MUST 不存在（**标题保留**）
      ⇒ Scenario 2（`与既有组件近乎重复时 MUST 登记而非删除`）补
      「裁决为删除时 MUST 走**独立 change**（含消费者迁移 + 回归禁令护栏）」
      ⚠️ 标题保留的原因同 T1：CLI 按标题匹配 Scenario

## 验收

- [x] `include/chlib/stream_arbiter.h`（剥注释）无 `stream_arbiter_sequence`
- [x] `stream_arbiter_priority` 仍在
- [x] `verify_stream_domain_closure.sh` PASS（R12 绿）+ `--self-test` 6/6
- [x] `ctest -LE perf` **197/197**（零回归）
- [x] `openspec validate stream-arbiter-sequence-removal-2026-11 --type change --strict` valid
- [x] `scripts/verify_openspec_delta.sh` 4 条全绿
- [x] 归档：**未**用 `--skip-specs`
      ⇒ 归档于 `openspec/changes/archive/2026-11-02-stream-arbiter-sequence-removal-2026-11/`
- [x] 计数锚点**不变**（CTEST_TOTAL 208 / verilator_e2e 41 / parity 32·192 / non-perf 197）
