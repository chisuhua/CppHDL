# RV32I 微架构 Architecture

> **主题文档**（per ADR-0057 的「ADR + theme-doc」双工件模型）：本文是 RV32I 微架构的
> **组合叙事 + 当前架构快照**；**裁决本身**在 Primary ADRs 里，不在本文。
>
> **范围**：`include/cpu/pipeline/**`（5 级流水线与冒险/前推）、`include/cpu/cache/**`、
> `include/cpu/riscv/**`、以及核 IP 顶层 `examples/riscv-mini/src/rv32i_core_top.h`。
> **不**覆盖：SoC 之外的 AXI 互连层、bundle/stream 组件库、框架层的仿真与 codegen 语义
> （那些见各自的主题文档 / capability）。
>
> **最后同步**：2026-11-11（随 B0 批 C5+C6 退役回填 —— 三个组件物理退役 + 残留表订正）

## Primary ADRs

| ADR | Title | Status | 管什么 |
|---|---|---|---|
| **ADR-046** | RV32I 的权威单一归属地 —— `include/cpu/` 为唯一权威 | Accepted（2026-10-23） | 收敛方向：lib 侧为唯一实现、example 侧只留 SoC 顶层；**D10 = SoC 32 端口冻结** |
| **ADR-047** | RV32I 流水线的数据前推 —— 裁决「接」、形态与验收判据 | Accepted（2026-10-27） | 前推**接**、mux 归属、档位语义与时间对齐、距离矩阵的契约面、**D11 = 距离 3 的读点旁路** |
| **ADR-048** | RV32I 取指的 skid buffer 与填充期语义 | Accepted（2026-10-27） | 取指侧的**停顿不丢指令**（skid buffer）与**填充期不双装**（`primed` 门控）；**明确否决 PC 回退** |
| ADR-041 | cache 子系统拆分为 Sync (1-cycle `sread`) + Async (0-cycle `aread`) | Accepted | **imem MUST 用 `sread`**（1 拍）⇒ 取指对齐与 `pc_lag` 的前提 |
| ADR-039 | cache complete 架构重写路径 —— 用 `ch_mem` 替代 `ch_reg` 数组 | Accepted（2026-10-18） | cache 存储原语（`ch_mem`）⇒ D-cache 的读-改-写字节道写 |
| ADR-037 | cache stub 物理删除策略 | Accepted（2026-10-17） | 历史 stub 的退役（2026-10-20 提前落地，variant b） |
| ADR-038 | `forwarding.h` namespace + location cleanup | Accepted（2026-10-17） | forwarding 实现的归属（`chlib::ForwardingMux` 是唯一数据 mux） |
| ADR-035 | Verilator 仿真后端 | 🟢 采纳 + e2e 验证通过 | 双后端之一；**64-bit 端口上限**、2-step clock toggle、reset pulse ⇒ 直接约束 RV32I 的可观测面设计 |

## 当前架构

### 1. 归属与目录（ADR-046）

```
include/cpu/                          # RV32I 微架构的**唯一权威**
├── pipeline/                        # 流水线数据通路（15 个头 = 10 + stages/5）
│   ├── rv32i_pipeline.h             # 5 级顶层（装配 + dbg_* 观测面）
│   ├── rv32i_pipeline_regs.h        # IF/ID/EX/MEM/WB 流水线寄存器（490 行）
│   ├── rv32i_decoder.h              # R/I/S/B/U/J 译码
│   ├── hazard_unit.h                # 冒险检测 + 停顿/冲刷控制（ADR-047：删 5 个预留端口）
│   ├── rv32i_alu.h / rv32i_regfile.h / rv32i_pc.h
│   ├── rv32i_branch_predict.h / branch_predictor_v2.h
│   ├── rv32i_tcm.h                  # InstrTCM / DataTCM（ch_mem 底座）
│   └── stages/{if,id,ex,mem,wb}_stage.h
├── cache/                           # L1 cache（8 个头）：{i,d}_cache_{sync,async}.h（4）+ {i,d}_cache_complete.h（2 · 兼容 shim 别名）+ {i_cache_prefetch,d_cache_write_buffer}.h（2 个 wrapper）
└── riscv/                           # 特权级（4 个头）：CSR / exception / address_decoder / clint

examples/riscv-mini/src/rv32i_core_top.h   # **唯一**留在 example 侧的头（核 IP 顶层）
```

