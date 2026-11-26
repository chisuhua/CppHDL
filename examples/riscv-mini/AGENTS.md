# AGENTS.md - RISC-V Mini Examples

Child of root `AGENTS.md`.

## OVERVIEW
RV32I 5-stage pipeline 的**示例与测试**。微架构组件本身已收敛到 `include/cpu/`
（本目录只留核 IP 顶层，ADR-046）。Phases 1-3 pass.

> **「riscv-mini 完成」的定义**（用户口径 · 2026-11-25）=
> **官方 riscv-tests 的 `rv32ui-p-*` 套件全过**（40 个 · 排除 2 个已登记项）
> ⇒ ⇒ 门禁 = `ctest -R test_rv32i_official_suite`（见 §RUN ⑥）
> ⇒ ⇒ 资产 / 排除项 / 复现 = `tests/riscv-tests/PROVENANCE.md`（**单一真相**）。
> ⚠️ **不是**「本仓自写用例全过」—— 两者是**不同**的判据面。
>
> ⚠️ **2026-11-25（change `rv32i-official-rtl-gate-2026-11`）——「全过」MUST 在
> **两层**上成立**（口径**收紧**，不是改口径）：
> | 层 | 判据面 | 载体 | 门禁 |
> |---|---|---|---|
> | **核级** | 本仓那份 **C++**（解释器） | `ch_device<Rv32iCoreTop, itcm, dtcm>` | `ctest -R test_rv32i_official_suite`（§RUN ⑥） |
> | **交付物级** | 交付给用户的 **Verilog 产物** | `--emit-rtl --firmware <镜像>` 的 `top.v` | `bash scripts/verify_rv32i_official_rtl.sh --full`（§RUN ⑦） |
>
> ⇒ ⇒ **为什么必须有第二层**：解释器与 codegen 由**同一份 C++**生成 ⇒ ⇒
> **同构缺陷两后端一致地错** ⇒ ⇒ codegen 侧缺陷（初值块 / 端口映射 / 时序形状）
> **只**在产物级可见 ⇒ ⇒ **实证**：该门禁初版抓出「带固件产物 **40/40 lint 全红**」
> （`WIDTHEXPAND` ⇒ 产物按仓内口径**编译失败**）—— 而核级门禁对此**结构性失明**。

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
│                            #   ⇒ `--firmware <dir>` = 从 **ELF 派生的镜像**装载
│                            #     （**方案 a**：`rv32i-fw-dump --format mem-split` 的产物 ⇒
│                            #      `ch_mem::init_data` ⇒ **零 `prog_*` 写**）
│                            #     （`firmware-load-chain` L2 路线 A）
│                            # ⚠️ 原三个发射薄壳（`emit_rtl.cpp` / `emit_rtl_fw_init.cpp` /
│                            #   `emit_rtl_hier.cpp`）**已删**（2026-11-22）⇒ ⇒ 删除留痕见
│                            #   `examples/removed_examples.txt`；**产物逐字节不变**
├── tb_rv32i_soc.sv         # **独立 Verilog TB**（**手写** · 24 条判据 · 与 C++ 侧同源）
│                            #   ⇒ `.verif` 封闭原语集表达不了 4 个形态 ⇒ MUST 保持手写
│                            #     （`generated=no` = **终态**，不是欠账 · ADR-076 复核 2026-11-21）
├── tb_rv32i_official.sv    # **交付物级 TB**（官方套件 · **镜像无关** · change
│                            #   `rv32i-official-rtl-gate-2026-11`）
│                            #   ⇒ 镜像在**产物**的 `initial` 块里 ⇒ ⇒ **TB 不装载**
│                            #   ⇒ 判据 = **上游 env 的 `tohost` 协议**（`tohost == 1` ⇒ PASS）
│                            #   ⇒ 期望值由门禁脚本用 `+tohost_expected=<N>` 给
│                            #     （负向对照 = 3 ⇒ MUST 如实读到 3）
├── src/                     # **3** 个头（核 IP 顶层 + MMIO 主设备 + 带 `.data` 的示例固件）
│   ├── rv32i_fw_demo.h      # 带 `.data` 的示例固件（I-TCM / D-TCM 两段字镜像 · 编码手推）
│   ├── axil_mmio_master.h  # 核侧 AXI4-Lite 主设备（S4a · MMIO 通道的核内一端
│   │                          #   ⇒ 请求面 7 根 + AXI4-Lite 19 根 · `ready` = **单拍完成脉冲**
│   │                          #   ⇒ 判据 = `test_axil_mmio_master`（5 用例 / 58 断言）
│   │                          #   ⇒ 核级端到端 = `test_rv32i_core_mmio`（7 用例 / 50 断言 · 含慢从设备档））
│   └── rv32i_core_top.h # 核 IP 顶层装配（ADR-046 D1 类别 1 / D3 #15）
│                             #  ✅ **已有消费者**（2026-10-25）：`tests/
│                             #  test_rv32i_soc_dual_parity.cpp`（核级双路
│                             #  parity 测试的 DUT）⇒ 单源化护栏的零消费者
│                             #  豁免条目已移除（ADR-046 D10-rev 端口契约）
└── tests/                   # ✅ 26 个 Catch2 target 已注册进 CTest（另有 1 个 noctest 只构建）
    ├── test_*.cpp           # 24 个 test source（另有 8 个已删除，留痕见 removed_examples.txt）
    ├── minitest_prog.h      # 测试辅助头（非 test source）
    ├── test_rv32i_stall_integrity.cpp  # **反压期操作数完整性**（3 形态 · A1/A2/A3）
    ├── test_rv32i_official_suite.cpp   # **官方 riscv-tests 套件**（40 个 · 4 个二进制）
    ├── riscv-tests/         # **官方 riscv-tests 资产**（change `riscv-tests-gate-2026-11`）
    │   ├── PROVENANCE.md    # 来源 / 改造 / **排除项** / 格式 / 复现方法（**单一真相**）
    │   ├── LICENSE.riscv-tests  # 上游 BSD-3-Clause 全文
    │   ├── env/riscv_test.h # 自带 env（**只替换 4 个 harness 宏** · 测试体一字未改）
    │   ├── env/link.ld      # **双 TCM 链接布局**（`.tohost` @ 0x8001_0000）
    │   ├── env/build.sh     # 从上游重建 40 份 + 与入库镜像逐字节比对
    │   └── images/          # 40 份 `rv32ui-p-*` 的 `itcm.mem` / `dtcm.mem`
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

