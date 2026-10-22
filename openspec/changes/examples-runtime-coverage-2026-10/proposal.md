# Change: examples-runtime-coverage-2026-10 — examples 的**运行**覆盖进 CI

- Status: **草案（尚未实施）**
- 立项自 2026-10-22 的 `samples-regression-guard-2026-10`（其 Non-Goal 4）
- 关联 capability：**新增** `example-runtime-coverage`（本 change **新建**该
  capability，delta 已起草于 `specs/example-runtime-coverage/spec.md`）；
  `sample-regression-guard`（samples 侧）本次**不改**

## Why

`samples/` 的运行覆盖已由 `samples-regression-guard-2026-10` 落地
（`scripts/run_all_samples.sh` + CI job `samples-regression`）。

但**同一类缺口在 `examples/` 上依然存在**，且它是 `samples-regression-guard`
的 proposal 里**显式记为 Non-Goal 4** 的那一项：

| 事实 | 取证 |
|---|---|
| `examples/` 的**编译**覆盖在 CI 里 | `ci.yml` 的 `build-and-test` job 跑 `cmake --build build`（含全部 example target） |
| `examples/` 的**运行**覆盖**不在** CI 里 | `grep -rn "run_all_ported" .github/workflows/` ⇒ **0 命中** |
| 本地有运行覆盖 | `run_all_ported_tests.sh` 覆盖 **28** 个 main()-bearing example |
| example 数量 | `grep -rl '^int main' examples --include='*.cpp' \| wc -l` = **32**（另 4 个是 demo，不由脚本跟踪） |
| ⚠️ **另有 19 个 Catch2 测试二进制从不执行** | `examples/riscv-mini/CMakeLists.txt` 的 19 个 `riscv_mini_catch_test` target 是已注册 target（每次 CI 都编译），但 `grep -rn "add_test" examples/` = **0** ⇒ `ctest -N` **0 命中** ⇒ **从不执行** |

⇒ 与 `samples/` 同样的形态：**example 坏了，CI 不知道**（只有编译错误会被发现；
运行时崩溃 / 行为退化**无人知晓**）。

> ℹ️ 本次审计（2026-10-22）已在 `samples/` 上抓到 **2 个既有 segfault**；
> `examples/` 的 28 个程序**从未**被 CI 运行过。
> ⚠️ **2026-10-22 实测更正**：在 pristine HEAD 上跑
> `./run_all_ported_tests.sh` ⇒ **28/28 PASS、0 FAIL、0 SKIP、exit=0**
> ⇒ 「那里极可能也藏着同类问题」这一推断**在 examples 侧不成立**（基线全绿）。
> ⇒ 但**缺口本身依然成立且更严重**：① 基线全绿说明 examples 今天**恰好**没坏，
> 护栏的作用是**守住**这个状态而非**修复**既有失败；② 那 19 个 Catch2 二进制
> **零运行覆盖**（比 4 个 demo 严重一个量级，见 D5）；
> ③ 护栏自身的**静默缩面**缺陷（`set -e` 中止，见 D2）才是 examples 侧
> 今天**真实存在**的问题。
> ⇒ 落地时仍 MUST **复核**基线（框架已变 ⇒ 基线可能已变）。

## What Changes

### D1: 新增 CI job 跑 `run_all_ported_tests.sh`

- **Linux-only**（`ubuntu-22.04`）—— 脚本用 `timeout 30`（GNU coreutils）；
  macOS 无 `timeout`（只有 `gtimeout`）⇒ 要么把脚本改成 `timeout`/`gtimeout`
  回退（与 `scripts/run_all_samples.sh` 的既有做法一致），要么该 job 只挂 Linux。
  **裁决**：**两个都做** —— 脚本加回退（与 samples 护栏一致）+ job 挂 Linux
  （保守：macOS runner 的 example 行为与 Linux 可能有差异，先只在 Linux 上守）。
- MUST 设 job 级 `timeout-minutes`（28 × 30s ≈ 14 min 上界 + 构建时间）。
  **裁决**：**30**（对齐既有 `samples-regression` job 的 `ci.yml:388`）——
  14 min 只是「全部挂死」的**最坏**上界，还要叠加构建；20 min 在冷 ccache 下偏紧。
- ⚠️ 该 job MUST 在**构建之后**跑（example 二进制由 `build-and-test` job 构建；
  新 job 需要自己构建或复用 artifact ⇒ **裁决**：新 job 自己构建，避免跨 job
  的 artifact 依赖）。
