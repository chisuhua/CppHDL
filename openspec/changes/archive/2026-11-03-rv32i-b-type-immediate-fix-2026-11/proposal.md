# 提案：RV32I B 型立即数 bit 11 修复（ADR-063 实施）

- **ADR**：`docs/adr/ADR-063-rv32i-b-type-immediate-bit11.md`（**Accepted** 2026-11-03）
- **立项来源**：`.rddf/improvements/rv32i-b-type-immediate-bit11-misplaced-2026-11.md`
  （路线 C 第 1 批的手推期望表挖出，独立探针确认）
- **关联 capability**：`rv32i-isa-semantics`（**ADDED ×1**）⇒ 归档**禁** `--skip-specs`
- **规模**：2 处各 1 行实现修复 + 3 处 golden + 1 个新 dual pair（等价性判据）
  + 1 条 capability Requirement（4 Scenario）
- **行为变更**：是（`|偏移| ≥ 2048` 的条件分支目标从「错 2047」变为正确）

## Why

RISC-V 的 B 型立即数布局是 `imm[12|10:5|4:1|11] = instr[31|30:25|11:8|7]`
⇒ `instr[7]` 承载 **`imm[11]`**。仓内**两处**实现都把 `instr[7]` **未移位**就 OR 到
立即数的 bit 0：

| 文件 | 位置 | 性质 |
|---|---|---|
| `include/cpu/pipeline/rv32i_decoder.h` | `:113` | 组件（3 个 example 消费者） |
| `include/cpu/pipeline/stages/id_stage.h` | `:145` | **生产路径**（ID/EX 流水线寄存器） |

两处的**注释都写着 `imm[11]`**，代码却把它放在 bit 0 ⇒ **笔误**，不是设计选择。

### 实测误差

| 指令 | 编码 | 规范值 | 修复前 | 误差 |
|---|---|---|---|---|
| `beq x1,x2,-8` | `0xFE208CE3` | `0xFFFFFFF8` | `0xFFFFF7F9` | **2047** |
| `beq x1,x2,+2048` | `0x002080E3` | `0x00000800` | `0x00000001` | **2047** |
| `beq x1,x2,+8` | `0x00208463` | `0x00000008` | `0x00000008` | 0 |

第三行解释了**为什么长期未暴露**：`|偏移| < 2048` ⇒ `imm[11] = 0` ⇒ 误置的 bit 0 也是 0
⇒ 无误差。仓内**所有**固件的条件分支偏移都是 `+8`。

### 为什么两后端 parity 看不见它

`Rv32iDecoder` / `IdStage` 是纯组合（后者寄存器只搬运组合值）⇒ 同一份 C++ 描述同时生成
interpreter 求值树与 Verilog ⇒ **两后端一致地错** ⇒ parity 恒绿。同型先例：ADR-047 的
「距离 3 读旧值」。⇒ 唯一能抓它的是**手推期望值**（正是路线 C 干的事）。

## What Changes

| 文件 | 改动 |
|---|---|
| `include/cpu/pipeline/rv32i_decoder.h` | `bits<7,7>(instr)` → `(bits<7,7>(instr) << ch_uint<4>(11_d))` |
| `include/cpu/pipeline/stages/id_stage.h` | 同上（生产路径） |
| `tests/test_rv32i_stage_dual.cpp` | 2 处期望值改规范值；**新增 pair 4**（D4 等价性判据） |
| `tests/test_rv32i_timing_dual.cpp` | 1 处期望值改规范值 |
| `tests/CMakeLists.txt` | 给 `test_rv32i_stage_dual` 补 `include/cpu/pipeline` 路径（`id_stage.h` 非自包含的绕过，沿用既有先例） |
| `openspec/specs/rv32i-isa-semantics/spec.md` | ADDED ×1（1 Requirement + 4 Scenario） |

### golden 影响面（实测确认「不变」）

| 位置 | 结论 |
|---|---|
| `test_rv32i_soc_dual_parity.cpp` 全部 golden | **不变**（固件分支偏移全 `+8`） |
| `test_rv32i_pipeline_behavior.cpp:573` | **不变**（B 型立即数 = 0） |
| `examples/riscv-mini/rv32i_phase{1,2,3}_test.cpp` | **不变**（读 `decoder.imm`，但 B 型用例 imm 全为 0、S 型为 256） |
| `ctest -LE perf` | **227/227 绿** |

