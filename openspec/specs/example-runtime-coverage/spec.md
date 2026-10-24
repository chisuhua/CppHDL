# example-runtime-coverage Specification

## Purpose
`examples/` 下的 **32 个 `main()` 程序**（其中 28 个被 `run_all_ported_tests.sh`
跟踪）在 CI 里**只有编译覆盖**、**没有运行覆盖** ⇒ example 的**运行时崩溃**与
**行为退化**只能靠人偶然发现。

⚠️ 另有一类**更严重**的缺口（**2026-10-24 已闭合**）：`examples/riscv-mini/`
注册的 **19 个 Catch2 测试 target** —— 它们**每次 CI 都在编译**，但
`grep -rn "add_test" examples/` = 0 ⇒ `ctest` **从不认它们** ⇒ **零运行覆盖**
（实测 2026-10-22）。根因是根 `CMakeLists.txt` 的 `enable_testing()` 排在
`add_subdirectory(examples)` **之后** ⇒ examples 子目录的 `add_test()` 是**空操作**
（`CTestTestfile.cmake` 根本不生成）。

闭合结果（change `riscv-mini-test-triage-2026-10`）：19 个里 **14 个进 CTest**
（12 个原 target + 2 个修好的高价值 orphan）、**1 个只构建不注册**
（`test_riscv_tests_pipeline` —— 全部用例依赖仓外 riscv-tests ⇒ Catch2 退出码 4
⇒ CTest 会判红）、**6 个删除**（删除留痕在 `examples/removed_examples.txt`，
另含 2 个从未进 CMakeLists 的孤儿源文件）。CTest 总数 185 → 199。

本 capability 规定 examples 的**运行护栏契约**：被跟踪的 example MUST 被一条
自动化路径执行并检查退出码；护栏 MUST 有超时保护；护栏 MUST NOT 因单个
example 缺失而**中止整轮**（那会让受护集合静默缩面）；实际执行数 MUST 等于
跟踪数；既有失败 MUST 被**显式**记录且豁免 MUST **可失效、不静默**；
example 内的 Catch2 测试 target MUST **注册进 CTest**（并实际生效），
其测试源文件 MUST 有**归属**且总数 MUST 与记录式锚点一致。

> ℹ️ 本 capability 是 `sample-regression-guard`（samples 侧）的**镜像** ——
> 两者是同一类缺口的两个实例，故用同一套契约措辞，便于对读。

## Requirements

### Requirement: 被跟踪的 example MUST 纳入自动化运行护栏

`run_all_ported_tests.sh` 跟踪的**每一个** example MUST 在 CI 中被执行，
且 MUST 以**退出码 0** 结束。

护栏 MUST 对**每个** example 检查退出码；任一 example 非零退出 MUST 使护栏
失败。

#### Scenario: example 在 CI 中被执行

- **WHEN** reviewer 检查 CI 配置
- **THEN** MUST 有一个 CI job 执行该脚本
- **AND** 该 job MUST 在 PR 上运行
- **AND** 该 job 失败 MUST 使该 workflow 失败
- **AND** 该 job MUST 设定**超时预算**（job 级 `timeout-minutes`）

#### Scenario: 脚本 MUST 有超时保护

- **WHEN** 某个 example 挂死（不退出）
- **THEN** 脚本 MUST 在**有限时间**内终止它并将其记为失败
- **AND** MUST NOT 让整个 CI job 无限期挂起
- **AND** 超时（退出码 124）与崩溃（退出码 139）MUST 在汇总中**可区分**

#### Scenario: 超时工具 MUST 可移植

- **WHEN** 护栏脚本在 macOS 上运行
- **THEN** MUST 使用可用的超时命令（`timeout` 或 `gtimeout`）
- **AND** MUST NOT 因缺少 GNU `timeout` 而 command-not-found

#### Scenario: 缺失二进制 MUST 判失败

- **WHEN** 被跟踪的 example 的二进制不存在（例如构建失败）
- **THEN** 护栏 MUST 把它记为**失败**
- **AND** MUST NOT 记为 SKIP —— SKIP 语义会让 example **静默退出**受护集合
  （与 samples 护栏的 C3 同型）

#### Scenario: 缺失二进制 MUST NOT 中止整轮运行

- **WHEN** 某个被跟踪的 example 的二进制不存在
- **THEN** 护栏 MUST **继续执行**其余被跟踪的 example
- **AND** MUST NOT 在该点**中止**（中止 ⇒ 后续 example 从不执行 ⇒ 受护集合
  **静默缩面** ⇒ 比 SKIP 更坏）
