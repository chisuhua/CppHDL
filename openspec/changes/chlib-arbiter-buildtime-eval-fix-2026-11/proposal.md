<!-- touched-components: stream_arbiter_lock, round_robin_arbiter -->

# 提案：chlib 仲裁器族的两处「建图期求值」缺陷修复（ADR-065 实施）

- **ADR**：`docs/adr/ADR-065-chlib-arbiter-buildtime-eval-fix.md`（**Accepted** 2026-11-03）
- **立项来源**：`.rddf/improvements/stream-arbiter-lock-selects-channel-zero-2026-11.md`
  · `.rddf/improvements/selector-round-robin-not-rotating-2026-11.md`
- **关联 capability**：`chlib-stream-arbitration`（**ADDED ×1**）⇒ 归档**禁** `--skip-specs`
- **行为变更**：是（两个组件的输出都会变 —— 这正是修复）

## Why

两个组件的**同一处根因**（`if (ch_bool_expr)` 在**建图期**求值 ⇒ 恒假）导致：

| 组件 | 修复前的实测行为 | 后果 |
|---|---|---|
| `stream_arbiter_lock` | `selected=0`、**`out.payload` 是通道 0 的数据**（唯一有效 = 通道 1、payload `0x22` ⇒ 实得 `0x11`） | **静默数据损坏**（`out.valid` 仍是对的 ⇒ 更难发现） |
| `round_robin_arbiter` | `next_ptr` 连续 3 拍恒 `0` | 不是轮转，而是「固定从最低位开始」⇒ **持续有效的低位通道饿死高位通道**（违反 capability 第 1 条） |

⚠️ **既有测试零判别力**：`tests/test_stream_arbiter.cpp` 的 lock 用例只断 `.width` +
1 条 `valid==true`（`selected`/`payload` **从未被断言**）；`tests/chlib/test_selector_arbiter.cpp`
的 12+ 个用例**全部裸 `context`**（无 `Simulator`）⇒ `next_ptr` 的动态推进**不可观测**
⇒ ⇒ 名字叫「holds lock until transaction」的用例**从未测过锁**。

⚠️ **实施期又挖出两个缺陷**（本 change 一并修）：
① `out.valid` 用的是「**任一**通道 valid」而非「**选中**通道的 valid」⇒ 锁定通道撤销
valid 而其它通道有效时 `out.valid=1` 但 payload 属于**另一个**通道；
② **框架层**：interpreter 的 `eq` 在「64 位 vs 127 位」且**移位量 ≥ 2** 时**判错**
（独立最小复现：`a=0x4`、`shl=0x4` ⇒ `eq` 返回 0）⇒ ⇒ 64 位宽度下轮转**不工作**
⇒ ⇒ 本 change **只绕开**（改用 `bit_select`），框架缺陷**独立立项**。

## What Changes

| 文件 | 改动 |
|---|---|
| `include/chlib/stream_arbiter.h` | A-3：选择改 `select` 链 + `->next` + **新增 `locked` 寄存器** + `out.valid` 改为选中通道的 valid |
| `include/chlib/selector_arbiter.h` | A-4：删外层 `if (result.valid)`（select 链无条件执行）+ 判据改 `bit_select` |
| `tests/chlib/test_selector_arbiter.cpp` | **8 处期望值**按正确语义 + 手推读模型更新 + 补 `next_ptr` 断言（84 → 120） |
| `tests/test_stream_arbiter.cpp` | 2 个 lock 用例补**行为断言**（`selected`/`payload`/`ready`；10 → 17） |
| `tests/chlib/test_stream_arbiter_lock_dual.cpp`（**新**） | **7** 个 `DUAL_BACKEND_TEST_PAIR` / **184 == 2 × 92** 断言 |
| `tests/CMakeLists.txt` | 注册新 dual target |
| `openspec/specs/chlib-stream-arbitration/spec.md` | ADDED ×1（1 Requirement + 5 Scenario） |

## 关键裁决（详见 ADR-065）

1. **两处都修**（不删除）：删除先例 `fwft_fifo` / `stream_arbiter_sequence` 的共同点是
   「能力已被别处覆盖」⇒ 此处**不成立**（锁存仲裁与自持轮转原语在仓内唯一）。
2. **锁存语义按「事务完成」**（`valid & ready`）释放，而**不是**「被锁通道是否仍然
   valid」⇒ 后者会在通道中途撤销 valid 时**换人**（打断已送出的数据）。
3. **不 MODIFIED 既有条款**：第 1 条「轮转 MUST 真正轮转」的适用面是 stream 层的
   `stream_arbiter_round_robin` ⇒ 在 ADDED 里显式声明适用范围（避免改既有 Scenario
   的标题 —— CLI 按标题匹配 ⇒ 改标题 = 丢弃 + 新增 ⇒ 拒绝归档）。

## Verification（已实测回填）

- [x] A-3：唯一有效 = 通道 k ⇒ `selected == k` 且 `payload == 通道 k 的 payload`
      （k = 0/1/2 各一例 ⇒ 能区分「选错」与「数据错」）
- [x] A-3：锁存保持（授予后更优先的通道变有效也不换人）+ 释放（fire 后换人）
- [x] A-3：锁定通道撤销 valid ⇒ `out.valid == 0`（不串数据）
- [x] A-4：`next_ptr` 每拍推进、`grant` 跟着轮转、偏请求无饿死、无请求保持
- [x] 变异 **6/6 抓** + **1 条等价变异如实登记**（链序反转）
- [x] 既有 12+ 个裸-context 用例逐条核对：**8 处转红 ⇒ 全部是「钉住了缺陷」**
- [x] 全量重建 0 error · `ctest -LE perf` **229/229** 全绿
- [x] capability ADDED ×1 ⇒ 归档**未**用 `--skip-specs`

## 不在本次范围

- **框架层**：`eq` 的宽操作数误判 ⇒ 独立立项（本 change 只绕开）
- `priority_encoder` 注释与实现相反 ⇒ 独立议题
- `chlib` 全仓「C++ 控制流作用于硬件值」的**普查** ⇒ B8 已否决该义务
