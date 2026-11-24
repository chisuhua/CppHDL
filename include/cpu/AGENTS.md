# AGENTS.md - CPU / RISC-V Subsystem

Child of `include/AGENTS.md` → root `AGENTS.md` (ZERO-DEBT POLICY + PHASE GATES).

## OVERVIEW
RV32I 5-stage pipeline + cache hierarchy + forwarding + hazard detection + SoC integration.
**30 headers, 7137 lines**（2026-11-16 实测，ADR-046 D9：磁盘是权威；6459 → 6464 因 ADR-063 落地（B 型立即数 `<< 11` 的两处注释）而变；6464 → 6506 因 `header-self-containment-fix-2026-11` 给 4 个 stage 头补 include/说明、并删除 `ex_stage.h` 的 `BRANCH_*` 死常量块而变；6506 → 6473 因 phase-6 **P9**（Zero-Debt）删除 cache wrapper 的**死数据 / 死配置面**（`WriteBufferEntry` / `WriteBufferConfig` / `PrefetchConfig` / 三个 `prefetch_*` 缓冲）⇒ ⇒ **头文件数不变**（30）；6473 → **6064** 因 phase-6 **B0 批**（`rv32i-dead-component-consolidation` **C5+C6 同批退役**）**物理删除 3 个头**（`pipeline/rv32i_forwarding.h` / `pipeline/hazard_detection_unit.h` / `pipeline/rv32i_hazard_complete.h`）⇒ ⇒ **头文件数 30 → 27** ⇒ ⇒ 行数差 409 ＝ 删 434 行 ＋ 其它头的注释修订（SLTU 修复等）⇒ ⇒ ⚠️ **该订正于 2026-11-13 补**（B0 批当时漏了 ⇒ ⇒ 护栏 `verify_no_duplicate_rv32i_headers.sh` C5 报红 ⇒ ⇒ 登记为 **F-1**）；**6064 → 6582 · 头文件数 27 → 30** 因 phase-6 **B3 批**（`memory-timing-contract-2026-11` · ADR-069 决策 **A0**）**新增** `pipeline/load_lane_selector.h`（load 的车道选择 + 符号/零扩展，**从 `MemStage` 迁出** ⇒ ⇒ 因为 `sread` 让 load 数据**晚 1 拍**可用 ⇒ ⇒ 车道选择 MUST 在 **M2 的下游**做 ⇒ ⇒ 见 ADR-069 **V5**）⇒ ⇒ **头文件数 +1 · 行数 +238**）；**6582 → 6606 · 头文件数不变** 因 `firmware-load-chain-2026-11` **L2（路线 A）**给 `InstrTCM` / `DataTCM` 增加**可选** `init_words` 构造参数（+24 行注释与参数）⇒ ⇒ 默认空 ⇒ **零行为变化**）；**6606 → 6611 · 头文件数不变** 因 `firmware-load-chain-2026-11` **L2（路线 A）** 后紧接的 **R8 改判（用户裁决 A2）**删掉 `riscv::Rv32iPipeline` 的 **`clk` 输入**（**零消费者** ⇒ ⇒ ZERO-DEBT）⇒ ⇒ 连带 SoC 顶层的 `ext_clk` 一并删 ⇒ ⇒ 顶层端口 32 → 31；**6611 → 6636 · 头文件数不变** 因 architect 批次订正 `hazard_unit.h` 的「注释 vs 行为」差异（`load_use_detected` 不是单拍脉冲 · 移位链「固定延迟」只对其自身成立 · **停顿总长 = 检测条件持续拍数 + 2**）⇒ ⇒ **行为零改动**（注释 + 推导，+25 行）；**6636 → 6587 · 头文件数不变** 因 **B4 · C3（EX 接入 `Rv32iAlu`）**：`ex_stage.h` 的**内联 10 个 ALU 分支已删**（−~50 行）且 `rv32i_alu.h` 的 `mem_size`/`mem_signed` **已删**（Δ8 · ZERO-DEBT）⇒ ⇒ 算术核心的唯一实现 = `rv32i_alu.h` ⇒ ⇒ **行为零变化**（既有 golden 全绿）；**6587 → 6564 · 头文件数不变** 因 **B4 · C4（ID 接入 `Rv32iDecoder`）**：`stages/id_stage.h` 的**内联 5 种立即数生成 + 8 级 `select` 链已删**（−~50 行）⇒ ⇒ 立即数的唯一实现 = `pipeline/rv32i_decoder.h` ⇒ ⇒ **行为零变化**（C4 差分逐条相等 · `ctest -LE perf` 254/254 · 独立 Verilog TB `checks=24 fails=0`）⇒ ⇒ ⚠️ 该子组件使 **8 个端口**冒泡到产物顶层（`id_stage_decoder_unnamed_output*` ⇒ 产物端口 118 → **126**）；**6564 → 6619 · 头文件数不变** 因 **B4 · G1+G4（未知 opcode 检测 + FENCE 显式化）**：`stages/id_stage.h` 增 `illegal_reason`（2 bit）· `pipeline/rv32i_decoder.h` 增 `OP_FENCE = 0x0F` · `pipeline/rv32i_pipeline.h` 增 `dbg_illegal_reason` + **冻结**（`if_stage`/`id_stage` 的 stall 均 OR 上 `illegal_reason != 0`）⇒ ⇒ 未知 opcode **不再被静默当 NOP 退休**（改为一进 ID 级就冻结）⇒ ⇒ `OP_SYSTEM`（ECALL/EBREAK/CSR/MRET）归「**已知未实现**」· `FENCE` 归「**已知 = NOP**」；**6619 → 6647 · 头文件数不变** 因 **ADR-071 审稿修正**（2026-11-15）：`stages/if_stage.h` 的 PC 更新门补 **`flush` 优先于冻结 stall**（判据 `!(stall & !(flush & branch_valid))`）⇒ ⇒ 修正「`flush` ∥ 冻结同拍时 **redirect 被丢掉 + 冻结只活 1 拍**」的真缺陷（oracle 复核发现）⇒ ⇒ 旧互斥论证注释（「flush 与 stall 不会同拍」= **事实错误**）已重写⇒ ⇒ 组件级既有契约（`stall=1 ∧ flush=1 ∧ branch_valid=0` ⇒ PC 冻结）**零改动**；**6647 → 6657 · 头文件数不变** 因 **R21-B（ADR-070 D2 · 2026-11-15）**：`pipeline/rv32i_pipeline.h` 的 **7 处** stage 成员 `ch_module` ⇒ `ch_layer`（+ 显式模块类型名 `IfStage` / `IdStage` / `ExStage` / `MemStage` / `WbStage` / `HazardUnit` / `ForwardingMux<32>`）⇒ ⇒ **只影响层次化产物**（扁平产物逐字节不变）；`load_lane` **保持** `ch_module`（ADR 的字面集合不含它）；**6745 → 6817 · 头文件数不变** 因 **S2（change `rv32i-mem-backpressure-2026-11`）**：`stages/id_stage.h` 新增 `freeze` 输入（MEM 反压冻结 · 与 `stall` 的「装气泡」语义分离） + `rv32i_pipeline.h` 的反压冻结链与注释；**6966 → 6993 · 头文件数不变** 因 **S4a-0（change `rv32i-commit-event-pulse-2026-11`）**：`pipeline/rv32i_pipeline.h` 的 `dbg_wb_write_en` 改**单拍脉冲**（`& !mem_stall`）+ 两处口径注释（store 写使能**不可**门控的**组合环**实测记录）+ 本节新增「**流水线观测面：事件端口 vs 级属性端口**」一节（+27 行）⇒ ⇒ **6993 → 6996** 因 critic NIT 订正（`rv32i_pipeline.h` 的「恰好一次」论证措辞精确化 · +5/−2）；**6996 → 7124 · 头文件数不变** 因 **S4a（change `rv32i-soc-mmio-channel-2026-11` · G-6）**：`rv32i_mem_map.h` 扩 **7 个 MMIO 段**（Clint/Uart/Gpio/Spi/I2c/Pwm/Dma · 每段 4 KB · 自 `0x8010_0000` **顺延派生**）+ `kMmioBase` / `kMmioBytes` + `in_mmio()` + 10 条编译期 `static_assert`（连续性 / 不重叠 / 对齐）；`address_decoder.h` 改**派生**（7 段基址 / 容量全部来自库层表）+ 补 **4 个片选**（`spi_cs`/`i2c_cs`/`pwm_cs`/`dma_cs` ⇒ ⇒ **总片选 9**）+ 修一处 **ODR 隐患**（`rv32i_mem_map.h` 的 include 原在 `namespace riscv` **内部** ⇒ ⇒ 符号落进 `riscv::riscv` ⇒ ⇒ 与直接 include 它的 TU **是两个实体** ⇒ ⇒ 已移到 namespace **之外**）；**7124 → 7129 · 头文件数不变** 因同批的**端口命名棘轮**注释（4 个新片选带显式名字串 + 6 行说明）与 include 移出 namespace 的空行归一（−1）；**7129 → 7137 · 头文件数不变** 因 **S4a-P5** 给 `pipeline/rv32i_asm_encode.h` 补 `LUI` 编码器（SoC 级程序需要与 PC 无关地装载 MMIO 基址 ⇒ `AUIPC` 的 `pc + imm` 表达不了）
All header-only.