# ④'' 从 **ELF 派生的镜像**装载（**方案 a**：ELF → mem-split → init_data → 核取指）
build/tools/rv32i-fw-dump --in fw.elf --format mem-split --out out/fwimg
build/examples/riscv-mini/tb_rv32i_soc --firmware out/fwimg
#   ⇒ 打印协议行 `FIRMWARE-TRACE: retired=… fetch0=… wb=… stores=…`
#   ⇒ ⚠️ TB **不**硬编码固件期望值（固件专属期望只在 `scripts/verify_rv32i_elf_e2e.sh`）
build/examples/riscv-mini/tb_rv32i_soc --emit-rtl --firmware out/fwimg --out out/fwprod

# ④' 带固件初值的**层次化**产物（两个模式标记正交 ⇒ 组合即得 · 判据见
#    scripts/verify_rv32i_fw_init_rtl.sh 的 F7a/F7b/F7c）
build/examples/riscv-mini/tb_rv32i_soc --emit-rtl --hierarchical --firmware-init \
    --out out/hier_fw --emit-hygiene

# ⑤ 一条命令跑通（生成 → lint → 编译 → 运行 ⇒ RESULT: checks=24 fails=0）
bash scripts/rv32i_rtl_sim_env.sh

# ⑥ 官方 riscv-tests 套件门禁（40 个 `rv32ui-p-*` · 4 个 CTest 条目 · 每个 ≈ 100 s）
ctest --test-dir build -R test_rv32i_official_suite --output-on-failure
#   ⇒ 判定口径 = **上游 env 的协议**（`tohost == 1` ⇒ PASS）
#   ⇒ ⇒ 「riscv-mini 完成」的定义 = 该套件**全过**
#   ⇒ ⇒ 资产（镜像 / env / 来源登记）= tests/riscv-tests/（见 PROVENANCE.md）

