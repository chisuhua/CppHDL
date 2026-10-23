# riscv-mini-test-triage-2026-10 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use skill_use("execute") to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 消除 `examples/riscv-mini/` 的「19 个 Catch2 target 编译但从不执行」缺口 —— 12 个绿的注册进 CTest、7 个红的修好或删除并带理由登记、7 个 orphan 源文件定性、新增 `scripts/verify_example_test_wiring.sh` 护栏、`CTEST_TOTAL` 同步。

**Architecture:** 改动集中在 3 个面 —— ①`examples/riscv-mini/CMakeLists.txt`（`riscv_mini_catch_test()` 补 `add_test`；删 3 个 target）；②`examples/riscv-mini/tests/*.cpp`（修 3 个文件、删 3 个文件）；③`examples/disabled_examples.txt` + 根 `AGENTS.md` NOTES（归属登记与计数同步）。**不触碰 `include/**` 与 `src/**`**（框架缺陷只登记不修）。

**Tech Stack:** C++23, Catch2 v3.7.0, CMake 3.20+, bash（护栏脚本），GitHub Actions。

---

## 设计要点：实测纠正的 4 处（与 improvement 草稿的预诊断不同）

> 本 plan 的所有修正均**先解码指令 / 读源码 / 跑二进制**确认，再落到代码。草稿 §1 的预诊断
> 「6 个 SIGSEGV 都是 `ch_module` 放 TEST_CASE 顶层」「44/49 是真 RTL 分歧」**均不准确**。

### R1. `ch_module` 放 TEST_CASE 顶层 ⇒ `CHFATAL` 而非静默失败

`include/module.h:47-50`：`ch_module` ctor 在 `Component::current()` 为 null 时
`CHERROR` 后**直接 return**，留下空 `child_component_sptr_` ⇒ 随后 `io()` 走
`CHFATAL("Child component has been destroyed unexpectedly in io()!")` ⇒ SIGSEGV。
⇒ 修法必须是「把 `ch_module` 放进某个 `Component::describe()` 内」，
**不是**草稿说的「改成 `ch_device`」（`ch_device` 只对**顶层组件**成立）。

### R2. `ch_device<Rv32iPipeline>` 结构性不可用 ⇒ 该 target 必删

`include/cpu/pipeline/rv32i_pipeline.h:75-81` 把 `ch_module<IfStage>` 等声明为**成员变量**。
成员在**构造函数**期初始化，而 `Component::current()` 直到 `Component::build()` →
`build_internal()`（`src/component.cpp:129-138`）才被设置 ⇒ 成员构造时 `current_` 为 null
⇒ `ch_module` 全部早退 ⇒ 随后 `io()` 必崩。

⇒ `ch_device<Rv32iPipeline>` **永远** SIGSEGV；经 `ch_module<Rv32iPipeline>` 路径可用，
但子 stage 实际挂到**祖父**节点下（父子关系错误）。
⇒ `test_rv32i_pipeline_device` 的前提（「direct, no wrapper」）不可实现 ⇒ **删**。
⇒ **框架缺陷登记**（本 change 不修）。

### R3. `Simulator::eval()` 对纯组合设计必崩 ⇒ `tick()` 才是正确 API

`src/simulator_init.cpp:152-154`：
```cpp
if (sequential_instr_list_.empty()) { default_clock_instr_ = nullptr; }
```
`AddressDecoder` 是纯组合设计（无寄存器/无 mem）⇒ `sequential_instr_list_` 为空
⇒ `default_clock_instr_ == nullptr`。
`src/simulator.cpp:356-361` 的 `Simulator::eval()` **无保护**地 `default_clock_instr_->eval()`
⇒ 空指针虚调用 ⇒ SIGSEGV（gdb 实测 `rdi=0` + `call *0x10(%rax)`）。
而 `Simulator::tick()`（`src/simulator.cpp:310`）有 `if (default_clock_instr_)` 保护 ⇒ 正常。

⇒ `test_riscv_tests_soc.cpp`（target `test_address_decoder`）的 `sim.eval()` → `sim.tick()`。
⇒ **框架缺陷登记**（本 change 不修）。