> ⚠️ 2026-10-24（change `rv32i-canonical-convergence-2026-10`，ADR-046）：
> 本目录是 RV32I 微架构的**唯一权威**。此前 `examples/riscv-mini/src/` 存在
> **10 组同 basename 头文件**，消歧纯靠 `-I` 顺序（无护栏 ⇒ 调序即静默换实现），
> 且根 `tests/` 反向依赖 example 源码。⇒ 已收敛：`examples/riscv-mini/src/`
> 现只剩核 IP 顶层 `rv32i_core_top.h`。
> 护栏：`scripts/verify_no_duplicate_rv32i_headers.sh`（**6 条检查** + `--self-test` 自测）。

## STRUCTURE
```
include/cpu/                             # 30 headers, 7137 lines（2026-11-16 实测）
│   ⚠️ **STRUCTURE 树里的 per-file 行数只对 `rv32i_pipeline.h` 保持同步**（2026-11-15 · R21-B 改行数后订正）
│      ⇒ ⇒ 另有 **6** 个 per-file 计数**陈旧**（`rv32i_decoder.h` / `rv32i_tcm.h` / `hazard_unit.h` /
│      `rv32i_alu.h` / `address_decoder.h` / …）⇒ ⇒ **预存在漂移** ⇒ ⇒ 已登记 **R25**（不在 R21-B 范围内 ⇒ 不顺手清理）
├── cache/                            # ch_mem-based cache (ADR-039, ADR-041)
│   ├── i_cache_sync.h                        # 1-cycle sread — RV32I I-L1 默认
│   ├── d_cache_sync.h                        # **hit 1 拍**（tag/data 组合读 + `rdata` 保持寄存器）
│   │                                          # · **AXI4 full 主口** · 4-beat burst fill · 真 line（S3）
│   ├── i_cache_async.h / d_cache_async.h     # 0-cycle aread — TCM-style 备选
│   ├── i_cache_complete.h / d_cache_complete.h  # 兼容 shim (= *_sync.h 别名)
│   ├── i_cache_prefetch.h                    # wrapper (instance ICacheSync)
│   └── d_cache_write_buffer.h                # AXI 写直通 shim（**无**内层 cache）
├── pipeline/                          # 流水线数据通路（ADR-046 D2）
│   ├── rv32i_decoder.h            # R/I/S/B/U/J format decode (161)
│   ├── rv32i_pipeline_regs.h       # IF/ID/EX/MEM/WB regs (490 — largest)
│   ├── rv32i_pipeline.h            # 5-stage top-level (847)
│   ├── rv32i_tcm.h                # Tightly-coupled memory (ITCM/DTCM) (133)
│   ├── hazard_unit.h               # 冒险检测 + 停顿/冲刷控制 (178)
│   ├── load_lane_selector.h        # load 车道选择 + 符号/零扩展（ADR-069 A0 · 从 MemStage 迁出）
│   ├── rv32i_alu.h                # ALU (172)
│   ├── rv32i_regfile.h            # 寄存器堆 (316)
│   ├── rv32i_pc.h                  # PC (47) —— ⚠️ **非流水线路径**（见下表）
│   ├── rv32i_branch_predict.h     # Branch/条件/预测/控制冒险 (475) —— ⚠️ **非流水线路径**
│   ├── branch_predictor_v2.h       # 2-bit 分支预测器 (151) —— ⚠️ **非流水线路径**
│   └── stages/{if,id,ex,mem,wb}_stage.h
└── riscv/                            # 体系结构 / 特权级
    ├── rv32i_mem_map.h             # **地址映射单一真相**（基址/容量派生 · L1）
    ├── rv32i_asm_encode.h          # **RV32I 指令编码器**（header-only constexpr · L3）
    ├── rv32i_csr.h                 # CSR bank (9 registers, 252)
    ├── rv32i_exception.h          # Trap entry + MRET (177)
    ├── address_decoder.h          # MMIO 6-region decoder (119)
    └── clint.h                    # Core Local Interrupt Controller (~200)
```