# ⑦ 官方 riscv-tests 套件的**交付物级**门禁（**RTL 产物**侧 · 需真 verilator）
#   ⚠️ 与 ⑥ **同口径**但**不同层**：⑥ 判「本仓那份 C++」· ⑦ 判「交付的 Verilog 产物」
bash scripts/verify_rv32i_official_rtl.sh --full      # 全量 40 个（≈ 10 min）
bash scripts/verify_rv32i_official_rtl.sh --subset 8   # 抽样（PR 快通道 ≈ 2 min）
#   ⇒ 流程 = 发射（带镜像）⇒ ⇒ lint ⇒ ⇒ 编译 ⇒ ⇒ 运行 ⇒ ⇒ 判 tohost
#   ⇒ ⇒ **lint 是门禁的一环**（带固件初值的产物 MUST lint 干净）
#   ⇒ ⇒ 含**负向对照**（手写镜像写 3 ⇒ MUST 如实读到 3 ⇒ 读到 1 ⇒ 门禁红）
#   ⇒ ⇒ CI 落点：`pr-verilator-fast`（抽样 · shard 1）· `nightly-verilator`（全量）
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

## 核 IP 的**53 端口契约表**（产品端口面 · **本文件 = 单一真相源**）
> 这是「核 = 可替换 IP」的**唯一物证**，也是 S3（总线化）的**变更基线**。
> ⚠️ **归属**：本表描述的是**核**（`Rv32iCoreTop`）⇒ ⇒ 契约表住在**核的家**
> （`examples/riscv-mini/AGENTS.md`）⇒ ⇒ SoC 侧只**引用**它（`examples/riscv-soc/AGENTS.md`）。
> ⇒ ⇒ **单一真相源 = 本表**（护栏 `scripts/verify_riscv_soc_core_ports.sh` 的 V1/V2 从**本表**解析）。
> ⚠️ 端口名是**产物名**（带实例路径前缀 `top_core_`）⇒ 期望表由
> `scripts/verify_riscv_soc_core_ports.sh` 从**本表**解析并与 SoC 产物里的核 module 比对
> （V1 逐字逐序 · V2 条数 53 · V3 与 C6 快照三方闭环）。
> ⚠️ **订正（2026-11-23 · R3e）**：本句原先写「由 `scripts/verify_examples_rtl_sim.sh` 解析」
> ⇒ ⇒ **指错了脚本**（该脚本不解析本表）⇒ ⇒ 实测确认唯一消费者是
> `verify_riscv_soc_core_ports.sh` ⇒ ⇒ 本句已改正。
> ⚠️ 核自有 51 条与 `scripts/verify_no_duplicate_rv32i_headers.sh` 的 **C6 快照逐字一致**
> （生成时已机械校验）⇒ 改核端口会**同时**打红 C6 与本表 ⇒ 强制重算。

> 这是「核可替换」的**唯一物证**，也是 S3（总线化）的**变更基线**。
> ⚠️ 端口名是**产物名**（带实例路径前缀 `top_core_`）⇒ 期望表由
> `scripts/verify_examples_rtl_sim.sh` 从**本表**解析并与产物比对（单一真相源 = 本表）。
> ⚠️ 核自有 51 条与 `scripts/verify_no_duplicate_rv32i_headers.sh` 的 **C6 快照逐字一致**
> （生成时已机械校验）⇒ 改核端口会**同时**打红 C6 与本表 ⇒ 强制重算。

