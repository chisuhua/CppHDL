# 提案：删除 `stream_arbiter_sequence`（B5）

- **架构依据**：`.rddf/checks/architect-decisions-pending-2026-11.md` 的 **B5 裁决**
  （删 `sequence`）+ capability `chlib-stream-arbitration` 的
  「与既有组件近乎重复时 MUST 登记而非删除 ⇒ **裁决为删除时 MUST 走独立 change**」
- **规格影响**：`chlib-stream-arbitration` 的 **MODIFIED ×1**
  （`仲裁器的命名与文档 MUST 与实现行为一致` —— 两个 Scenario 的**内容**订正，标题保留）
  ⇒ 归档**禁** `--skip-specs`
- **规模 / 风险**：小（1 个组件 + 4 处测试引用 + 1 条护栏判据）
- **前置**：`fwft-fifo-physical-removal-2026-11`（同型处置，已归档）

## Why

`stream_arbiter_sequence` 与 `stream_arbiter_priority`（`include/chlib/stream.h`）
的**仲裁逻辑逐位相同** —— 两者都是：

```cpp
valid_vector → priority_encoder(valid_vector) → mux(payloads, selected_idx)
```

唯一差异是 ready 链：`sequence` 多一个 `& has_valid` 门控（**协议合法**，且
**无生产消费者依赖**）。

而 `sequence` 的**名字与注释**承诺的是另一件事：
「Cycles through inputs in fixed order **using round-robin selection**」
+「Fixed order arbitration, **wraps around**」⇒ ⇒ **实现既不轮转、也不 wrap**
⇒ ⇒ A1（2026-11-01）已把注释**订正为如实描述**（零行为变更）⇒ ⇒
但**组件本身**仍是「**名字说谎的重复组件**」。

| 证据 | 值 |
|---|---|
| 生产消费者（`include/` `src/` `examples/` `samples/`） | **0** |
| 测试消费者 | **4**（`tests/chlib/test_stream.cpp` ×1 · `tests/test_stream_arbiter.cpp` ×3） |
| 与 `priority` 的语义差 | 仅 ready 链 `& has_valid`（`priority` 无） |
| 既有「wraps around」用例 | ⚠️ 它断言的是**不存在的行为**（组件从不 wrap）⇒ ⇒ **零判别力** |

## What Changes

| 文件 | 改动 |
|---|---|
| `include/chlib/stream_arbiter.h` | **删除** `StreamArbiterSequenceResult` + `stream_arbiter_sequence`（≈ 92 行）；文件头注释去掉 "and sequential" |
| `tests/chlib/test_stream.cpp` | 反压用例的 variant 1：`stream_arbiter_sequence` → `stream_arbiter_priority`（覆盖面不减：lock / priority 是仅存的两个非轮转仲裁器） |
| `tests/test_stream_arbiter.cpp` | 3 个 sequence 用例：**移植 2 个**到 `priority`（basic + no-valid，并补 `payload` 断言）；**删 1 个**（"wraps around" —— 断言的是不存在的行为） |
| `scripts/lib/stream_domain_closure.py` | **新增 R12**（两面判据）：① `stream_arbiter.h`（剥注释）MUST NOT 出现 `stream_arbiter_sequence` / `StreamArbiterSequenceResult` ② `stream_arbiter_priority` MUST 仍在 |
| `openspec/specs/chlib-stream-arbitration/spec.md` | MODIFIED delta：两个 Scenario 就地订正（Scenario 1 改为检查 `priority` + 声明 `sequence` MUST 不存在；Scenario 2 补「裁决为删除时 MUST 走独立 change」） |

## Non-Goals

- **不改** `stream_arbiter_priority` / `stream_arbiter_lock` / `round_robin` 的行为；
- **不新增**轮转语义的变体（那会**制造**重复，正是本 change 要消除的）；
- **不动** `stream_demux` / `stream_fork`（B2 / B3，独立 change）。

## Verification（前瞻 · 可勾选）

- [ ] `include/chlib/stream_arbiter.h`（剥注释）无 `stream_arbiter_sequence` / `StreamArbiterSequenceResult`
- [ ] `stream_arbiter_priority` 仍在
- [ ] 护栏 R12 有**判别力**：把符号写回 ⇒ 红；还原 ⇒ 绿
- [ ] 移植后的 2 个 `priority` 用例有**行为**断言（`valid` + `payload`），不只是 width
- [ ] `openspec validate … --strict` valid · `verify_openspec_delta.sh` 4 条全绿
- [ ] `ctest -LE perf` 零回归 · **计数锚点不变**（无新增/删除 ctest 条目）
