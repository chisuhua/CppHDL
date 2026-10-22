# sample-regression-guard Specification

## Purpose

`samples/` 下的 **40 个示例程序**长期处于「零自动化覆盖」状态 ⇒ 任何 sample 的腐坏
（编译失败、运行时崩溃、行为退化）**只能靠人偶然发现** —— `samples/fifo_bundle` 的
失败从落地起存在了数月才被审计发现，就是这一模式的直接代价。

本 capability 规定 samples 的**回归护栏契约**：每个 sample MUST 被纳入一条自动化
执行路径并检查退出码；覆盖集合 MUST **守恒**（护栏不许因为「sample 消失」而静默
缩面 —— 那正是它要根除的病的同型复刻）；既有失败 MUST 被**显式**记录且豁免 MUST
**可失效、不静默**；护栏 MUST 在 CI 中执行且 MUST 有超时保护。

> ℹ️ 本 capability 只约束**护栏**（覆盖 + 判定 + 记录 + CI），
> **不**要求 sample 具备行为断言（sample 的职责是「演示 + 自检」，
> 行为断言属于 `tests/`）。

## Requirements

### Requirement: 全部 sample MUST 被纳入自动化回归护栏的覆盖集合

`samples/` 下的**每一个** `.cpp` 源文件 MUST 属于以下二者之一，且 MUST NOT 处于
「既未注册、也未被记录」的中间态：

- 被注册进 `samples/CMakeLists.txt` 的 `CH_SAMPLE_TARGETS`（⇒ 有二进制 ⇒ 被护栏执行）；
- 被列入**带理由**的禁用清单（⇒ 未构建，但**原因被显式记录**）。

护栏 MUST 执行**全部已注册**的 sample，MUST NOT 存在「只能靠人手工跑」的 sample。

> ℹ️ 「覆盖集合」以**源码**为准、以**构建产物**为准的执行 ⇒ 二者 MUST 由护栏
> 做**守恒校验**。否则「一个 sample 编译失败 ⇒ 二进制消失 ⇒ 护栏静默缩面」——
> 那正是本能力要根除的「静默腐坏」的同型复刻。

#### Scenario: 每个 sample 源文件 MUST 有明确归属

- **WHEN** reviewer 枚举 `samples/*.cpp`
- **THEN** 每个源文件 MUST 要么出现在 `CH_SAMPLE_TARGETS` 里，
  要么出现在禁用清单里
- **AND** 禁用清单里的每一项 MUST 带有**理由**
- **AND** MUST NOT 存在「既未注册、也无理由记录」的 sample

#### Scenario: 存在覆盖全部已注册 sample 的执行脚本

- **WHEN** reviewer 检查仓库的自动化覆盖
- **THEN** MUST 存在一个脚本（如 `scripts/run_all_samples.sh`）遍历**全部已注册**的
  sample
- **AND** 该脚本 MUST 对**每个** sample 检查退出码
- **AND** 任一 sample 非零退出 MUST 使该脚本失败

#### Scenario: 覆盖集合 MUST 守恒（护栏不许静默缩面）

- **WHEN** 护栏运行时，实际执行的 sample 数与「已注册数」不等
  （例如某个 sample 编译失败导致二进制缺失）
- **THEN** 脚本 MUST **失败**
- **AND** MUST NOT 把「二进制缺失」记为 SKIP / 通过
- **AND** MUST 在输出中点名缺失的 sample

#### Scenario: 脚本 MUST 有超时保护

- **WHEN** 某个 sample 挂死（不退出）
- **THEN** 脚本 MUST 在**有限时间**内终止该 sample 并将其记为失败
- **AND** MUST NOT 让整个 CI job 无限期挂起
- **AND** 超时（退出码 124）与崩溃（退出码 139）MUST 在汇总中**可区分**

#### Scenario: 超时工具 MUST 可移植

- **WHEN** 护栏脚本在 macOS 上运行
- **THEN** MUST 使用可用的超时命令（`timeout` 或 `gtimeout`）
- **AND** MUST NOT 因缺少 GNU `timeout` 而 command-not-found

#### Scenario: 护栏 MUST 在 CI 中执行

- **WHEN** reviewer 检查 CI 配置
- **THEN** MUST 有一个 CI job 执行该脚本
- **AND** 该 job MUST 在 PR 上运行
- **AND** 该 job 失败 MUST 使该 workflow 失败
- **AND** 该 job MUST 设定**超时预算**（job 级 `timeout-minutes`）

### Requirement: sample MUST 以零退出码结束

每个 sample MUST 以**退出码 0** 结束。

若某个 sample 因**既有缺陷**失败，则该失败 MUST 被**显式记录**（缺陷单 / 本
capability 的豁免清单）并 MUST NOT 被静默跳过。