⚠️ 收敛前 `include/cpu/pipeline/` 与 `examples/riscv-mini/src/` 是**两套不同架构**；
ADR-046 D1–D12 把它们收敛为一套 ⇒ 护栏 `scripts/verify_no_duplicate_rv32i_headers.sh`。

⚠️ **2026-11-11 · B0 批 C5 + C6 同批退役** —— 下列三个头文件已**物理删除**
（不是 stub）：`rv32i_forwarding.h`（`riscv::ForwardingUnit`）·
`hazard_detection_unit.h`（`riscv::HazardDetectionUnit`）·
`rv32i_hazard_complete.h`（`riscv::HazardUnitComplete`）
⇒ ⇒ 理由 = **零交付路径消费者** + 独有语义为空集/死语义 + capability **不指名**它们
⇒ ⇒ ⇒ **这是显式批准的覆盖缩面**（用户 2026-11-11 批准）
⇒ ⇒ ⇒ 权威登记 = `.rddf/checks/public-api-surface.md` 的「已移除 / 已改名的公开符号」章
⇒ ⇒ ⇒ 仓内**权威**的 hazard 组件 = `hazard_unit.h`（`riscv::HazardUnit`），
流水线侧的数据 mux 组件 = `include/chlib/forwarding.h`（`chlib::ForwardingMux`）
⇒ ⇒ ⚠️ 但**逻辑层**还有同头内的函数式副本 `chlib::forward_select<>`（`include/chlib/forwarding.h:353-376`）
—— 该头 `:343-345` 自陈「本函数与组件的 `describe()` 是**同一份逻辑的两处实现**」
⇒ ⇒ ⇒ 故本文**不说**「唯一实现」（`.rddf/improvements/rv32i-dead-component-consolidation-2026-11.md:275`
已把「同一语义 MUST NOT 有两份实现」判为**永远为红**）⇒ ⇒ 只说「**流水线侧唯一的数据 mux 组件**」
⇒ ⇒ ⚠️ **保留但未接入流水线**的组件（`rv32i_branch_predict.h` / `branch_predictor_v2.h`
/ `rv32i_pc.h`）= 见 `include/cpu/AGENTS.md` 的「保留但不接入流水线」登记表。

### 2. 5 级数据通路

```
imem(sread 1拍) ──► IF/ID ──► ID/EX ──► EX/MEM ──► MEM/WB ──► WB ──► regfile
                     ▲          │          │          │
                     │          └─ 前推 mux ─┘         └─ 写回
                     └─ 冒险检测（EX 级 load vs ID 级消费者）⇒ stall / flush
```

| 级 | 组件 | 关键契约 |
|---|---|---|
| IF | `IfStage` | PC 寄存器（复位 0x80000000）；`fetch_pc_reg` 提供 **1 拍延迟**以对齐 imem 的 `sread`；`flush_d1` 让 flush **多杀一拍**（错误路径指令永不进 ID） |
| ID | `IdStage` | 译码 + 寄存器堆读；**stall 时 ID/EX 装「气泡」而非冻结**（数据字段会刷新但 valid=0） |
| EX | `ExStage` | ALU + 分支判定；`branch_taken`/`branch_target` 的语义是「**重定向成立/目标**」（含 `JAL`/`JALR`） |
| MEM | `MemStage` | 数据访存；`store_byte_enable_out` 供字节道写 |
| WB | `WbStage` | 写回；`link_value` 是 `JAL`/`JALR` 的返回地址通路（**不是** ALU 结果） |

### 3. 冒险与前推（ADR-047）

- **数据前推**：档位 `0` = 寄存器堆、`1` = EX/MEM 级、`2` = MEM/WB 级的合并写回值；
  档位 select **打一拍**（`fwd_a_d1`/`fwd_b_d1`）⇒ 档位编号 = **延迟之后**的语义；
  档位 `1` 的源选择收在 `chlib::ForwardingMux` **组件内部**（`is_jump`/`jump_result` 端口）；
  mux = **既有组件**（零新增组件）⇒ 数据源叶子 MUST 是流水线寄存器（防组合环）。
