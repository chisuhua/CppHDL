# AGENTS.md - RISC-V Mini Examples

Child of root `AGENTS.md`.

## OVERVIEW
RV32I 5-stage pipeline 的**示例与测试**。微架构组件本身已收敛到 `include/cpu/`
（本目录只留 SoC 顶层，ADR-046）。Phases 1-3 pass.

## STRUCTURE
```
examples/riscv-mini/
├── CMakeLists.txt
├── rv32i_phase1_test.cpp    # RegFile + ALU + Decoder
├── rv32i_phase2_test.cpp    # Branch + Jump (13/13)
├── rv32i_phase3_test.cpp    # Load/Store (13/13)
├── tb_rv32i_soc.cpp         # **激励驱动**（三角色分离 · 与 `examples/axi4` / `examples/stream` 同型）
│                            #   ⇒ 默认模式 = **验证模式**（解释器路径 ⇒ 24 条手推判据
│                            #     ⇒ `RESULT: checks=24 fails=0`）
│                            #   ⇒ `--emit-rtl` = **发射模式**（扁平 RTL 产物）
│                            #   ⇒ `--emit-rtl --hierarchical` = 层次化产物（诉求 ⑤ 的产品级交付面
│                            #     ⇒ 走库层 CLI 的 `--hierarchical`（`toVerilogHierarchical`）
│                            #     ⇒ 产物 = 单文件 8 module · 顶层端口表与扁平**同源**（34）
│                            #     ⇒ **复用同一个独立 TB** `tb_rv32i_soc.sv`）
│                            #   ⇒ `--emit-rtl --firmware-init` = 带固件初值的扁平产物
│                            #     （`firmware-load-chain` L2 路线 A）
│                            # ⚠️ 原三个发射薄壳（`emit_rtl.cpp` / `emit_rtl_fw_init.cpp` /
│                            #   `emit_rtl_hier.cpp`）**已删**（2026-11-22）⇒ ⇒ 删除留痕见
│                            #   `examples/removed_examples.txt`；**产物逐字节不变**
├── tb_rv32i_soc.sv         # **独立 Verilog TB**（**手写** · 24 条判据 · 与 C++ 侧同源）
│                            #   ⇒ `.verif` 封闭原语集表达不了 4 个形态 ⇒ MUST 保持手写
│                            #     （`generated=no` = **终态**，不是欠账 · ADR-076 复核 2026-11-21）
├── src/                     # **2** 个头（SoC 顶层 + 带 `.data` 的示例固件）
│   ├── rv32i_fw_demo.h      # 带 `.data` 的示例固件（I-TCM / D-TCM 两段字镜像 · 编码手推）
│   └── rv32i_pipeline_top.h # SoC 顶层装配（ADR-046 D1 类别 1 / D3 #15）
│                             #  ✅ **已有消费者**（2026-10-25）：`tests/
│                             #  test_rv32i_soc_dual_parity.cpp`（SoC 级双路
│                             #  parity 测试的 DUT）⇒ 单源化护栏的零消费者
│                             #  豁免条目已移除（ADR-046 D10-rev 端口契约）
└── tests/                   # ✅ 15 个 Catch2 target 已注册进 CTest（2026-10-25 +1：
                              #   test_rv32i_soc_dual_parity —— SoC 级双后端 parity）
    ├── test_*.cpp           # 17 个 test source（另有 8 个已删除，留痕见 removed_examples.txt）
    ├── minitest_prog.h      # 测试辅助头（非 test source）
    └── （Catch2 框架在根 `tests/catch_amalgamated.{cpp,hpp}`，不在本目录）
```

> ⚠️ **2026-10-23（change `riscv-mini-test-triage-2026-10`）**：本目录的 14 个
> Catch2 target **已注册进 CTest**（此前「每次 CI 都编译、却从未被执行」——
> 根因是根 `CMakeLists.txt` 的 `enable_testing()` 排在 `add_subdirectory(examples)`
> 之后 ⇒ examples 子目录的 `add_test()` 是空操作）。
> 护栏：`scripts/verify_example_test_wiring.sh`。

## RUN（SoC 产品面 · 三角色）
```bash
# ① C++ 侧（解释器路径）· 24 条手推判据
build/examples/riscv-mini/tb_rv32i_soc
#   ⇒ RESULT: checks=24 fails=0

