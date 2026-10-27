# RV32I 微架构 Architecture

> **主题文档**（per ADR-0057 的「ADR + theme-doc」双工件模型）：本文是 RV32I 微架构的
> **组合叙事 + 当前架构快照**；**裁决本身**在 Primary ADRs 里，不在本文。
>
> **范围**：`include/cpu/pipeline/**`（5 级流水线与冒险/前推）、`include/cpu/cache/**`、
> `include/cpu/riscv/**`、以及 SoC 顶层 `examples/riscv-mini/src/rv32i_pipeline_top.h`。
> **不**覆盖：SoC 之外的 AXI 互连层、bundle/stream 组件库、框架层的仿真与 codegen 语义
> （那些见各自的主题文档 / capability）。
>
> **最后同步**：2026-10-27（随 ADR-048 落盘）

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
├── pipeline/                        # 流水线数据通路（19 个头 = 14 + stages/5）
│   ├── rv32i_pipeline.h             # 5 级顶层（装配 + dbg_* 观测面）
│   ├── rv32i_pipeline_regs.h        # IF/ID/EX/MEM/WB 流水线寄存器（490 行，最大）
│   ├── rv32i_decoder.h              # R/I/S/B/U/J 译码
│   ├── hazard_unit.h                # 冒险检测 + 停顿/冲刷控制（ADR-047：删 5 个预留端口）
│   ├── hazard_detection_unit.h      # Load-Use 冒险检测（独立组件）
│   ├── rv32i_hazard_complete.h      # 组合式 hazard 单元
│   ├── rv32i_alu.h / rv32i_regfile.h / rv32i_pc.h
│   ├── rv32i_forwarding.h           # riscv::ForwardingUnit（**源选择**；不是数据 mux）
│   ├── rv32i_branch_predict.h / branch_predictor_v2.h
│   ├── rv32i_tcm.h                  # InstrTCM / DataTCM（ch_mem 底座）
│   └── stages/{if,id,ex,mem,wb}_stage.h
├── cache/                           # L1 cache（6 个头）：{i,d}_cache_{sync,async}.h + wrapper + 兼容 shim
└── riscv/                           # 特权级（4 个头）：CSR / exception / address_decoder / clint

examples/riscv-mini/src/rv32i_pipeline_top.h   # **唯一**留在 example 侧的头（SoC 顶层）
```

⚠️ 收敛前 `include/cpu/pipeline/` 与 `examples/riscv-mini/src/` 是**两套不同架构**；
ADR-046 D1–D12 把它们收敛为一套 ⇒ 护栏 `scripts/verify_no_duplicate_rv32i_headers.sh`。

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

### 5. SoC 顶层与可观测面（ADR-046 D10）

`rv32i_pipeline_top.h` 暴露 **32 个端口**（5 输入 + 27 输出 = 11 功能 `obs_*` + 16 诊断 `dbg_*`）：

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
| CLINT `mtime` 不自增 | 只构成静态比较器 | `feat-riscv-mini-rtl-debt` ②（P2） |
| 两个 cache wrapper 的分支**从不发射** | 建图期 C++ `if` ⇒ 分支体不发射 | `feat-riscv-mini-rtl-debt` ①（P2） |
| `riscv::ForwardingUnit` 与 `chlib::ForwardingMux` **并存** | ADR-038 §2.3 裁决「合并留未来」 | 独立立项（backlog） |
| JIT 对 `ch_mem` 读端口 `UNSUPPORTED_OP` ⇒ RV32I 必然回落 interpreter | **有意设计**（`jit_compiler.cpp`）⇒ 回落原因**可归因**且被断言 | ✅ 已契约化（`verilator-rv32i-e2e`） |

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
- **ISA 覆盖缺口**：CSR / ECALL / EBREAK / FENCE / LB·LH·LBU·LHU 仍无固件验证
  （现有固件只覆盖 ALU + LW/SW + BEQ/BNE + JAL/JALR + SB/SH）
- **取指侧的判据边界**：`primed` 是**「靠时序假设」的正确性依赖**（依赖 imem 的 1 拍
  `sread` 延迟）⇒ 若将来换 imem 变体（`aread`）或引入预测取指，MUST 重审 ADR-048 D6 与
  D3/D4 的前提（该约束已写进 `rv32i-commit-path` 的 Scenario 与 ADR-048 §3「负面/代价」）

## 维护约定

- 裁决改 ⇒ 先改 **ADR**（含对抗审稿记录）⇒ 再同步本文的「当前架构 / 已知残留」两节；
- capability 契约变更 ⇒ 走 openspec change + archive 合并（**禁**直接改主 spec、
  **禁** `--skip-specs`，除非该 change 以 `.openspec.yaml` 的 `skip_specs: true` 显式声明无 spec 变更）；
- 计数类事实（`CTEST_TOTAL` / `EXAMPLES_TEST_SOURCES` / `ctest -L verilator_e2e`）
  ⇒ 以根 `AGENTS.md §NOTES` 的 CI 锚点行为准（**磁盘是权威**）。