- **AND** MUST 在汇总中给出**失败计数**与**缺失清单**

#### Scenario: 实际执行数 MUST 等于跟踪数

- **WHEN** 护栏跑完一轮
- **THEN** 实际执行的 example 数 MUST 等于被跟踪的 example 数
- **AND** 不等时 MUST 判**失败**（对齐 samples 护栏的 C2 数量守恒）

#### Scenario: 被跟踪集合 MUST 有**单一来源**

- **WHEN** reviewer 检查「哪些 example 被跟踪」
- **THEN** 该集合 MUST 由**一处**定义，且 MUST 同时驱动构建侧的聚合 target
  与运行侧的护栏
- **AND** MUST NOT 存在「构建侧有、运行侧无」的新增即漏缺口

#### Scenario: 未被跟踪的程序 MUST 有归属

- **WHEN** `examples/` 下存在 main() 程序或已注册的测试二进制，但它们**不在**
  被跟踪集合内
- **THEN** 它们 MUST 被**显式登记**（带理由与归位条件）
- **AND** MUST NOT 处于「既不被跟踪、也没有归属」的静默状态

### Requirement: example 的既有失败 MUST 被显式记录，豁免 MUST 可失效

若某个 example 因**既有缺陷**失败，则该失败 MUST 被**显式记录**并 MUST NOT
被静默跳过。

豁免 MUST 是**数据驱动**的（每项含 example 名、根因、关联缺陷单 / change、
**失效条件**），MUST 在运行时**逐条打印**（豁免 MUST NOT 是静默的），
且 MUST NOT 成为**永久免检**：

- 被豁免的 example **仍然被执行**（作为 stale 探针）；
- 豁免**只影响失败计数**：被豁免项的失败 MUST NOT 计入失败数；
- 一旦被豁免的 example **转绿**（退出码 0），护栏 MUST 把它报为
  **stale 豁免**并失败。

#### Scenario: 失败 MUST 显式记录

- **WHEN** 引入护栏时发现既有失败的 example
- **THEN** MUST 逐个定性其根因
- **AND** MUST 修复它，或在豁免清单中**显式**记录（附根因与关联的缺陷单）
- **AND** MUST NOT 用「跳过它」的方式让护栏变绿

#### Scenario: 豁免项 MUST 仍然被执行

- **WHEN** 某个 example 处于生效中的豁免状态
- **THEN** 护栏 MUST **仍然执行**它
- **AND** 它的失败 MUST NOT 计入失败数
- **AND** MUST 在输出中逐条打印该豁免（example 名 + 根因 + 关联 change）

#### Scenario: stale 豁免 MUST 使护栏失败

- **WHEN** 某个被豁免的 example 已经转绿（退出码 0）
- **THEN** 护栏 MUST 把它报为 **stale 豁免**
- **AND** 护栏 MUST **失败**（MUST NOT 因为「反正豁免了」而放过）

#### Scenario: 豁免 MUST 有失效条件与摘除责任

- **WHEN** 豁免项的根因被修复（关联 change 落地）
- **THEN** 该豁免 MUST 被移除
- **AND** 移除责任 MUST 落在**根因所属的 change** 的任务里
- **AND** 移除后 MUST 全量重跑护栏

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

#### Scenario: 测试源文件总数 MUST 有记录式锚点

- **WHEN** 仓库维护 `examples/**` 下的测试源文件
- **THEN** 其**总数** MUST 与 root `AGENTS.md` 的 `EXAMPLES_TEST_SOURCES` 锚点一致
- **AND** 该锚点 MUST 被自动化护栏（`scripts/verify_example_test_wiring.sh` 的 C2）校验
- **AND** 任何增减 MUST 显式更新锚点 —— 这防的是「源文件与其 CMake 声明
  **一起**被删除」的静默缩面（只比对「声明集 vs 覆盖集」抓不到：两边同减 ⇒ 守恒恒等）

#### Scenario: 测试源文件 MUST 被编译
- **WHEN** `examples/**/tests/` 下存在 `test_*.cpp` 源文件
- **THEN** 它 MUST 被某个已注册的 target 编译
- **AND** MUST NOT 处于「从未被任何 TU 编译」的静默腐坏状态

#### Scenario: 测试源文件 MUST 有归属

- **WHEN** `examples/**/tests/` 下存在 `test_*.cpp` 源文件
- **THEN** 它 MUST 被注册进 CTest，或登记在 `examples/disabled_examples.txt`（带理由 + 归位条件）
- **AND** MUST NOT 处于「两者皆非」的静默状态
