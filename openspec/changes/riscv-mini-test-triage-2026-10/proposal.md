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

### 机制性根因：`enable_testing()` 的位置让 `add_test()` 成了空操作

「19 个 target 编译但从不执行」的**机制**在此：根 `CMakeLists.txt` 的 `enable_testing()`
原本位于**第 324 行**，而 `add_subdirectory(examples)` 在**第 257 行** ⇒ examples 子目录
在 testing 启用**之前**被处理 ⇒ CMake 不为它生成 `CTestTestfile.cmake`
（实测 `build/examples/riscv-mini/CTestTestfile.cmake` **根本不存在**）
⇒ 其中的 `add_test()` **静默无效**。

⇒ 这是「补 `add_test` 就够了」这一预判的**唯一拦路虎**：只加 `add_test` 而不调顺序，
CTest 仍然一个都认不出来。修复 = 把 `enable_testing()` 提到所有子目录之前。

### 顺带挖出的真实 RTL bug：CLINT 地址解码用了错误的位域

`include/cpu/riscv/clint.h` 用 `bits<1, 0>(io().addr)` 解码地址偏移，但文件头注释声明的
寄存器映射是 `0x0/0x4/0x8/0xC` ⇒ 偏移必须取 **bits[3:2]**。用 bits[1:0] 时
**四个寄存器全部解码为偏移 0** ⇒ `mtime_hi` / `mtimecmp_lo` / `mtimecmp_hi` **完全不可达**
⇒ 整个 CLINT 寄存器组只有 `mtime_lo` 可写。

该缺陷此前**零覆盖** —— 因为 CLINT 唯一的测试（`test_interrupt_flow.cpp`，502 LOC /
10 用例 / 28 断言）**从未被编译过**。⇒ 修 1 行（`bits<1,0>` → `bits<3,2>`）后，
该测试的 2 个 mtip 断言即转绿。

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

### C1-a 绿 target 注册进 CTest（**含根 CMakeLists 的 `enable_testing()` 顺序修复**）
- **根因修复**：把根 `CMakeLists.txt` 的 `enable_testing()` 提到
  `add_subdirectory(examples)` **之前**（否则 examples 内的 `add_test()` 是空操作）。
- 在 `examples/riscv-mini/CMakeLists.txt` 的 `riscv_mini_catch_test()` helper 内
  统一补 `add_test`（一次覆盖全部 18 个 target）+ `LABELS "base"` + `TIMEOUT 300`。
- `examples/disabled_examples.txt` 移除已归位条目。

### C1-b 7 个红 target 修或删（逐个带理由）
| target | 处置 | 理由 |
|---|---|---|
| `test_pipeline_minimal` | **修** | 用 fixture Component 包裹（`ch_module` 必须在 `describe()` 内） |
| `test_pipeline_debug` | **修** | 同上；9 个用例是 5 个 stage + HazardUnit 的逐组件构造冒烟，有真实回归价值 |
| `test_pipeline_debug4` | **删** | 16 个「增量接地输入」用例**互为重复**——未显式驱动的输入默认为 0 ⇒ 16 例行为完全相同，零增量价值（292 LOC） |
| `test_pipeline_debug5` | **删** | 与 debug4 同型的 2 个端点用例（62 LOC） |
| `test_pipeline_debug3` | **删** | 与已删的 `test_hazard_minimal` **逐字同型**（同一个 0 断言的 HazardUnit 构造冒烟），且 `ch_module` 放 TEST_CASE 顶层违反 AGENTS.md 约定 |
| `test_pipeline_debug6` | **删** | 与已删的 `test_pipeline_debug4` 同型（16 输入全 0 ≡ 完全不驱动 ⇒ 那 16 行 `set_input_value` 零判别力） |
| `test_pipeline_smoke` | **删** | `Rv32iPipelineTop` 的端口列表在 **ADR-046 D10 被冻结**（C3 的 DUT 依赖它）⇒ MUST NOT 为其加观测端口；而其现有端口**全部是输入**（零可观测输出）⇒ **无法写出非空洞断言**。覆盖由 `test_pipeline_wrapper`（带观测 + 真实断言）承接 |
| `test_rv32i_pipeline_device` | **删** | 其前提「`ch_device<Rv32iPipeline>` 直接可用」在当前组件设计下**结构性不可实现**；等价覆盖由 `test_pipeline_minimal`（修后）+ `test_pipeline_smoke`（`Rv32iPipelineTop`）提供 |
| `test_address_decoder` | **修** | `sim.eval()` → `sim.tick()`（`eval()` 是时钟沿原语，对无时钟的纯组合设计不适用） |
| `test_riscv_tests` | **修** | ① 修 4 个手写用例的指令编码；② 依赖仓外 riscv-tests 二进制的 43 个用例在资源缺失时显式 `SKIP`（用 `is_regular_file` 而非 `exists` —— 目录会让 `load_binary` 以 `std::bad_alloc` 崩掉）；③ `RISCV_TESTS_PATH` 改为可由环境变量覆盖 |
| `test_riscv_tests_pipeline` | **只构建、不进 CTest** | 46 个用例**全部**依赖仓外二进制 ⇒ CI 中全部 SKIP ⇒ Catch2 v3 退出码 **4**（= 没有真正跑起来的用例）⇒ **CTest 判红** ⇒ 登记在 `examples/disabled_examples.txt`（带归位条件） |

