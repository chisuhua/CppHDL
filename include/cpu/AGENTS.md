# AGENTS.md - CPU / RISC-V Subsystem

Child of `include/AGENTS.md` → root `AGENTS.md` (ZERO-DEBT POLICY + PHASE GATES).

## OVERVIEW
RV32I 5-stage pipeline + cache hierarchy + forwarding + hazard detection + SoC integration.
**30 headers, 6376 lines**（2026-10-28 实测，ADR-046 D9：磁盘是权威）。
All header-only.

> ⚠️ 2026-10-24（change `rv32i-canonical-convergence-2026-10`，ADR-046）：
> 本目录是 RV32I 微架构的**唯一权威**。此前 `examples/riscv-mini/src/` 存在
> **10 组同 basename 头文件**，消歧纯靠 `-I` 顺序（无护栏 ⇒ 调序即静默换实现），
> 且根 `tests/` 反向依赖 example 源码。⇒ 已收敛：`examples/riscv-mini/src/`
> 现只剩 SoC 顶层 `rv32i_pipeline_top.h`。
> 护栏：`scripts/verify_no_duplicate_rv32i_headers.sh`（**6 条检查** + `--self-test` 自测）。

## STRUCTURE
```
include/cpu/                             # 30 headers, 6376 lines（2026-10-28 实测）
├── cache/                            # ch_mem-based cache (ADR-039, ADR-041)
│   ├── i_cache_sync.h / d_cache_sync.h       # 1-cycle sread — RV32I L1 默认
│   ├── i_cache_async.h / d_cache_async.h     # 0-cycle aread — TCM-style 备选
│   ├── i_cache_complete.h / d_cache_complete.h  # 兼容 shim (= *_sync.h 别名)
│   ├── i_cache_prefetch.h                    # wrapper (instance ICacheSync)
│   └── d_cache_write_buffer.h                # AXI 写直通 shim（**无**内层 cache）
├── pipeline/                          # 流水线数据通路（ADR-046 D2）
│   ├── rv32i_decoder.h            # R/I/S/B/U/J format decode (161)
│   ├── rv32i_pipeline_regs.h       # IF/ID/EX/MEM/WB regs (490 — largest)
│   ├── rv32i_pipeline.h            # 5-stage top-level (247)
│   ├── rv32i_tcm.h                # Tightly-coupled memory (ITCM/DTCM) (133)
│   ├── hazard_unit.h               # 冒险检测 + 停顿/冲刷控制 (178)
│   ├── hazard_detection_unit.h     # Load-Use 冒险检测（独立组件） (65)
│   ├── rv32i_hazard_complete.h     # 组合式 hazard 单元 (195)
│   ├── rv32i_alu.h                # ALU (172)
│   ├── rv32i_regfile.h            # 寄存器堆 (316)
│   ├── rv32i_pc.h                  # PC (47)
│   ├── rv32i_forwarding.h         # riscv::ForwardingUnit（源选择） (174)
│   ├── rv32i_branch_predict.h     # Branch/条件/预测/控制冒险 (475)
│   ├── branch_predictor_v2.h       # 2-bit 分支预测器 (151)
│   └── stages/{if,id,ex,mem,wb}_stage.h
└── riscv/                            # 体系结构 / 特权级
    ├── rv32i_csr.h                 # CSR bank (9 registers, 252)
    ├── rv32i_exception.h          # Trap entry + MRET (177)
    ├── address_decoder.h          # MMIO 6-region decoder (119)
    └── clint.h                    # Core Local Interrupt Controller (130)
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
| Forwarding (源选择) | `pipeline/rv32i_forwarding.h`（`riscv::ForwardingUnit`）|
| Forwarding (数据 mux) | `include/chlib/forwarding.h`（`chlib::ForwardingMux`）—— ADR-046 D3 #14 / ADR-047 D2（**已接入真实流水线**）|
| Branch 预测 / 控制冒险 | `pipeline/rv32i_branch_predict.h` |
| 冒险检测（Load-Use） | `pipeline/hazard_detection_unit.h` |
| I-Cache (1-cycle, RV32I L1 默认) | `cache/i_cache_sync.h` (`i_cache_complete.h` 是兼容 shim 别名) |
| I-Cache (0-cycle, TCM-style) | `cache/i_cache_async.h` |
| D-Cache (1-cycle, RV32I L1 默认) | `cache/d_cache_sync.h` (`d_cache_complete.h` 是兼容 shim 别名) |
| D-Cache (0-cycle, TCM-style) | `cache/d_cache_async.h` |
| I-Cache wrapper | `cache/i_cache_prefetch.h` (instance ICacheSync) |
| D-Cache 写直通 shim | `cache/d_cache_write_buffer.h`（**无**内层 cache / **无**读通路） |
| SoC 顶层装配 | `examples/riscv-mini/src/rv32i_pipeline_top.h`（**唯一**留在 example 侧的头；ADR-046 D3 #15）|
| 流水线诊断观测 | `pipeline/rv32i_pipeline.h` 的 `dbg_*` 输出（ID 级指令流 / 分支判定 / 有效链 / 写回 / EX-MEM 数据通路；**SoC 顶层须逐个引出**，否则 verilator 读不到 ⇒ 2026-10-25 追出 17 个流水线缺陷）|

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
- **load 侧的字节道契约**（2026-10-28，change `rv32i-load-byte-lane-2026-10`）：
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
    名字 over-promise ⇒ `ready` 无背压语义）；`ICachePrefetch` 的**预取缓冲**
    是死数据（真预取未实现）；内层 cache 的 `miss`(1 拍 `sread`) 与
    `axi_addr`(当拍地址) 存在 **1 拍错位**（属 cache 本体 ⇒ wrapper 转发
    **不**加剧、也**不**修它）。
    ⇒ 判别力证据：`examples/riscv-mini/tests/test_cache_smoke.cpp`
    （**8 用例 / 58 断言**，**双后端**，含内层 **fill → hit** 路径）。

## ANTI-PATTERNS
- **Namespace inconsistency**: CPU components split across 4 namespaces (`riscv`, `chlib`, `cpu`, `ch::chlib`)
- **Duplicate forwarding**: `include/chlib/forwarding.h`（`chlib::ForwardingUnit` + `chlib::ForwardingMux`）vs `include/cpu/pipeline/rv32i_forwarding.h`（`riscv::ForwardingUnit`）—— 并存，靠 namespace 区分（ADR-038 §2.3 已裁决「合并留给未来 change」）；`chlib::ForwardingMux` 是 ADR-046 D3 #14 指定的**唯一**数据 mux 实现
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
| `test_interrupt_flow` | `examples/riscv-mini/tests/test_interrupt_flow.cpp` | CLINT 定时器 / CSR 中断条件 / 陷阱入口 / MRET（10 用例 / 28 断言；2026-10-23 注册进 CTest，此前从未编译） |
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
（SoC 顶层 `rv32i_pipeline_top.h`，ADR-046 D1 类别 1 / D3 #15）——
其余 31 个头已删除（17 个死头）或提升到本目录（10 个）或删除重复副本（4 个）。
`rv32i_pipeline_top.h` **已归位**（不再是零消费者；2026-10-25，change
`rv32i-pipeline-commit-path-2026-10`）—— 此前它**零消费者**（其唯一消费者 `test_pipeline_smoke`
在 C1 被删除）⇒ 已登记在 `scripts/verify_no_duplicate_rv32i_headers.sh` 的
零消费者豁免表里，**归位条件 = C3**（`rv32i-soc-dual-parity-2026-10`）落地。

## PHASE GATES
Follow root Zero-Debt Policy: **编译通过 + 测试覆盖 + 文档同步**.
Pipeline phases (1/2/3) pass — full integration runtime requires `ch_device` wrapper.