- **保守边界**：档位 `1` **不覆盖 load**（`& !ex_is_load`）⇒ load-use 停顿契约**零回退**。
  ⚠️ **2026-11-12 dated 注记（ADR-069）**：**「零回退」指的仍是「档位 1 不覆盖 load」这条语义不变**
  ⇒ ⇒ 但 **load-use 停顿的**拍数**已由 `docs/adr/ADR-069-mem-data-alignment.md` 修订（3 → 4 拍）**
  ⇒ ⇒ 推导见 `docs/adr/ADR-047-rv32i-data-forwarding.md` §5.5 ⇒ ⇒ **原文逐字保留**（历史语境）。
- **距离 3**（生产者与消费者相隔 2 条指令）不经前推档位 ⇒ 由**寄存器堆读点的 WB→ID 组合旁路**解决
  （ADR-047 D11）—— ⚠️ 该缺陷**两后端一致地错** ⇒ 双后端 parity **抓不到** ⇒ 靠判别性固件。

### 4. 存储层次

| 部件 | 实现 | 时序 | 约束 |
|---|---|---|---|
| imem（`InstrTCM`） | `ch_mem` + `sread` | **1 拍** | ADR-041 强制 `sread`（`aread` 会打破 `pc_lag`）⇒ 取指地址与 `fetch_pc_reg` 对齐 |
| dmem（`DataTCM`） | `ch_mem` + `aread` | **0 拍** | MEM 级在**当前拍**采样读数据 |
| L1 cache | `ICacheSync` / `DCacheSync` | 1 拍 `sread` | RV32I L1 默认；Async 变体是 TCM-style 备选 |
| 地址单位 | 字节地址（PC）↔ 字索引（`ch_mem`） | — | SoC 侧 `bits<kTcmAddrWidth+1, 2>` 转换 |

⚠️ **字节道写**：`DataTCM` 无字节使能写 API ⇒ 写路径是**读-改-写**（`strbe` 端口 + `byte_lane.h`）。

### 5. 核 IP 顶层与可观测面（ADR-046 D10）

`rv32i_core_top.h` 暴露 **32 个端口**（5 输入 + 27 输出 = 11 功能 `obs_*` + 16 诊断 `dbg_*`）：

- **为什么需要这么多**：verilator **只能读顶层端口** ⇒ 逐拍观测（指令流 / 有效链 / 写回 /
  冒险脉冲）必须经顶层导出 ⇒ 少了 ⇒ 「两后端逐拍全等」无法验证。
- ⚠️ **D10 冻结**：这 32 个端口被**冻结** ⇒ 新增观测端口 ⇒ **先改 D10**（rdd-arch 职责），
  **不得**在 change 内自行扩端口（ADR-048 D12 同此约束）。

### 6. 契约的归属（capability）

| capability | 治什么 |
|---|---|
| `rv32i-canonical-home` | 归属与「唯一权威」的不变量 |
| `rv32i-commit-path` | 退休语义 / 有效链 / 级间信号不串级 / 观测面 / 停顿契约 |
| `rv32i-isa-semantics` | ISA 语义边界（字节道 / 跳转 / SRA / 立即数） |
| `rv32i-data-forwarding` | **距离矩阵的唯一权威**（档位 / 时间对齐 / 判别性证据） |
| `verilator-rv32i-e2e` | RV32I 在 verilator 下的 e2e 契约（观测面 / 双路 parity / JIT 回落） |
| `mem-read-timing` | `aread` / `sread` 的时序语义（Verilog 对齐） |
| `cpu-cache-subsystem` | cache 子系统语义 |

⚠️ **「距离矩阵」只有一份**：其它文档 MUST **引用** `rv32i-data-forwarding`，MUST NOT 自述
⇒ 两份必然漂移。

## 已知残留与归属（**如实登记**，非待办清单）