## NAMESPACE MAP
| Namespace | Files | Purpose |
|-----------|-------|---------|
| `riscv` | `pipeline/*.h`（13 个：ALU / RegFile / PC / decoder / 流水线寄存器 / stage 组件 / hazard / forwarding / branch 预测 / TCM）、`pipeline/stages/*.h`（5 个）、`riscv/*.h`（4 个）| 全部 RV32I 微架构组件 |
| `chlib` | `cache/*.h` | Cache controllers (`i_cache_*`, `d_cache_*`) — forwarding.h moved to `include/chlib/` per ADR-038 |
| `cpu` | (no in-tree headers under this namespace) | Historic placeholders `branch_predictor/*.h` and `hazard/*.h` were removed; their live successors now live under `include/cpu/pipeline/` (`riscv` namespace) |
| `ch::chlib` | `clint.h` | CLINT timer |

## KEY FILES
| Task | File |
|------|------|
| 5-stage pipeline | `pipeline/rv32i_pipeline.h` |
| Pipeline registers | `pipeline/rv32i_pipeline_regs.h` (490 lines) |
| Instruction decode | `pipeline/rv32i_decoder.h` |
| CSR registers | `riscv/rv32i_csr.h` |
| Exception handling | `riscv/rv32i_exception.h` |
| ALU | `pipeline/rv32i_alu.h` |
| Register file | `pipeline/rv32i_regfile.h` |
| Forwarding (数据 mux) | `include/chlib/forwarding.h`（`chlib::ForwardingMux`）—— ADR-046 D3 #14 / ADR-047 D2（**已接入真实流水线**）|
| Branch 预测 / 控制冒险 | `pipeline/rv32i_branch_predict.h` —— ⚠️ **非流水线路径**（见下表）|
| I-Cache (1-cycle, RV32I L1 默认) | `cache/i_cache_sync.h` (`i_cache_complete.h` 是兼容 shim 别名) |
| I-Cache (0-cycle, TCM-style) | `cache/i_cache_async.h` |
| D-Cache（hit 1 拍 · **已接入核** · S3） | `cache/d_cache_sync.h` (`d_cache_complete.h` 是兼容 shim 别名) —— AXI4 full 主口 + 4-beat burst fill + 真 line；`ready`/`miss` **组合**（对当拍 `addr`）· `rdata` **保持寄存器**（S2 的 MEM/WB 冻结语义强制）· 前端含 `strbe` |
| D-Cache (0-cycle, TCM-style) | `cache/d_cache_async.h` |
| I-Cache wrapper | `cache/i_cache_prefetch.h` (instance ICacheSync) |
| D-Cache 写直通 shim | `cache/d_cache_write_buffer.h`（**无**内层 cache / **无**读通路） |
| 核 IP 顶层装配 | `examples/riscv-mini/src/rv32i_core_top.h`（**唯一**留在 example 侧的头；ADR-046 D3 #15）|
| 流水线诊断观测 | `pipeline/rv32i_pipeline.h` 的 `dbg_*` 输出（ID 级指令流 / 分支判定 / 有效链 / 写回 / EX-MEM 数据通路；**核 IP 顶层须逐个引出**，否则 verilator 读不到 ⇒ 2026-10-25 追出 17 个流水线缺陷）|

## ⚠️ **保留但不接入流水线**的组件（2026-11-11 · B0 批 C7 交接登记）

> **判据**：组件**保留**（仓内仍有价值）但**零生产消费者** ⇒ ⇒ 它**看起来像流水线的一部分**
> ⇒ ⇒ 实则**不在真实流水线路径上** ⇒ ⇒ 本表消除该误导。
> **依据**：`rv32i-dead-component-consolidation-2026-11.md` C7（C6 三级准则的「② 无消费者
> **且** 独有语义**未被任何 capability 明文要求** ⇒ ⇒ 退役」是它的**对立面**：
> 下面这些组件**有**仓内价值（独立使用场景）⇒ ⇒ **保留**，但**必须登记**）。

| 组件 | 状态 | 未接入的**可观测后果** |
|---|---|---|
| `riscv::BranchPredictor`（`pipeline/rv32i_branch_predict.h`） | ⚠️ **非流水线路径 · 仅独立使用场景** | **每个 taken 分支（含 `JAL`/`JALR`）付 3 拍冲刷** —— 真实流水线**无分支预测** ⇒ ⇒ 控制重定向一律走完整冲刷路径（`rv32i_pipeline.h:599`：「`JAL`/`JALR` —— 它们同样让流水线空 3 拍」）⇒ ⇒ `perf_control_stalls` 统计**全部**控制重定向拍数（与 `perf_branch_mispredict` **分离**）|
| `riscv::BranchPredictorV2`（`pipeline/branch_predictor_v2.h`） | ⚠️ **非流水线路径 · 仅独立使用场景** | 同上（2-bit 预测器未接入 ⇒ 无预测收益）|
| `riscv::BranchDetector`（`pipeline/rv32i_branch_predict.h`） | ⚠️ **非流水线路径 · 仅独立使用场景** | 分支**探测**（`is_branch` / `is_jal` / `is_jalr`）在真实流水线里由 `stages/id_stage.h` 自行判定 ⇒ ⇒ 本组件不在真实路径上 |
| `riscv::BranchConditionUnit`（`pipeline/rv32i_branch_predict.h`） | ⚠️ **非流水线路径 · 仅独立使用场景** | 分支**条件判定**在真实流水线里由 `stages/ex_stage.h` 自行判定 ⇒ ⇒ 本组件不在真实路径上 |
| `riscv::ControlHazardUnit`（`pipeline/rv32i_branch_predict.h`） | ⚠️ **非流水线路径 · 仅独立使用场景** | 控制冒险冲刷在真实流水线里由 `stages/id_stage.h` 的 `flush` 端口 + `rv32i_pipeline.h` 自行编排 ⇒ ⇒ 本组件不在真实路径上 |
| `riscv::BranchPredictorState`（`pipeline/rv32i_branch_predict.h`） | ⚠️ **非流水线路径 · 仅独立使用场景** | 预测器**状态**同上 |
| `riscv::Rv32iPc`（`pipeline/rv32i_pc.h`） | ⚠️ **非流水线路径 · 仅独立使用场景** | PC 由 `stages/if_stage.h` 自行管理 ⇒ ⇒ 本组件不在真实路径上 |

