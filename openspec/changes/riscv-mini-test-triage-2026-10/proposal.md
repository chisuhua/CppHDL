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
  统一补 `add_test`（一次覆盖全部 19 个 Catch2 target）+ `LABELS "base"` + `TIMEOUT 300`。
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
| `test_rv32i_pipeline.cpp`（example 副本） | **不是**根副本的过期副本 —— 根副本 12 个用例、example 副本 **5 个**用例，且测的是**不同组件**（example 副本测 example 侧**前推启用版** `hazard_unit.h`，根副本测 `rv32i_core.h` 的 `HazardDetectionUnit`）⇒ **修好并注册进 CTest**（实测 23 断言 / 5 用例全绿 —— 断言数随第五轮新增的「EX>MEM 双源优先级」用例从 10 增至 23），**不删** |

### 第六轮审稿（REJECT）后的整改

本轮两处**新写的收窄声明**被实现证伪（都落在「护栏自称覆盖」的范围内），故**修实现**而非改声明：

| # | 缺陷 | 整改 |
|---|---|---|
| B1 | `#elif` **从不求值自己的条件**：`#if 1 / #elif 1 / REQUIRE(x)` 里 `#elif` 体是**死的**，却被当活代码计入 ⇒ 把 52 条活断言包进死的 `#elif` 即可让被掏空的文件回到 `52 ≥ 52` 全绿。直接违反 delta 新写的「死分支内的断言 MUST NOT 计入」 | 条件状态机重写：每层记 `gres`（求值结果 0/1/2未知）+ `gtaken`（是否已有分支被取）。`#if 1` 之后的 `#elif`/`#else` **都是死的**；`#if 0 / #elif 1` 的 `#elif` 体**是活的**（同时修掉少算方向的噪声） |
| B2 | `#ifdef` 的 `#else` 被**无条件当活代码**：`#ifdef __linux__ / #else / REQUIRE(x)` 在 Linux 上 `#else` 恰是死分支 ⇒ 计数只算死的那半、漏掉活的那半，与脚本头「宁可少算，**不会多算**」的声明**方向相反** | `#ifdef`/`#ifndef`/非常量 `#if` 一律记 `gres=2` ⇒ **连同其 `#elif`/`#else` 整组跳过**（保守：只可能少算） |
| SF-1 | C4b 的逐用例扫描**丢了 C4 的词边界保护** ⇒ `MYREQUIRE(` / `XCHECK(` 被计入该用例的断言数（头注释却声称「已覆盖词边界」） | C4b 改用与 C4 **同一个** `count_asserts()`（共享 `ASM` 宏表 + 词边界） |
| SF-2 | C4b 不认 `REQUIRE_THROWS` / `REQUIRE_NOTHROW` / `STATIC_REQUIRE` 等 ⇒ 合法用例被误报「0 条断言」 | `ASM` 宏表补齐 Catch2 的 throws/nothrow/STATIC 系断言宏（本仓当前 0 处使用，属预防性） |
| SF-3 | `#ifdef` 整组包住的 `TEST_CASE` 对 C4b **完全不可见**（漏报方向） | 新增 `S` 计数 + `NOTICE`：被跳过区段里出现的用例宏**点名**报出（不静默） |
| SF-4 | 「9 个字面用例」实为 **8**（第 9 个 `grep` 命中的 `TEST_CASE` 就在宏体里）⇒ 脚本锚点条目 / 头注释 / proposal / commit message 四处同错 | 四处统一为 8 |
| SF-5 | `examples/riscv-mini/AGENTS.md` 与 proposal 仍写 `test_rv32i_pipeline` 是「5 tests, **10** assertions」 | 改为 23（第五轮新增双源优先级用例后） |
| SF-6 | proposal 风险表写「删 **3** 个测试源文件」且「登记在 `examples/disabled_examples.txt`」——两者都错（实删 **8** 个；删除留痕在 `removed_examples.txt`） | 逐项订正 |
| SF-7 | 同文件另一处仍写「**8** 个 target 被删」，与已改的「6 target + 2 孤儿源」口径冲突 | 统一 |

⚠️ **两轮连续出现「新写的收窄声明立刻被实现证伪」**（第四轮 C4 的立法理由、第六轮 delta 的「只统计活代码」）。
结论：**本 change 不再新增能力声明**；后续若有人要扩展 C4/C4b 的覆盖面，MUST 先补变异 fixture 再改文档。