| 残留 | 现状 | 归属 |
|---|---|---|
| ~~load-use 停顿**丢弃「消费者之后的第一条指令」**~~ | ✅ **已修**（1 级 skid buffer；「停顿点之后 MUST 是 NOP」的固件约束已取消） | ✅ ADR-048 D1–D5（change `rv32i-fetch-skid-buffer-2026-10`） |
| ~~取指填充期**同一条指令被装两次**~~ | ✅ **已修**（`primed` 门控 `valid` ⇒ 退休数 179 → **178**、`non_retired` 20 → **21**） | ✅ ADR-048 D6（同一 change 的 C2） |
| 停顿之后的**重复装载**（同一 word 连装两拍） | ✅ **被 skid 的排空拍顶替**（既有金标零变化）⇒ 该形态已无独立项 | ✅ 已随 C1 消除（ADR-048 未记载，实施时实测发现） |
| 寄存器堆读点的**同沿竞态**（距离 3 读旧值） | **已修**（ADR-047 D11 的读点旁路） | ✅ |
| imem 的 1 拍 `pc_lag` | 设计前提（ADR-041 强制 `sread`） | ✅ 契约化 |
| CLINT `mtime` 不自增 | ✅ **已修**（2026-10-30 · ADR-049 · change `clint-mtime-autoincrement-2026-10` **已归档**）⇒ `mtime` = 64 位自由运行计数器（编译期 `TIMER_DIVIDER` · 写优先 · 复位 0）⇒ ⇒ ⚠️ 64 位读**允许撕裂**（已知边界） | ✅ 已收口（`feat-riscv-mini-rtl-debt` ② · feature 已 `completed`） |
| 两个 cache wrapper 的分支**从不发射** | ✅ **已修**（2026-10-28 · change `riscv-mini-cache-wrapper-live-logic-2026-10` **已归档**）⇒ 3 处建图期 C++ `if` 全部清除 + 10 个 `ch_out` 全部无条件驱动 | ✅ 已收口（`feat-riscv-mini-rtl-debt` ① · feature 已 `completed`） |
| ~~**两个 `ForwardingUnit` 并存**（`riscv::ForwardingUnit` vs `chlib::ForwardingUnit`，靠 namespace 区分）~~ | ✅ **已消解**（2026-11-11 · B0 批 **C5 同批退役**）—— `riscv::ForwardingUnit`（`include/cpu/pipeline/rv32i_forwarding.h`）**物理删除**（零交付路径消费者 + 独有语义为空集：WB 档无覆盖 · `id_reg_read` 死输入）⇒ ⇒ 消解的是 **ADR-046 D6**（`docs/adr/ADR-046-rv32i-canonical-home.md:210-218`「两个 `ForwardingUnit` 并存，靠 namespace 区分」）这条**处置** ⇒ ⇒ ⚠️ **ADR-038 §2.3 / §5 的裁决本身仍有效**（其合并对象 = `chlib::ForwardingUnit` + `riscv::HazardUnit` —— **两者都还在**）⇒ ⇒ ⚠️ 失效的只是 ADR-038 的 **2026-10-27 更新注**（其前提「ADR-047 不动 `riscv::ForwardingUnit`」已不成立） | ✅ 已消解（`rv32i-dead-component-consolidation` **C5**） |
| `chlib::ForwardingUnit`（`include/chlib/forwarding.h:75`）组件类**零实例化** | ⚠️ **仓内零消费者**（实测：全仓 `ForwardingUnit<` 的唯一命中是该头 `:59` 的**文档注释**；`tests/chlib/test_forwarding.cpp` 用的是**函数式** `forward_detect<>` / `forward_select<>`）⇒ ⇒ 其**函数式**副本仍被测试消费 ⇒ ⇒ 按 C6 三级准则**待裁决**（接入 / 退役 / 登记定位） | 独立立项（backlog）—— ⚠️ **不属本次回填范围** |
| ~~`hazard_unit.h` 称移位链「**固定延迟 · 不依赖 rd**」+「免疫 load 被冲刷」~~ | ✅ **已订正（注释）**（2026-11-14 · architect 裁决 · **行为零改动**）：该表述只对**移位链**成立，**不对停顿总长成立** —— `stall` 含**组合**项（当前检测结果）⇒ ⇒ **停顿总长 = 检测条件持续拍数 + 2**；条件**掉一拍** ⇒ 停顿**提前结束**；而检测条件**本身依赖 rd**。⚠️ 流水线里「4 拍」= 条件持续 2 拍 + 2 ⇒ **正确设计，MUST NOT 改** | ✅ 已收口 ⇒ 判据 = `tests/chlib/test_hazard_unit_stall.cpp`（V12 真值表）|
| ~~`hazard_unit.h` 称 `load_use_detected` 是「**单拍脉冲**」~~ | ✅ **已订正（注释）**（2026-11-14 · architect 裁决 · **行为零改动**）：它 = 检测条件的**组合**输出 ⇒ ⇒ 条件持续多少拍就高多少拍（**不自清**）⇒ ⇒ 「单拍」只在调用方只给一拍条件时成立 | ✅ 已收口 ⇒ 判据 = 同上（V12 真值表）|
| JIT 对 `ch_mem` 读端口 `UNSUPPORTED_OP` ⇒ RV32I 必然回落 interpreter | **有意设计**（`jit_compiler.cpp`）⇒ 回落原因**可归因**且被断言 | ✅ 已契约化（`verilator-rv32i-e2e`） |

