# 提案：`chlib::fwft_fifo` 物理删除（B4）

- **架构依据**：ADR-045 §2.4（「`fwft_fifo` 重写/改名 单独立项」）+
  ADR-008（单时钟域约束，§4 曾把它列为替代方案）+
  `.rddf/checks/architect-decisions-pending-2026-11.md` 的 **B4 裁决**（物理删除）
- **规格影响**：`chlib-stream-semantics` 的 **MODIFIED ×1**（`FIFO 类组件的输出时序 MUST 被定义`
  —— 只改 `fwft_fifo` 那个 Scenario 的**内容**，标题保留）⇒ 归档**禁** `--skip-specs`
- **规模 / 风险**：小（1 个头文件的删除 + 1 条 spec Scenario 订正 + 1 条护栏判据）
- **关联台账**：`.rddf/improvements/chlib-naming-honesty-audit-2026-11.md`

## Why

`chlib::fwft_fifo` 的**名字**承诺 FWFT（First-Word Fall-Through = show-ahead），
而**实测行为不是**：写入后 `q` 恒为初值，**直到发起读才出现队首**。

| 证据 | 值 |
|---|---|
| **设计**消费者 | **0**（`include/` / `src/` / `examples/` / `samples/` 无引用） |
| **唯一**消费者 | 它**自己的测试** `tests/chlib/test_fifo.cpp` 的 `TEST_CASE("FIFO: fwft_fifo")` ⇒ ⚠️ **该用例对 FWFT 语义零判别力**（只断言 reset 后 `empty==true` / `full==false`，**从未断言 show-ahead**）⇒ ⇒ 删掉它**不损失任何判别力** |
| 真 FWFT 能力 | **已在** `sync_fifo` 的 `q_async`（0-cycle `aread`）⇒ ⇒ 且有双后端 parity 判据（`tests/test_stream_fifo_dual.cpp`） |
| 现状记录 | `include/chlib/fifo.h` 的头注释已被 A1 订正为「本组件【不是】FWFT」（⇒ **注释说了实话，名字仍在说谎**） |
| 处置选项 | 改名 / 重写 / **物理删除** ⇒ 前两者对**零设计消费者**的组件是**纯成本** ⇒ ⇒ ZERO-DEBT「Never leave for later」 |

> ⚠️ **实施期订正（2026-11-01）**：初稿写「零消费者」**不准确** —— 编译期实测
> `tests/chlib/test_fifo.cpp:417` 仍调用它 ⇒ ⇒ 真值 = 「零**设计**消费者 +
> **一个**无判别力的自测」。⇒ ⇒ 处置相应加一步：**删掉那个自测用例**
> （`ctest` 条目数**不变** —— 它是同一二进制内的用例）。

⇒ ⇒ **名字 over-promise 的组件留在仓里，会被下一个人当成 FWFT 参照** —— 这正是
本仓「静默失效」病症的一个变种（**误导**而不是误行为）。

## What Changes

| 文件 | 改动 |
|---|---|
| `include/chlib/fifo.h` | **删除** `FwftFifoResult` + `fwft_fifo()` 及其文档块（≈ 92 行）；两处 async-fifo 注释不再引用已删符号（改为「本仓没有 FWFT 组件 ⇒ 请用 `sync_fifo::q_async`」） |
| `tests/chlib/test_fifo.cpp` | **删除** `TEST_CASE("FIFO: fwft_fifo")`（**唯一**消费者，且对 FWFT 语义**零判别力** ⇒ 只断言 reset 后 `empty`/`full`）⇒ ⇒ `ctest` 条目数**不变**（同一二进制内的用例） |
| `scripts/lib/stream_domain_closure.py` | **新增 R11**（两面判据）：① `fifo.h`（剥注释后）MUST NOT 出现 `fwft_fifo` / `FwftFifoResult`（**回归禁令**）② 真 FWFT（`q_async`）MUST 仍在（**反向判据**：防「为让判据绿而误删真能力」） |
| `docs/adr/ADR-008-single-clock-domain-constraint.md` | §4 的替代方案清单去掉 `fwft_fifo`（v1.2）；**决策（单时钟域约束）零改动** |
| `openspec/specs/chlib-stream-semantics/spec.md` | MODIFIED delta：`fwft_fifo` 那个 Scenario 的**内容**改为「MUST 不存在 + MUST NOT 重新引入同名组件」⇒ 标题保留（CLI 按标题匹配，改标题会被判「丢弃旧 + 新增」⇒ 拒绝归档） |
| `.rddf/checks/stream-domain-closure.md` | 第 14 行：实例**已消除** + 回归禁令 R11；**通用条款**（B8）仍未兑现 ⇒ 该行保持 ❌（**不因删了实例就改成 ✅**） |

## Non-Goals

- **不改** `sync_fifo` / `stream_fifo`（它们的 `q_async` 就是真 FWFT）；
- **不实现**真 FWFT 组件（零需求；`q_async` 已覆盖）；
- **不加** B8 的「命名诚实性条款」（独立 change，等本轮沉淀）。

## Verification（前瞻 · 可勾选）

- [ ] `include/chlib/fifo.h`（剥注释后）不含 `fwft_fifo` / `FwftFifoResult`
- [ ] `q_async` 仍在（真能力未被误删）
- [ ] 护栏 R11 有**判别力**：把符号写回 ⇒ 红；还原 ⇒ 绿
- [ ] `docs/adr/ADR-008` 不再把已删组件列为替代方案
- [ ] `openspec validate … --strict` valid · `verify_openspec_delta.sh` 4 条全绿
- [ ] `ctest -LE perf` 零回归 · 计数锚点**不变**（无新增测试文件）