### 第五轮审稿（REJECT）后的整改

| # | 缺陷 | 整改 |
|---|---|---|
| B1 | `real_assert_count()` 数的是「文件里的 `REQUIRE` 字面量」而非「可执行断言」：① 追加一个**从不展开**的 `#define DEAD_NEVER_EXPANDED()` 宏体（体内 52 条 `REQUIRE`）即可把被掏空的文件灌回锚点 ⇒ 护栏全绿；② `#if 1 / #else` 的**死分支**同样可灌。⚠️ **in-tree 已存在实例**：`test_riscv_tests.cpp` 的 41 个宏生成用例的断言在宏体里，计数只认 8 个字面用例（第 9 个 `grep` 命中的 `TEST_CASE` 就在宏体里） | 计数器重写为「**活代码提取**」awk：宏体**整行不计**（可能永不展开）、`#if 0` 与 `#if 1/#else` 的死分支不计、`#else` 之后的活代码**计入**（修掉反向假阳性）。`test_riscv_tests.cpp` 锚点随之 15 → 14（8 个字面用例；如实反映「宏生成用例不计」这一已声明局限） |
| B2 | delta 写的「**每个**用例 MUST 至少断言一个语义输出」**零机制**：往文件尾追加一个只有 `SUCCEED` 的 `TEST_CASE` 即可违反而护栏全绿（文件级下限被同文件其它用例掩盖） | 新增 **C4b**：`analyze_source` 增加 brace-matching 的逐用例扫描，每个**字面**用例 MUST ≥ 1 条可执行断言；复现该攻击 ⇒ 现在报违规 |
| SF1 | `#if 0 ... #else <活断言> ... #endif` 被误判成死代码 ⇒ 语义等价的重写被误报 | 同上（`#else` 后的活代码计入） |
| SF2 | `proposal.md` 仍写「防『掏空测试』的**等量替换**」，与刚同步的三处冲突 | 同步为「防断言被删除/注释掉/条件编译掉；**不防**等量替换（由 code review 承担）」 |
| SF3 | `AGENTS.md` KNOWN LIMITATIONS 引用 `tests/test_cache_smoke.cpp`（路径不存在） | 改为 `examples/riscv-mini/tests/test_cache_smoke.cpp` |
| SF4 | `test_cache_smoke.cpp` 交叉引用「框架缺陷 #6」，实际编号为 #5 | 改为 #5 |
| SF5 | 「**8** 个 target 被删除」与史实不符 —— 全历史扫描确认只有 **6** 个曾声明为 CMake target（`test_pipeline_debug3/4/5/6`、`test_pipeline_smoke`、`test_rv32i_pipeline_device`）；`test_pipeline_debug2` / `test_hazard_minimal` **从未出现在任何 CMakeLists** | 文档改为「6 个 target + 2 个从未进 CMakeLists 的孤儿源文件 = 8 条留痕」 |
| SF6 | `hazard_unit.h` 文档化的优先级 `EX=1 > MEM=2 > REG=0` **无判别力**（两个用例从不让 EX 与 MEM 同时命中同一 rs1）⇒ 实测把优先级倒过来仍全绿 | 新增「EX beats MEM when both match same rs1」用例（断言 `forward_a == 1` 且数据取自 EX）⇒ 变异（优先级反转）**转红** |
| NIT | `EX_TARGETS` 混入 CMake 关键字；`base%.cpp` 只剥 `.cpp`；`declared_targets_init` 的 `mktemp` 无 `trap` 清理；`xargs -r`（GNU 扩展）| 分别：收窄到 `_test/_demo/_example/_smoke/_prog/_check` 结尾；改 `base%.*`；加 `trap ... EXIT INT TERM`；改 POSIX `while read` 循环 |

### 第三轮审稿（REJECT）后的整改