## 契约明细（逐条 · 迁移自根 `AGENTS.md` 的 `KNOWN LIMITATIONS`）

> 这些是**当前态约束**，不是历史。根 `AGENTS.md` 只保留「红线」摘要 + 指向本文；
> **本文是逐条权威**（改动契约时先改 ADR / capability，再同步本节）。

### 1. ISA 覆盖边界

固件已覆盖：ALU（ADD/SUB/ADDI）· `LUI`/`AUIPC` · `LW`/`SW`/`SB`/`SH` · `LB`/`LH`/`LBU`/`LHU` ·
`BEQ`/`BNE` · `JAL`/`JALR` · `SRA` · `FENCE`/`FENCE.I`（opcode `0x0F` · 本版 = **NOP**：
单核无 cache ⇒ 无排序语义）⇒ ⇒ 均已进 capability `rv32i-isa-semantics`。

**仍未实现 / 未验证** = **CSR**（`csrrw`/`csrrs`/`csrrc`）+ **`ECALL`/`EBREAK`**（不含**预测取指**）
⇒ ⇒ 这两项**不在**任何仓内 capability 覆盖内 ⇒ ⇒ 需**独立立项**（不可顺手塞进相邻条目）。

⚠️ **非法指令**（含 `CSR`/`ECALL`/`EBREAK` 所属的 `OP_SYSTEM`）MUST **可观测**：
`dbg_illegal_reason` 是 **4 态 + 优先级（`1` > `3` > `2` > `0`）**：
`0` 合法 / `1` **已知未实现** / `2` **未知 opcode** / `3` **保留组合** ⇒ ⇒ 进入 ID 级即**冻结**
（PC 不前进 · 该指令**永不退休**）⇒ ⇒ 「跑飞」不再**静默**（此前未知 opcode 被**当 NOP 逐条退休**）。
⇒ 权威载体 = capability `rv32i-isa-semantics` 的「4 态 + 优先级」条款 · 实现 =
`include/cpu/pipeline/rv32i_decoder.h:217-220`。

### 2. 数据前推（ADR-047）

档位 1 = EX/MEM 级、档位 2 = MEM/WB 级的合并写回值。

⚠️ **成环仍然被禁止**：数据源的**叶子** MUST 是**流水线寄存器**（MUST NOT 取 EX/MEM 级的
**组合输出** —— 那正是 `git show bf78367` 的事故形态）⇒ 负向对照见
`tests/chlib/test_forwarding.cpp`（**常驻**回归）。

⚠️ **档位 1 保守地不覆盖 load**（那是一个**选择**，不是无奈）⇒ 「档位 1 覆盖 load」= **V2**
独立立项。

⚠️ **load-use 停顿拍数与距离矩阵的唯一权威 = capability `rv32i-data-forwarding`**
（含 ADR-069 决策 A0 的拍级推导）⇒ ⇒ **任何文档 MUST NOT 另写一份数字**（两份必然漂移）。
依据 = `docs/adr/ADR-047-rv32i-data-forwarding.md` **§5.5**（dated addendum）+
`docs/adr/ADR-069-mem-data-alignment.md`（决策 A0 · 停顿 3 → 4 拍的级数推导）。

⚠️ **停顿链口径**（`include/cpu/pipeline/hazard_unit.h:159` 的注释订正 · 行为零改动）：
`stall` 含**组合**项（当前检测结果）⇒ **停顿总长 = 检测条件持续拍数 + 2** ⇒ 条件**掉一拍**
⇒ 停顿**提前结束**；检测条件**本身依赖 rd** ⇒ 流水线里观测到的「4 拍」= 条件持续 2 拍 + 2
⇒ **正确设计，MUST NOT 改**。判据 = `tests/chlib/test_hazard_unit_stall.cpp`（V12 真值表）。

