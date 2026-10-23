# Capability: example-runtime-coverage

## ADDED Requirements

### Requirement: example 内的 Catch2 测试 target MUST 注册进 CTest 体系

`examples/**` 下的 Catch2 测试 target MUST 通过 `add_test` 注册进 CTest，
且该注册 MUST 实际生效（`ctest -N` 列出它、`ctest` 执行它）。

这些 target MUST NOT 以「example 运行护栏」代替 CTest 注册 —— 它们是**测试**
而不是 demo。

注册进 CTest 的测试 MUST 以**退出码 0** 结束，且运行日志 MUST NOT 含
error/fatal 级条目（「跑但报错」≠ 通过）；既有例外 MUST 走**显式 allowlist**
（带根因 + 归位条件 + stale 检查）。

既有失败 MUST 被**显式记录**（带根因与归位条件），MUST NOT 通过「跳过它」
让 CTest 变绿。

每个 example 测试源文件 MUST 满足二者之一：① 被某个**已进 CTest** 的 target
编译；② 登记在 `examples/disabled_examples.txt`（带理由 + 归位条件）。

#### Scenario: example 内的测试 target MUST 被 ctest 执行

- **WHEN** reviewer 检查 `examples/**` 下注册的 Catch2 测试 target
- **THEN** 每个 target MUST 有对应的 `add_test` 注册
- **AND** `ctest -N` MUST 列出该 target
- **AND** `ctest` MUST 实际执行它并检查退出码

#### Scenario: 测试 target MUST NOT 用 example 运行护栏代替 CTest 注册

- **WHEN** reviewer 检查某个 example 内的 Catch2 测试 target 的覆盖方式
- **THEN** 它 MUST 通过 `add_test` 注册进 CTest
- **AND** MUST NOT 仅依赖 `run_all_ported_tests.sh` 的跟踪集合来获得运行覆盖

#### Scenario: 注册 MUST 实际生效（不得是空操作）

- **WHEN** reviewer 检查某个 example 内的测试 target 的 CTest 注册
- **THEN** `ctest -N` MUST 列出该 target
- **AND** `ctest` MUST 实际执行它
- **AND** MUST NOT 仅在 CMake 里写了 `add_test()` 而该调用**静默无效**
  （例如 CMake 的 `enable_testing()` 排在注册 `add_test()` 的子目录**之后**
  ⇒ 该子目录不生成 `CTestTestfile.cmake` ⇒ `add_test()` 是空操作）

#### Scenario: 依赖仓外资源的用例 MUST 条件跳过

- **WHEN** 某个测试用例依赖**仓外**预编译资源（如外部测试套件的二进制）且该资源不存在
- **THEN** 该用例 MUST 显式 SKIP
- **AND** MUST NOT 因此失败
- **AND** 该测试二进制的 Catch2 汇总行 MUST 报告 skipped 计数
  （`test cases: N | P passed | S skipped`）—— 这是「跳过」的可审计形态
  （CTest 的粒度是二进制，不是用例）

#### Scenario: 全部用例被跳过的二进制 MUST NOT 注册进 CTest

- **WHEN** 某个测试二进制的**全部**用例都因仓外资源缺失而被 SKIP
- **THEN** 该二进制的退出码 MUST NOT 被 CTest 判为失败
  （Catch2 v3 在「没有真正跑起来的用例」时返回 **4**）
- **AND** 它 MUST NOT 注册进 CTest，或 MUST 显式登记为「待归位」

#### Scenario: 既有失败 MUST 显式登记

- **WHEN** 引入 CTest 注册时发现某个 example 内的测试 target 既有失败
- **THEN** MUST 逐个定性其根因
- **AND** MUST 修复它，或把它登记在 `examples/disabled_examples.txt`（附根因与归位条件）
- **AND** MUST NOT 用「跳过它」的方式让 CTest 变绿

#### Scenario: 被注册进 CTest 的测试 MUST 真的断言语义输出

- **WHEN** 一个 examples 内的测试 target 被注册进 CTest
- **THEN** 它的**每个**用例 MUST 至少断言一个**语义确定的输出**
  （`REQUIRE` / `CHECK` / 其 throws / nothrow / STATIC 变体）
- **AND** MUST NOT 仅以 `SUCCEED(...)` 作为用例的全部内容
  （那只能证明「构造没崩」，不能证明行为对 ⇒ 是**账面覆盖**）
- **AND** 该测试源的可执行断言数 MUST 不低于仓库登记的下限锚点
  （机制与 `CTEST_TOTAL` 同型：让**删除/注释掉/条件编译掉**断言成为必须显式
  改锚点的行为）
- **AND** 该约束的**自动化执行**由 capability `example-assertion-quality` 承担
  （C1 只负责把「注册」这件事做对；断言质量的护栏见该 capability）

#### Scenario: 测试源文件 MUST 被编译
- **WHEN** `examples/**/tests/` 下存在 `test_*.cpp` 源文件
- **THEN** 它 MUST 被某个已注册的 target 编译
- **AND** MUST NOT 处于「从未被任何 TU 编译」的静默腐坏状态

#### Scenario: 测试源文件 MUST 有归属

- **WHEN** `examples/**/tests/` 下存在 `test_*.cpp` 源文件
- **THEN** 它 MUST 被注册进 CTest，或登记在 `examples/disabled_examples.txt`（带理由 + 归位条件）
- **AND** MUST NOT 处于「两者皆非」的静默状态
