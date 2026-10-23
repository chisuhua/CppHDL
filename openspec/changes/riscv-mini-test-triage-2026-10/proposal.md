# riscv-mini-test-triage-2026-10

## Why

`examples/riscv-mini/CMakeLists.txt` 注册了 **19 个 Catch2 测试 target**，但
`grep -rn "add_test" examples/` **恒为 0** ⇒ 它们**每次 CI 都在编译、却从未被执行过**
⇒ 这 19 个 target 的运行时崩溃 / 行为退化**无人知晓**。

2026-10-23 实测（构建全部 19 个 target 后逐个运行，`timeout 30`）：

| 结果 | 数量 | target |
|---|---|---|
| exit 0（绿） | 12 | `test_pipeline_exec` / `test_soc_hello_world` / `test_riscv_tests_pipeline` / `test_pipeline_smoke` / `test_pipeline_debug3` / `test_pipeline_debug6` / `test_pipeline_wrapper` / `test_wrapper_simple` / `test_wrapper_ifstage` / `test_wrapper_pipeline` / `test_ifstage_device` / `riscv_mini_cache_smoke` |
| exit 139（SIGSEGV） | 6 | `test_pipeline_minimal` / `test_pipeline_debug` / `test_pipeline_debug4` / `test_pipeline_debug5` / `test_rv32i_pipeline_device` / `test_address_decoder` |
| exit 44（44/49 断言失败） | 1 | `test_riscv_tests` |

**Why now**：
1. 这是 `objective-riscv-mini-convergence` 的第一块（覆盖假象）——「消除技术债」无法绕过。
2. **它是 C2（单源化收敛）的回归护栏前置** —— C2 会大改 include 路径；若这批测试不先变绿
   并注册进 CTest，C2 期间「我改坏了」与「本来就红」无法归因。

### 根因（实测，**与既有预诊断不同**）

预诊断（improvement 草稿 §1）认为 6 个 SIGSEGV 都是「`ch_module` 放 TEST_CASE 顶层」
这一条根因。**实测证明只覆盖了一半**：

| target | 实际根因 | 判定 |
|---|---|---|
| `test_pipeline_minimal` / `test_pipeline_debug` | `ch_module<T>` 放 TEST_CASE 顶层（无 parent）⇒ `ch_module` ctor 早退 ⇒ `io()` 触发 `CHFATAL` | **测试写法问题**，可修 |
| `test_pipeline_debug4` / `test_pipeline_debug5` | 同上 | **测试写法问题**，但用例本身无价值（见下） |
| `test_rv32i_pipeline_device` | `ch_device<Rv32iPipeline>` —— `Rv32iPipeline` 把 `ch_module` 声明为**成员变量**，成员在构造函数期初始化，**早于** `build()` 设置 `Component::current()` ⇒ 永远绑不到 parent | **框架/组件设计缺陷**，前提不可实现 ⇒ 删 |
| `test_address_decoder` | `Simulator::eval()` **无空指针保护**地解引用 `default_clock_instr_`；而 `simulator_init.cpp:152` 在 `sequential_instr_list_` 为空（纯组合设计）时把该指针置 null；`tick()` 有 `if (default_clock_instr_)` 保护，`eval()` 没有 | **框架缺陷**（1 行可修）+ 测试用错了 API |

`test_riscv_tests` 的 44 个失败**不是 RTL 分歧**（草稿 §1 的预诊断有误）：
- **40 个**是 `Failed to load binary: /workspace/project/riscv-tests/build/rv32ui/*.bin`
  —— 依赖**仓外** riscv-tests 套件的预编译二进制，硬编码绝对路径，CI 中永不存在；
- **4 个**是手写用例的**指令编码写错**（实测逐条解码确认）：
  `0x002181B3` 实际是 `add x3, x3, x2`（rs1 应为 x1）、`0x00022023` 实际是 `sw x0, 0(x0)`、
  `0x00020103` 实际是 `lb x2, 4(x0)`、`0x00408263` 的分支偏移是 +4 而非 +8、
  `0x00100113` 实际是 `addi x2, x2, 1`（rd 应为 x3）。
  ⇒ 是**测试文件内嵌的软件参考解释器 `Rv32iInterpreter` 的用法错误**，与 CppHDL RTL 无关。

## What Changes

**In Scope**:

### C1-a 12 个绿 target 注册进 CTest
- 在 `examples/riscv-mini/CMakeLists.txt` 的 `riscv_mini_catch_test()` 里补 `add_test`。
- `examples/disabled_examples.txt` 移除对应行（归位）。

### C1-b 7 个红 target 修或删（逐个带理由）
| target | 处置 | 理由 |
|---|---|---|
| `test_pipeline_minimal` | **修** | 用 fixture Component 包裹（`ch_module` 必须在 `describe()` 内） |
| `test_pipeline_debug` | **修** | 同上；9 个用例是 5 个 stage + HazardUnit 的逐组件构造冒烟，有真实回归价值 |
| `test_pipeline_debug4` | **删** | 16 个「增量接地输入」用例**互为重复**——未显式驱动的输入默认为 0 ⇒ 16 例行为完全相同，零增量价值（292 LOC） |
| `test_pipeline_debug5` | **删** | 与 debug4 同型的 2 个端点用例（62 LOC） |
| `test_rv32i_pipeline_device` | **删** | 其前提「`ch_device<Rv32iPipeline>` 直接可用」在当前组件设计下**结构性不可实现**；等价覆盖由 `test_pipeline_minimal`（修后）+ `test_pipeline_smoke`（`Rv32iPipelineTop`）提供 |
| `test_address_decoder` | **修** | `sim.eval()` → `sim.tick()`（`eval()` 是时钟沿原语，对无时钟的纯组合设计不适用） |
| `test_riscv_tests` | **修** | ① 修 4 个手写用例的指令编码；② 依赖仓外 riscv-tests 二进制的 41 个用例在资源缺失时显式 `SKIP`；③ `RISCV_TESTS_PATH` 改为可由环境变量覆盖 |