| # | 缺陷 | 整改 |
|---|---|---|
| B1 | 自述「6 补断言」实为 3 个：3 个 target 仍 0 断言（变异清理时 `git checkout --` 把改写覆盖掉了） | 4 个文件全部改写并复核（`test_wrapper_ifstage` / `test_ifstage_device` / `test_wrapper_pipeline`，`test_wrapper_pipeline` 原从未改） |
| B2 | 「MUST 断言一个语义输出」**无自动化护栏**、delta 也无该 Requirement ⇒ 把 52 断言的文件掏空成 `REQUIRE(1==1)` 全绿 | 新增 **C4**：每个测试源 MUST 登记「真断言数**下限**」锚点（`CH4_ANCHOR`，机制同 `CTEST_TOTAL`）+ delta 新增 Requirement。⚠️ **该机制的措辞与实现能力在第四、五轮审稿后各被纠正一次** —— 后来者请以「第五轮审稿后的整改」表与 delta 的最新措辞为准（C4 **不防**等量替换；计数只认**活代码**） |
| B3 | 源文件枚举漏 `.cc`/`.cxx` 与 `SCENARIO` 等非 `TEST_CASE` 宏；C3 只遍历「有 `.cpp` 声明行」的 target ⇒ 用 `.cc` 注册的失败测试逃逸 | 枚举扩展为「4 种后缀 × 全部 Catch2 用例宏 ∪ `tests/` 下任意文件」；**C3 改为遍历 CTest 条目 ∩ `build/examples/` 下的可执行文件**（后缀无关） |
| B4 | `is_declared_target` 只匹配**单行** `fn(name` ⇒ 多行声明（本仓自己就用）被误判 STALE-ARTIFACT ⇒ 静默掩盖真问题（与自述的安全方向相反） | 判据改为**构建系统自己的 target 列表**（`cmake --build --target help`，对语法变体免疫）+ 折行文本匹配回退 |
| B5 | `rv32i_pipeline_top.h` 删除其唯一消费者后成为**孤儿头**；`removed_examples.txt` 的「等价覆盖」声明是虚假的且自指 | 修正措辞；把「该头文件暂时零消费者」作为**已知缺口**登记（归位条件：C3 落地时它成为 C3 的 DUT） |

**SHOULD_FIX**：allowlist 改为记**期望条数**（原先整条豁免 ⇒ 注入任意多条无关错误仍判 OK）；
C3b 对不在 CTest 的 allowlist 行改为报 STALE（原先静默跳过 ⇒ 拼错的行永久残留）；
`test_pipeline_wrapper` 的「ITCM 写入」归因被变异证伪 ⇒ 用例改名/改注释为实际验证的语义；
文档计数（`18 个` ×4 → 14、「本清单 10」→ 5、`catch_amalgamated` 位置、DEBUG 3 的指令描述）。

### C1-d 新护栏（`scripts/verify_example_test_wiring.sh`）

> ⚠️ **2026-10-24 拆分**：本护栏原含两个维度 —— 「**归属守恒**」与「**断言质量**」。
> 后者依赖一个自研的「C 子集静态分析器」，而它**不可能被完全闭合**
> （第 4/5/6 轮对抗审稿的**全部** BLOCKER 都出在它身上：词频冒充可执行断言 /
> 宏体与死分支灌计数 / `#elif` 与 `#ifdef` 的死分支被计入）。
> ⇒ 「断言质量」已拆为独立 change **`example-assertion-quality-2026-10`**
> （脚本 `scripts/verify_example_assertion_quality.sh`，自带 fixture 集作为规格）。
> ⇒ **本护栏只保留归属守恒**（判据 = `ctest -N`、基准 = 文件系统源文件集合、
> 锚点 = root `AGENTS.md` 的 `EXAMPLES_TEST_SOURCES`、运行期 exit + error/fatal 判据）
> —— 这部分**不依赖**静态分析器，简单可审、portable。