### R4. `test_riscv_tests` 的失败是**指令编码写错**，不是 RTL 分歧

逐条解码（`opcode[6:0] / rd[11:7] / funct3[14:12] / rs1[19:15] / rs2[24:20]`）：

| 位置 | 测试写的 | 实际是 | 应为 |
|---|---|---|---|
| `:595` | `0x002181B3` | `add x3, x3, x2`（rs1=x3） | `0x002081B3`（rs1=x1） |
| `:630` | `0x00022023` | `sw x0, 0(x0)` | `0x0020A023`（`sw x2, 0(x1)`） |
| `:636` | `0x00020103` | `lb x2, 4(x0)` | `0x0000A103`（`lw x2, 0(x1)`） |
| `:656` | `0x00408263` | `beq x1, x2, +4` | `0x00208463`（`+8`） |
| `:657` | `0x00100113` | `addi x2, x2, 1` | `0x00100193`（`addi x3, x0, 1`） |
| `:658` | `0x00200113` | `addi x2, x2, 2` | `0x00200193`（`addi x3, x0, 2`） |

另 41 个用例的失败是 `Failed to load binary: /workspace/project/riscv-tests/build/rv32ui/*.bin`
—— 依赖**仓外** riscv-tests 套件的预编译二进制（硬编码绝对路径，CI 中永不存在）。

### R5. `test_pipeline_debug4` 的 16 个用例互为重复 ⇒ 删

未显式驱动的 `ch_in` 输入默认为 0 ⇒ 「只接地 1 个输入」与「接地 16 个输入」的**行为完全相同**
⇒ 16 个增量用例零增量价值（292 LOC）。`test_pipeline_debug5` 同理（62 LOC）。

### R6. `test_pipeline_debug` 的 9 个用例有真实价值 ⇒ 修不删

`DEBUG 1..6` = HazardUnit + 5 个 stage 的**逐组件构造冒烟**（能抓住「某 stage 构造即崩」这类
回归）；`DEBUG 7..9` = pipeline 接地输入 / 释放复位 / 10 tick。

### R7. `add_test` 的位置与 `riscv_mini_catch_test()` 的关系

`riscv_mini_catch_test()`（`examples/riscv-mini/CMakeLists.txt:24-37`）是 19 个 target 的
公共 helper ⇒ **`add_test` 加在 helper 内**即可一次覆盖全部（不逐个 target 重复写）。
⚠️ `add_test` 的 `COMMAND` 必须用 `$<TARGET_FILE:NAME>` 形式（跨 generator 正确）。

### R8. `CTEST_TOTAL` 同步点（3 处）

根 `AGENTS.md` §NOTES：`CTEST_TOTAL: 185` 单行锚点 + 紧随其后的计数说明块；
`scripts/verify_test_counts.sh` 是 CI 防 drift 护栏。
⚠️ `CTEST_TOTAL` 只统计 **CTest 条目**；example 内的 target 现在会变成 CTest 条目 ⇒ 计数 +N。

### R9. 实施期修正：example 版 `test_rv32i_pipeline.cpp` **不是**过期副本 ⇒ 注册而非删除

计划原文按「根副本是权威」判定为删除。实施时逐条比对 TEST_CASE 后**证伪**：

| | 根 `tests/test_rv32i_pipeline.cpp` | example `tests/test_rv32i_pipeline.cpp` |
|---|---|---|
| 行数 / 用例数 | 545 / **12** | 203 / **5** |
| 测的组件 | `rv32i_core.h` 的 `HazardDetectionUnit` + `rv32i_branch_predict.h` | example 侧 `hazard_unit.h`（**前推启用版**） |

⇒ 二者测的是**不同组件** ⇒ example 副本有 5 个**根副本没有**的用例
⇒ 删除即丢覆盖 ⇒ **注册进 CTest**（实测 10 断言 / 5 用例全绿）。
⚠️ 计划原文的「删 example 副本前 MUST 确认根副本覆盖其断言」这道闸门救了这一手。