### C1-c 7 个 orphan 测试源文件定性
| 源文件 | 处置 |
|---|---|
| `test_pipeline_debug2.cpp` / `test_hazard_minimal.cpp` / `test_interrupt_flow.cpp` | **从未编译**；`test_interrupt_flow.cpp` 依赖仓外 riscv-tests 二进制且含 `sim.eval()`（框架缺陷）⇒ 删（带理由登记） |
| `test_csr_unit.cpp` | 已以 `test_csr_unit` 注册进根 `tests/CMakeLists.txt` ⇒ **已归位**（不删，登记已归位） |
| `test_riscv_tests_soc.cpp` | 已以 `test_address_decoder` 注册 ⇒ **已归位** |
| `test_cache_smoke.cpp` | 已以 `riscv_mini_cache_smoke` 注册 ⇒ **已归位** |
| `test_rv32i_pipeline.cpp`（example 副本） | 根 `tests/test_rv32i_pipeline.cpp` 是权威副本（545 行 vs example 203 行，内容不同）⇒ **删 example 副本**（带理由登记） |

### C1-d 新护栏
- 新增 `scripts/verify_example_test_wiring.sh`（命名对齐 `scripts/verify_rst_n_wiring.sh` 范式）：
  每个 `examples/**/tests/test_*.cpp` 要么被注册进 CTest、要么登记在
  `examples/disabled_examples.txt`（带理由 + 归位条件），否则 exit 1。
  挂进 CI。

### C1-e 计数同步
- 根 `AGENTS.md` `CTEST_TOTAL` 与 NOTES §Test counts 同步（新增 N 个 ctest 条目）。

**Out of Scope**:

- **不改任何 RV32I 实现语义**（hazard 前推等留给 C2 / ADR-046 D5）。
- **不修框架缺陷**（`ch_module` 作为成员无法绑 parent / `Simulator::eval()` 空指针解引用）
  —— 本 change 只**登记**它们（见下），修复单独立项。
- **不收敛 include 路径**（C2 的事）。
- **不做 e2e parity**（C3 的事）。
- **不引入外部 riscv-tests 依赖**（CI 不拉仓外仓库）。

## Capabilities

- **MODIFIED/ADDED `example-runtime-coverage`** —— 新增一条 Requirement：
  example 内的 Catch2 测试 target MUST 注册进 CTest 体系（不得用 example 运行护栏代替）；
  依赖仓外资源的用例 MUST 条件 SKIP；测试源文件 MUST 有归属。

## Impact

### 风险
| 风险 | 缓解 |
|---|---|
| `test_riscv_tests` 的 41 个用例在 CI 中恒为 SKIP ⇒ 表面覆盖 | 4 个自包含用例仍真实执行；SKIP 在 CTest 输出中可见（不是静默跳过） |
| 删 3 个测试源文件（debug4 / debug5 / test_rv32i_pipeline 副本）⇒ 覆盖净减少 | 每个删除都**带理由**登记在 `examples/disabled_examples.txt` + 护栏强制归属 |
| 修 `test_riscv_tests` 的指令编码可能暴露解释器的更深缺陷 | 逐条解码核对；若修完仍红 ⇒ 登记为独立 change，不阻塞 C1 |
| `add_test` 改变 CTEST_TOTAL ⇒ 根 `AGENTS.md` 漂移 | 同步 NOTES + `scripts/verify_test_counts.sh` 绿 |

### 登记的框架缺陷（本 change 不修，单独立项）
1. **`ch_module` 作为 `Component` 成员永远绑不到 parent** —— 成员在构造函数期初始化，
   早于 `build()` → `build_internal()` 设置 `Component::current()`；⇒
   `ch_device<Rv32iPipeline>` 结构性 SIGSEGV，且经 `ch_module` 路径使用时
   子 stage 实际挂到**祖父**节点下（父子关系错误但可运行）。
2. **`Simulator::eval()` 无空指针保护** —— `default_clock_instr_` 在纯组合设计下为 null
   （`src/simulator_init.cpp:152`），`eval()` 直接解引用（`tick()` 有保护）。

## Acceptance

- [ ] `cmake --build build -j$(nproc)` 0 error 0 warning
- [ ] 19 个原「不执行」target 全部有归属：绿的进 CTest、红的修好或删除并登记
- [ ] `ctest -N` 中新增条目数与 root `AGENTS.md` `CTEST_TOTAL` 一致
- [ ] `bash scripts/verify_example_test_wiring.sh` 绿
- [ ] `bash scripts/verify_openspec_delta.sh riscv-mini-test-triage-2026-10` 绿
- [ ] `ctest -LE perf` 零回归（基线 174/174）