> ⚠️ **「零生产消费者」的实测口径**（2026-11-11）：上表 7 个组件的**生产引用数 = 0**
> （口径 = `include/**` + `src/**` + `examples/**` 中排除**自身头文件**与 `tests/**` 后的
> 符号命中数）⇒ ⇒ **测试消费者不算**（测试是「独立使用场景」的体现）⇒ ⇒ 故**保留**。

> ⚠️ **退役登记（2026-11-11 · B0 批 C5 + C6 同批）** —— 已**物理删除**（不是 stub）：
> `riscv::ForwardingUnit`（`pipeline/rv32i_forwarding.h`）· `riscv::HazardDetectionUnit`
> （`pipeline/hazard_detection_unit.h`）· `riscv::HazardUnitComplete`（`pipeline/rv32i_hazard_complete.h`）
> ⇒ ⇒ 理由 = **零交付路径消费者** + 独有语义为空集/死语义 + capability **不指名**它们
> ⇒ ⇒ ⇒ 详见 `.rddf/improvements/rv32i-dead-component-consolidation-2026-11.md` C5/C6
> ⇒ ⇒ ⇒ **这是显式批准的覆盖缩面**（用户 2026-11-11 批准 · metis finding 9 要求登记）：
> `DUAL_BACKEND_PARITY_PAIRS` 290→284 · `DUAL_BACKEND_PARITY_TOTAL` 66→65 · `VERILATOR_E2E_TOTAL` 77→76。

## CONVENTIONS
- **Pipeline instantiation**: `ch::ch_module<T>` inside `Component::describe()` — see root AGENTS.md
- **Runtime**: Pipeline requires `ch::ch_device<Rv32iSoc>` wrapper in main() (per README)
- **Bundle vs Port**: Pipeline uses port-based `hazard_unit.h` (riscv namespace)。⚠️ 旧的 bundle 版 `hazard_unit_bundle.h`（cpu namespace）**已于 2026-10-24 作为零消费者头删除**（ADR-046 D4）
- **跳转 / 重定向语义**（2026-10-27，T3）：`ex_stage.branch_taken` / `branch_target` 的语义
  已扩展为「**重定向成立 / 重定向目标**」（含 `JAL`/`JALR` 的**无条件**跳，不只是条件分支）
  ⇒ `if_stage` 的 PC 选择器**仍是两路**（`branch_valid` / `branch_target`），**没有**第三路
  「`jump_valid`」—— 因为 flush 的唯一来源仍是 `ex_branch & ex_branch_taken`，而
  `ex_branch = is_branch | is_jump`（两成分互斥）。⚠️ 引入**第三个** flush 来源前 MUST 重读
  `if_stage.h` 的不变量注释。
  ⚠️ `JAL`/`JALR` 的返回地址（`pc + 4`）走**独立通路** `link_value`（EX 级算 ⇒
  `exmem_link_value` ⇒ `memwb_link_value` ⇒ `wb_stage.link_value`），**不是** `alu_result`。
- **`alu_op` 的 SUB/SRA 判定 MUST 只对 R 型 与 SRAI 生效**（2026-10-27 修复）：`funct7[5]`
  （= `instr[30]`）对 **I 型**是 **`imm[10]`** ⇒ 直接用它判 SUB 会把 `imm >= 0x400`
  或负数误判成 SUB（实测 `addi x, x0, -8` ⇒ `0 - (-8)` = 8）。
- **数据前推（2026-10-27，ADR-047）**：lib 流水线**已接**数据前推 ——
  mux = `chlib::ForwardingMux<32>`（**零新增组件**），档位 select **打一拍**
  （`fwd_a_d1`/`fwd_b_d1`）⇒ 档位 1 = EX/MEM 级、档位 2 = MEM/WB 级合并写回值。
  ⚠️ 数据源的**叶子** MUST 是**流水线寄存器**（MUST NOT 取 EX/MEM 级的**组合输出**
  —— `bf78367` 的事故形态）⇒ **负向对照**是**常驻**测试
  （`tests/chlib/test_forwarding.cpp`）。
  ⚠️ `HazardUnit` 的 5 个**预留输入端口已删**（D7）⇒ 只剩 4 个直通端口。
  ⚠️ 前推判定带 `& !ex_is_load` 门控（D6）⇒ 档位 1 **不覆盖 load**
  ⇒ **load-use 停顿契约不变**。
- **TCM**: `rv32i_tcm.h` provides single-cycle instruction/data memories
  - ⚠️ **`DataTCM` 的字节道契约**（2026-10-26 新增 `strbe` 端口）：`addr` 是**字地址**、
    `wdata` MUST 已**lane 对齐**（`rs2 << 8*addr[1:0]`，由流水线负责）、`strbe` 是车道掩码
    ⇒ 写路径是**读-改-写**（`ch_mem` 无字节使能写 API）。⚠️ `strbe` 是**新增输入端口**
    ⇒ **全部实例化点 MUST 显式驱动**（未驱动端口读到的是**全 1** 而非 0 ⇒ 整字写会「碰巧」通过）
- ⚠️ **`DataTCM` 的读口契约**（2026-11-14 · B3 批 **T1** · ADR-068 **V7** · ADR-069 决策 **A0**）：
  读口由**全组合读**（`aread` · **0 拍**）改为**寄存读**（`sread` · **1 拍**）⇒ ⇒
  **地址在拍 N 呈现 ⇒ 数据在拍 N+1 可用** ⇒ ⇒ MEM 级（拍 N+1）**已无**正确数据
  ⇒ ⇒ load 的**车道选择 / 扩展** MUST 在 **M2 的下游**做（`riscv::LoadLaneSelector`）
  ⇒ ⇒ ⚠️ **字节写的「读-改-写」仍用 `aread`**（`ch_mem` 无字节使能写 API ⇒ ⇒ 需**同拍**旧字）
  ⇒ ⇒ ⚠️ **沿级 read-old 不变**（同址 store→load 的**指令级**语义 = **read-new** ⇒ ⇒ ISA 不变）