### R10. 实施期修正：`test_interrupt_flow.cpp` **不是**死脚手架 ⇒ 修好并注册

502 LOC / **10 用例 / 28 断言**（CLINT 定时器 / CSR 中断条件 / 陷阱入口 / MRET）。
修 3 处后全绿：①`sim.eval()` → `sim.tick()`（框架缺陷 F2）；
②补写 `mtimecmp_hi`（复位值 0xFFFFFFFF ⇒ 64 位比较的 hi 分支恒不相等 ⇒ mtip 永不触发）；
③读组合输出前先 tick 一次（`mstatus_value` 是 `io().x = reg` 的组合输出，不求值读到的是 0）。

### R11. 实施期发现：`enable_testing()` 排序是「编译但从不执行」的**机制性根因**

根 `CMakeLists.txt` 的 `enable_testing()` 原在第 324 行，`add_subdirectory(examples)` 在第 257 行
⇒ examples 子目录在 testing 启用**前**被处理 ⇒ **不生成** `CTestTestfile.cmake`
⇒ 其 `add_test()` 是**空操作**。⇒ 只补 `add_test` 而不调顺序，CTest 仍一个都认不出。
⇒ 必须把 `enable_testing()` 提到所有子目录之前。

### R12. 实施期发现：CLINT 地址解码是**真实 RTL bug**

`include/cpu/riscv/clint.h` 用 `bits<1,0>(io().addr)` 解码，但寄存器映射是 `0x0/0x4/0x8/0xC`
⇒ 必须用 `bits<3,2]` ⇒ 当前实现下 **4 个寄存器全部别名到 `mtime_lo`**，
`mtime_hi` / `mtimecmp_lo` / `mtimecmp_hi` **完全不可达**。
该缺陷零覆盖，因为 CLINT 唯一的测试从未被编译过。⇒ 修 1 行。

### R13. example 运行护栏必须认得 CTest 归属

`run_all_ported_tests.sh` 的「已构建但未跟踪」检查只看
`CH_TRACKED_EXAMPLE_TARGETS` ∪ `disabled_examples.txt` ⇒ 18 个已进 CTest 的
riscv-mini 测试二进制会被判为「无归属」。⇒ 该检查须把「已进 `ctest -N`」也算作归属
（`ctest -N` 是 CTest 条目的唯一真相）。

### R14. bash 陷阱：`pipefail` + `grep -q` ⇒ 明明命中却判未命中

`grep -q` 命中即提前关闭管道 ⇒ 上游 grep 收 SIGPIPE(141) ⇒ 在 `set -o pipefail` 下
整条管道被判为失败。⇒ 一律用 `grep -F ... > /dev/null`（读完整个输入，不提前关管道）。
（本 change 的两个脚本都踩过：护栏的 C1b 与 example 护栏的 CTest 归属检查。）

### R15. 对抗审稿（REJECT）后的护栏重写：三个不可退化的设计约束

审稿证伪了护栏的第一版（判据 = 解析 CMake + basename 匹配）：
① 根 `tests/CMakeLists.txt` 存在**同名** `test_rv32i_pipeline.cpp` ⇒ basename
匹配把 example 侧源文件误判成根 tests 的 ⇒ 注释掉 example 侧 target 护栏仍 exit 0；
② 只剥整行注释 ⇒ 行尾注释里的文件名被当成「已使用」；
③ 只认固定 helper 名 ⇒ 不走 helper 的 `add_executable` 完全看不见；
④ 对**自己治的那个病**（`enable_testing()` 顺序）零感知。

⇒ 重写后的三个约束（**勿退化**）：
1. 判据是 **`ctest -N`（CTest 条目的唯一真相）**；
2. 基准是**文件系统上的源文件集合**（外部事实），不是「CMake 里声明了什么」
   —— 否则删一条声明会让声明集与覆盖集**同步缩小** ⇒ 守恒恒等 ⇒ 对缩面无判别力；
3. 源文件→target 的映射 MUST 用**精确路径**，不得用 basename。
变异验证 5 条全被抓：注释掉 example 侧 target / 行尾注释塞文件名 / 新增不走
helper 的 `add_executable` / `enable_testing()` 移回 `examples` 之后 / 删除源文件。

