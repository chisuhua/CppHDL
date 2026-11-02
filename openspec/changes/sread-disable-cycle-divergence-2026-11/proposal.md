# 提案：`sread` 的**禁读拍**语义在两条后端不一致（真 parity 分歧）

- **登记来源**：`dual-backend-parity-route-b-pipeline-2026-11` 实施期实测（2026-11-02）
  写 `chlib::memory` 的 dual 覆盖时被**第一次**撞出
- **规格影响**：**有**（capability `mem-read-timing` 需新增/订正一条 Requirement）
- **规模**：1 行代码 + 1 条 capability delta + 双后端判据 + `chlib::memory` 的 dual 覆盖
- **严重度**：🔴 **真分歧** —— 同一个设计在两条后端上给出**不同输出**

## 分歧本体（两侧代码逐字对照）

`ch_mem::sread(addr, enable)` 在 **`enable == 0`（禁读拍）** 时：

| 后端 | 行为 | 代码位置 | 代码 |
|---|---|---|---|
| **interpreter** | **保持上一拍的值** | `src/ast/instr_mem.cpp:258-259` | `if (!enabled) return;` |
| **Verilog / Verilator** | **清零** | `src/codegen_verilog.cpp:1261` | `else proxy <= 1'b0;` |

⇒ ⇒ **同一个 DUT、同一个 tick，两后端输出不同** ⇒ ⇒ 这是 `dual-backend-test-coverage`
要抓的**正是这一类**问题，之前一直没抓到是因为**这条路径从未被双后端覆盖**。

## 精确影响面（已实测确认，不是推测）

**① `aread` 不受影响**：`ch_mem::aread` 传给 `enable` 的是 `nullptr`
（`include/core/mem.h:148` 的 `create_mem_read_port(..., nullptr, ...)`）
⇒ ⇒ `instr_mem.cpp` 与 codegen 两侧都走「无 enable ⇒ 恒读」分支。

**② 只有「带 `enable` 且会取 0」的 `sread` 受影响**：

| 位置 | 用法 | 是否受影响 |
|---|---|---|
| `include/chlib/memory.h:34`（`single_port_ram`） | `mem.sread(addr, !we)` | 🔴 **是** |
| `include/chlib/memory.h:63` / `:67`（`dual_port_ram` 两端口） | `mem.sread(addr_x, !we_x)` | 🔴 **是** |
| `include/chlib/fifo.h:70` / `:418` | `sread(read_ptr, ch_bool(true))` | 否（恒使能） |
| `tests/test_mem_read_timing_dual.cpp:63` | `sread(addr, ch_bool(true))` | 否（恒使能） |
| `examples/riscv-mini/.../rv32i_pipeline_top.h` | `sread(io().addr, …)` | 取决于 RTL 是否传恒 1 |

⇒ ⇒ **生产组件里唯一踩中的是 `chlib::memory` 的 RAM 族**（**写的那一拍**的输出）。

**③ 既有断言覆盖 = 0**：`tests/test_mem.cpp` 里 `sread` 相关的 4 个 SECTION
**全部是结构断言**（`impl() != nullptr` / `has_enable()`），**不跑仿真**
⇒ ⇒ 没有任何断言钉住禁读拍的输出值 ⇒ ⇒ 分歧**不可能被既有测试发现**。

## 为什么这**不是**「我们该有的行为差异」

`codegen_verilog.cpp` 的 `else proxy <= 1'b0;` 附带的注释写的是

> Reset branch clears the read proxy (matches the CppHDL Simulator
> data_map default-zero), preventing X from reaching the datapath.

⇒ ⇒ **作者意图是「与 Simulator 一致」**，但实测不一致（Simulator 保持、Verilog 清零）
⇒ ⇒ **是实现与注释意图之间的 bug**，不是有意的语义分工。

## 需要裁决的一件事（**实施前 MUST 定**）

两侧对齐到哪一边？**两种都能做到「一致」**，但契约不同：

### 方案 A：**对齐到 Verilog（禁读拍清零）**