- ⚠️ **`Rv32iPipeline::data_ready` 的语义**（2026-11-14 · B3 批 **T3** · ADR-068 **V7**）：
  **`data_ready` = 「固定 1 拍延迟」**（**不是握手**）⇒ ⇒ **driver 仍恒 1**
  ⇒ ⇒ ⚠️ **禁**在「D-TCM 时序契约」这一批里实现**真反压**（「驱动 0 ⇒ MEM 冻结」
  ⇒ ⇒ 属「**接 cache**」独立立项 ⇒ ⇒ ADR-068 V7 明写它**不在**本批）
- **load 侧的字节道契约**（2026-10-28，change `rv32i-load-byte-lane-2026-10`；
  **2026-11-14 迁出** ⇒ ⇒ 车道选择的**实现载体**改为 `pipeline/load_lane_selector.h`
  的 `riscv::LoadLaneSelector` ⇒ ⇒ 语义**逐字不变**）：
  `stages/mem_stage.h` 的 `MemStage` 实现 `LB`/`LH`/`LW`/`LBU`/`LHU`：
  - **车道选择**：字节道按 `addr[1:0]`（4 个车道）；半字道按 `addr[1]`（2 个车道）
    ⇒ ⚠️ 半字道**只**看 `addr[1]` ⇒ **半字对齐**（`addr[0]` 被忽略）
  - **扩展方式**：`LB`/`LH` **符号**扩展、`LBU`/`LHU` **零**扩展、`LW` 整字
  - ⚠️ **符号扩展 MUST 用 `sext<32>` 原语**（`include/core/operators_width.h`），
    **MUST NOT** 手写填充掩码 —— 此前 LB 用 `ch_uint<32>(4278190080_d)`（=
    `0xFF000000`，而注释写 `0xFFFFFF00`）⇒ `lb` 返回 `0xFF0000C3`
    （应为 `0xFFFFFFC3`）；`sext` 在 interpreter / JIT / Verilog codegen
    **三后端都支持** ⇒ 不会造成双后端语义分叉
  - ⚠️ 该缺陷**从未被暴露**：LB/LBU/LH/LHU 在仓内的唯一用例依赖**仓外**
    riscv-tests 二进制（`lb.bin`）⇒ CI 中全部 SKIP ⇒ 有效覆盖为 0
    ⇒ 固件级覆盖见 `examples/riscv-mini/tests/test_rv32i_soc_dual_parity.cpp`
    的 `Rv32iSoc - LoadByteLaneDiscriminating`
  - ⚠️ **造字 MUST 用 `SB`/`SH`**：`SW` 的字节使能是 `0xF`（**整字**）且其数据
    **不**按 `addr[1:0]` 移位 ⇒ 4 次 `SW` 会**互相覆盖** ⇒ 被读字的 4 个车道
    退化成「1 个非零 + 3 个相同的 0x00」⇒ **判别力归零**
- **取指侧契约（2026-10-27，ADR-048 / change `rv32i-fetch-skid-buffer-2026-10`）**：
  `stages/if_stage.h` 的 `IfStage` 内置 **1 级 skid buffer** 与 `primed` 标志，二者都
  **依赖 imem 的 1 拍 `sread`** 这个前提 ⇒ 改取指结构前 MUST 重读 ADR-048：
  - **skid**（D1–D5）：填充只在**停顿拍**抓 `(fetched_instr, fetch_pc_reg)` 这一对
    （⚠️ `skid_pc` MUST 取 `fetch_pc_reg`，取 `pc_reg` 会错 4 字节）；排空**必须与
    `if_id_update_en` 同拍原子**；`flush | flush_d1 | rst` 一律清空。
    ⚠️ **不做 PC 回退**（停顿已冻结 `pc_reg` ⇒ 在途数据至多一条；回退会制造双装）。
  - **`primed`**（D6）：复位后首个非复位周期置 1，**只门控 IF/ID 的 `valid`**
    ⇒ 幻影装载的 pc 仍写入（`dbg_id_pc` 序列不变）但 `valid=0` ⇒ 不进 EX。
    ⚠️ 判据 MUST 内生于 `IfStage` —— `instr_ready` **恒为 1**，不携带信息。
  - ⚠️ 编译期护栏 `stall_drop_slots_are_nop` 与测试里的「连续重复去重」掩码**已删除**
    （前提消失）⇒ 判别性证据在 `test_rv32i_soc_dual_parity.cpp` 的
    `FetchSkidBufferDiscriminating` 用例与 `tests/test_rv32i_pipeline_behavior.cpp` 的
    **Batch 4**（`IfStage` 的 `flush × skid` 防御性契约 —— 流水线级**不可达** ⇒ 只有组件级能测）。
- **Cache variant selection** (ADR-041):
  - **RV32I L1 imem/dmem** (默认) → `ICacheSync` / `DCacheSync` (1-cycle sread)
  - **软核 TCM-style** (低频, 延迟关键) → `ICacheAsync` / `DCacheAsync` (0-cycle aread)
  - **imem 必须 sread** (per `verilator-e2e-coverage` spec §R1 invariant 3): 不要在 RV32I imem 用 Async
