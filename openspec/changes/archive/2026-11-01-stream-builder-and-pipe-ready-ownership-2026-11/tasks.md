# Tasks：`chlib` 链式组合的 ready 归属（B1 + B6）

> ⚠️ **提交粒度 = 单 commit**（初稿写「两个 commit」，实施后订正 ⇒ 理由）：
> C1（builder）与 C2（pipe）**共用**同一份护栏基础设施 ——
> `scripts/lib/stream_ready_ownership.py` 的 `check` 入口 / `POISON_TARGETS` 表 /
> `scripts/verify_stream_ready_ownership.sh` 的 `poison_bin`·`poison_target`
> 分派。两批改动**必然**同时落在这两个文件上 ⇒ ⇒ 强行拆成两个 commit 需要
> `git add -p` 切割同一函数内的相邻行 ⇒ ⇒ 「commit 粒度」带来的可读性收益
> 抵不上「切割错误导致中间 commit 不可编译」的风险（本仓 `rv32i-fetch-skid-buffer-2026-10`
> 能拆两个 commit 是因为**两批不共享任何文件**）。
> ⇒ ⇒ ⇒ 且 capability delta 本就是**一份**（ADDED ×1，ADR-055 §2.5）⇒ ⇒
> 一个 change 一个 commit 与「delta 不可分割」一致。

## C1 —— `StreamBuilder` 链内闭包不变式（B1）

- [x] `include/chlib/stream_builder.h`：加 `pending_ready_` / `pending_is_source_`
      / `adopt_stage_output()` / `drive_pending_ready()`
- [x] `queue<>()`：`drive_pending_ready(result.push_stream.ready)`
      ⇒ ⇒ 本级的容量信号驱动**上一级**的输出 ready
- [x] 过滤类（`haltWhen` / `continueWhen` / `throwWhen` / `takeWhen` / `map`）：
      只 `adopt_stage_output(...)`（**不动**入参 ready —— 过滤类**自己**驱动它，
      且那条驱动里含 `| condition` ⇒ 抹掉会**丢数据**）
- [x] 三处**响亮拒绝**（`CHFATAL_EXCEPTION`）：无节点 / literal / 链首多驱动
- [x] `tests/test_stream_builder.cpp`：既有反压用例**接线订正**（`as_master()`
      + `<<=`）+ 断言 `source.ready.num_srcs() > 0`
- [x] 同文件：2 个反例用例（缺 `as_master()` / ready 被喂 literal ⇒ 抛）
- [x] `tests/test_stream_builder_backpressure_dual.cpp`（**新**）：2 个
      `DUAL_BACKEND_TEST_PAIR`（两级链 8 词 / 一级链 4 词）
      ⇒ **判别性断言 = 空链 `in_ready == 1`**
- [x] `tests/CMakeLists.txt`：`add_dual_backend_test(test_stream_builder_backpressure_dual)`
- [x] 护栏 R10（`scripts/lib/stream_ready_ownership.py`）：
      存在性（`queue()` 调 `drive_pending_ready(`）· `<<=` · 3 处 `CHFATAL_EXCEPTION`
- [x] 2 种投毒（`builder-drop-chain-closure` / `builder-upstream-operator-assign`）
      ⇒ 落 `stream_builder.h` ⇒ 判别力二进制 = `test_stream_builder`
- [x] `scripts/verify_stream_ready_ownership.sh`：自检按投毒目标选二进制
- [x] N2′ 判据自检：`54 == 2 × 27`（有/无 verilator 的断言数）
- [x] 计数锚点：`CTEST_TOTAL` 208 · `VERILATOR_E2E_TOTAL` 41 ·
      `DUAL_BACKEND_PARITY_TOTAL` 32 · `DUAL_BACKEND_PARITY_PAIRS` 192 ·
      non-perf 197

## C2 —— `stream_pipeline.h` 的 pipe 族（B6）

- [x] `stream_m2s_pipe`：`output.ready` 建成**可覆盖字段**（默认恒就绪）；
      `input.ready = !valid_reg | output.ready`（组合）；`valid_reg` **保持**
      未被消费的数据（修复前 `valid_reg->next = input_fire` ⇒ 下游一停顿就丢）
- [x] `stream_s2m_pipe`：`output.ready` 同上；`input.ready = **寄存**的
      `output.ready`（⚠️ 修复前寄存的是 `input_stream.ready` = **方向反了**）
- [x] `stream_half_pipe`：`input.ready = toggle & (!valid_reg | output.ready)`；
      toggle **自由运行**（MUST NOT 由传输驱动 ⇒ 否则无数据时死锁）；
      `valid_reg` **保持**未被消费的数据
- [x] 3 个 pipe 均：改为 `detail::drive_upstream_ready(...)`（**删掉**
      `input_stream.ready = …` 的 `operator=` 形式）
- [x] `detail::drive_upstream_ready`：三处响亮拒绝，且多驱动判据**允许**覆盖
      「默认常量驱动」（否则链式 pipe 被误判 ⇒ `test_stream_operators` 4 个
      链式用例全崩 ⇒ 实测 11 用例 / 45 断言全绿）
- [x] `tests/test_stream_pipeline.cpp`：全部接线订正（`as_master()`）+
      新增 2 个行为用例（m2s 数据通过 + 反压到达 / half 隔拍且不丢数据）
      ⇒ **68 断言 / 12 用例**
- [x] 护栏：R8 的扫描面加 `stream_pipeline.h`（已收口 13 → **16**）
- [x] **新增 R11**：`drive_upstream_ready()` 的 `<<=` / `type_lit` /
      `num_srcs()` / ≥3 处 `CHFATAL_EXCEPTION` + 3 个 pipe **都必须调它**
      （⚠️ R8 看不见 helper 内部 ⇒ ⇒ R8 与 R11 **不可互相替代**）
- [x] 投毒：`pipe-upstream-operator-assign` ⇒ 行为 + R11 双红
- [x] 计数锚点：不变（用例进既有 target，无新测试文件）

## 验收

- [x] `ctest -LE perf` 全绿（零回归）
- [x] `scripts/verify_stream_ready_ownership.sh --self-test` 全绿
      （**13/13** 投毒逐条转红 + 还原后复绿）
- [x] `openspec validate stream-builder-and-pipe-ready-ownership-2026-11 --type change --strict` valid
- [x] `scripts/verify_openspec_delta.sh` 全绿（4 条）
- [x] `scripts/verify_test_counts.sh` / `scripts/verify_dual_backend_parity.sh` 通过
- [x] 归档：**未**用 `--skip-specs`（ADDED ×1 已合并进主 spec ⇒ 6 → **7** Requirement / 20 → **28** Scenario）
      ⇒ 归档于 `openspec/changes/archive/2026-11-01-stream-builder-and-pipe-ready-ownership-2026-11/`