### R16. Catch2 v3 的 exit 4：「全部跳过」= CTest 判红

对抗审稿的 B3 牵出这个机制：当一个 Catch2 二进制的**全部**用例都因「仓外资源
缺失」而被 SKIP 时，Catch2 v3 返回 **4**（= 没有真正跑起来的用例）⇒ **CTest 判 FAILED**。
⚠️ `ctest -N` 仍会列出它 ⇒「CTest 里认得它」**不等于**「它会绿」。

⇒ 处置：`test_riscv_tests_pipeline`（46 个用例全部依赖仓外二进制）**只构建、
不进 CTest**，登记在 `examples/disabled_examples.txt`（带归位条件）；
`CTEST_TOTAL` 因此是 **202**（基线 185 + **17**，不是 +18）。
⇒ 并给护栏加 **C3**：examples 的每个 CTest 条目 MUST **实际跑出 exit 0**
（实测约 12 s，与 ctest 跑一遍同量级）。

### R17. `test_pipeline_debug` 的 9 个 `SUCCEED` ⇒ 补真实断言

审稿的 S2 指出「删 debug4 的判据（0 断言 ⇒ 零价值）同样适用于保留的
`test_pipeline_debug`」—— 判据不自洽。⇒ **统一判据为「组件覆盖」**，并给
`test_pipeline_debug` 的 9 个用例**各补至少一个语义确定的输出断言**
（0x80000004 的 PC / 0x13 的 opcode / 5+7=12 的 ALU / 0x1234 的透传 / 0xABCD 的写回 /
stall/flush/forward_a）⇒ 从 **0 断言 / 9 SUCCEED** 变为 **14 断言 / 9 用例全绿**。
⚠️ pipeline 的 `commit_count` 只统计 **load** 指令（`mem_valid_out = is_load & mem_valid`）
⇒ 断言改用 `perf_cycles`（每拍递增、复位期冻结）。

### R18. 十六进制字面量的后缀是 `_h` 不是 `_d`

`ch_uint<32>(0x80000000_d)` 编译失败（`_d` = 十进制）⇒ 仓库约定十六进制用 `_h`。
⚠️ 且 `ch_uint<4>(0x33_h)` 也不行（0x33 需 6 bit）—— ex_stage 的 opcode 字段只有 4 bit。

### R19. 第二轮审稿（REJECT）后的 6 个设计约束

见 `scripts/verify_example_test_wiring.sh` 头部注释（**勿退化**）：
① 判据 = `ctest -N`；② 基准 = 文件系统源文件集合；③ 映射用精确路径；
④ 源文件枚举**按内容/目录**（不按 `test_*.cpp` 命名）；
⑤ 源文件总数 vs `EXAMPLES_TEST_SOURCES` **锚点**（防「源文件+声明一起删」）；
⑥ 删除留痕与豁免**分开**（`removed_examples.txt`）；
⑦ 运行期检查 MUST 同时看 **exit 0** 与**日志 error/fatal**。

变异验证 6 条全被抓：不叫 `test_*.cpp` 的测试源 / 藏在 `tests/` 之外的测试源 /
源文件+声明一起删 / 新建与已删项同名 / `enable_testing()` 移回 examples 之后 /
一个 target 打印 `[ERROR]` 但 exit 0。

### R20. 统一判据：examples 测试 target MUST 断言一个语义输出

9 个「0 断言」target ⇒ 6 补断言 + 3 删除。补断言手法：DUT 的语义输出引到顶层
`ch_out`，断言非平凡值。删除的 3 个各有明确理由（同型重复 / 纯脚手架 /
ADR-046 D10 冻结端口 ⇒ 零可观测输出）。

### R21. `debug` 的 2 个恒真断言（第二轮审稿 S7）

`DEBUG 3` 原本用 `addi x0,x0,0`(0x13) 断言 `rs1_addr==0 && rs2_addr==0` ——
该指令的 rs1/rs2 本来就是 x0 ⇒ 解码器坏了也过（恒真）。改用
`add x3, x1, x2`(0x002081B3) ⇒ 断言 opcode=0x33 / rs1=1 / rs2=2，才有判别力。