- **cache wrapper 的「组合逻辑真发射」契约**（2026-10-28，change
  `riscv-mini-cache-wrapper-live-logic-2026-10`）：
  `cache/i_cache_prefetch.h` 与 `cache/d_cache_write_buffer.h` 是 **wrapper**
  —— ⚠️ `i_cache_prefetch.h` 是 **`ICacheSync` 的 wrapper**；
  `d_cache_write_buffer.h` 是 **AXI 写直通 shim**（**没有**内层 cache、**没有**读通路）。
  三条硬约束：
  - ⚠️ **MUST NOT 用 C++ `if` 决定「是否发射」** —— `ch_bool` 有
    `explicit operator bool()` ⇒ C++ `if` 拿到的是「**当前节点是否非零**」，
    而 `describe()` 在**建图期**执行 ⇒ 该条件建图期恒为假 ⇒
    **分支体从不发射** ⇒ 输出**恒为未驱动**（读到 0）⇒ wrapper「功能上是死的」。
    ⇒ 条件 MUST 落在**输出端口的取值**里（`select(...)` / 布尔 `&` / `|` / `!` 皆可；
    ⚠️ 布尔 AND 用 `&` —— 框架已把 `&&` 标为 **deprecated**）。
  - ⚠️ **每个 `ch_out` MUST 被无条件驱动** ⇒ 条件为假时也有确定值 ⇒
    **不是**「条件为真才 `<<=`」。⚠️ 特别地：`axi_addr` / `axi_wdata` / `axi_we`
    这类端口**只把 `if` 换掉也仍然未驱动**（驱动被注释掉 / 从未被驱动过）⇒
    会发出「地址 0、无写使能」的 AXI 请求 ⇒ **比「恒未驱动」更危险**。
  - ⚠️ **AXI 主端的 `axi_valid` MUST NOT 依赖 `axi_ready`**（AXI：「VALID
    **不得**等 READY 才拉高」）⇒ READY 为低时该请求仍 MUST 保持在总线上；
    而 wrapper **无状态/队列**可保留它 ⇒ 含了 READY 就等于**写被静默丢弃**。
    ⇒ `ICachePrefetch` 的做法是**转发内层** `ICacheSync` 的 AXI 主端
    （内层**本身**就是完整 AXI 主端 ⇒ 不在 wrapper 层重推导 ⇒ 纪律天然成立）。
  - ⚠️ **内层 cache 的每个 `ch_in` MUST 被显式驱动**（未驱动端口读到的是
    **未定义值**）⇒ 否则内层永远停在「冷 miss」⇒ hit / miss **不可区分**。
    ⚠️ 本条**仅适用于 `i_cache_prefetch.h`**（`d_cache_write_buffer.h` 没有内层 cache）。
  - ⚠️ **已知债务（如实登记）**：`DCacheWriteBuffer` **并不真缓冲**（直通式 ⇒
    名字 over-promise ⇒ `ready` 无背压语义）—— ⚠️ **2026-11-09（phase-6 **P9** ·
    Zero-Debt）**：原 `WriteBufferEntry buffer[4]` 死数组**已删除**（零消费者）
    ⇒ ⇒ **本组件现在不含任何缓冲状态** ⇒ 真 FIFO 写缓冲 = **独立立项**；
    `ICachePrefetch` 的**预取缓冲** —— ⚠️ **2026-11-09（phase-6 **P9** · Zero-Debt）**：
    三个 `prefetch_*` 缓冲 + `PrefetchConfig` **已删除**（零消费者）
    ⇒ ⇒ **本组件现在不含任何预取状态** ⇒ 真预取 = **独立立项**；
    内层 cache 的 `miss`(1 拍 `sread`) 与 `axi_addr`(当拍地址) 存在 **1 拍错位**
    （属 cache 本体 ⇒ wrapper 转发**不**加剧、也**不**修它）
    ⇒ ⇒ **契约：调用方 MUST 在 `miss` 期间保持 `addr` 不变** —— 否则 AXI 请求
    跟**新地址**发出 ⇒ ⇒ 取回的是**新块**的数据
    ⇒ ⇒ **契约载体** = capability `cpu-cache-subsystem` 的「`miss` 期间的地址稳定性」
    Requirement + **本段**（双载体：机器判 + 人读）
    ⇒ ⇒ 判别力证据：`examples/riscv-mini/tests/test_cache_smoke.cpp`
    （**双后端**：内层 **fill → hit** 路径 + **miss 期间改 `addr` ⇒ `axi_addr`
    跟随当拍地址**的错位探针 —— 若内层改成跟随 `miss` 所指地址 ⇒ 该探针**转红**）。

## 陷阱入口的 CSR 写入契约（2026-11-16 · B4 · G5）

- **`mcause` 现在真的写得进去**（此前**永不可达** ⇒ 缺陷零信号）：
  `ExceptionUnit` 的 CSR 写 = `write_mepc = write_mcause = do_trap_entry` ⇒ ⇒ 两者**同时**为真
  ⇒ ⇒ `select(write_mepc, MEPC, …)` ⇒ ⇒ **MEPC 恒命中** ⇒ ⇒ **`mcause` 分支不可达**
  ⇒ ⇒ 既有测试**只查写口**、**从不查 `CsrBank` 读回** ⇒ ⇒ 该缺陷活到今天。
- ⚠️ **修法 = 跨两拍**（`CsrBank` **零改动**）：**拍 1** 写 `mepc`（= `current_pc`）并**锁存**
  `exception_cause` ⇒ ⇒ **拍 2** 写 `mcause`（= **锁存的**原因码 ⇒ ⇒ 与 `mepc` **同配对**）。
- ⚠️ **观察点**：写口是**组合输出** ⇒ ⇒ 沿后读到的是**下一拍**的写 ⇒ ⇒ 「陷阱当拍写的是 `mepc`」
  **不能用写口证明** ⇒ ⇒ MUST 用**复合 DUT**（`ExceptionUnit` → `CsrBank`）的 **`CsrBank.mepc`
  读回值** ⇒ ⇒ 判据载体 = `examples/riscv-mini/tests/test_csr_unit.cpp` 的
  「Trap entry writes BOTH mepc and mcause (two-phase)」。
- ⚠️ **`exception_valid` 是电平**：持续拉高 ⇒ ⇒ 每 **2** 拍重复一次「写 mepc / 写 mcause」序列
  ⇒ ⇒ 两个 CSR 的**值**始终等于**最近一次异常** ⇒ ⇒ **无数据丢失** ⇒ ⇒ 使用方 SHOULD 在取走
  陷阱后**撤销**它。
- ⚠️ **`mtval` 恒 0**：本组件**没有数据源**（只有 `exception_cause`）⇒ ⇒ 写入属**特权级立项**。
- **端口面零变更**（`ExceptionUnit.__io` 未动 ⇒ 无「新增端口 ⇒ 全部实例化点 MUST 驱动」义务）。
- 契约载体 = capability **`rv32i-privilege`**（2026-11-16 新增 · 种子 Requirement）。

## 非法指令判定：**唯一权威 = `Rv32iDecoder`**（2026-11-16 · B4 · G1 扩展 · ADR-071 v1.2）

