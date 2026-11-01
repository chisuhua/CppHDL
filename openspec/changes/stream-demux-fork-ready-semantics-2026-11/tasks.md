# Tasks：`stream_demux` / `stream_fork` 的 ready 语义（B2 + B3）

> 一个 change、**一个 commit**：两者共用 `detail::drive_master_ready` 与同一份
> 护栏基础设施（R8 豁免清理 + R13），且 capability delta 是**一份**（ADDED ×1）。

## C1 —— 实现（ADR-056 / ADR-057）

- [x] `include/chlib/stream.h`：新增 `detail::drive_master_ready`（三处
      `CHFATAL_EXCEPTION`：无节点 / literal / 已被真信号驱动）
- [x] `stream_demux` 重写：`input.ready = in_range & OR_i((select==i) & output_i.ready)`
      · `valid_i = input.valid & (select==i)` · 输出 `ready` 形态 E ·
      越界 ⇒ `in_range=0` ⇒ ready 落 0（ADR-056 D1~D5）
- [x] `stream_fork` 重写：`synchronous=false` ⇒ `CHFATAL_EXCEPTION` ·
      `input.ready = AND_i output_i.ready` · `valid`/`payload` 只依赖输入
      （ADR-057 D1~D5）
- [x] 头注释写清「`synchronous=false` 目前拒绝 + 归位条件」（命名诚实性）

## C2 —— 调用点迁移

- [x] `tests/chlib/test_stream.cpp`：demux/fork 接线订正（`as_master()` + 删自驱动
      `ready`）· fork 异步 SECTION 改 `REQUIRE_THROWS_AS`（⚠️ 模板实参要加括号）
- [x] `tests/chlib/test_stream_mux_demux.cpp`：2 处 `output_streams[i].ready = ...`
      删除（形态 E 默认已恒就绪）+ `as_master()`
- [x] `examples/stream/stream_demux_demo.cpp` / `stream_fork_demo.cpp`：补
      `as_master()` + 删节点重绑定式赋值

## C3 —— 判据与护栏

- [x] 新 `tests/test_stream_demux_fork_dual.cpp`（2 个 `DUAL_BACKEND_TEST_PAIR`）：
      demux（默认 1 → 被选中拉低 0 → 非选中隔离 → 切换 → 越界）·
      fork（AND 两面 + 数据面独立）
- [x] `tests/CMakeLists.txt`：`add_dual_backend_test(test_stream_demux_fork_dual)`
- [x] N2′ 自检：有 verilator **44** 断言 / 无 **22** ⇒ `44 == 2 × 22` ✔
- [x] `scripts/lib/stream_ready_ownership.py`：**清空 R8 豁免清单**
      （`stream_fork` / `stream_demux` 收口）· R8 认 `drive_master_ready(` ·
      **新增 R13** · 2 种新投毒
- [x] `scripts/verify_stream_ready_ownership.sh`：`demux-fork-*` ⇒ 判别力二进制
      = `test_stream_demux_fork_dual`；还原时也重建它

## C4 —— 计数与文档

- [x] 计数锚点：`CTEST_TOTAL` 209 · `VERILATOR_E2E_TOTAL` 42 ·
      `DUAL_BACKEND_PARITY_TOTAL` 33 · `PAIRS` 194 · non-perf 198 ·
      `tests/test_*.cpp` 121
- [x] 收口清单：第 11 行（demux）· 第 12 行（fork）→ ✅ · 锚点 16/3 → **16/1**
      ⇒ ⚠️ 第 14 行（命名诚实性通用条款 / B8）**仍为 ❌**
- [x] 根 `AGENTS.md` NOTES 条目

## 验收

- [ ] `ctest -LE perf` 零回归
- [ ] `verify_stream_ready_ownership.sh --self-test` **15/15** 投毒全红 + 还原复绿
- [ ] `openspec validate … --strict` valid · `verify_openspec_delta.sh` 4 条全绿
- [ ] `verify_test_counts.sh` / `verify_dual_backend_parity.sh` 通过
- [ ] 归档：**禁用** `--skip-specs`