### R22. 陈旧产物 vs 无归属（`run_all_ported_tests.sh`）

删 target 后，本地旧构建目录残留的孤儿二进制会被 C1 报成「无归属」——
而它们**不是**归属问题。⇒ 新增 `is_declared_target()`（收窄到 `examples/` 域，
顺带解掉第二轮审稿 S8 的跨域同名风险）：不在任何 CMakeLists 里声明者报
`STALE-ARTIFACT`（不计失败），其余才是 `UNTRACKED`。

---

## File Structure

```
openspec/changes/riscv-mini-test-triage-2026-10/
├── proposal.md                                    # 已写
├── specs/example-runtime-coverage/spec.md         # 已写（ADDED 1 Requirement）
└── tasks.md                                       # 本 plan 派生

.rddf/plans/riscv-mini-test-triage-2026-10.md      # 本文件

examples/riscv-mini/
├── CMakeLists.txt                    # MODIFY：helper 加 add_test；删 3 个 target
├── src/                              # 不动
└── tests/
    ├── test_pipeline_minimal.cpp     # MODIFY：ch_module → fixture Component
    ├── test_pipeline_debug.cpp       # MODIFY：同上
    ├── test_pipeline_debug4.cpp      # DELETE
    ├── test_pipeline_debug5.cpp      # DELETE
    ├── test_riscv_tests.cpp          # MODIFY：修 6 处编码 + 41 处条件 SKIP + 路径可配
    ├── test_riscv_tests_soc.cpp      # MODIFY：sim.eval() → sim.tick()
    ├── test_rv32i_pipeline.cpp       # DELETE（example 副本，根副本是权威）
    ├── test_hazard_minimal.cpp       # DELETE
    └── test_interrupt_flow.cpp       # DELETE

examples/disabled_examples.txt        # MODIFY：移除 12 行归位；新增删除项登记
scripts/verify_example_test_wiring.sh # CREATE
.github/workflows/ci.yml              # MODIFY：护栏接入
AGENTS.md                             # MODIFY：CTEST_TOTAL + 计数说明
```

---

### Task 1: 建护栏脚本（先立尺子，再改代码）

**Files:**
- Create: `scripts/verify_example_test_wiring.sh`

- [ ] **Step 1: 写护栏脚本**

护栏规则（3 条）：
1. **C1 归属守恒**：每个 `examples/**/tests/test_*.cpp` ⇒ ①被注册进 CTest（`add_test` 覆盖其
   target）**或** ②登记在 `examples/disabled_examples.txt`；两者皆非 ⇒ exit 1。
2. **C2 数量守恒**：`add_test` 注册的 example 测试 target 数 == `disabled_examples.txt` 中
   「已归位」条目数 + 「仍豁免」条目数（总数守恒，防静默缩面）。
3. **C3 无反向 include**：`examples/**` 与 `tests/**` 不得 `#include` 对方路径之外的非本目录头。

- [ ] **Step 2: 脚本自身自证（变异验证）**

```bash
bash scripts/verify_example_test_wiring.sh; echo "exit=$?"   # 期望：当前 exit 1（尺子已立）
```

- [ ] **Step 3: 记录基线失败输出**（供 Task 6 对比）

---

### Task 2: 修 `test_pipeline_minimal` + `test_pipeline_debug`（ch_module → fixture）

**Files:**
- Modify: `examples/riscv-mini/tests/test_pipeline_minimal.cpp`
- Modify: `examples/riscv-mini/tests/test_pipeline_debug.cpp`

- [ ] **Step 1: 写失败测试（先复现 SIGSEGV）**

```bash
cd build && ./examples/riscv-mini/test_pipeline_minimal; echo "exit=$?"   # 期望 139
```

- [ ] **Step 2: 引入 fixture Component 模式**

在文件内定义一个承载 `ch_module` 的 `Component` 子类，**`ch_module` 放在 `describe()` 内**：