- **`illegal_reason`（2 bit · 4 态）**：`0` 合法 · `1` **已知未实现**（`OP_SYSTEM`）·
  `2` **未知 opcode** · **`3` = 已知 opcode 但 `funct3` / `funct7` 是 ISA 保留组合**。
  ⇒ ⇒ **优先级**：`1` > `3` > `2` > `0`（`OP_SYSTEM` 已是「已知未实现」⇒ 不重复判其 `funct3`）。
- ⚠️ **判定在 `Rv32iDecoder`（单一来源）** ⇒ ⇒ `IdStage` **只转发** + 保留 `valid` 门控
  ⇒ ⇒ **MUST NOT** 在两处各写一份判据（两份判据 = **静默分叉**）。
- ⚠️⚠️ **U 型（`LUI`/`AUIPC`）与 J 型（`JAL`）没有 `funct3` 字段**（bits 14:12 是**立即数位**）
  ⇒ ⇒ 它们的 bits 14:12 **任何取值都合法** ⇒ ⇒ 把「`funct3` MUST 为 `000`」套上去 =
  **误判合法指令为非法**（实测：`lui x10, 0x1` = `0x00001537` ⇒ bits 14:12 = `001` ⇒ 合法）。
  ⇒ ⇒ 判定表（含 `FENCE` 不参与）的唯一权威 = `docs/adr/ADR-071-fence-and-illegal-opcode-semantics.md` **§2.6.1**。
- **行为**：reason ≠ 0 ⇒ ⇒ **冻结**（PC 不前进 · ID/EX 装气泡 · 永不退休）。

## 流水线观测面：**事件端口** vs **级属性端口**（2026-11-24 · change `rv32i-commit-event-pulse-2026-11`）

> **两类端口 MUST 可区分** ⇒ ⇒ 把「级属性」错改成「事件」会制造**第二套语义** ⇒ ⇒ 禁止。

| 类别 | 语义 | 正确形态 | 本仓实例 |
|---|---|---|---|
| **事件端口** | 「本拍**发生**了 X 次」 | **单拍脉冲**（与事件**时长**无关） | `dbg_wb_write_en`（写回事件）· `obs_data_write_en`（store 提交事件）· `dbg_load_use_detected`（已是脉冲） |
| **级属性端口** | 「**当前寄存器**里那条指令是 X」 | **电平**（跟随寄存器内容） | `dbg_exmem_is_store` · `dbg_id_is_store` · `dbg_exmem_valid` · `dbg_mem_valid_out` |

- ⚠️ **为什么事件端口 MUST 是脉冲**：MEM/WB 与 EX/MEM 在数据通路反压（`mem_stall`）时
  **冻结** ⇒ ⇒ 被冻结的指令在 WB 级**停留多拍** ⇒ ⇒ **电平**端口会持续为高
  ⇒ ⇒ 「事件次数」被污染成「事件**时长**之和」⇒ ⇒ **期望值不可手推**
  ⇒ ⇒ 违反本仓纪律「**期望值不得依赖实现时序**」。
- ⚠️ **口径锚点** = `commit_count`（`rv32i_pipeline.h:833`）：
  `commit_valid = mem_valid_out & !mem_stall` ⇒ ⇒ 事件端口 MUST 与它**同口径**。
- ⚠️ **实现纪律（实测踩中）**：事件端口**只在观测面**门控 `!mem_stall`。
  ⚠️ **`io().data_write_en` MUST NOT 门控**（它在**功能路径**上：核顶层 → D-L1 的 `we`
  → `aw_fire` → `ready` → `data_ready` → `mem_stall` ⇒ ⇒ **组合环** ⇒ ⇒ 仿真直接
  `FATAL: Cycle detected in IR graph during topological sort`）⇒ ⇒ store 提交事件 MUST
  在**总线侧**派生（核顶层：`data_write_en & data_ready_calc`）。
- ⚠️ **脉冲不丢也不重**：MEM/WB 的内容只在**非停顿拍**才更新 ⇒ ⇒ 每条指令的 MEM/WB
  驻留的**首个**拍必是非停顿拍 ⇒ ⇒ `write_en & !mem_stall` **恰好一次**。
- **判别力证据**：`examples/riscv-mini/tests/test_rv32i_soc_dual_parity.cpp` 的
  `Rv32iSoc - CommitEventPulse`（双后端）⇒ ⇒ 变异（去掉 `& !mem_stall`）⇒ ⇒ **必红**。

## CLINT 的 `mtime` 契约（2026-10-30，ADR-049）

- **`mtime` 是 64 位**自由运行计数器**（不是「只能由软件写」的静态比较器）**——
  每个递增拍 +1（无符号回绕）⇒ 软件可以靠读它做时基；
- **递增频率**：`Clint<TIMER_DIVIDER>` 的**编译期**常量（默认 1 = 每拍 +1）
  ⇒ ⇒ **不是**运行期端口（新增输入端口 ⇒ 全部实例化点 MUST 显式驱动，
  而未驱动端口读到**未定义值**）；
- ⚠️ **分频相位 = 自复位以来的拍数** ⇒ ⇒ **任何** tick 都推进它
  （包括写 `mtimecmp` 的拍）⇒ ⇒ 测试 MUST 逐拍计，期望值 MUST **手推**；
- **写优先**（RISC-V 允许软件写 `mtime` 校时）：写的那一拍取**写值**、
  **另一半保持**、**本拍不递增** ⇒ ⇒ 写后的值成为**下一次**递增的起点；
  ⚠️ 写拍**照常推进**分频器（相位与写**无关**）⇒ ⇒ 写拍若恰是递增拍，
  该次递增**被丢弃**但相位**不变**；
- **复位值**：`mtime` = 0（`mtimecmp` 保持**全 1**）⇒ ⇒ 复位后**不**产生中断；
- **64 位读允许撕裂**（已知边界）：经 `read_data` + 地址偏移分两次读
  ⇒ 软件应读两次高半检测翻转 ⇒ ⇒ **不**新增原子读端口；