豁免 MUST 是**数据驱动**的（每项含 sample 名、根因、关联缺陷单 / change、
**失效条件**），MUST 在运行时**逐条打印**（豁免 MUST NOT 是静默的），
且 MUST NOT 成为**永久免检**：

- 被豁免的 sample **仍然被执行**（作为 stale 探针）⇒ 覆盖集合的守恒公式
  **不因豁免而减少**；
- 豁免**只影响失败计数**：被豁免项的失败 MUST NOT 计入失败数；
- 一旦被豁免的 sample **转绿**（退出码 0），护栏 MUST 把它报为 **stale 豁免**
  并失败。

#### Scenario: 失败 MUST 显式记录

- **WHEN** 引入护栏时发现既有失败的 sample
- **THEN** MUST 逐个定性其根因
- **AND** MUST 修复它，或在豁免清单中**显式**记录（附根因与关联的缺陷单）
- **AND** MUST NOT 用「跳过它」的方式让护栏变绿

#### Scenario: 豁免项 MUST 仍然被执行

- **WHEN** 某个 sample 处于生效中的豁免状态
- **THEN** 护栏 MUST **仍然执行**它
- **AND** 它的失败 MUST NOT 计入失败数
- **AND** MUST 在输出中逐条打印该豁免（sample 名 + 根因 + 关联 change）

#### Scenario: stale 豁免 MUST 使护栏失败

- **WHEN** 某个被豁免的 sample 已经转绿（退出码 0）
- **THEN** 护栏 MUST 把它报为 **stale 豁免**
- **AND** 护栏 MUST **失败**（MUST NOT 因为「反正豁免了」而放过）

#### Scenario: 豁免 MUST 有失效条件与摘除责任

- **WHEN** 豁免项的根因被修复（关联 change 落地）
- **THEN** 该豁免 MUST 被移除
- **AND** 移除责任 MUST 落在**根因所属的 change** 的任务里
- **AND** 移除后 MUST 全量重跑护栏

#### Scenario: 崩溃 MUST 被检出

- **WHEN** 某个 sample 发生段错误 / 异常终止
- **THEN** 护栏 MUST 把它记为失败（不是「通过」也不是「跳过」）

### Requirement: 禁用清单 MUST 可收缩，被修复的 sample MUST 移出清单

`samples/disabled_samples.txt`（禁用清单）里的每一项 MUST 带**理由**与
**归位条件**。当某个 sample 的根因被修复（它**未经修改即可编译**，或**经 API
漂移修复后编译通过**）时：

- 该 sample MUST 被补进 `samples/CMakeLists.txt` 的 `CH_SAMPLE_TARGETS`
  （⇒ 从此被护栏**执行**）；
- 该行 MUST 从禁用清单中**移除**；
- 移除后 MUST 全量重跑护栏。

⇒ 禁用清单 MUST 是一个**收敛**的集合，MUST NOT 成为「永久排除名单」。

> ℹ️ 与「全部 sample MUST 被纳入自动化回归护栏的覆盖集合」互为补充：
> 后者保证「没有 sample 无归属」，本条保证「无归属是**暂时的**」。

#### Scenario: 修好的 sample MUST 移出禁用清单并被注册

- **WHEN** 某个被禁用的 sample 未经修改即可编译，或经 API 漂移修复后编译通过
- **THEN** 它 MUST 被补进 `CH_SAMPLE_TARGETS`
- **AND** 它 MUST 从禁用清单里移除
- **AND** 护栏 MUST 全量重跑并保持绿
- **AND** ⚠️ **「经 API 漂移修复后编译通过」是本 change 的主路径**
  （改 sample 源码）—— 只覆盖「未经修改即可编译」会让该路径**不受契约约束**

#### Scenario: 编译通过但**运行**失败的 sample MUST NOT 移出清单

- **WHEN** 某个 sample 编译通过、但运行护栏报告它失败
- **THEN** 它 MUST **留在**禁用清单里
- **AND** 它的归位条件 MUST 指向一个**具体的运行期缺陷**
  （MUST NOT 移出清单 —— 移出后护栏会因它运行失败而红）

#### Scenario: 禁用清单 MUST 记录归位条件

- **WHEN** reviewer 检查禁用清单
- **THEN** 每一项 MUST 带有**为什么不能编译**的理由
- **AND** 每一项 MUST 带有**归位条件**（什么时候回来）
- **AND** 需要**框架层**新能力才能编译的 sample MUST 把归位条件写成对那个
  具体框架变更的依赖

#### Scenario: 清单收缩 MUST 与护栏守恒一致

- **WHEN** 禁用清单变短（sample 被移出）
- **THEN** 已注册数 MUST 相应增加
- **AND** 护栏的「实际执行数 == 已注册数」断言 MUST 仍成立