```cpp
class PipelineSmokeTop : public ch::Component {
public:
    PipelineSmokeTop(ch::Component* parent = nullptr, const std::string& name = "top")
        : ch::Component(parent, name) {}
    void create_ports() override {}
    void describe() override {
        ch::ch_module<Rv32iPipeline> pipeline{"pipeline"};
        pipeline.io().rst <<= ch_bool(true);
        // ... 其余输入接地
    }
};
```
⚠️ `ch_module` 是 `describe()` 的**局部变量** ⇒ `describe()` 结束即析构 ⇒
**所有 `io()` 访问 MUST 发生在 `describe()` 内部**。若测试需要在 `describe()` 之外读端口，
改用 `ch_out` 把观测值导出到顶层（与 `tests/test_stream_fifo_dual.cpp` 的约束同型）。

- [ ] **Step 3: 逐个 stage 重复该模式**（`test_pipeline_debug` 的 DEBUG 1..9）

- [ ] **Step 4: 验证绿**

```bash
cd build && ./examples/riscv-mini/test_pipeline_minimal; echo "exit=$?"   # 期望 0
cd build && ./examples/riscv-mini/test_pipeline_debug;   echo "exit=$?"   # 期望 0
```

---

### Task 3: 修 `test_riscv_tests_soc.cpp`（eval → tick）

**Files:**
- Modify: `examples/riscv-mini/tests/test_riscv_tests_soc.cpp`

- [ ] **Step 1: 复现**

```bash
cd build && ./examples/riscv-mini/test_address_decoder; echo "exit=$?"   # 期望 139
```

- [ ] **Step 2: 全部 `sim.eval()` → `sim.tick()`**（4 处）

- [ ] **Step 3: 验证**

```bash
cd build && ./examples/riscv-mini/test_address_decoder; echo "exit=$?"   # 期望 0
```

---

### Task 4: 修 `test_riscv_tests.cpp`（编码 + 条件 SKIP + 路径可配）

**Files:**
- Modify: `examples/riscv-mini/tests/test_riscv_tests.cpp`

- [ ] **Step 1: 修 6 处指令编码**（按 R4 表）

- [ ] **Step 2: 强化 load/store 断言**（当前 `lw x2, 0(x1)` 写入的 `x2` 原本就等于期望值 ⇒
断言恒真）⇒ 改为 `lw x3, 0(x1)` 并断言 `regs[3] == test_value` ⇒ 断言才真正区分
「load 生效」与「load 没生效」。

- [ ] **Step 3: `RISCV_TESTS_PATH` 支持环境变量覆盖**

```cpp
const char* RISCV_TESTS_PATH_DEFAULT = "/workspace/project/riscv-tests/build/rv32ui/";
// 优先读环境变量 RISCV_TESTS_PATH
```

- [ ] **Step 4: 41 个依赖仓外二进制的用例在资源缺失时显式 SKIP**

在 `run_riscv_test()` 入口处判存在性 → `SKIP("riscv-tests binary not present: " + path)`。
⚠️ `SKIP()` 从被测函数内部抛出，Catch2 v3 支持（会中止该 test case 并标记为 skipped）。

- [ ] **Step 5: 验证**

```bash
cd build && ./examples/riscv-mini/test_riscv_tests; echo "exit=$?"   # 期望 0（41 skipped + 8 passed）
```

---

### Task 5: 删 3 个测试源文件 + 3 个 target

**Files:**
- Delete: `examples/riscv-mini/tests/test_pipeline_debug4.cpp`
- Delete: `examples/riscv-mini/tests/test_pipeline_debug5.cpp`
- Delete: `examples/riscv-mini/tests/test_rv32i_pipeline.cpp`（example 副本）
- Delete: `examples/riscv-mini/tests/test_hazard_minimal.cpp`
- Delete: `examples/riscv-mini/tests/test_interrupt_flow.cpp`
- Modify: `examples/riscv-mini/CMakeLists.txt`（删对应 target 行）

