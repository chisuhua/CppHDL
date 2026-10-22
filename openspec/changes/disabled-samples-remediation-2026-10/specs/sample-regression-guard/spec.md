# Capability: sample-regression-guard

## ADDED Requirements

### Requirement: 禁用清单 MUST 可收缩，被修复的 sample MUST 移出清单

`samples/disabled_samples.txt`（禁用清单）里的每一项 MUST 带**理由**与
**归位条件**。当某个 sample 的根因被修复（它**未经修改即可编译**）时：

- 该 sample MUST 被补进 `samples/CMakeLists.txt` 的 `CH_SAMPLE_TARGETS`
  （⇒ 从此被护栏**执行**）；
- 该行 MUST 从禁用清单中**移除**；
- 移除后 MUST 全量重跑护栏。

⇒ 禁用清单 MUST 是一个**收敛**的集合，MUST NOT 成为「永久排除名单」。

> ℹ️ 这条 Requirement 与「全部 sample MUST 被纳入自动化回归护栏的覆盖集合」
> 互为补充：后者保证「没有 sample 无归属」，本条保证「无归属是**暂时的**」。

#### Scenario: 修好的 sample MUST 移出禁用清单并被注册

- **WHEN** 某个被禁用的 sample 未经修改即可编译
- **THEN** 它 MUST 被补进 `CH_SAMPLE_TARGETS`
- **AND** 它 MUST 从禁用清单里移除
- **AND** 护栏 MUST 全量重跑并保持绿

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
