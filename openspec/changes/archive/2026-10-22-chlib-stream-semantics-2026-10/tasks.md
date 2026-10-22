# Tasks: chlib-stream-semantics-2026-10

> **状态：实施中（2026-10-22）**
>
> ⚠️ **TDD 流程：RED → GREEN → REFACTOR**（`openspec/config.yaml` 的 `rules.tasks`）
> ⚠️ 每个任务 MUST 对应一个**可验证的测试**（`rules.tasks`）
> ⚠️ 完成后必须：`clang-tidy clean`、`clang-format applied`

## 契约先行（AGENTS.md 的 ZERO-DEBT：API 变更 MUST 有文档 + capability）

- [x] ### Task 0: 写 ADR-045（4 项裁决）
  - 裁决 1：输入 ready 所有权 = **两阶段**（决策 0-B）；连接手段 MUST 用 `<<=`
  - 裁决 2：FIFO 时序契约（`sync_fifo` 自保护 / `stream_fifo` FWFT / `q_async` /
    `DEPTH` 2 的幂 / `fwft_fifo` 不是 FWFT）
  - 裁决 3：组合子的构造期纪律（不读未赋值字段 / 不硬编码常量 ready / empty 时
    数据输出是 don't-care）
  - 裁决 4：契约 MUST 与代码**同批**落地
  - 产物：`docs/adr/ADR-045-chlib-stream-semantics.md`
  - 验证：`openspec validate chlib-stream-semantics-2026-10 --strict` 绿

- [x] ### Task 1: 写 capability delta
  - 新建 capability `chlib-stream-semantics`（ADDED ×5 — ⚠️ 归档后收口为 **3 条**，见 delta 顶部「收口订正」）
  - MODIFIED `verilator-e2e-coverage`（归因订正 + FWFT parity Scenario）
  - 产物：`openspec/changes/chlib-stream-semantics-2026-10/specs/**`
  - 验证：同上

- [x] ### Task 2: 写会失败的用例（RED）—— Phase 1
  - `tests/chlib/test_fifo.cpp` 新增 5 组断言：
    ① 满 FIFO 再写 MUST NOT 破坏已写入的数据
    ② 读走唯一元素的那一拍，`q` MUST 即为该元素
    ③ count MUST 只统计真正发生的传输（空读 / 满写）
    ④ 排空后再写 MUST 能被读回
    ⑤ 写 N 读 N 逐个比对（**非判别性**回归护栏）
  - 变异验证（**语义**上，不是「能否构造」上）：
    - ① 撤写口门 ⇒ ① 转红
    - ② 撤读指针门 ⇒ ② 转红
    - ③ 撤写指针门 ⇒ ④ 转红
    - ④ `next_count` 用裸 `wren`/`rden` ⇒ ③ 转红（多条）

- [x] ### Task 3: GREEN —— Phase 1（`sync_fifo` 自保护）
  - `include/chlib/fifo.h`：写口 / 写指针 / 读指针改用 `will_write` / `will_read`
  - 删除 `threshold` 死参数 + 连带清理 `tests/chlib/test_fifo.cpp:117/120/128/134/138`
  - 验证：`tests/chlib/test_fifo.cpp` 全绿（54 assertions / 5 test cases）；
    **既有的 19 条断言零回归**（双实现干跑验证）

- [x] ### Task 4: 写会失败的用例（RED）—— Phase 2
  - `tests/chlib/test_stream.cpp` FIFO 段补 **三元组**断言
    （tick 序号 + `valid` + `payload`）—— ⚠️ 只断言 `payload` 会被 sread 实现在
    `valid=0` 的那一拍骗过
  - `tests/test_stream_fifo_dual.cpp`（新建）：`DUAL_BACKEND_TEST_PAIR` 覆盖
    payload/valid 逐拍一致

- [x] ### Task 5: GREEN —— Phase 2（`stream_fifo` FWFT）
  - `include/chlib/fifo.h`：`sync_fifo` 增加 `aread` 读端口 + `q_async` 字段
    （**`q` 的 1-cycle 语义保持不变** ⇒ 既有断言零改动）
  - `include/chlib/stream.h`：`stream_fifo` 的 `pop_stream.payload` 改用 `q_async`
  - `include/chlib/stream.h`：`stream_fifo` 加 `DEPTH` 2 的幂 `static_assert`
  - `include/chlib/stream.h:277-291`：修**错误安全保证注释**
    （它把 `fifo.h:69` 说成保护读指针，实际只保护 count ⇒ 改为语义引用）

- [x] ### Task 6: 补 sample 级断言
  - `samples/stream_fifo_example.cpp`：自检补 payload 断言
    （当前 10 拍全打印 `0x0` 却 exit 0 ⇒ **sample 绿没有信号**）
  - `samples/spinalhdl_stream_example.cpp`：同上

- [x] ### Task 7: 判别性变异复验 + 全量回归
  - Phase 2 的 2 条变异（payload 改常量 0 / 退回 sread）逐条转红
  - `ctest -LE perf` 全绿 + 计数同步（`AGENTS.md` + `scripts/verify_test_counts.sh`）
  - `bash scripts/run_all_samples.sh` / `bash run_all_ported_tests.sh` 全绿

- [x] ### Task 8: 归档
  - `openspec archive chlib-stream-semantics-2026-10`
  - ⚠️ **禁止** `--skip-specs`
  - ⚠️ 新 capability 的 `## Purpose` **delta 不合并** ⇒ 归档后**手工补写**
  - 验证：主 spec 生效 + 无 dangling 引用 + `openspec validate --specs --strict`