## D4 等价性判据（为什么加、它抓不到什么）

新增的 `ImmConsistencyDut` 把 `Rv32iDecoder` 与 `IdStage` **并排实例化**、喂同一条指令，
断言 `dec_imm == id_imm`。它防的是**「只改一处」的复发**。

⚠️ **如实登记的判别力边界**：两份实现**一起错**时该判据恒绿（实测变异 M-c 证实）
⇒ 「一起错」由各自的**绝对期望值表**抓 ⇒ 两者互补，缺一不可。

## 变异证据（每条先确认仍可编译；还原用 `cp` 后重建）

| 变异 | 内容 | 结果 |
|---|---|---|
| M-a | **只**回退 `rv32i_decoder.h` 的 `<<11` | 🔴 C1 **4 failed**（DecCase×2 + D4 pair×2）；C2 绿（不涉及 decoder） |
| M-b | **只**回退 `id_stage.h` 的 `<<11` | 🔴 C1 2 failed（D4 pair）+ C2 2 failed |
| M-c | 两处都回退（= 缺陷形态） | 🔴 C1 2 + C2 2（⚠️ D4 pair **绿**：两边一致地错 ⇒ 边界已登记） |
| M-d | `<<11` 写成 `<<12` | 🔴 C1 2 + C2 2 |
| M-e | `sext<32>` 换成零扩展 | 🔴 C1 2 + C2 2 |

⇒ **5/5 全抓**，其中 M-a / M-b 是本 ADR 的核心判据（直接回答「D4 补偿是否有效」）。

⚠️ **实施期如实登记两件事**：
1. M-d 首版的替换文本把行尾分号吃掉了（注释覆盖 `;`）⇒ **编译失败** ⇒ 注入器正确地
   报 "not judged"（未声称抓到）⇒ 修正后重跑才转红。这是 T4「fail-closed」的另一面：
   正则命中 ≠ 语法有效。
2. **M-c 下 D4 pair 恒绿** ⇒ 见上「判别力边界」。

## Verification（已全部实测回填）

- [x] `beq x1,x2,-8` ⇒ `0xFFFFFFF8`（两处实现）
- [x] `beq x1,x2,+2048` ⇒ `0x00000800`
- [x] `beq x1,x2,+8` ⇒ `0x00000008`（负向对照）
- [x] 两份实现一致性判据存在且可执行
- [x] 3 处 golden 已改；「不变」清单逐项实测成立
- [x] 变异 5/5 抓（含判别力边界登记）
- [x] 全量重建后 `ctest -LE perf` **227/227** 绿
- [x] `openspec validate --strict` + `verify_openspec_delta.sh` 通过
- [x] capability ADDED ×1 ⇒ 归档**未**用 `--skip-specs`

## 计数变化

| 锚点 | 变化 | 说明 |
|---|---|---|
| `CTEST_TOTAL` | **238 不变** | 无新 ctest 条目 |
| `VERILATOR_E2E_TOTAL` | **70 不变** | 无新 target |
| `DUAL_BACKEND_PARITY_TOTAL` | **61 不变** | 无新源文件 |
| `DUAL_BACKEND_PARITY_PAIRS` | **271 → 272** | C1 新增 1 个 pair |

⚠️ **这正是 objective §2 已登记的「口径盲区」实例**：在既有 dual 文件里加 pair 对
**源文件数**零贡献（但边际价值高）⇒ 口径奖励「开新文件」而非「加深既有文件」
⇒ 归 planner（`objective-test-coverage-100pct` 的 T-10）。

## 不在本次范围（ADR-063 D8）

- 立即数提取「两份 → 单一实现」的收敛（选项 F，代码级去重）⇒ 独立议题，
  建议与 `id-stage-header-not-self-contained-2026-11` 合并
- 移位量容器宽度统一（「刚好够」的写法虽脆弱但当前全部正确）
- `id_stage.h` 非自包含本身（零行为影响，已独立立项）
- `ex_stage.h` 的 `BRANCH_*` 常量与 funct3 相反（零消费者，已独立立项）
