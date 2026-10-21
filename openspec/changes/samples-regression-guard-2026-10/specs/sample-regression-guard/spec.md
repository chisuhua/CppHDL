# Capability: sample-regression-guard

## ADDED Requirements

### Requirement: 全部 sample MUST 有自动化回归护栏

`samples/` 下的**每一个**可执行 sample MUST 由一条**自动化**路径执行并检查退出码。
MUST NOT 存在「只能靠人手工跑」的 sample。

#### Scenario: 存在覆盖全部 sample 的执行脚本

- **WHEN** reviewer 检查仓库的自动化覆盖
- **THEN** MUST 存在一个脚本（如 `scripts/run_all_samples.sh`）遍历**全部** sample
- **AND** 该脚本 MUST 对**每个** sample 检查退出码
- **AND** 任一 sample 非零退出 MUST 使该脚本失败

#### Scenario: 脚本 MUST 有超时保护

- **WHEN** 某个 sample 挂死（不退出）
- **THEN** 脚本 MUST 在**有限时间**内终止该 sample 并将其记为失败
- **AND** MUST NOT 让整个 CI job 无限期挂起

#### Scenario: 护栏 MUST 在 CI 中执行

- **WHEN** reviewer 检查 CI 配置
- **THEN** MUST 有一个 CI job 执行该脚本
- **AND** 该 job 的失败 MUST 阻断合并

### Requirement: sample MUST 以零退出码结束

每个 sample MUST 以**退出码 0** 结束。

若某个 sample 因**既有缺陷**失败，则该失败 MUST 被**显式记录**（缺陷单 / 本
capability 的豁免清单）并 MUST NOT 被静默跳过。

#### Scenario: 失败 MUST 显式记录

- **WHEN** 引入护栏时发现既有失败的 sample
- **THEN** MUST 逐个定性其根因
- **AND** MUST 修复它，或在豁免清单中**显式**记录（附根因与关联的缺陷单）
- **AND** MUST NOT 用「跳过它」的方式让护栏变绿

#### Scenario: 崩溃 MUST 被检出

- **WHEN** 某个 sample 发生段错误 / 异常终止
- **THEN** 护栏 MUST 把它记为失败（不是「通过」也不是「跳过」）