> ⚠️ 本护栏经**三轮**对抗审稿 REJECT 后重写。七条不可退化的设计约束
> （详见脚本头部注释）：
> ① 判据是 **`ctest -N`（CTest 条目的唯一真相）**，不是解析 CMake 源码；
> ② 基准是**文件系统上的源文件集合**（外部事实），不是「CMake 里声明了什么」
> —— 否则删一条声明会让「声明集」与「覆盖集」同步缩小 ⇒ 守恒恒等 ⇒ 对缩面无判别力；
> ③ 源文件→target 的映射 MUST 用**精确路径**，不得用 basename
> —— 否则 example 侧的 `test_rv32i_pipeline.cpp` 会被根 `tests/` 的同名文件顶替；
> ④ 源文件枚举 **MUST 按内容/目录**（4 种后缀 × 全部 Catch2 用例宏 ∪ `tests/` 下任意文件），
> 不得按 `test_*.cpp` 命名 —— 否则 `pipeline_check.cpp` / `.cc` 源完全不可见；
> ⑤ 「跑得起来」还不够：MUST 同时看 **exit 0** 与**日志无 error/fatal**；
> ⑥ 删除留痕 MUST NOT 与「豁免」混在一个文件里（否则退化成按名字的永久自动豁免）；
> ⑦ 测试 MUST 真的断言：按**可执行断言数下限**锚点检查（防「断言被删除 / 注释掉 /
>    条件编译掉」）+ **逐用例**下限（防「某用例只有 `SUCCEED`」）。
>    ⚠️ 它**不防「等量替换」**（`REQUIRE(pc == X)` → `REQUIRE(1 == 1)` 断言数不变）——
>    那一条由 **code review** 承担，护栏不假装覆盖。
>
> 检查项：**C0** `enable_testing()` 顺序（源码级，陈旧构建目录掩盖不了）/
> **C1** 源文件归属守恒 / **C2** 源文件数 vs `EXAMPLES_TEST_SOURCES` 锚点 /
> **C3** 每个 CTest 条目 exit 0 且日志干净（例外走带**期望条数**的 allowlist）/
> **C3b** allowlist stale / **C4** 可执行断言数下限（`CH4_ANCHOR`）/ **C4b** 逐用例下限。
> 挂进 CI（2 处 job）。
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
| 删 **8** 个 `examples/riscv-mini/tests/` 下的测试源文件（`test_hazard_minimal` / `test_pipeline_debug2..6` / `test_pipeline_smoke` / `test_rv32i_pipeline_device`）⇒ 覆盖净减少（example 版 `test_rv32i_pipeline.cpp` **存活**，只被改写） | 每个删除都**带理由**登记在 `examples/removed_examples.txt`（**删除留痕**，8 条）+ 护栏强制归属（`examples/disabled_examples.txt` 是**另一件事**：存活但未跑的豁免） |
| 修 `test_riscv_tests` 的指令编码可能暴露解释器的更深缺陷 | 逐条解码核对；若修完仍红 ⇒ 登记为独立 change，不阻塞 C1 |
| `add_test` 改变 CTEST_TOTAL ⇒ 根 `AGENTS.md` 漂移 | 同步 NOTES + `scripts/verify_test_counts.sh` 绿 |

### 本 change 超出「纯测试面」的一处（已实测并论证）
- **修 `include/cpu/riscv/clint.h` 的地址解码**（`bits<1,0>` → `bits<3,2]`，1 行）。
  理由：① 该缺陷使 CLINT 的 3/4 寄存器**完全不可达**，是**真实 RTL bug**；
  ② 它是**新注册的 `test_interrupt_flow` 唯一的失败根因** —— 不修则该
  28 断言的测试无法进 CTest；③ 修复面 = 1 行，回归由该测试 + 现有 192 个 CTest 覆盖。
  ⚠️ 这是本 change 唯一触碰 `include/**` 的地方，已在验收中显式记录。

### 登记的框架缺陷（本 change 不修，单独立项）

1. **`ch_module` 作为 `Component` 成员永远绑不到 parent** —— 成员在构造函数期初始化，
   早于 `build()` → `build_internal()` 设置 `Component::current()`；⇒
   `ch_device<Rv32iPipeline>` 结构性 SIGSEGV，且经 `ch_module` 路径使用时
   子 stage 实际挂到**祖父**节点下（父子关系错误但可运行）。
2. **`Simulator::eval()` 无空指针保护** —— `default_clock_instr_` 在纯组合设计下为 null
   （`src/simulator_init.cpp:152`），`eval()` 直接解引用（`tick()` 有保护）。
3. **CLINT 的 `mtime` 不会自动递增**（`include/cpu/riscv/clint.h`）：RISC-V
   规范要求 `mtime` 是自由运行计数器，而当前实现只能由软件通过写端口显式写入
   ⇒ 只构成「静态比较器」。文件头注释此前**声称**「每个 `tick()` 递增」而
   `tick()` 是空实现 ⇒ 本 change 删掉了该空实现与 2 个恒返 0 的占位 getter，
   并把注释改成如实描述。
   ⇒ **修复已移出本 change** ⇒ draft `.rddf/improvements/clint-mtime-autoincrement-2026-10.md`
     （建议在 C2 之后实施，避免同一批文件被两个 change 反复改）。
