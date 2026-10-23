# Capability: example-runtime-coverage

## ADDED Requirements

### Requirement: example 内的 Catch2 测试 target MUST 注册进 CTest 体系

`examples/**` 下注册的 Catch2 测试 target MUST 通过 `add_test` 注册进 CTest，
使 `ctest` MUST 能执行它们。

这些 target MUST NOT 以「example 运行护栏」（`run_all_ported_tests.sh`）代替 CTest
注册 —— 它们是**测试**而不是 demo；护栏的语义是「跑 main() 程序并检查退出码」，
不提供 CTest 的 per-case 报告与标签体系。

注册进 CTest 的测试 MUST 以**退出码 0** 结束。若某个 target 因**既有缺陷**失败，
该失败 MUST 被**显式记录**（带根因与归位条件），MUST NOT 通过「跳过它」让护栏变绿。

依赖**仓外**资源（例如外部测试套件的预编译二进制）的测试用例，
在该资源缺失时 MUST **显式 SKIP**，MUST NOT 以失败告终；
SKIP MUST 在 CTest 输出中可见，MUST NOT 是静默跳过。

每个 `examples/**/tests/test_*.cpp` 源文件 MUST 满足以下二者之一：
① 已被注册进 CTest；② 登记在 `examples/disabled_examples.txt`（带理由 + 归位条件）。
MUST NOT 处于「既未注册、也没有登记」的静默状态 —— 那会让测试源文件
「编译进 target 却从不被执行」，与本 capability 要消除的缺口同型。

#### Scenario: example 内的测试 target MUST 被 ctest 执行

- **WHEN** reviewer 检查 `examples/**` 下注册的 Catch2 测试 target
- **THEN** 每个 target MUST 有对应的 `add_test` 注册
- **AND** `ctest -N` MUST 列出该 target
- **AND** `ctest` MUST 实际执行它并检查退出码

#### Scenario: 测试 target MUST NOT 用 example 运行护栏代替 CTest 注册

- **WHEN** reviewer 检查某个 example 内的 Catch2 测试 target 的覆盖方式
- **THEN** 它 MUST 通过 `add_test` 注册进 CTest
- **AND** MUST NOT 仅依赖 `run_all_ported_tests.sh` 的跟踪集合来获得运行覆盖

#### Scenario: 依赖仓外资源的用例 MUST 条件跳过

- **WHEN** 某个测试用例依赖**仓外**预编译资源（如外部测试套件的二进制）且该资源不存在
- **THEN** 该用例 MUST 显式 SKIP
- **AND** MUST NOT 因此失败
- **AND** SKIP MUST 在 CTest 输出中可见

#### Scenario: 既有失败 MUST 显式登记

- **WHEN** 引入 CTest 注册时发现某个 example 内的测试 target 既有失败
- **THEN** MUST 逐个定性其根因
- **AND** MUST 修复它，或把它登记在 `examples/disabled_examples.txt`（附根因与归位条件）
- **AND** MUST NOT 用「跳过它」的方式让 CTest 变绿

#### Scenario: 测试源文件 MUST 有归属

- **WHEN** `examples/**/tests/` 下存在 `test_*.cpp` 源文件
- **THEN** 它 MUST 被注册进 CTest，或登记在 `examples/disabled_examples.txt`（带理由 + 归位条件）
- **AND** MUST NOT 处于「两者皆非」的静默状态