- ⚠️ **三条已实测的实现陷阱**（ADR-049 §1.4 + 实施期探针）MUST 规避：
  ① **进位 / 分频边界 MUST 用硬件原语**（`select`）—— C++ 三元在 `ch_bool`
  上是**建图期**求值 ⇒ **恒取 0** ⇒ 计数器**永远跨不过边界**；
  ② **「保高半换低半」MUST 用 `bits_update<32>`** —— `<<` 在 64 位上
  **编译不过**；手写 `hi | lo` **漏移位** ⇒ **静默清零高半**；
  ③ ⚠️ **`bits_update<32>` 在 64 位 target 上返回 64 位值** ⇒ MUST 用
  **64 位**变量接收（用 32 位接收 ⇒ **静默截断** ⇒ 「写高半」完全失效 ——
  实施期**实测踩中**过）。

## ANTI-PATTERNS
- **Namespace inconsistency**: CPU components split across 4 namespaces (`riscv`, `chlib`, `cpu`, `ch::chlib`)
- ~~**Duplicate forwarding**~~ ✅ **已消解（2026-11-11 · B0 批 C5 同批退役）**：`riscv::ForwardingUnit`（`include/cpu/pipeline/rv32i_forwarding.h`）**已退役** ⇒ ⇒ 「两套 ForwardingUnit 并存、靠 namespace 区分」这个反模式**不再存在** ⇒ ⇒ `chlib::ForwardingMux` 现是**唯一**的数据 mux 实现（ADR-046 D3 #14）⇒ ⇒ ⇒ ⇒ ⚠️ `chlib::ForwardingUnit`（源选择侧，`include/chlib/forwarding.h`）**仍在** ⇒ ⇒ ⇒ ⇒ 若日后要动它，**先**读 ADR-038 §2.3（「合并留给未来 change」的旧裁决）
- **把数据 mux 放进 `HazardUnit`**：会与真实流水线装配形成**组合环**（实测 `FATAL: Cycle detected in IR graph during topological sort`；`git show bf78367` 是当年的消环事故记录）⇒ mux MUST 由独立的 `ForwardingMux` 承担

## TESTS
Active CPU / RV32I test targets registered in `tests/CMakeLists.txt`
(the `test_interrupt_flow` row below is registered in
`examples/riscv-mini/CMakeLists.txt` instead):

| Target | Source path | Focus |
|--------|-------------|-------|
| `test_forwarding` | `tests/chlib/test_forwarding.cpp` | `chlib::ForwardingMux` + IfIdRegs struct field access |
| `test_hazard` | `tests/chlib/test_hazard_complete.cpp` | Hazard detection + forwarding |
| `test_rv32i_pipeline` | `tests/test_rv32i_pipeline.cpp` | 5-stage pipeline integration (Catch2) |
| `test_csr_unit` | `examples/riscv-mini/tests/test_csr_unit.cpp` | CSR register reset, R/W, MRET, exceptions, WFI, interrupts |
| `test_interrupt_flow` | `examples/riscv-mini/tests/test_interrupt_flow.cpp` | CLINT 定时器 / CSR 中断条件 / 陷阱入口 / MRET（**18 用例 / 68 断言**；2026-10-30 ADR-049 落地 `mtime` 自由运行语义 ⇒ 新增 **8** 个判别性用例；2026-10-23 注册进 CTest，此前从未编译） |
| `test_phase4_chlib` | `tests/chlib/test_phase4_integration.cpp` | Phase-4 chlib / CPU integration smoke (cache 用 `ICacheSync` / `DCacheSync` + hazard) |

`examples/riscv-mini/tests/` 的 Catch2 测试 target 自 2026-10-23 起
**已注册进 CTest**（change `riscv-mini-test-triage-2026-10`，**14** 个 target；
另有 1 个 `riscv_mini_catch_test_noctest` 只构建不注册 ⇒ 共 15 个声明），
由 `examples/riscv-mini/CMakeLists.txt` 的 `riscv_mini_catch_test()` helper
统一 `add_test`。⚠️ 此前它们「每次 CI 都编译、却从未被执行」——根因是根
`CMakeLists.txt` 的 `enable_testing()` 排在 `add_subdirectory(examples)` **之后**
⇒ examples 子目录的 `add_test()` 是空操作。护栏：
`scripts/verify_example_test_wiring.sh`（归属守恒 + 数量守恒）。

⚠️ **2026-10-24 收敛后**：`examples/riscv-mini/src/` 只剩 **1** 个头
（核 IP 顶层 `rv32i_core_top.h`，ADR-046 D1 类别 1 / D3 #15）——
其余 31 个头已删除（17 个死头）或提升到本目录（10 个）或删除重复副本（4 个）。
`rv32i_core_top.h` **已归位**（不再是零消费者；2026-10-25，change
`rv32i-pipeline-commit-path-2026-10`）—— 此前它**零消费者**（其唯一消费者 `test_pipeline_smoke`
在 C1 被删除）⇒ 已登记在 `scripts/verify_no_duplicate_rv32i_headers.sh` 的
零消费者豁免表里，**归位条件 = C3**（`rv32i-soc-dual-parity-2026-10`）落地。

## PHASE GATES
Follow root Zero-Debt Policy: **编译通过 + 测试覆盖 + 文档同步**.
Pipeline phases (1/2/3) pass — full integration runtime requires `ch_device` wrapper.；**6817 → 6966 · 头文件数不变** 因 **S3（change `rv32i-dcache-burst-2026-11`）**：`cache/d_cache_sync.h` 的 AXI 侧改 **AXI4 full 主口**（AR/R/AW/W/B）+ **4-beat burst fill** + 修「**假 line**」（`offset` 参与读/写选择）；**6966 行**因 **S3（change `rv32i-dcache-burst-2026-11`）第二/三阶段**：`d_cache_sync.h` 的 `rdata` 改**保持寄存器**（S2 的 MEM/WB 冻结语义强制）、`ready`/`miss` 改**组合**（对当前 `addr`）⇒ ⇒ 另新增 `axi4/tcm_axi_slave.h`（不属于 `include/cpu/**` ⇒ 不计入本目录行数）。⚠️ **核接线后的三条硬纪律**（实施期实测，违反即静默失效）：① `data_ready ← !data_req | cache.ready`（`data_ready` 是**无条件**冻结信号 ⇒ 空闲拍被拖低 ⇒ 整条流水线永久冻结）；② `data_req` 只在 **MEM 级 load/store** 时为 1（cache 读**有副作用**：miss ⇒ AXI burst ⇒ 否则 ALU/NOP 触发 miss 风暴）；③ 调用方 MUST 在 `req` 有效期间**保持 `addr`**（真握手 ⇒ `ready=1` 才可改）