4. **`examples/riscv-mini/src/rv32i_pipeline_top.h` 暂时零消费者（孤儿头）** ——
   本 change 删掉了它的唯一消费者 `test_pipeline_smoke`（该测试无法写出非空洞断言：
   它的端口**全部是输入**，且 ADR-046 D10 冻结了端口列表 ⇒ MUST NOT 为观测加输出端口）。
   该头文件是 **ADR-046 D10 指定的 C3 DUT** ⇒ 本 change 只删它的**测试**、不删头文件，
   因此它暂时违反 ZERO-DEBT「每个头文件都必须被 include」。
   **归位条件**：C3（`rv32i-soc-dual-parity-2026-10`）落地时它成为 C3 的 DUT。
5. **riscv-mini 的两个 cache wrapper 的分支驱动输出从不发射**
   （`examples/riscv-mini/src/{i_cache_prefetch,d_cache_write_buffer}.h`）：
   `if (io().req == ch_bool(true) && io().we == ch_bool(true)) { io().ready <<= ...; }`
   这类分支的条件在**建图期**求值 —— `ch_bool` 有 `explicit operator bool()`
   ⇒ C++ `if` 拿到的是「当前节点是否非零」（建图期为假），**不是**运行期条件
   ⇒ 分支体内的 `io().xxx <<=` **从不发射** ⇒ `DCacheWriteBuffer::ready` 与
   两个 wrapper 的 `axi_valid`/`axi_addr` **恒为未驱动**（读到 0）。
   正确写法是 `select(...)` 表达式或 `chlib/if_stmt.h` 的 `_if(...)` DSL
   （仓库其它组件都这么写）。本 change **不修**该 wrapper（属功能补全，
   需连同「把 `addr`/`req` 接到内层 cache」一起做，单独立项）；
   仅把 `test_cache_smoke` 的断言从「不可证伪」改为**可证伪**（实测挖出）。
   ⇒ **修复已移出本 change** ⇒ draft
     `.rddf/improvements/riscv-mini-cache-wrapper-dead-branch-2026-10.md`
     （**强前置 = C2**：C2 会把这些 wrapper 收敛进 `include/cpu/**`）。
   ⚠️ `test_cache_smoke` 中钉住「未驱动 ⇒ 读 0」的那两条断言在缺陷修好后
   **会转红**，那时 MUST 连同该 draft 一起更新。
6. **4 个进 CTest 的 target（+1 个不进 CTest 的）在运行期打印 error 级 CHERROR，退出码仍是 0**
   （`[ERROR] Requirement failed [driver_impl->get_parent() == lnodeimpl::current_component()]`，
   `include/core/io.h:385`；`CHREQUIRE` 只记日志不中止）：
   `test_riscv_tests_pipeline`(135，不进 CTest) / `test_pipeline_debug`(9) /
   `test_pipeline_wrapper`(9) / `test_wrapper_pipeline`(6) / `test_pipeline_minimal`(3)。
   ⇒ 进 CTest 的 4 个走护栏 `CHERROR_ALLOWLIST`（记**期望条数**，带 stale 检查）。
   ⇒ 这是缺陷 #1 的**可观测症状**。
   ⇒ 「exit 0」**不等于**「日志干净」——本 change 如实登记该计数，不掩盖。

## Acceptance

- [x] `cmake --build build -j$(nproc)` 0 error 0 warning
- [x] 19 个原「不执行」target 全部有归属：绿的进 CTest、红的修好或删除并登记；
      另 2 个高价值 orphan（`test_interrupt_flow` / example 版 `test_rv32i_pipeline`）
      修好并进 CTest
- [x] `ctest -N` = **199**（基线 185 + 14；**6** 个 0 断言/不可观测 target 被删（另有 2 个从未进 CMakeLists 的孤儿源文件一并删除，共 8 条留痕）、1 个因 Catch2 exit 4 只构建），
      与 root `AGENTS.md` `CTEST_TOTAL` 一致
- [x] `bash scripts/verify_example_test_wiring.sh` 绿
- [x] `bash scripts/verify_openspec_delta.sh riscv-mini-test-triage-2026-10` 绿
- [x] `ctest -LE perf` 零回归（**188/188 全绿**，基线 174 + 14 新增）
- [x] `bash scripts/verify_test_counts.sh` 绿
- [x] `bash run_all_ported_tests.sh` 绿（28/28，PASS 28 / FAIL 0）