| # | 端口（产物名） | 方向 | 位宽 | 归属 |
|---|---|---|---|---|
| 1 | `default_clock` | INPUT | 1 | 框架 |
| 2 | `default_reset` | INPUT | 1 | 框架 |
| 3 | `top_core_ext_rst` | INPUT | 1 | 核自有 |
| 4 | `top_core_prog_we` | INPUT | 1 | 核自有 |
| 5 | `top_core_prog_addr` | INPUT | 32 | 核自有 |
| 6 | `top_core_prog_data` | INPUT | 32 | 核自有 |
| 7 | `top_core_axil_awready` | INPUT | 1 | 核自有 |
| 8 | `top_core_axil_wready` | INPUT | 1 | 核自有 |
| 9 | `top_core_axil_bresp` | INPUT | 2 | 核自有 |
| 10 | `top_core_axil_bvalid` | INPUT | 1 | 核自有 |
| 11 | `top_core_axil_arready` | INPUT | 1 | 核自有 |
| 12 | `top_core_axil_rdata` | INPUT | 32 | 核自有 |
| 13 | `top_core_axil_rresp` | INPUT | 2 | 核自有 |
| 14 | `top_core_axil_rvalid` | INPUT | 1 | 核自有 |
| 15 | `top_core_obs_instr_addr` | OUTPUT | 32 | 核自有 |
| 16 | `top_core_obs_data_addr` | OUTPUT | 32 | 核自有 |
| 17 | `top_core_obs_data_write_data` | OUTPUT | 32 | 核自有 |
| 18 | `top_core_obs_data_write_en` | OUTPUT | 1 | 核自有 |
| 19 | `top_core_obs_perf_cycles` | OUTPUT | 48 | 核自有 |
| 20 | `top_core_obs_perf_instructions` | OUTPUT | 48 | 核自有 |
| 21 | `top_core_obs_perf_branch_count` | OUTPUT | 32 | 核自有 |
| 22 | `top_core_obs_perf_branch_mispredict` | OUTPUT | 32 | 核自有 |
| 23 | `top_core_obs_perf_load_use_stalls` | OUTPUT | 32 | 核自有 |
| 24 | `top_core_obs_perf_control_stalls` | OUTPUT | 32 | 核自有 |
| 25 | `top_core_dbg_id_instr` | OUTPUT | 32 | 核自有 |
| 26 | `top_core_dbg_id_pc` | OUTPUT | 32 | 核自有 |
| 27 | `top_core_dbg_id_valid` | OUTPUT | 1 | 核自有 |
| 28 | `top_core_dbg_id_is_branch` | OUTPUT | 1 | 核自有 |
| 29 | `top_core_dbg_id_is_load` | OUTPUT | 1 | 核自有 |
| 30 | `top_core_dbg_id_is_store` | OUTPUT | 1 | 核自有 |
| 31 | `top_core_dbg_branch_taken` | OUTPUT | 1 | 核自有 |
| 32 | `top_core_dbg_wb_write_en` | OUTPUT | 1 | 核自有 |
| 33 | `top_core_dbg_wb_write_data` | OUTPUT | 32 | 核自有 |
| 34 | `top_core_dbg_wb_rd_addr` | OUTPUT | 5 | 核自有 |
| 35 | `top_core_dbg_exmem_alu_result` | OUTPUT | 32 | 核自有 |
| 36 | `top_core_dbg_exmem_imm` | OUTPUT | 32 | 核自有 |
| 37 | `top_core_dbg_exmem_rs2_data` | OUTPUT | 32 | 核自有 |
| 38 | `top_core_obs_data_rdata` | OUTPUT | 32 | 核自有 |
| 39 | `top_core_dbg_exmem_valid` | OUTPUT | 1 | 核自有 |
| 40 | `top_core_dbg_mem_valid_out` | OUTPUT | 1 | 核自有 |
| 41 | `top_core_dbg_load_use_detected` | OUTPUT | 1 | 核自有 |
| 42 | `top_core_dbg_illegal_reason` | OUTPUT | 2 | 核自有 |
| 43 | `top_core_axil_awaddr` | OUTPUT | 32 | 核自有 |
| 44 | `top_core_axil_awprot` | OUTPUT | 2 | 核自有 |
| 45 | `top_core_axil_awvalid` | OUTPUT | 1 | 核自有 |
| 46 | `top_core_axil_wdata` | OUTPUT | 32 | 核自有 |
| 47 | `top_core_axil_wstrb` | OUTPUT | 4 | 核自有 |
| 48 | `top_core_axil_wvalid` | OUTPUT | 1 | 核自有 |
| 49 | `top_core_axil_bready` | OUTPUT | 1 | 核自有 |
| 50 | `top_core_axil_araddr` | OUTPUT | 32 | 核自有 |
| 51 | `top_core_axil_arprot` | OUTPUT | 2 | 核自有 |
| 52 | `top_core_axil_arvalid` | OUTPUT | 1 | 核自有 |
| 53 | `top_core_axil_rready` | OUTPUT | 1 | 核自有 |

⇒ **53** = 框架 2（`default_clock` / `default_reset`）+ 核自有 **51**（**12 INPUT + 39 OUTPUT**）。
> ⚠️ **2026-11-24 · S4a（`rv32i-soc-mmio-channel-2026-11` · G-9）**：34 → **53**
> = 新增 **19 根 `axil_*`**（**11 输出 + 8 输入**）作为 MMIO 通道（AXI4-Lite 主口 ·
> 端口集合与 `AxiLiteMatrix2x2` 的 `m0_*` 面**同型** ⇒ 接线无适配层）。
> ⇒ ⇒ **判据**：`scripts/verify_riscv_soc_core_ports.sh` V1/V2/V3（53 / 51）·
> C6 快照同步 32 → **51** ⇒ 「头文件 → 产物 → 登记表」三方闭环。