- ⚠️ **新 job MUST 镜像 `samples-regression` job 的全套工具链步骤**
  （`.github/workflows/ci.yml:399-437`）—— 否则裸 runner 上 **configure 即失败**：
  根 `CMakeLists.txt:13` 的 `CH_JIT_ENABLE` 默认 **ON** ⇒ `:73` 的
  `find_package(LLVM REQUIRED)` 在没有 LLVM 的 runner 上直接失败
  （`:102` 还有一道 `FATAL_ERROR` 兜底）⇒ MUST 装 LLVM（APT 的
  `llvm-${LLVM_VERSION}-dev` + `clang-`）+ g++-13（PPA 失败则回退 g++-12）
  + `BUILD_VERILATOR=OFF`。
- ⚠️ **失败可诊断**：现脚本 `run_all_ported_tests.sh:56` 把每个 example 的
  stdout/stderr **全部丢进 `/dev/null`** ⇒ CI 里失败只剩一个退出码 ⇒
  新 job MUST 把逐例日志落盘并在失败时上传 artifact（对齐 samples 侧护栏）。

> ⚠️ **2026-10-22 实测更正**：**没有** `all-examples` 这样的聚合 target ——
> 每个 example 是独立 target，由根 `CMakeLists.txt` 的 **21** 个
> `add_subdirectory(examples/...)` 引入（`CMakeLists.txt:252-280`）⇒
> `cmake --build build --target <examples>`
> **无法执行**。
> ⇒ **裁决**：新增聚合 target `all-examples`（**镜像** samples 侧既有的
> `all-samples`，`samples/CMakeLists.txt:69-70`）⇒ 新 job 跑
> `cmake --build build --target all-examples`。
> ⚠️ 该 target MUST **显式列出**被脚本跟踪的 28 个（见 D4），MUST NOT 整目录收编。
>
> ✅ **已实测（2026-10-22）**：被脚本跟踪的 **28 个** example **全部**是已注册的
> CMake target、**全部**会被默认构建（`build/` 下 28/28 二进制存在；
> `examples/**/CMakeLists.txt` 无 `EXCLUDE_FROM_ALL`）⇒ D2 的「缺失二进制 = 失败」
> **不会**让 CI 在第一天就红。

### D2: 「缺失二进制」的真实行为 MUST 先纠正 —— 它不是 SKIP，是**静默中止**

⚠️ **2026-10-22 实测更正（推翻本提案 D2 的原有描述）**：
`run_all_ported_tests.sh:14` 是 `set -euo pipefail`，而 `run_test()` 在
「二进制缺失」时 `:53` **`return 2`** ⇒ 该 `run_test` 是**顶层简单命令**
（`:72` 起）⇒ 在 `set -e` 下**脚本在第一个缺失二进制处直接中止**（exit=2），
后续 example **从不执行**，末尾汇总（`:121-122`）**永远走不到**。

⇒ 真实行为**不是**「记 SKIPPED 并继续」，而是「**静默中止 + 报告被截断**」——
这比 SKIP 更坏（SKIP 至少会出现在汇总里）。已用等价 bash 片段实测复现：
函数 `return 2` ⇒ 后续语句不执行、exit=2。

> ⚠️ 连带：`scripts/run_all_samples.sh:10-13` 的注释写着
> 「The guard is deliberately stronger than `run_all_ported_tests.sh`,
> which SKIPs a missing binary」—— **这句注释本身也是错的** ⇒ 本次 MUST 一并订正。

**裁决**（本次改的**不是** SKIP 的字样，而是**控制流**）：

1. 缺失二进制分支 MUST 与「正常失败」分支**同构**：记 `FAIL`、`FAIL_NAMES+=`、
   **返回 0**（**MUST NOT** 返回非零 —— 那正是中止的根因）；
2. MUST 引入 **EXECUTED 计数**并加一条 **C2 式数量守恒**
   （`EXECUTED == 跟踪数`）⇒ 对齐 `scripts/run_all_samples.sh:259-266` 的既有形态；
   **否则**即使修好了中止，受护集合仍可能静默缩面；
3. 汇总行的 `SKIP` 计数随之恒为 0 ⇒ MUST 移除或改义（不得留下一个恒 0 的
   「看起来像有跳过」的槽位）。

⚠️ **边界（本地开发者）**：`run_all_ported_tests.sh` 同时是仓库的
**验收门**（根 `AGENTS.md:183` 的 PHASE COMPLETION GATE 表格 + `:192` 的
COMMANDS 段）**和** `openspec/config.yaml:50` 的「测试验证命令: Examples」。
⇒ 改成「缺失即失败」后，**只做了增量构建**的本地开发者会直接看到失败 ⇒
MUST 给出**明确的报错文案**（指名缺哪个二进制、提示「MUST 先完整构建」），
并同步更新这 **5 处**引用（`openspec/config.yaml:50` / `AGENTS.md:183` / `:192` /
`:228` / `:232`）—— ZERO-DEBT：不留孤儿引用。