⚠️ `test_rv32i_pipeline.cpp`（example 副本）**不是**根 `tests/test_rv32i_pipeline.cpp`
的重复文件（545 行 vs 203 行，内容不同）—— 根副本才是权威且已被根 `tests/CMakeLists.txt`
注册。删 example 副本前 MUST 确认根副本覆盖其断言。

- [ ] **Step 1: 逐个确认根副本已覆盖**

```bash
grep -c "TEST_CASE" tests/test_rv32i_pipeline.cpp examples/riscv-mini/tests/test_rv32i_pipeline.cpp
```

- [ ] **Step 2: `git rm` 5 个源文件 + 删 CMake target 行**

- [ ] **Step 3: 构建通过**

```bash
cmake --build build -j$(nproc) 2>&1 | grep -E "error|Error" ; echo "build_exit=$?"
```

---

### Task 6: 12 个绿 target 注册进 CTest

**Files:**
- Modify: `examples/riscv-mini/CMakeLists.txt`
- Modify: `examples/disabled_examples.txt`

- [ ] **Step 1: 在 `riscv_mini_catch_test()` helper 内加 `add_test`**

```cmake
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
```
⚠️ `add_test` 的 `COMMAND` 用 target 名（CMake 会解析为可执行文件路径）。

- [ ] **Step 2: 验证 CTest 认得它们**

```bash
cd build && ctest -N | grep -c "riscv-mini"    # 期望 ≥ 12
cd build && ctest -L base -R "test_pipeline_smoke" --output-on-failure
```

- [ ] **Step 3: `examples/disabled_examples.txt` 移除 12 行归位条目** + 为 5 个删除的源文件
补登记行（带根因 + 归位条件）

---

### Task 7: CI 接入护栏 + `CTEST_TOTAL` 同步

**Files:**
- Modify: `.github/workflows/ci.yml`
- Modify: `AGENTS.md`

- [ ] **Step 1: 护栏接入 CI**

在已有 `examples-runtime` / `samples-regression` job 附近加一步
（`bash scripts/verify_example_test_wiring.sh`）。

- [ ] **Step 2: 实测新的 CTest 总数并同步 `AGENTS.md`**

```bash
cd build && ctest -N | tail -1
cd build && ctest -N -LE perf | tail -1
```
把 `CTEST_TOTAL: <N>` 与 §NOTES 计数说明块同步到实测值。

- [ ] **Step 3: 跑计数护栏**

```bash
bash scripts/verify_test_counts.sh; echo "exit=$?"
```

---

### Task 8: 全量回归 + 归档前 gate

- [ ] **Step 1: 全量非 perf 回归**

```bash
cd build && ctest -LE perf --output-on-failure | tail -5
```

- [ ] **Step 2: delta 自检 + openspec validate**

```bash
bash scripts/verify_openspec_delta.sh riscv-mini-test-triage-2026-10; echo "exit=$?"
openspec validate riscv-mini-test-triage-2026-10 --strict; echo "exit=$?"
```
⚠️ 本仓 CLI 钉在 1.4.1；本机是 1.14.3 ⇒ 「Requirement 正文 > 500 字符」「Purpose 仍是占位符」
两条新规则会让**既有主 spec** 变红（与本 change 无关）⇒ 只看**本 change** 的 delta 报告。

- [ ] **Step 3: 护栏绿**

```bash
bash scripts/verify_example_test_wiring.sh; echo "exit=$?"
```

---

## Notes for implementer

- **不要**为了「让护栏变绿」而删除有真实断言价值的测试（`test_pipeline_debug` 的 9 个用例
  是 5 个 stage 的逐组件冒烟）。删除只在「用例互为重复」或「前提不可实现」时进行。
- **不要**在本 change 里修 `include/**` / `src/**` 的框架缺陷（R2 / R3）—— 那是独立 change；
  本 change 只**登记**。
- `SKIP()` 与 `REQUIRE()` 的区别必须体现在 CTest 输出里：SKIP 计入 skipped，**不**计入 failed。
- 删除的测试源文件 MUST 在 `examples/disabled_examples.txt` 留痕（理由 + 归位条件），
  否则 Task 1 的护栏 C1 会红。