> ⚠️ **语义脚注（2026-11-24 · change `rv32i-commit-event-pulse-2026-11`）** ——
> **端口名 / 方向 / 位宽全部不变**（⇒ ⇒ 本表**不动** · C6 快照**不动**）⇒ ⇒
> 但两根**事件**端口的**语义**由「**电平**」订正为「**单拍脉冲**（与事件时长无关）」：
> - `top_core_obs_data_write_en` = **store 提交事件**（= 写使能 **且** 总线就绪
>   ⇒ ⇒ 对 store 而言 = AXI 写地址握手那一拍 ⇒ ⇒ 每次 store **恰好一拍**）；
> - `top_core_dbg_wb_write_en` = **写回事件**（= `write_en & !mem_stall`
>   ⇒ ⇒ 与 `obs_perf_instructions` **同口径**）。
> ⇒ ⇒ **下游影响**：把这两根端口当「电平」用（如「高电平持续 ⇒ 事务未完成」）的判据
> ⇒ ⇒ MUST 改判「该拍发生了事件」⇒ ⇒ **今日零可观测变化**（既有 golden 逐项不变）
> ⇒ ⇒ 仅含**数据通路反压**的程序会看到差异。
> ⇒ ⇒ **判别力证据**：`tests/test_rv32i_soc_dual_parity.cpp` 的
> `Rv32iSoc - CommitEventPulse`（双后端）· 变异（去掉 `& !mem_stall`）⇒ ⇒ **必红**。
> ⇒ ⇒ ⚠️ **禁**把「级属性」端口（`top_core_dbg_exmem_is_store` /
> `top_core_dbg_id_is_store` / `top_core_dbg_exmem_valid` /
> `top_core_dbg_mem_valid_out`）改成脉冲 ⇒ ⇒ 那是**另一类语义**（反映寄存器当前内容）。

## IP 集成契约（**复用方必读** · 2026-11-24 · change `rv32i-ip-freeze-2026-11`）

> 上面的端口表管「**有哪些针脚**」；本节管「**用它们时的语义**」。
> ⇒ ⇒ 换 SoC / 接新宿主时 MUST 先读本节（**端口表不足以开工**）。

| 契约 | 内容 | 判据载体 |
|---|---|---|
> ⚠️ **本表的首列 MUST NOT 是纯数字**：`scripts/verify_riscv_soc_core_ports.sh` 按 `^\| *[0-9]+ *\|` 识别**端口表**（V1/V2 的期望源）⇒ ⇒ 数字编号行会被误当成端口行。
| **端口面** | **53** = 框架 2 + 核自有 51（**12 IN + 39 OUT**）· 单一真相 = 上面的端口表 | `scripts/verify_riscv_soc_core_ports.sh` V1/V2/V3 · C6 快照 |
| **复位** | `ext_rst` **高有效**（核内流水线 / D-L1 / MMIO 主设备**同极性**）；框架另有一条 `default_reset`（ADR-046）⇒ ⇒ 装载固件期 MUST 保持 `ext_rst = 1`（否则计数器被装载污染） | 核级判据 S1/S6（`tb_rv32i_soc.cpp`） |
| **MMIO 通道** | **19 根 AXI4-Lite 主口**（无 ID / 无 burst / **单事务**）· **分流判据 = 地址 ∈ MMIO 段**（边界**派生**自 `include/cpu/riscv/rv32i_mem_map.h`）⇒ ⇒ MMIO 请求**不进 D-L1** | `tests/test_rv32i_core_mmio.cpp`（AC1 / AC5） |
| **事务语义** | 核内 `ready` 是**单拍完成脉冲**（与事务时长无关）⇒ ⇒ 该拍 `data_ready = 1` ⇒ 流水线恢复；从设备**不应答** ⇒ 核**一直等**（不超时、不自造响应） | `tests/test_rv32i_core_mmio.cpp`（AC3 **反证档**） |
| **错误语义（现状）** | `bresp` / `rresp` **不消费** ⇒ ⇒ DECERR / SLVERR 表现为「事务完成 + 数据取回」⇒ ⇒ **软件无法区分**。⇒ ⇒ 补错误观测面 = **S5**（与 `irq` 同批） | `tests/test_rv32i_core_mmio.cpp`（AC4）· 登记 `.rddf/checks/soc-rtl-expectations.md` §5 |
| **中断** | **无 `irq` 端口**（G-5 契约已定：1 bit · 电平 · 高有效；实施在 **S5**）⇒ ⇒ **IP v1 = polling-only** | 端口表（53 根里没有 `irq`） |
| **已知限制** | ① 数据侧 TCM **越界静默别名**（登记在 `rv32i_mem_map.h`）② MMIO 段里**未接**的外设由宿主互联回 DECERR ③ 逐拍 flat≡hier 等价已覆盖 **12/12 个输入** + **21/39 个输出**（旧 9 + 本批 11 根 `axil_*` + `obs_data_write_data` 值锚）⇒ ⇒ 余 18 个输出仍只有「端口表存在」级的覆盖（R8′）；⚠️ **结构性盲区**：恒 0 信号（`axil_awprot` / `axil_arprot`）与本节第 5 条的 `bresp` / `rresp` 在现有激励下读 0 == 正确值 ⇒ ⇒ **漏接它们逐拍等价判不出** | ① `rv32i_mem_map.h` 文件头 ② ③ `tests/test_rv32i_hier_rtl.cpp` |

