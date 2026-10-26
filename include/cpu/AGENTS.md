# AGENTS.md - CPU / RISC-V Subsystem

Child of `include/AGENTS.md` → root `AGENTS.md` (ZERO-DEBT POLICY + PHASE GATES).

## OVERVIEW
RV32I 5-stage pipeline + cache hierarchy + forwarding + hazard detection + SoC integration.
**30 headers, 6016 lines**（2026-10-25 实测，ADR-046 D9：磁盘是权威）。
All header-only.

> ⚠️ 2026-10-24（change `rv32i-canonical-convergence-2026-10`，ADR-046）：
> 本目录是 RV32I 微架构的**唯一权威**。此前 `examples/riscv-mini/src/` 存在
> **10 组同 basename 头文件**，消歧纯靠 `-I` 顺序（无护栏 ⇒ 调序即静默换实现），
> 且根 `tests/` 反向依赖 example 源码。⇒ 已收敛：`examples/riscv-mini/src/`
> 现只剩 SoC 顶层 `rv32i_pipeline_top.h`。
> 护栏：`scripts/verify_no_duplicate_rv32i_headers.sh`（**6 条检查** + `--self-test` 自测）。

## STRUCTURE
```
include/cpu/                             # 30 headers, 6016 lines（2026-10-25 实测）
├── cache/                            # ch_mem-based cache (ADR-039, ADR-041)
│   ├── i_cache_sync.h / d_cache_sync.h       # 1-cycle sread — RV32I L1 默认
│   ├── i_cache_async.h / d_cache_async.h     # 0-cycle aread — TCM-style 备选
│   ├── i_cache_complete.h / d_cache_complete.h  # 兼容 shim (= *_sync.h 别名)
│   ├── i_cache_prefetch.h                    # wrapper (instance ICacheSync)
│   └── d_cache_write_buffer.h                # wrapper (instance DCacheSync)
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
| Forwarding (数据 mux) | `include/chlib/forwarding.h`（`chlib::ForwardingMux`）—— ADR-046 D3 #14 |
| Branch 预测 / 控制冒险 | `pipeline/rv32i_branch_predict.h` |
| 冒险检测（Load-Use） | `pipeline/hazard_detection_unit.h` |
| I-Cache (1-cycle, RV32I L1 默认) | `cache/i_cache_sync.h` (`i_cache_complete.h` 是兼容 shim 别名) |
| I-Cache (0-cycle, TCM-style) | `cache/i_cache_async.h` |
| D-Cache (1-cycle, RV32I L1 默认) | `cache/d_cache_sync.h` (`d_cache_complete.h` 是兼容 shim 别名) |
| D-Cache (0-cycle, TCM-style) | `cache/d_cache_async.h` |
| I-Cache wrapper | `cache/i_cache_prefetch.h` (instance ICacheSync) |
| D-Cache wrapper | `cache/d_cache_write_buffer.h` (instance DCacheSync) |
| SoC 顶层装配 | `examples/riscv-mini/src/rv32i_pipeline_top.h`（**唯一**留在 example 侧的头；ADR-046 D3 #15）|
| 流水线诊断观测 | `pipeline/rv32i_pipeline.h` 的 `dbg_*` 输出（ID 级指令流 / 分支判定 / 有效链 / 写回 / EX-MEM 数据通路；**SoC 顶层须逐个引出**，否则 verilator 读不到 ⇒ 2026-10-25 追出 17 个流水线缺陷）|

## CONVENTIONS
- **Pipeline instantiation**: `ch::ch_module<T>` inside `Component::describe()` — see root AGENTS.md
- **Runtime**: Pipeline requires `ch::ch_device<Rv32iSoc>` wrapper in main() (per README)
- **Bundle vs Port**: Pipeline uses port-based `hazard_unit.h` (riscv namespace)。⚠️ 旧的 bundle 版 `hazard_unit_bundle.h`（cpu namespace）**已于 2026-10-24 作为零消费者头删除**（ADR-046 D4）
- **TCM**: `rv32i_tcm.h` provides single-cycle instruction/data memories
  - ⚠️ **`DataTCM` 的字节道契约**（2026-10-26 新增 `strbe` 端口）：`addr` 是**字地址**、
    `wdata` MUST 已**lane 对齐**（`rs2 << 8*addr[1:0]`，由流水线负责）、`strbe` 是车道掩码
    ⇒ 写路径是**读-改-写**（`ch_mem` 无字节使能写 API）。⚠️ `strbe` 是**新增输入端口**
    ⇒ **全部实例化点 MUST 显式驱动**（未驱动端口读到的是**全 1** 而非 0 ⇒ 整字写会「碰巧」通过）
- **Cache variant selection** (ADR-041):
  - **RV32I L1 imem/dmem** (默认) → `ICacheSync` / `DCacheSync` (1-cycle sread)
  - **软核 TCM-style** (低频, 延迟关键) → `ICacheAsync` / `DCacheAsync` (0-cycle aread)
  - **imem 必须 sread** (per `verilator-e2e-coverage` spec §R1 invariant 3): 不要在 RV32I imem 用 Async

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