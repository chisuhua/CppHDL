# Change: stream-arbiter-rr-rotation-2026-11

## Why

🔴 **`stream_arbiter_round_robin` 名为 round-robin，行为却是固定优先级** ——
通道饿死，缺陷由**实测**确认（非静态阅读推断）：

| 探针（`/tmp/opencode/p2/probe_rr.cpp`，2026-11-01） | 实测输出 |
|---|---|
| `N=4`，输入 **0 与 2** 持续有效 | `selected` 恒为 **2** ⇒ 通道 0 **饿死** |
| `N=4`，**四个输入全有效** | `selected` 恒为 **1** ⇒ 通道 0/2/3 **全饿死** |
| `N=4`，仅输入 2 有效 | `selected` = 2（与既有测试一致 ⇒ **既有测试零判别力**） |

**根因已定位到单一位置**（`include/chlib/stream.h`）：

- `:658` `auto current_channel = ch_reg<ch_uint<...>>(0_d);` ⇒ **死节点**
  （建了，**从未接到任何地方**）；
- `:675` `ch_uint<N_INPUTS> last_grant = 0_d;` ⇒ **恒为 0**；
- `:676` `round_robin_selector<N_INPUTS>(valid_vector, last_grant)`
  ⇒ ⇒ 每拍拿到的 `last_grant` **都相同** ⇒ 每拍 `start_pos` **都相同**
  ⇒ ⇒ ⇒ **轮转从不发生**。

⚠️ **选择器本身是正确的**（`/tmp/opencode/p2/probe_selector.cpp` 实测
`request=0b1111`，`last_grant` 依次为 none / idx0 / idx1 / idx2 / idx3
⇒ grant_idx = **1 / 1 / 2 / 3 / 0**，与手推期望**逐项一致**）
⇒ ⇒ ⇒ ⇒ **缺陷 100% 在调用方** ⇒ ⇒ **修法无需改动选择器**。

⚠️ **一个同族缺陷顺带发现**（台账未预料）：`include/chlib/stream_arbiter.h`
的 `stream_arbiter_sequence` 注释写
「Cycles through inputs in fixed order **using round-robin selection**」+
「Fixed order arbitration, **wraps around**」，实现却是
`priority_encoder(valid_vector)`（**固定优先级、不 wraps around、不轮转**）
⇒ ⇒ 同族「**名字/注释 over-promise**」。

## What Changes

### ① `stream_arbiter_round_robin` ⇒ **接上反馈寄存器**（真轮转）

- **删除死节点** `current_channel`（Zero-Debt：不留「建了不用」的节点）；
- 用**一个** `ch_reg<ch_uint<N_INPUTS>>` 保存**上一次实际授予**（one-hot）
  ⇒ ⇒ 喂给 `round_robin_selector` 的 `last_grant`；
- 更新条件 = **本拍真的产生授予**（`arb_result.valid`）⇒ ⇒ 否则**保持**
  （无授予的拍不推进指针 —— 否则会「空转」丢掉轮转相位）；
- ⚠️ **反馈经寄存器** ⇒ ⇒ **不可能形成组合环**
  （本仓 `bf78367` 的组合环事故形态是「源直接取组合输出」）；
- ⚠️ **零公开 API 变更**（无新端口 / 无新结果字段）。

### ② `stream_arbiter_sequence` ⇒ **只修正注释**（零行为变更）

- 把「using round-robin selection」/「wraps around」改成**如实**描述
  （固定优先级扫描：**较高索引优先**，与 `priority_encoder` 的实际行为一致）；
- **不改行为**（用户 2026-11-01 裁决）⇒ ⇒ 理由：改成真轮转会让仓内出现
  **两个语义相同的轮转仲裁器**（它本已与 `stream_arbiter_priority` 近乎重复）
  ⇒ ⇒ 制造新的重复；而「sequence」这个名字本就支持「固定顺序」。

### ③ 契约侧 ⇒ **新建** capability `chlib-stream-arbitration`

⇒ ⇒ 仲裁语义此前**无任何** capability 声明（`chlib-stream-semantics` 的
Purpose 是 **FIFO 类输出时序**，不覆盖仲裁）⇒ ⇒ 新建 + **ADDED** Requirement。

## 为什么不做什么（Non-Goals）

- ❌ **不改 `round_robin_selector`**（`include/chlib/selector_arbiter.h`）——
  实测证明它**正确**；改共享工具函数会无谓扩大爆炸半径；
- ❌ **不改 `stream_arbiter_priority`** —— 它就是固定优先级，名字与行为**一致**；
- ❌ **不改 `stream_arbiter_lock`** —— lock 语义（锁存首个通道）与轮转正交，
  且 P1-a 已收口其 ready 链；
- ❌ **不合并 `sequence` 与 `priority`** —— 「两者近乎重复」是**架构裁决**
  （组件身份/去留），不属本 change ⇒ ⇒ 登记 backlog，**不在本次顺手删**
  （⚠️ 仓内 `examples/` / `samples/` 有 `sequence` 消费者，删除是 API 变更）；
- ❌ **不改 `demux` / `fork` 的 ready 链** —— 属 P1-b / P1-c「未收口」，
  已由 P1-a 的护栏 R8 **显式豁免 + `::notice` 点名**。

## Impact

- **改动文件**：
  - `include/chlib/stream.h`（`stream_arbiter_round_robin`）
  - `include/chlib/stream_arbiter.h`（`stream_arbiter_sequence` 注释 only）
  - `tests/chlib/test_stream.cpp`（**判别力用例**：交替授予 + 反向判据）
  - `openspec/specs/chlib-stream-arbitration/spec.md`（**新建** capability）
- **爆炸半径**：**小** —— 单个函数体 + 一处注释
- **行为变更（可观测）**：授予顺序从「固定」变「轮转」⇒ ⇒
  ⚠️ **既有断言已逐个核对，零破坏**：
  - `examples/stream/stream_arbiter_demo.cpp` ⇒ **只打印，无断言**；
  - `samples/stream_arbiter_example.cpp` ⇒ 断言「选中**某个有效**输入」
    （`∈ {0,2}`）⇒ 轮转后**仍满足**；
  - `tests/chlib/test_stream.cpp`（唯一有效输入）⇒ 唯一有效者**必被选中**；
  - `tests/chlib/test_stream.cpp`（`N=2` 的 SECTION）⇒ **只做结构检查**
    （`width` 断言，注释明写 "avoid relying on current round-robin selector
    behavior"）。
- **风险**：**低** —— 无持久化面；回滚 = revert 实施 commit
- **计数锚点**：判别力用例进**既有** target ⇒ ⇒ `CTEST_TOTAL` 等**不变**

## 权限说明

- 本 change **不需要** ADR：它不是架构取舍，而是**让实现兑现既有名字**
  （`round_robin` 这个名字就是契约）⇒ ⇒ 属实现缺陷修复；
- **不改任何已签批 ADR 的决策内容**；
- **不动** `.rddf/roadmap/**` 的 feature `status` / objective 判据（planner 职责）。

## 评审记录

（见 `design.md` 的实施记录与 critic 处置台账。）