- 改动：`instr_mem.cpp` 的 `if (!enabled) return;` → 写 0 到输出后返回
- 契约：**禁读拍输出恒 0**
- 优点：① 与 codegen **现有**注释的意图一致；② 「不读就没有数据」比「读到上一拍的陈旧值」更不容易被误用（陈旧值可能被下游当成有效数据）；③ 不改 codegen ⇒ 生成的 Verilog 对既有用户**零变化**
- 缺点：**改变 interpreter 的既有行为** ⇒ 依赖「保持」的下游会看到 0。⚠️ 已知受影响者 = `chlib::memory` 的 RAM 族（**写拍**输出由「上一拍读值」变 0）⇒ 需要同步其 dual 期望值（本来就是待补的）

### 方案 B：**对齐到 interpreter（禁读拍保持）**

- 改动：删掉 `codegen_verilog.cpp:1260-1261` 的 `else proxy <= 1'b0;`
  （`always_ff` 无 else 分支天然保持）
- 契约：**禁读拍输出保持上一拍的值**
- 优点：不改 interpreter
- 缺点：① 与 codegen **现有注释的明示意图相反**；② **`always_ff` 无复位分支**时，禁读拍会保留**复位前的 X/未知值** ⇒ 正是该注释要防的事（`preventing X from reaching the datapath`）⇒ ⇒ 需要另加复位分支处理，改动面反而更大

### 方案 C（**不推荐**）：允许两后端不同 ⇒ 把分歧写进契约

- 与 `mem-read-timing` §Purpose 的「两者**完全一致**」**直接矛盾**
  ⇒ ⇒ 等于把 drift 制度化（该 capability 的立项目的正是消除 drift）

> **建议**：**方案 A**。理由：改动只在一侧（interpreter），codegen 零变化；
> 且「清零」比「保持陈旧值」更安全。⚠️ 但这属**契约级选择**
> ⇒ ⇒ 实施前按仓库纪律请 architect / 用户裁决（可能需要 ADR）。

## What Changes（实施时）

| 文件 | 内容 |
|---|---|
| `src/ast/instr_mem.cpp` | 按裁决把禁读拍行为对齐（方案 A ⇒ 写 0） |
| `openspec/specs/mem-read-timing/spec.md` | **ADDED** 一条 Requirement：`sread` 禁读拍的输出语义在两条后端 MUST 一致，且 MUST 有双后端判据 |
| `tests/test_mem_timing_dual.cpp`（**新**）或扩充既有 dual | 带 `enable` 的 `sread` 在**禁读拍**的逐拍双后端判据（N2′） |
| `tests/chlib/test_memory_dual.cpp`（**新**） | 补回 `chlib::memory` 的 dual 覆盖（写拍输出按新契约）⇒ 复用留档 `/tmp/opencode/sread-divergence/test_memory_dual.cpp` |
| `AGENTS.md` | `mem-read-timing` 的「Memory read port timing」段补禁读拍语义 |

## 为什么**不在** `dual-backend-parity-route-b-pipeline-2026-11` 里顺手修

`plan §6 风险 3` 的明文纪律：**dual 覆盖暴露真实 parity 分歧 ⇒ 停下开独立 change**，
不在覆盖 change 里顺手改 codegen / interpreter。理由：覆盖 change 的验收面是
「新增判据」，一旦掺入**行为变更**，就无法区分「新测试红了是因为期望值写错」
还是「框架被改坏了」—— 且 `chlib::memory` 的期望值**依赖**修复方向（A/B 不同）。

## Verification（前瞻 · 可勾选）

- [ ] 裁决落地（A / B / C 之一），并在 proposal/design 记录裁决理由
- [ ] 禁读拍的**逐拍**双后端判据存在且**期望值手推**
- [ ] N2′：断言数(with verilator) == 2 × 断言数(interpreter-only)
- [ ] 变异：把禁读拍行为改回**分歧**的那一侧 ⇒ **必红**（这是本 change 的核心判别力）
- [ ] `chlib::memory` 的 dual 覆盖补回（3 个 RAM 变体）
- [ ] `ctest -LE perf` 零回归；`ctest -L verilator_e2e` 零回归
- [ ] `openspec validate mem-read-timing --strict` valid