# ② 扁平 RTL 产物（发射入口 = TB 自身）
build/examples/riscv-mini/tb_rv32i_soc --emit-rtl --out out/flat --emit-hygiene

# ③ 层次化 RTL 产物（8 module · 顶层端口表与扁平同源）
build/examples/riscv-mini/tb_rv32i_soc --emit-rtl --hierarchical --out out/hier --emit-hygiene

# ④ 带固件初值的扁平产物（TCM 的 `initial` 初值块）
build/examples/riscv-mini/tb_rv32i_soc --emit-rtl --firmware-init --out out/fw --emit-hygiene

# ④' 带固件初值的**层次化**产物（两个模式标记正交 ⇒ 组合即得 · 判据见
#    scripts/verify_rv32i_fw_init_rtl.sh 的 F7a/F7b/F7c）
build/examples/riscv-mini/tb_rv32i_soc --emit-rtl --hierarchical --firmware-init \
    --out out/hier_fw --emit-hygiene

# ⑤ 一条命令跑通（生成 → lint → 编译 → 运行 ⇒ RESULT: checks=24 fails=0）
bash scripts/rv32i_rtl_sim_env.sh
```

⚠️ **fail-closed**：默认模式收到**任何**未知参数 ⇒ exit **2**；
发射模式缺 `--out` ⇒ exit **2**（MUST NOT 回落到库层默认 `rtl_out/`）。

## PASSING TESTS
- Phase 1 (RegFile/ALU/Decoder): ✅ 537 lines test_forwarding, 235 lines test_hazard_complete
- Phase 2 (Branch/Jump): ✅ 13/13 instructions
- Phase 3 (Load/Store): ✅ 13/13 instructions
- cache-complete-rewrite smoke (`riscv_mini_cache_smoke`): ✅ compile probe + 3 PASS instantiate (2026-10-20 `fix-riscv-mini-cache-prefetch-sigsegv` 修复: ch_reg array → ch_mem + ch_module 替换)

## KNOWN ISSUES
- ~~`rv32i_pipeline_cache.h` — I/D-Cache integration chain incomplete~~ —— ⚠️ 该文件**在磁盘上不存在**（零消费者 ⇒ 已于 2026-10-24 按 ADR-046 D4 删除）
- `dynamic_branch_predict.h` — DELETED during code debt cleanup
- `phase3b_complete.cpp` — DELETED (used `ch::log2_ceil` and `ch_array` which don't exist)
- `tests/` subdirectory — ✅ 已注册进 CTest（2026-10-23，14 个 target）；
  `test_csr_unit` 早已以根 `tests/` 的 target 身份存在
- 5-stage pipeline `test_rv32i_pipeline.cpp` — ✅ rewritten, passes (5 tests, 23 assertions)；
  **2026-10-23 起以 `test_rv32i_pipeline_example` 注册进 CTest**（此前从未编译）。
  ⚠️ 它与根 `tests/test_rv32i_pipeline.cpp`（12 用例）**不是**副本关系：前者测
  **前推通路**（`HazardUnit` + `chlib::ForwardingMux` 组合 DUT，见
  `tests/forwarding_dut_fixture.h`），后者测**流水线各级**。
  ⚠️ **2026-11-11（B0 批 C5+C6 同批退役）**：`riscv::ForwardingUnit` 与
  `riscv::HazardDetectionUnit` **已退役** ⇒ ⇒ 后者原先测的 `HazardDetectionUnit`
  相关 SECTION 已删 ⇒ ⇒ 前者的覆盖改由 `riscv::HazardUnit`（仓内权威组件）承担。
  ⚠️ 2026-10-24（ADR-046 D3 #14）：前推数据 mux 从 `HazardUnit` 拆到
  **既有的** `chlib::ForwardingMux` —— 因为 mux 留在 `HazardUnit` 里并接进真实
  流水线装配会形成**组合环**（实测 `FATAL: Cycle detected in IR graph during
  topological sort`）。断言的**期望值不变**。
- `test_interrupt_flow.cpp` — ✅ **2026-10-23 修好并注册进 CTest**（10 用例 / 28 断言；
  覆盖 CLINT 定时器 / CSR 中断条件 / 陷阱入口 / MRET）。修它时挖出
  `include/cpu/riscv/clint.h` 的**真实 RTL bug**：地址解码用 `bits<1,0>` 而非
  `bits<3,2>` ⇒ 4 个寄存器全部别名到 `mtime_lo`（已修 1 行）。
- **`i_cache_prefetch.h` SIGSEGV** ✅ **FIXED** (2026-10-20, change `fix-riscv-mini-cache-prefetch-sigsegv`): (1) `ch_reg<ch_bool>[N]` + runtime init for-loop → `ch_mem<ch_uint<1>, N>` (per ADR-039 + cache-complete-doa-note §6.1 R1); (2) `ICacheComplete icache{this, ...}` 替换为 `ch::ch_module<ICacheComplete>` (per ROOT AGENTS.md "COMPONENT LIFECYCLE PATTERNS"). Smoke test 2 SKIP → 2 PASS instantiate.
- **`d_cache_write_buffer.h` review** (2026-10-20): buffer 是普通 struct 数组 (WriteBufferEntry buffer[4])，**非 ch_reg 数组**, 无根架构问题; instantiate PASS 验证无 SIGSEGV.
  ⚠️ 2026-11-09（phase-6 P9 · Zero-Debt）：该 struct 数组（**零消费者**的死数据）**已删除** ⇒ 本条仅存历史；wrapper 的 `__io` 签名与可观测行为**未变**。
- **wrapper 端口契约错误** (2026-10-19) — `i_cache_prefetch.h:91` 引用 `icache.io().hit` (不存在); 已修: `ch_bool(icache.io().miss.impl())`. 同时 `0xFFFFFFF0_d` → `0xFFFFFFF0_h` typo fix.

## WHERE TO LOOK
| Task | Location |
|------|----------|
| Forwarding (数据 mux) | `include/chlib/forwarding.h`（`chlib::ForwardingMux`）—— **已接入真实流水线**（ADR-047 D2）|
| Hazard detection | `include/cpu/pipeline/hazard_unit.h`（`riscv::HazardUnit`）—— **已接入真实流水线** |
| Branch predictor | `include/cpu/pipeline/branch_predictor_v2.h` / `rv32i_branch_predict.h` —— ⚠️ **非流水线路径**（未接入 ⇒ 每个 taken 分支付 3 拍冲刷）|
| I-Cache wrapper | `include/cpu/cache/i_cache_prefetch.h` (✅ FIXED 2026-10-20) |
| D-Cache wrapper | `include/cpu/cache/d_cache_write_buffer.h` (✅ review PASS 2026-10-20) |
| Pipeline stages | `include/cpu/pipeline/stages/` |
| ALU / RegFile / PC / decoder | `include/cpu/pipeline/{rv32i_alu,rv32i_regfile,rv32i_pc,rv32i_decoder}.h` |
| SoC 顶层（唯一留在本目录的头）| `src/rv32i_pipeline_top.h` |
| SoC 的**激励驱动**（C++ 侧 + 发射入口）| `tb_rv32i_soc.cpp`（**24 条判据**的 C++ 侧实现）|
| SoC 的**独立 Verilog TB**（手写）| `tb_rv32i_soc.sv`（**24 条判据**的 Verilog 侧实现）|
| 期望值**单一真相**（两侧共用）| `.rddf/checks/examples-rtl-expectations.md#riscv-mini-rv32i-soc` |

> ⚠️ **2026-10-24（change `rv32i-canonical-convergence-2026-10`，ADR-046）**：
> 本目录的 `src/` 曾有 **32** 个头，其中 10 组与 `include/cpu/` **同名**
> （消歧纯靠 `-I` 顺序 ⇒ 调序即静默换实现）⇒ 已收敛到**只剩 1 个**
> （SoC 顶层）。所有可复用的微架构组件现在都在 `include/cpu/`。
> 护栏：`scripts/verify_no_duplicate_rv32i_headers.sh`。

## PHASE GATES
Follow root Zero-Debt Policy: **编译通过 + 测试覆盖 + 文档同步**. For riscv-mini:
- `.cpp` registered in `CMakeLists.txt` (no untracked binaries)
- `tests/` subdirectory must be wired to main CMake (delete if not needed)
- **No stubs**: empty test files use `SKIP()` or are removed
- **No dead code**: `phase3b_complete.cpp` must be fully integrated or removed
