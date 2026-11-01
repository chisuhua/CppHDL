# chlib-stream-arbitration Specification

## Purpose
定义 `chlib` 的 **stream 仲裁器**（`stream_arbiter_round_robin` /
`stream_arbiter_priority` / `stream_arbiter_lock`）
的**仲裁语义契约**：**名字承诺的语义 MUST 兑现**（round-robin 必须真轮转，
不许饿死任何持续有效的通道）；**固定优先级类 MUST 如实描述**；
**ready 链**受 `chlib-stream-ready-ownership` 管辖（本 capability 不重复）。

**为什么需要**：`stream_arbiter_round_robin` 名为 round-robin，实现却**恒传
`last_grant = 0_d`** ⇒ ⇒ 每拍扫描起点相同 ⇒ ⇒ **轮转从不发生** ⇒ ⇒
**低位通道饿死**（实测：输入 0 与 2 持续有效 ⇒ 恒选 2；四输入全有效 ⇒
恒选 1）。且 `stream_arbiter_sequence` 的注释谎称
「using round-robin selection」/「wraps around」而实现是 `priority_encoder`。
⇒ ⇒ **仲裁语义此前无任何契约载体** ⇒ ⇒ 同类缺陷可**静默复发**
（既有测试只测「唯一有效输入」⇒ 该场景下轮转与固定优先级**逐位相同**
⇒ ⇒ **零判别力**）。

⇒ ⇒ ⇒ **当前状态（2026-11-01）**：`stream_arbiter_round_robin` **已真轮转**
（反馈寄存器 + `valid` 条件更新 ⇒ 实测 `0,2,0,2,…` / `2,3,0,1,2,3,0,1`）
⇒ ⇒ 判别力用例（`[rotation]` tag · 逐拍金标）已进 `tests/chlib/test_stream.cpp`
⇒ ⇒ 护栏投毒 `rr-feedback-const`（9 种投毒全被抓）
⇒ ⇒ ⇒ `stream_arbiter_sequence` 的注释**已订正**（**零行为变更**）⇒ ⇒
⚠️ 它与 `stream_arbiter_priority` 的**重复去留**曾是 **backlog**（架构裁决）
⇒ ⇒ 2026-11-01 **裁决为删除**，已于 2026-11-02 走**独立 change** 物理删除
（消费者已迁移：3 个用例移植/删除 + 1 处在 `test_stream.cpp` 改用 priority；
回归禁令 = 结构护栏 R12）⇒ ⇒ 本 capability 的仲裁器清单收敛为 **3 个**。
⇒ ⇒ 来源：`openspec/changes/archive/2026-11-01-stream-arbiter-rr-rotation-2026-11/`。

## Requirements

### Requirement: 轮转仲裁器 MUST 真正轮转（MUST NOT 饿死持续有效的通道）

`chlib` 中**名字或文档承诺轮转**的仲裁器（当前 = `stream_arbiter_round_robin`）
MUST 兑现该承诺：当**多个输入持续有效**时，授予 MUST **在有限拍内覆盖全部
有效通道** ⇒ ⇒ MUST NOT 存在「某个持续有效的通道**永远**得不到授予」的情形。

⇒ ⇒ 该判据 MUST 由**行为**用例承载（**MUST NOT** 词法/结构判据）：期望值
**逐拍手推**，且 MUST 有**反向判据**证明「变异后断言转红」。
MUST NOT 要求某一具体相位（首拍授予谁取决于实现的状态初值）。

#### Scenario: 多通道持续有效 MUST 全部被服务

- **WHEN** `stream_arbiter_round_robin<T, 4>` 的**四个输入全部持续有效**，
  消费端 ready 恒为 1，连续运行 `4` 拍
- **THEN** 授予序列 MUST 覆盖 **全部 4 个通道**（周期 = `N`，**相位不限**）
- **AND** 修复前该场景恒选同一通道（实测恒选 `1`）⇒ ⇒ 本条 MUST 能区分两者

#### Scenario: 两个有效通道 MUST 交替（饿死禁止）

- **WHEN** 仅输入 `0` 与 `2` 持续有效，其余无效，消费端 ready 恒为 1
- **THEN** `0` 与 `2` MUST **各占一半、严格交替** ⇒ ⇒ **相位不限**
      （首拍是 `0` 还是 `2` 都合法 —— 相位取决于实现的状态初值
      ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 锁死字面量会冻错主 spec**）
- **AND** 修复前该场景**恒选 `2`**（⇒ ⇒ 通道 `0` **饿死**）⇒ ⇒ 本条 MUST 能区分两者

#### Scenario: 唯一有效通道 MUST 仍被选中（既有语义的回归锁）

- **WHEN** 只有**一个**通道有效
- **THEN** 该通道 MUST 被选中 ⇒ ⇒ 轮转的实现 MUST NOT 改变这一基本语义
- **AND** 既有测试（只测「唯一有效输入」）对轮转缺陷**零判别力**
  ⇒ ⇒ 它 **MUST NOT** 被当作「轮转已正确」的证据

#### Scenario: 变异 MUST 被行为用例抓住（反向判据）

- **WHEN** 把「上一次授予」的反馈改成**常量**（= 轮转失效的实现形态）
- **THEN** 上述行为用例 MUST **转红**
- **AND** 该变异 MUST **先确认可编译**（不可编译的变异会跑旧二进制 ⇒ 假绿）
- **AND** 还原后 MUST **复绿**，且还原 MUST 刷新文件 mtime（否则二进制陈旧）

### Requirement: 仲裁器的命名与文档 MUST 与实现行为一致

任何 `chlib` 仲裁器的**名字**与**头注释** MUST 如实描述其**实际仲裁策略**
⇒ ⇒ MUST NOT 声称轮转 / wrap-around 而实现是固定优先级
（「名字/注释 over-promise」会**诱导**调用方写出依赖不存在行为的代码）。

⇒ ⇒ 若**无法**在不制造组件重复的前提下兑现名字 ⇒ ⇒ MUST **订正文档**
（零行为变更）并把「组件去留」作为**独立架构裁决**登记，MUST NOT 在本类
缺陷修复中**顺手改行为**。

#### Scenario: 固定优先级仲裁器 MUST NOT 声称轮转

- **WHEN** 检查固定优先级仲裁器（现为 `stream_arbiter_priority`）的头注释
- **THEN** 它 MUST NOT 声称 round-robin / wrap-around
- **AND** 它 MUST 说明实际策略（固定优先级扫描、较高索引优先）
- **AND** `stream_arbiter_sequence` MUST **不存在**（它的注释曾谎称
  「using round-robin selection」/「wraps around」而实现是 `priority_encoder`
  ⇒ ⇒ 经**独立架构裁决**后于 2026-11-01 **物理删除**）
- **AND** 该「不得回归」MUST 由**结构护栏**守住（源文件 MUST NOT 再出现该符号）

#### Scenario: 与既有组件近乎重复时 MUST 登记而非删除

- **WHEN** 发现两个仲裁器在**仲裁逻辑上逐位相同**（`stream_arbiter_sequence`
  与 `stream_arbiter_priority` 都用 `priority_encoder(valid_vector)`，
  仅 ready 链的门控不同）
- **THEN** 该重复 MUST 作为**独立架构裁决**登记（组件身份/去留）
- **AND** MUST NOT 在该类**语义修复**中**删除**其中一个（仓内有消费者
  ⇒ 删除是 API 变更）
- **AND** 裁决为「删除」时，删除 MUST 走**独立的 change**（含消费者迁移 +
  回归禁令护栏）⇒ ⇒ MUST NOT 夹在语义修复里