### 统一判据（第二轮审稿后确立）

> **一个 examples 内的测试 target MUST 至少断言一个语义确定的输出**
> （不能只有 `SUCCEED`）。达不到 ⇒ 补断言，或删 + 留痕
> （留痕在 `examples/removed_examples.txt`，**不是**豁免清单）。

按此判据，9 个「0 断言」target 的处置：**6 个补断言**（`test_pipeline_minimal` /
`test_pipeline_wrapper` / `test_wrapper_simple` / `test_wrapper_ifstage` /
`test_wrapper_pipeline` / `test_ifstage_device`）+ **3 个删除**（`debug3` / `debug6` /
`smoke`）。补断言的手法：把 DUT 的某个语义输出引到顶层 `ch_out`（`ch_device<T>`
的顶层组件则直接用其自身端口），断言一个**非平凡值**（0x80000004 / 0x33 / 1 / 2 /
0x1234 / 0xABCD / 前推选择非零 …）。

### C1-c 7 个 orphan 测试源文件定性
| 源文件 | 处置 |
|---|---|
| `test_pipeline_debug2.cpp` | **从未编译**；标题即「Minimal reproduction of "Child component has been destroyed" crash」= 纯崩溃复现脚手架（43 LOC / 1 用例 / **0 断言**）⇒ **删**（带理由登记） |
| `test_hazard_minimal.cpp` | **从未编译**；19 LOC / 1 用例 / **0 断言**的「HazardUnit instantiation only」，与 `test_pipeline_debug` 的「DEBUG 1: HazardUnit standalone」完全同型且被其覆盖 ⇒ **删**（带理由登记） |
| `test_interrupt_flow.cpp` | **从未编译**，但**不是**死脚手架 —— **502 LOC / 10 用例 / 28 断言**，覆盖 CLINT 定时器 / CSR 中断条件 / 陷阱入口 / MRET ⇒ **修好并注册进 CTest**（实测转绿；顺带暴露 CLINT 地址解码 bug） |
| `test_csr_unit.cpp` | 已以 `test_csr_unit` 注册进根 `tests/CMakeLists.txt` ⇒ **已归位**（不删） |
| `test_riscv_tests_soc.cpp` | 已以 `test_address_decoder` 注册 ⇒ **已归位**（修 `sim.eval()` → `sim.tick()`） |
| `test_cache_smoke.cpp` | 已以 `riscv_mini_cache_smoke` 注册 ⇒ **已归位** |
| `test_rv32i_pipeline.cpp`（example 副本） | **不是**根副本的过期副本 —— 根副本 12 个用例、example 副本 **5 个**用例，且测的是**不同组件**（example 副本测 example 侧**前推启用版** `hazard_unit.h`，根副本测 `rv32i_core.h` 的 `HazardDetectionUnit`）⇒ **修好并注册进 CTest**（实测 10 断言 / 5 用例全绿），**不删** |

### 第三轮审稿（REJECT）后的整改