### 3. 取指侧 / 存储时序

- **imem MUST `sread`（1 拍，ADR-041）** ⇒ 指令比地址晚 1 拍到 ⇒ IF/ID 的 pc **必须同样延迟**
  （`fetch_pc_reg`），否则 ID 拿到「比它译码的指令晚一个字」的 pc ⇒ 分支目标算错 4 字节。
- **数据侧相反**：D-TCM（dmem）读口 = **固定 1 拍 `sread`**（ADR-069 决策 A0 ⇒ **不是** 0 拍
  组合读）⇒ ⇒ 仅**字节写 RMW**（`rv32i_tcm.h:137`）与**寄存器堆**（`rv32i_pipeline.h:275`）
  仍是 `aread`。权威 = capability `rv32i-memory-timing` R1/R2。
- **取指侧的停顿丢指令 / 同一条指令被装两次（ADR-048）均已消除**（`IfStage` 内置 1 级 skid
  buffer + `primed` 门控）⇒ 「停顿点之后 MUST 是 NOP」这条固件约束**已取消**。
- ⚠️ **判据边界**：`primed` 是「**靠时序假设**」的正确性依赖（依赖 imem 的 1 拍 `sread` 延迟）
  ⇒ 若将来换 imem 变体（`aread`）或引入预测取指，MUST 重审 ADR-048 D6 与 D3/D4 的前提。

### 4. `HazardUnit` 端口面

只保留 4 个直通端口（`rs1_data_raw`/`rs2_data_raw`/`rs1_data`/`rs2_data`）⇒ 作为**独立组件**
仍可用。⚠️ 不得再把 `ex_alu_result` 接到 EX 级**组合输出** —— 那是**环饵** ⇒ 留着就诱使后人
复现 `bf78367` 的组合环。

### 5. cache 子系统

- **变体选择**（ADR-039 ch_mem 重写 + ADR-041 Sync/Async 拆分）：完整实现位于
  `include/cpu/cache/` 下的 `{i,d}_cache_{sync,async}.h`；`*_complete.h` 是兼容 shim 别名指向
  `*_sync.h`；RV32I L1 默认 `ICacheSync` / `DCacheSync`（1 拍 sread）；TCM-style 用
  `ICacheAsync` / `DCacheAsync`（0 拍 aread）⇒ 新代码用 `_sync` / `_async` 直接引用。
  历史 pass-through stub 头文件（`*_stub.h`）已物理删除（ADR-037）—— 其语义不构成合法 cache
  行为，且**没有**替代的 BYPASS 模式。
- **已知债务**：`DCacheWriteBuffer` **并不真缓冲**（直通式 ⇒ 名字 over-promise ⇒ `ready` 无
  背压语义）；`ICachePrefetch` 的**预取缓冲**是死数据（真预取未实现）⇒ ⇒ **真 FIFO 写缓冲 /
  真预取 = 独立立项**；内层 cache 的 `miss`（1 拍 `sread`）与 `axi_addr`（当拍地址）存在
  **1 拍错位**（属 cache 本体）。判别力见 `examples/riscv-mini/tests/test_cache_smoke.cpp`
  （**12 个用例 / 78 断言**，双后端）。
- ⚠️ **契约**「调用方 MUST 在 `miss` 期间保持 `addr` 不变」（否则 AXI 请求跟**新地址**发出 ⇒
  取回的是**新块**数据）⇒ 载体 = capability `cpu-cache-subsystem` 的「`miss` 期间的地址稳定性」
  Requirement ⇒ 判别力 = 错位探针用例。

### 6. CLINT `mtime`

64 位自由运行计数器（ADR-049）—— **不是**「只能由软件写」的静态比较器 ⇒ 递增频率 =
**编译期**常量 `TIMER_DIVIDER`（默认 1）· **写优先** · 复位值 0。⚠️ 64 位读**允许撕裂**（已知边界）。
⇒ 权威 = `include/cpu/AGENTS.md` §「CLINT 的 `mtime` 契约」。

## 决策依赖关系

