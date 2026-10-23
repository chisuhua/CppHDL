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