| # | 缺陷 | 整改 |
|---|---|---|
| B1 | 自述「6 补断言」实为 3 个：3 个 target 仍 0 断言（变异清理时 `git checkout --` 把改写覆盖掉了） | 4 个文件全部改写并复核（`test_wrapper_ifstage` / `test_ifstage_device` / `test_wrapper_pipeline`，`test_wrapper_pipeline` 原从未改） |
| B2 | 「MUST 断言一个语义输出」**无自动化护栏**、delta 也无该 Requirement ⇒ 把 52 断言的文件掏空成 `REQUIRE(1==1)` 全绿 | 新增 **C4**：每个测试源 MUST 登记「真断言数**下限**」锚点（`CH4_ANCHOR`，机制同 `CTEST_TOTAL`）+ delta 新增 Requirement |
| B3 | 源文件枚举漏 `.cc`/`.cxx` 与 `SCENARIO` 等非 `TEST_CASE` 宏；C3 只遍历「有 `.cpp` 声明行」的 target ⇒ 用 `.cc` 注册的失败测试逃逸 | 枚举扩展为「4 种后缀 × 全部 Catch2 用例宏 ∪ `tests/` 下任意文件」；**C3 改为遍历 CTest 条目 ∩ `build/examples/` 下的可执行文件**（后缀无关） |
| B4 | `is_declared_target` 只匹配**单行** `fn(name` ⇒ 多行声明（本仓自己就用）被误判 STALE-ARTIFACT ⇒ 静默掩盖真问题（与自述的安全方向相反） | 判据改为**构建系统自己的 target 列表**（`cmake --build --target help`，对语法变体免疫）+ 折行文本匹配回退 |
| B5 | `rv32i_pipeline_top.h` 删除其唯一消费者后成为**孤儿头**；`removed_examples.txt` 的「等价覆盖」声明是虚假的且自指 | 修正措辞；把「该头文件暂时零消费者」作为**已知缺口**登记（归位条件：C3 落地时它成为 C3 的 DUT） |

**SHOULD_FIX**：allowlist 改为记**期望条数**（原先整条豁免 ⇒ 注入任意多条无关错误仍判 OK）；
C3b 对不在 CTest 的 allowlist 行改为报 STALE（原先静默跳过 ⇒ 拼错的行永久残留）；
`test_pipeline_wrapper` 的「ITCM 写入」归因被变异证伪 ⇒ 用例改名/改注释为实际验证的语义；
文档计数（`18 个` ×4 → 14、「本清单 10」→ 5、`catch_amalgamated` 位置、DEBUG 3 的指令描述）。

### C1-d 新护栏（`scripts/verify_example_test_wiring.sh`）

> ⚠️ 本护栏经对抗审稿 REJECT 后**重写**过两次。三个不可退化的设计约束：
> ① 判据是 **`ctest -N`（CTest 条目的唯一真相）**，不是解析 CMake 源码；
> ② 基准是**文件系统上的源文件集合**（外部事实），不是「CMake 里声明了什么」
> —— 否则删一条声明会让「声明集」与「覆盖集」同步缩小 ⇒ 守恒恒等 ⇒ 对缩面无判别力；
> ③ 源文件→target 的映射 MUST 用**精确路径**，不得用 basename
> —— 否则 example 侧的 `test_rv32i_pipeline.cpp` 会被根 `tests/` 的同名文件顶替。
> 变异验证（5 条全被抓）：注释掉 example 侧 target / 行尾注释塞文件名 /
> 新增不走 helper 的 `add_executable` / `enable_testing()` 移回 `examples` 之后 /
> 删除源文件。

### C1-d 新护栏
- 新增 `scripts/verify_example_test_wiring.sh`（命名对齐 `scripts/verify_rst_n_wiring.sh` 范式）：
  - **C0（源码级）**：根 `CMakeLists.txt` 的 `enable_testing()` MUST 排在
    `add_subdirectory(examples)` 之前 —— 陈旧构建目录会掩盖这个问题 ⇒
    不能只靠 `ctest -N`。
  - **C1 归属守恒**：每个 example 测试源 MUST 被某个「已进 CTest」或
    「已被 example 运行护栏跟踪」的 target 编译，否则登记在 `disabled_examples.txt`。
  - **C2 数量守恒**：源文件总数 MUST 等于 root `AGENTS.md` 的
    `EXAMPLES_TEST_SOURCES` **锚点**（机制与 `CTEST_TOTAL` 同型）。
  - **C3 运行期**：examples 的每个 CTest 条目 MUST 跑出 exit 0 **且日志无
    error/fatal**；既有例外走显式 allowlist（带 stale 检查）。
  挂进 CI（2 处 job）。
