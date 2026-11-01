# 提案：`stream_demux` / `stream_fork` 的 ready 语义落地（B2 + B3）

- **架构依据**：**ADR-056**（demux · `docs/adr/ADR-056-stream-demux-ready-direction.md`）
  + **ADR-057**（fork · `docs/adr/ADR-057-stream-fork-sync-semantics.md`）
- **规格影响**：`chlib-stream-ready-ownership` 的 **ADDED ×1**（9 个 Scenario）
  ⇒ 归档**禁** `--skip-specs`
- **规模 / 风险**：中（1 个头文件 + 4 处调用点 + 1 个新 dual 测试 + 1 条新护栏判据）
- **关联台账**：`.rddf/improvements/stream-forward-ready-closure-2026-10.md#demux` / `#fork`

<!-- touched-components: stream_demux, stream_fork -->

## Why（两个组件都「不在任何 ready 契约的覆盖范围内」）

### 1. `stream_demux` 的 ready 方向**颠倒**

```cpp
result.output_streams[i].ready = is_selected & input_stream.ready;  // 输出的 ready 由【输入】驱动
result.input_stream.ready = selected_ready;                          // operator= ⇒ 只重绑定本地句柄
```

⇒ ① `ready` 的语义方向是**从端 → 主端**，其源 MUST 是消费端；
⇒ ② `operator=` ⇒ **调用方持有的是孤儿节点** ⇒ 上游**永远收不到反压**；
⇒ ③ ② 的值由 ① 推出 ⇒ **自引用**（改 `<<=` 即成环）；
⇒ ④ `select` 越界**未定义**。

### 2. `stream_fork` 的 `synchronous` 参数**不改变任何真实语义**

| 分支 | 现状 |
|---|---|
| `synchronous = true` | `input.ready = input_stream.ready`（自引用）· 各输出 `ready` 由**输入**赋值 ⇒ **AND 从未被计算** |
| `synchronous = false` | `valid_i = input.valid & input.ready` ⇒ **数据面被反压面污染** |

### 3. 两者的既有测试对 `ready` **零判别力**

demux 的 3 处用例与 fork 的 3 处用例**只断言 `valid` / `payload`**；
护栏 R8 对两者都是**豁免**状态（理由：「方向修正是行为变更」/「同步语义从未工作过」）。

## What Changes

| 文件 | 改动 |
|---|---|
| `include/chlib/stream.h` | 新增 `detail::drive_master_ready`（三处响亮拒绝，demux/fork 共用）；`stream_demux` 按 ADR-056 D1~D5 重写；`stream_fork` 按 ADR-057 D1~D5 重写（`synchronous=false` ⇒ `CHFATAL_EXCEPTION`） |
| `tests/chlib/test_stream.cpp` | demux/fork 用例接线订正（`as_master()` + 删自驱动 `ready`）；`synchronous=false` 用例改为 `REQUIRE_THROWS_AS` |
| `tests/chlib/test_stream_mux_demux.cpp` | 2 个 demux 用例接线订正 |
| `tests/test_stream_demux_fork_dual.cpp`（**新**） | 2 个 `DUAL_BACKEND_TEST_PAIR`：demux（方向 / 隔离 / 越界）· fork（AND 两面 / 数据面独立） |
| `examples/stream/{stream_demux_demo,stream_fork_demo}.cpp` | 接线订正（`as_master()` + 删节点重绑定式的 ready 赋值） |
| `scripts/lib/stream_ready_ownership.py` | **R8 豁免清单清空**（两个都已收口）· R8 认 `drive_master_ready(` · **新增 R13**（helper 的三处响亮拒绝 + 两处都必须调用它）· 2 种新投毒 |
| `scripts/verify_stream_ready_ownership.sh` | 自检按 `demux-fork-*` 前缀选判别力二进制 |
| 计数锚点 | `CTEST_TOTAL` 208 → 209 · `VERILATOR_E2E_TOTAL` 41 → 42 · `DUAL_BACKEND_PARITY_TOTAL` 32 → 33 · `PAIRS` 192 → 194 · non-perf 197 → 198 |

## Non-Goals

- **不实现**异步 fork（`synchronous=false` 的真语义）⇒ 独立立项（ADR-057 D2）；
- **不改** `stream_mux` / arbiter 族（它们的 ready 已是正确形态）；
- **不在**本 change 给其余组合子补 `drive_master_ready` 式守卫（那是独立议题：
  现有 15 个组合子直接 `<<=` 上游 ready，**无**多驱动守卫）⇒ 登记为已知缺口。

## Verification（前瞻 · 可勾选）

- [ ] demux：默认态 `in_ready == 1`（判别性）· 被选中拉低 ⇒ 0 · 非选中拉低 ⇒ 不变 · 越界 ⇒ 0 且全 valid 为 0
- [ ] fork：`AND` 的**两面**（全高 ⇒ 1 · 任一低 ⇒ 0）· ready 为 0 时 valid/payload 不变
- [ ] fork：`synchronous=false` ⇒ 建图期 `std::runtime_error`
- [ ] 双后端 parity：有 verilator 的断言数 == 2 × 无 verilator 的
- [ ] R8 收口数 15 → **17**（demux/fork 移出豁免）· R13 绿 · 15 种投毒全红
- [ ] `ctest -LE perf` 零回归