### D3: 基线先行（**基线已实测**）

⚠️ **2026-10-22 实测**：在 pristine HEAD 上跑 `./run_all_ported_tests.sh` ⇒
**28/28 PASS、0 FAIL、0 SKIP、exit=0**（逐例输出已核对）。

⇒ **本提案原有的推断「落地时很可能又抓到一批既有失败（samples 侧就抓到
2 个 segfault）」在 examples 侧【不成立】** —— 基线是**全绿**的。

⇒ 由此产生两个后果：
- Task 0 的「既有失败逐个定性」**预期得到一个空清单** ⇒ 豁免机制**当场没有
  探针、也没有使用者**。豁免机制仍 MUST 落地（否则未来第一次失败会让 CI
  直接红、没人敢修），但它的**验收方式 MUST 换掉**：不能用「已知失败项」做
  探针（不存在），MUST 用**人为变异**（临时移走一个二进制）做探针。
- **真实风险不是「既有失败」，而是「护栏自身静默缩面」**（D2 的 `set -e` 中止
  + 无数量守恒）⇒ 风险表 MUST 改写（见 Impact 表）。

⇒ 落地时 MUST **复核**该基线（框架已变 ⇒ 基线可能已变），并把逐例退出码与
耗时记进 `docs/reports/verification-examples-runtime-coverage-2026-10.md`。

### D4: 跟踪集合 MUST **单一来源**（否则新增即漏）

现状是**四份清单**，彼此可能漂移：

| 清单 | 位置 | 数量 |
|---|---|---|
| 脚本里的 `run_test` 行 | `run_all_ported_tests.sh:72-117` | 28 |
| CMake 已注册 target | 20+ 个 `examples/**/CMakeLists.txt` | 28 + 3 demo + 19 Catch2 |
| `main()` 源文件 | `grep -rl '^int main' examples --include='*.cpp'` | 32 |
| **本次新增**的 `all-examples` 聚合 target | 根 `CMakeLists.txt` | 28 |

⚠️ 漂移方向不对称：**脚本 → CMake**（脚本里有、CMake 里没有）能被
「缺失二进制 = 失败」抓到；**CMake → 脚本**（新 example 注册进 CMake、
忘了加进脚本）**永远静默** ⇒ 这正是本 change 要治的病在护栏自身上的复现。

**裁决**：
- 28 个跟踪 target 的名字 MUST 落在**一处**（建议
  `examples/CMakeLists.txt` 的 `set(CH_TRACKED_EXAMPLE_TARGETS …)`），
  由它**同时**驱动 `all-examples` 聚合 target **和**护栏脚本
  （samples 侧已有现成范式：`scripts/run_all_samples.sh:95-99` 的
  `registered_samples()` 就是解析 `CH_SAMPLE_TARGETS`）；
- ⇒ Non-Goal 3 的措辞 MUST 改：「不引入**完整注册清单**」成立，
  但「引入**跟踪清单**（单源，不是注册清单机制）」MUST 落地；
- ⚠️ `all-examples` MUST **显式列出**这 28 个，MUST NOT 整目录收编 ——
  `examples/riscv-mini/CMakeLists.txt` 还注册了 **19 个 Catch2 测试 target**
  （见 D5），整目录收编会把它们误纳入 example 护栏；
- ⚠️ `all-examples` MUST 定义在根 `CMakeLists.txt` 的
  `if(CPPHDL_BUILD_EXAMPLES)` 块**内**（该 option 可关 ⇒ 块外定义会让
  `add_dependencies` 指向不存在的 target ⇒ configure 失败）。

### D5: 未被跟踪的 main() 程序 MUST 有**归属**（新增即漏的同型）

| 类别 | 数量 | 现状 |
|---|---|---|
| verilator demo（`verilator_01_minimal` / `02_counter` / `03_component`） | 3 | **是**已注册 target（默认构建），但**不被脚本跟踪** |
| `examples/spinalhdl-ported/testbench_template.cpp` | 1 | **未**注册为 target |
| **`examples/riscv-mini/` 的 Catch2 测试 target** | **19** | **是**已注册 target（每次 CI 都编译），但 `grep -rn "add_test" examples/` = **0** ⇒ `ctest -N` **0 命中** ⇒ **从不执行** |

⚠️ **第三类是本次审计发现的、比 4 个 demo 严重一个量级的缺口**：那 19 个是
**测试**而不是 demo，它们**每次 CI 都在编译、却从未被执行过**（实测二进制
存在于 `build/examples/riscv-mini/`，但 ctest 不认它们）。