```
ADR-035（verilator 后端：64-bit 端口上限 / 2-step clock / reset pulse）
   └─► 约束可观测面设计（verilator 只能读顶层端口）
         └─► ADR-046（唯一归属 + D10 32 端口冻结）
               └─► ADR-047（前推：接 / mux 归属 / 档位 / D11 读点旁路）
                     └─► ADR-048（取指：skid buffer + 填充期语义；否决 PC 回退）
                           └─► change rv32i-fetch-skid-buffer-2026-10（✅ 已归档）
                                 （C1 缺陷 A / C2 缺陷 B，**同一 change 两个 commit**）

ADR-039（ch_mem 底座） ─► ADR-041（Sync/Async 拆分） ─► imem MUST sread（1 拍）
                                                            └─► 取指对齐 / pc_lag（ADR-046/047/048 的共同前提）
ADR-037（stub 退役） ─► ADR-038（forwarding 归属） ─► ADR-047 的 mux 复用
```

⚠️ **次序敏感**：
① ④ 依赖 ③ ⇒ ③ 未落地时固件必须插 NOP；
② ③ 落地后 ④ 的**严重性升级**（双装不再恒幂等）⇒ ④ 的两个缺陷**必须同 change 修**
（ADR-048 D13.1）；
③ `aread`/`sread` 的时序语义（`mem-read-timing`）是 ④ 的前提 ⇒ **MUST NOT** 改 `aread` 语义。

## 未决与独立立项（不在本文裁决范围）

- **V2**：档位 `1` 覆盖 load ⇒ 消除 load-use 停顿（独立立项）
- ~~**框架层加 API**（ADR-048 D7）~~：D6 的**局部修法（`primed`）已足够** ⇒ D7 **不启动**
  （`sread` 的初值语义与 `aread` 契约零改动）
- **ISA 覆盖缺口**：**CSR**（`csrrw`/`csrrs`/`csrrc`）+ **`ECALL`/`EBREAK`**
  （不含**预测取指**）仍无实现 / 无验证 ⇒ ⇒ 这两项**不在**任何仓内 capability 覆盖内
  ⇒ ⇒ 需**独立立项**（`rv32i-isa-gap-closure-2026-11` · phase-6 P6 边界 · 不可顺手塞进本条）
  ⇒ ⇒ 逐条权威 = 本文 §「契约明细」1
  ⚠️ **`FENCE` / `FENCE.I` 不算缺口**（opcode `0x0F` · 本版 = **NOP** · 已进 capability
  `rv32i-isa-semantics`）—— 此前本条把它列为缺口 = **已过时**，2026-11-19 订正
  ⇒ ⇒ ✅ **已覆盖**：ALU（ADD/SUB/ADDI）· `LUI`/`AUIPC` · `LW`/`SW`/`SB`/`SH` ·
  `LB`/`LH`/`LBU`/`LHU`（判别性固件 `Rv32iSoc - LoadByteLaneDiscriminating`，
  `examples/riscv-mini/tests/test_rv32i_soc_dual_parity.cpp:1688`）· `BEQ`/`BNE` ·
  `JAL`/`JALR` · `SRA` ⇒ ⇒ 权威 = capability `rv32i-isa-semantics`
- **取指侧的判据边界**：`primed` 是**「靠时序假设」的正确性依赖**（依赖 imem 的 1 拍
  `sread` 延迟）⇒ 若将来换 imem 变体（`aread`）或引入预测取指，MUST 重审 ADR-048 D6 与
  D3/D4 的前提（该约束已写进 `rv32i-commit-path` 的 Scenario 与 ADR-048 §3「负面/代价」）

## 维护约定

- 裁决改 ⇒ 先改 **ADR**（含对抗审稿记录）⇒ 再同步本文的「当前架构 / 已知残留」两节；
- capability 契约变更 ⇒ 走 openspec change + archive 合并（**禁**直接改主 spec、
  **禁** `--skip-specs`，除非该 change 以 `.openspec.yaml` 的 `skip_specs: true` 显式声明无 spec 变更）；
- 计数类事实（`CTEST_TOTAL` / `EXAMPLES_TEST_SOURCES` / `ctest -L verilator_e2e`）
  ⇒ 以根 `AGENTS.md §NOTES` 的 CI 锚点行为准（**磁盘是权威**）。