- **`run_all_ported_tests.sh` 的 C1 检查需认得 CTest 归属**：17 个 riscv-mini 测试
  现在**是构建产物但由 CTest 覆盖**，不属于 example demo 护栏 ⇒ 该检查必须把
  「已进 CTest」也算作归属（用 `ctest -N` 作唯一真相）。

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

### 本 change 超出「纯测试面」的一处（已实测并论证）
- **修 `include/cpu/riscv/clint.h` 的地址解码**（`bits<1,0>` → `bits<3,2]`，1 行）。
  理由：① 该缺陷使 CLINT 的 3/4 寄存器**完全不可达**，是**真实 RTL bug**；
  ② 它是**新注册的 `test_interrupt_flow` 唯一的失败根因** —— 不修则该
  28 断言的测试无法进 CTest；③ 修复面 = 1 行，回归由该测试 + 现有 192 个 CTest 覆盖。
  ⚠️ 这是本 change 唯一触碰 `include/**` 的地方，已在验收中显式记录。

### 登记的框架缺陷（本 change 不修，单独立项）

3. **CLINT 的 `mtime` 不会自动递增**（`include/cpu/riscv/clint.h`）：RISC-V
   规范要求 `mtime` 是自由运行计数器，而当前实现只能由软件通过写端口显式写入
   ⇒ 只构成「静态比较器」。文件头注释此前**声称**「每个 `tick()` 递增」而
   `tick()` 是空实现 ⇒ 本 change 删掉了该空实现与 2 个恒返 0 的占位 getter，
   并把注释改成如实描述。修复（递增频率 + `mtimecmp` 语义联动）单独立项。
4. **6 个 target 在运行期打印 error 级 CHERROR，但退出码仍是 0**
   （`[ERROR] Requirement failed [driver_impl->get_parent() == lnodeimpl::current_component()]`，
   `include/core/io.h:385`；`CHREQUIRE` 只记日志不中止）：
   `test_riscv_tests_pipeline`(135，不进 CTest) / `test_pipeline_debug`(9) /
   `test_pipeline_wrapper`(9) / `test_wrapper_pipeline`(6) / `test_pipeline_minimal`(3)。
   ⇒ 进 CTest 的 4 个走护栏 `CHERROR_ALLOWLIST`（带 stale 检查）。
   ⇒ 这是缺陷 #1（`ch_module` 作为 `Component` 成员无法绑 parent ⇒ 子 stage
   挂到**祖父**节点下 ⇒ `<<=` 的 driver 归属检查失败）的**可观测症状**。
   ⇒ 「exit 0」**不等于**「日志干净」——本 change 如实登记该计数，不掩盖。
1. **`ch_module` 作为 `Component` 成员永远绑不到 parent** —— 成员在构造函数期初始化，
   早于 `build()` → `build_internal()` 设置 `Component::current()`；⇒
   `ch_device<Rv32iPipeline>` 结构性 SIGSEGV，且经 `ch_module` 路径使用时
   子 stage 实际挂到**祖父**节点下（父子关系错误但可运行）。
2. **`Simulator::eval()` 无空指针保护** —— `default_clock_instr_` 在纯组合设计下为 null
   （`src/simulator_init.cpp:152`），`eval()` 直接解引用（`tick()` 有保护）。

## Acceptance

- [x] `cmake --build build -j$(nproc)` 0 error 0 warning
- [x] 19 个原「不执行」target 全部有归属：绿的进 CTest、红的修好或删除并登记；
      另 2 个高价值 orphan（`test_interrupt_flow` / example 版 `test_rv32i_pipeline`）
      修好并进 CTest
- [x] `ctest -N` = **199**（基线 185 + 14；**8** 个 0 断言/不可观测 target 被删、1 个因 Catch2 exit 4 只构建），
      与 root `AGENTS.md` `CTEST_TOTAL` 一致
- [x] `bash scripts/verify_example_test_wiring.sh` 绿
- [x] `bash scripts/verify_openspec_delta.sh riscv-mini-test-triage-2026-10` 绿
- [x] `ctest -LE perf` 零回归（**188/188 全绿**，基线 174 + 14 新增）
- [x] `bash scripts/verify_test_counts.sh` 绿
- [x] `bash run_all_ported_tests.sh` 绿（28/28，PASS 28 / FAIL 0）