**裁决**：本次 MUST 至少把第三类**显式定性**——要么给它们 `add_test`
（最省事，直接进 CTest 体系，且它们本就是 Catch2 测试），要么明确登记为
「examples 内的测试，暂不进护栏」并写归位条件。
⚠️ **不得**默认忽略：那会让「examples 有运行覆盖」这句话**名不副实**
（护栏只覆盖 28/32 个 main() 程序，而 19 个测试二进制**零运行覆盖**）。

## Impact

| 项 | 影响 |
|---|---|
| `.github/workflows/ci.yml` | 新增 1 个 **Linux-only** job（**含 LLVM + g++-13 工具链步骤**，见 D1） |
| `run_all_ported_tests.sh` | 「缺失二进制」由**静默中止**改为**失败且继续跑**（D2）；加 `EXECUTED` 数量守恒；加 `timeout`/`gtimeout` 回退；逐例日志不再丢进 `/dev/null` |
| `examples/CMakeLists.txt`（新） | 跟踪 target 的**单源清单** `CH_TRACKED_EXAMPLE_TARGETS` |
| 根 `CMakeLists.txt` | 新增 `all-examples` 聚合 target（**显式列 28 个**，定义在 `if(CPPHDL_BUILD_EXAMPLES)` 块**内**） |
| `examples/disabled_examples.txt`（新） | 登记 4 个未被跟踪的 main() 程序 + 19 个从不执行的 Catch2 二进制（**含理由 + 归位条件**） |
| `openspec/specs/example-runtime-coverage/spec.md` | **新增**（本 change **新增 capability** `example-runtime-coverage` —— delta 已起草，见 `specs/example-runtime-coverage/spec.md`） |
| `scripts/run_all_samples.sh:10-13` | 订正那句**错误**的注释（说 `run_all_ported_tests.sh` 会 SKIP 缺失二进制 —— 实为静默中止） |
| `docs/reports/verification-examples-runtime-coverage-2026-10.md` | **新增**（基线清单 + 变异/豁免结果） |
| `openspec/config.yaml:50` + 根 `AGENTS.md`（`:183` / `:192` / `:228` / `:232`） | 同步验收门描述（5 处，ZERO-DEBT 不留孤儿引用） |
| 计数 | **CTEST_TOTAL 不变**（examples 不是 ctest target）；⚠️ 若 D5 裁决给那 19 个 Catch2 target 加 `add_test` ⇒ CTEST_TOTAL **会变** ⇒ MUST 同步 `scripts/verify_test_counts.sh` |
| 风险 | 🟡 中。① **实测基线 28/28 全绿** ⇒ 无既有失败要修（真实风险是「护栏自身静默缩面」）；② 脚本语义改动（缺失中止 → 失败且继续）**会改变本地验收门的行为**（增量构建者会看到失败 ⇒ MUST 给明确报错文案）；③ D5 的 19 个 Catch2 二进制若裁决加 `add_test` ⇒ 影响 CTEST_TOTAL 口径 |

## Non-Goals

1. **不**把 examples 改写成 ctest target（与 samples 侧同款裁决）
2. **不**给 example 加行为断言（example 的职责是「演示」；行为断言属于 `tests/`）
3. **不**引入 examples 的**完整注册清单机制**（= samples 侧 `disabled_samples.txt`
   那套「每个源文件要么已注册要么登记」的形态）—— 但 **D4 的跟踪清单（单源）
   MUST 落地**，两者不是一回事
4. **不**处理 macOS runner（保守起见只守 Linux）—— 但脚本的 `timeout`/`gtimeout`
   回退 MUST 落地（本地 macOS 开发者也在用这个验收门）
5. **不**在本 change 内修 example 的行为缺陷（基线实测全绿 ⇒ 没有既有缺陷；
   若落地时基线变了 ⇒ 按 Task 0 的流程定性，**MUST NOT** 静默豁免）

## 落地顺序

MUST 在 `samples-regression-guard-2026-10` **之后**落地（两者都改
`run_all_*.sh` 与 CI；串行更安全）。

⚠️ **本 change 与 `disabled-samples-remediation-2026-10` 的关系**：
两者都改运行护栏与 CI，但文件面**不重叠**（本 change 碰 `examples/` +
`run_all_ported_tests.sh` + `ci.yml`；那个碰 `samples/` +
`scripts/run_all_samples.sh`）⇒ 可**并行开发**，但 MUST **串行合并**
（都写根 `AGENTS.md` + `ci.yml` ⇒ 并行合并必冲突）。