> ⚠️ **复用方的两个高频坑**
> - **核级没有从设备 ⇒ 8 根 AXI4-Lite 输入 MUST 显式 tie 0**
>   （`tb_rv32i_soc.cpp` 的 `tie_axil_inputs()`）⇒ ⇒ 核级程序 MUST NOT 做 MMIO 访问
>   （会**永久冻结**流水线）。
> - **MMIO 请求 MUST NOT 进 D-L1**：`DCacheSync` 的 `aw_valid = we & req`
>   ⇒ ⇒ store-to-MMIO 会向 D-TCM 的 AXI 从口发起**别名地址写** ⇒ ⇒ **静默污染** D-TCM。
>   ⇒ ⇒ 分流点（`dcache.req = data_req & !addr_in_mmio`）**不得**被「顺手简化」。

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
| 核 IP 顶层（唯一留在本目录的头）| `src/rv32i_core_top.h` |
| SoC 的**激励驱动**（C++ 侧 + 发射入口）| `tb_rv32i_soc.cpp`（**24 条判据**的 C++ 侧实现）|
| SoC 的**独立 Verilog TB**（手写）| `tb_rv32i_soc.sv`（**24 条判据**的 Verilog 侧实现）|
| 官方套件的**交付物级 TB**（镜像无关）| `tb_rv32i_official.sv`（判上游 env 的 `tohost` 协议）· 门禁 = `scripts/verify_rv32i_official_rtl.sh`（§RUN ⑦）|
| 期望值**单一真相**（两侧共用）| `.rddf/checks/examples-rtl-expectations.md#riscv-mini-rv32i-soc` |
| **跑 ELF / 跑官方 riscv-tests**（用户指南）| `RUNNING-ELF.md`（链路 · 链接脚本 · 三层跑法 · 装载约束 · 故障排查 · 官方套件现状与实测）|
| **官方 riscv-tests 套件**（来源 / 改造 / **排除项** / 复现）| `tests/riscv-tests/PROVENANCE.md`（**单一真相**）· 门禁 = `tests/test_rv32i_official_suite.cpp`（**4 个独立二进制** · 每个 10 个用例）|
| **反压期操作数完整性**（缺陷判据）| `tests/test_rv32i_stall_integrity.cpp`（A1 后继别名型 · A2 决策回退型 · A3 控制面）|

> ⚠️ **2026-10-24（change `rv32i-canonical-convergence-2026-10`，ADR-046）**：
> 本目录的 `src/` 曾有 **32** 个头，其中 10 组与 `include/cpu/` **同名**
> （消歧纯靠 `-I` 顺序 ⇒ 调序即静默换实现）⇒ 已收敛到**只剩 1 个**
> （核 IP 顶层）。所有可复用的微架构组件现在都在 `include/cpu/`。
> 护栏：`scripts/verify_no_duplicate_rv32i_headers.sh`。

## PHASE GATES
Follow root Zero-Debt Policy: **编译通过 + 测试覆盖 + 文档同步**. For riscv-mini:
- `.cpp` registered in `CMakeLists.txt` (no untracked binaries)
- `tests/` subdirectory must be wired to main CMake (delete if not needed)
- **No stubs**: empty test files use `SKIP()` or are removed
- **No dead code**: `phase3b_complete.cpp` must be fully integrated or removed
