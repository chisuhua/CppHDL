# riscv-mini 测试知识入口（Testing Entry Point）

> **版本**：3.0 · **重写日期**：2026-11-26
> **取代**：2026-04-14 的《riscv-mini 集成测试架构方案》v2.0（状态标「执行中」）
> —— 该版的 **Layer 4 FreeRTOS OS 语义层从未落地**（规划已归档到 `docs/archive/freertos-planning/`），
> Layer 1 的「41 个」与实际入库的 **40** 个不符，且**完全没有** RTL 产物 / Verilator 层
> ⇒ ⇒ 本版按**当前实际资产**重写，不再保留「方案」形态。
>
> **本篇的角色**：riscv-mini **测试面**的唯一入口 —— 资产地图 · 完整清单 · 跑法 ·
> 门禁 · 判据纪律 · 护栏矩阵 · 计数锚点 · 已知边界 · 导航。
>
> **本篇不做的事**：**不复述**其它文档的正文 ⇒ 需要的细节**链出去**（见 §9）。
> 权威分层：**能力**在 `openspec/specs/**`（capability）· **决策**在 `docs/adr/**`（ADR）
> ⇒ ⇒ 本篇只管**测试面**（怎么测、怎么跑、凭什么信）。

---

## 0. 一句话地图

```
        ┌─────────────────────────── riscv-mini 测试面 ───────────────────────────┐
        │                                                                     │
        │  L0 组件级          L1 核 IP 级            L2 交付物级                    │
        │  include/cpu/**    src/rv32i_core_top.h   生成的 Verilog                │
        │        │                  │                     │                        │
        │   根 tests/        examples/riscv-mini/     verilator + 独立 .sv TB      │
        │   （8 个 RV32I）      tests/（24 源）        （flat / hier 两形态）      │
        │        │                  │                     │                        │
        │   组件行为断言        固件镜像 + 写回序列      RESULT: checks=N fails=0    │
        │   逐拍/逐值          退休计数 / CSR 读回      官方套件 tohost 协议         │
        │                                                                     │
        │        └──────────────► 三道门禁 ◄──────────────┘                     │
        │              核级官方套件 │ 交付物级官方套件 │ examples 运行护栏           │
        └─────────────────────────────────────────────────────────────────────────┘
```

- **L0 组件级** ⇒ DUT = `include/cpu/**` 的单个组件 ⇒ 载体 = 仓根 `tests/`
- **L1 核 IP 级** ⇒ DUT = `examples/riscv-mini/src/rv32i_core_top.h` ⇒ 载体 = `examples/riscv-mini/tests/`
- **L2 交付物级** ⇒ DUT = **生成的 Verilog**（flat / hier）⇒ 载体 = 手写独立 TB（`.sv`）+ verilator

⚠️ **三层不可互相替代** ⇒ 见 §5.2「两后端一致 ≠ 正确」与 §4.2。

> ⚠️ **两套分层口径别混**（§0 vs §1）：§0 的 **L0/L1/L2 按「载体文件位置」**分层
> （仓根 `tests/` vs `examples/riscv-mini/tests/`）；§1 的「组件级 / 核级」按 **DUT 层级**
> 分层 ⇒ ⇒ **`test_csr_unit` 是组件级 DUT**（`cpu/riscv/rv32i_csr.h`）**但文件位置**在
> examples 侧 ⇒ ⇒ 它落在 §2.1 的 **L1 行**（归位按**位置**，不按 DUT）。

---

## 1. 判据类型（正交的四个面）

同一个设计要同时被四类判据覆盖才算「测过」：

| 面 | 问的问题 | 载体 | 例 |
|---|---|---|---|
| **译码面** | 指令 → 译码结果对不对 | 组件级 / 核级 | `illegal_reason` · `sys_trap_kind` |
| **行为面** | 跑起来对不对（逐拍 / 退休计数 / 写回序列） | 核级固件镜像 | `test_rv32i_trap_entry` · `test_rv32i_soc_dual_parity` |
| **契约面** | 寄存器 / CSR / 端口语义守不守 | 组件级 | `test_csr_unit`（复位值 + WARL）· 53 端口契约表 |
| **产物面** | 交付的 Verilog 对不对 | verilator + 独立 TB | `tb_rv32i_soc.sv` · `tb_rv32i_official.sv` |

⚠️ **只测译码面是最常见的不足** ⇒ 判据必须落到**可观测行为**（写回序列 / 退休计数 /
CSR 读回 / `tohost`），而不是「某个内部信号看起来对」。

---

## 2. 完整清单

### 2.1 L1 核 IP / SoC 级 —— `examples/riscv-mini/tests/`（**24 个源 → 26 个 CTest 条目**）

> 判据 = `scripts/verify_example_test_wiring.sh`（C1 归属守恒 · C2 数量守恒 ·
> **C3 = 逐条 exit 0 且日志无 error/fatal** · C3b = allowlist stale 检查）
> ⇒ ⇒ **增删测试 MUST 同批**更新
> 根 `AGENTS.md` 的 `EXAMPLES_TEST_SOURCES` 锚点（**26**）。
> ⚠️ **接线表（源 → 条目）的运行期副本** = `examples/riscv-mini/AGENTS.md` 的 **§TEST** 段
> ⇒ ⇒ 两处 MUST 同批改（否则漂移）。

| 源文件（`examples/riscv-mini/tests/`） | CTest 条目 | 判据面 / 覆盖 | 后端 | TIMEOUT | 规模<br>（用例 / 断言） |
|---|---|---|---|---|---|
| `test_pipeline_exec.cpp` | `test_pipeline_exec` | 行为面 · 流水线执行 | 解释器 | 300 | 16 / 69 |
| `test_soc_hello_world.cpp` | `test_soc_hello_world` | 行为面 · SoC 冒烟 | 解释器 | 300 | 4 / 11 |
| `test_riscv_tests.cpp` | `test_riscv_tests` | 行为面 · 仓内 riscv-tests（⚠️ **43/49 用例 SKIP**，见 §8.1） | 解释器 | 300 | 49（**6 passed · 43 skipped**）/ 10 |
| `test_riscv_tests_pipeline.cpp` | **（无 · 只构建）** | ⚠️ 46 用例全依赖仓外二进制 ⇒ Catch2 exit **4** ⇒ **不进 CTest**（§8.2） | — | — |
| `test_csr_unit.cpp` | `test_csr_unit` | 契约面 · CSR 存储体（复位值 / WARL 掩码） | 解释器 | 300 | 15 / 60 |
| `test_riscv_tests_soc.cpp` | `test_address_decoder` | 契约面 · 地址译码 | 解释器 | 300 | 10 / 132 |
| `test_axil_mmio_master.cpp` | `test_axil_mmio_master` | 行为面 · 核侧 AXI4-Lite 主设备（一次事务恰好一拍 ready） | 解释器 | 300 | 5 / 58 |
| `test_rv32i_core_mmio.cpp` | `test_rv32i_core_mmio` | 行为面 · 核级 MMIO 通道（IP 接口面单独验证） | 解释器 | 300 | 7 / 50 |
| `test_rv32i_stall_integrity.cpp` | `test_rv32i_stall_integrity` | 行为面 · 反压期操作数完整性 | 解释器 | 300 | 3 / 21 |
| `test_rv32i_csr_instructions.cpp` | `test_rv32i_csr_instructions` | 译码面 + 行为面 · 6 种 CSR 指令 | 解释器 | 300 | 8 / 31 |
| `test_rv32i_trap_entry.cpp` | `test_rv32i_trap_entry` | 行为面 · trap 入口（非法指令 / `ECALL` / `EBREAK` / `WFI` / `MRET`） | 解释器 | 300 | 11 / 1196 |
| `test_rv32i_official_suite.cpp` | `test_rv32i_official_suite_g1`<br>`…_g2` `…_g3` `…_g4` | 行为面 · **官方 `rv32ui-p-*` 40 个**（每组 10 个 · 4 个独立二进制） | 解释器 | 300 | 各 11 / 22 |
| `test_pipeline_minimal.cpp` | `test_pipeline_minimal` | 行为面 · pipeline 最小载体 | 解释器 | 300 | 1 / 1 |
| `test_pipeline_debug.cpp` | `test_pipeline_debug` | 行为面 · pipeline 调试观测 | 解释器 | 300 | 9 / 15 |
| `test_pipeline_wrapper.cpp` | `test_pipeline_wrapper` | 行为面 · pipeline wrapper | 解释器 | 300 | 3 / 5 |
| `test_wrapper_simple.cpp` | `test_wrapper_simple` | 行为面 · wrapper（简单） | 解释器 | 300 | 2 / 4 |
| `test_wrapper_ifstage.cpp` | `test_wrapper_ifstage` | 行为面 · wrapper（IF stage） | 解释器 | 300 | 2 / 4 |
| `test_wrapper_pipeline.cpp` | `test_wrapper_pipeline` | 行为面 · wrapper（整条流水线） | 解释器 | 300 | 2 / 3 |
| `test_ifstage_device.cpp` | `test_ifstage_device` | 行为面 · IF stage 独立 device | 解释器 | 300 | 1 / 2 |
| `test_rv32i_pipeline.cpp` | `test_rv32i_pipeline_example` | 行为面 · 前推通路（⚠️ **与仓根 `tests/test_rv32i_pipeline.cpp` 不是同一份**） | 解释器 | 300 | 6 / 25 |
| `test_interrupt_flow.cpp` | `test_interrupt_flow` | 行为面 · CLINT / 中断 / 陷阱入口 / `MRET` | 解释器 | 300 | 18 / 68 |
| `test_rv32i_soc_dual_parity.cpp` | `test_rv32i_soc_dual_parity` 🔷 | 行为面 · **双后端** SoC parity | 解释器 + verilator | 600 | 15 / **23213** |
| `test_rv32i_hier_rtl.cpp` | `test_rv32i_hier_rtl` 🔷 | 产物面 · **双后端** 层次化产物（8 module / 53 端口） | 解释器 + verilator | 900 | 4 / 188 |
| `test_cache_smoke.cpp` | `riscv_mini_cache_smoke` 🔷 | 产物面 · **双后端** cache wrapper smoke | 解释器 + verilator | 600 | 12 / 78 |

🔷 = 带 **`verilator_e2e`** 标签 ⇒ 需 verilator + `RESOURCE_LOCK "verilator_backend"`
⇒ ⇒ **必须串行**（与其它 verilator 任务并发会互抢锁 ⇒ 表现为随机失败）。

> **规模列 = 2026-11-26 实测**（逐个二进制跑出来的 Catch2 汇总行）⇒ ⇒
> `test_rv32i_official_suite_g1..g4` 每组 = **10 个官方测试 + 1 个负向对照用例**。
> ⚠️ 该列是**快照** ⇒ ⇒ 改动测试后 MUST 同批刷新（§10）。

⚠️ **`test_rv32i_official_suite.cpp` 一源四 target 是刻意的**（不是重复）：
40 个官方测试跑完 ≈ **7 min** ⇒ 超过仓内单测 **300 s** 上限 ⇒ ⇒ 拆 4 组 × 10 个
（每组 ≈ 105 s）⇒ 且护栏 C3 按「**ctest 条目名 = 可执行文件名**」找二进制 ⇒
「同一二进制 + 过滤参数」会被护栏判成「没跑」⇒ ⇒ MUST 是**独立二进制**。

### 2.2 L0 组件级 —— 仓根 `tests/`（8 个 RV32I 条目）

| CTest 条目 | 源 | 判据面 | 后端 |
|---|---|---|---|
| `test_rv32i_pipeline` | `tests/test_rv32i_pipeline.cpp` | 行为面 · 流水线各级 | 解释器 |
| `test_rv32i_alu` | `tests/test_rv32i_alu.cpp` | 行为面 · ALU | 解释器 |
| `test_rv32i_alu_differential` | `tests/test_rv32i_alu_differential.cpp` | 行为面 · ALU 差分 | 解释器 |
| `test_rv32i_pipeline_behavior` 🔷 | `tests/test_rv32i_pipeline_behavior.cpp` | 行为面 · **20 个双后端 pair**（跨 Batch 1/2/3/4/5 = 组合逻辑 / 流水线寄存器 / 分支预测 / skid 防御契约 / trap 重定向） | 解释器 + verilator |
| `test_rv32i_arch_dual` 🔷 | `tests/test_rv32i_arch_dual.cpp` | 契约面 · 拍表（CSR 复位值 + WARL） | 解释器 + verilator |
| `test_rv32i_stage_dual` 🔷 | `tests/test_rv32i_stage_dual.cpp` | 行为面 · 各级 | 解释器 + verilator |
| `test_rv32i_mem_backpressure` | `tests/test_rv32i_mem_backpressure.cpp` | 行为面 · 存储反压 | 解释器 |
| `test_rv32i_timing_dual` 🔷 | `tests/test_rv32i_timing_dual.cpp` | 行为面 · 时序契约 | 解释器 + verilator |

### 2.3 非 CTest 载体

| 载体 | 是什么 | 怎么跑 |
|---|---|---|
| `examples/riscv-mini/tb_rv32i_soc` | **激励驱动 + RTL 发射入口**（三角色分离）⇒ 默认 = 验证模式（24 条手推判据）· `--emit-rtl` = 扁平产物 · `--emit-rtl --hierarchical` = 层次化产物 · `--emit-rtl --firmware-init` = 带固件初值 | `./build/examples/riscv-mini/tb_rv32i_soc`（**不是** CTest 条目 ⇒ `ctest` 跑不到它） |
| `examples/riscv-mini/tb_rv32i_soc.sv` | **手写**独立 Verilog TB（24 条判据的 Verilog 侧实现）⇒ 两种形态**共用**（顶层端口表同源 ⇒ 连线逐字相同） | 由 `scripts/rv32i_rtl_sim_env.sh` / `verify_examples_rtl_sim.sh` 驱动 |
| `examples/riscv-mini/tb_rv32i_official.sv` | **手写**独立 TB（官方套件 · **镜像无关** · `tohost` 协议） | 由 `scripts/verify_rv32i_official_rtl.sh` 驱动 |
| `rv32i_phase1_test` / `_phase2_test` / `_phase3_test` | Phase 1–3 的历史自测程序（**带 `main()`** ⇒ 属 examples 运行护栏的被跟踪集合） | `./build/examples/riscv-mini/rv32i_phase1_test` |
| `test_riscv_tests_pipeline` | ⚠️ 46 用例全依赖**仓外** riscv-tests 预编译二进制 ⇒ **只构建不注册** | 手工跑（需 `RISCV_TESTS_PATH`） |

### 2.4 已退役（**删除留痕**，不参与任何自动化判定）

`examples/removed_examples.txt` 记录了 **11 条**已删除 target 的理由与归位条件
（**8 个**测试相关：6 个曾声明过的 CMake target + 2 个从未进 CMakeLists 的孤儿源文件
· **3 个**发射薄壳 `emit_rtl_*`）⇒ ⇒ **不要**在别处记「某某测试没了」—— 那里有完整留痕。

⚠️ **删除留痕 ≠ 豁免**：豁免在 `examples/disabled_examples.txt`（**存活但未跑** ⇒ 按名字判定）
⇒ ⇒ 两个文件 MUST 分开（混在一起会让删除留痕退化成「按名字的永久自动豁免」）。

---

## 3. 怎么跑

### 3.0 铁律：跑 ctest 前 MUST 先全量重建

```bash
cmake --build build -j$(nproc)     # 改了 src/** 或 include/** 之后必做
```

⚠️ `ctest` **不会**自动重新编译 ⇒ 只重建了被点名的 target 时，其余 target 跑的是
**旧二进制** ⇒ ⇒ **假绿**（仓内实测踩过：`sread` 语义改动后「213/213 全绿」是假的，
全量重建后立刻转红）。
⚠️ 同型铁律：**还原源码 ≠ 复原**（**二进制也是状态**）⇒ ⇒ 变异还原后 MUST 重建再跑。

### 3.1 分组跑

```bash
# ① 只跑 L1 核 IP 级那 26 条目
#    ⚠️ **按子目录跑**（`build/examples/riscv-mini/CTestTestfile.cmake`）
#    ⇒ ⇒ 不用维护正则 ⇒ ⇒ 不会误选（仓根 `tests/` 的 RV32I 条目）+ 不会漏选
ctest --test-dir build/examples/riscv-mini --output-on-failure

# ② 全量（仓级；-LE perf 排除 perf 组）
ctest --test-dir build -LE perf --output-on-failure -j$(nproc)

# ③ 只跑 L0 组件级那 8 条目（**锚定** ⇒ 不含 examples 侧的同名/近名条目）
ctest --test-dir build --output-on-failure \
  -R "^test_rv32i_(pipeline|alu|alu_differential|arch_dual|pipeline_behavior|stage_dual|mem_backpressure|timing_dual)$"

# ④ 只跑双后端（需 verilator ⇒ 串行 ⇒ 慢）
ctest --test-dir build -L verilator_e2e --output-on-failure
```

> ⚠️ **不要用「一个大正则」筛本目录的测试** ⇒ ⇒ 一条覆盖 `rv32i|riscv|…` 的大正则会
> **同时**选中仓根 `tests/` 的 **8** 条 RV32I 条目与 **3** 条 `spinalhdl_rv32i_phase*`
> ⇒ ⇒ 命中数 = **26（本目录）+ 8 + 3 = 37** ≠ 26
> ⇒ ⇒ 要么按**子目录**跑（①），要么按**锚定正则**跑（③）。

### 3.2 单个跑

```bash
# 直接跑二进制（反馈最快 · 可带 Catch2 过滤串）
./build/examples/riscv-mini/test_rv32i_trap_entry
./build/examples/riscv-mini/test_rv32i_soc_dual_parity \
  "Rv32iSoc - IllegalOpcodeObservableNotRetired + FenceIsNop"

# 走 ctest（带标签 / 超时 / 资源锁 / 环境注入）
ctest --test-dir build -R test_rv32i_trap_entry --output-on-failure
```

### 3.3 慢条目与前置

| 条目 | 典型耗时 | 前置 |
|---|---|---|
| `test_rv32i_official_suite_g1..g4` | 各 ≈ **105 s**（合计 ≈ 7 min） | 40 份镜像在**源码树**内（CMake 已注入 `CPPHDL_SOURCE_DIR`） |
| `test_rv32i_hier_rtl` 🔷 | **冷** verilator cache：分钟级（warm cache 实测 ≈ **3 s**） | verilator（见下） |
| `test_rv32i_soc_dual_parity` 🔷 | 分钟级（实测 ≈ **3 min** · warm cache 亦然 ⇒ 成本在**仿真拍数**不在编译） | verilator |
| `riscv_mini_cache_smoke` 🔷 | **冷** verilator cache：分钟级（warm cache 实测 ≈ **1 s**） | verilator |

⚠️ **verilator 只装 submodule**：`apt install verilator` 是**错的** ⇒ 用
`third_party/verilator/`（`bash scripts/init-submodules.sh` +
`cmake --build build --target verilator` · 冷编 10–30 min）⇒ 见
`docs/developer_guide/verilator-integration.md`。
⚠️ **verilator 后端 = 显式 Linux-only**（macOS / Windows 在 configure 期 FATAL_ERROR · 刻意如此）。

### 3.4 排障速查

| 症状 | 首查 |
|---|---|
| 全绿但**怀疑没真跑** | 「还原源码 ≠ 复原」⇒ 重建；`ctest -N` 数条目；退出码是不是 0（≠ 4） |
| `test_riscv_tests` 大量 SKIP | 缺**仓外** riscv-tests 预编译二进制 ⇒ 需 `RISCV_TESTS_PATH`（仓内那 40 份镜像**不**依赖它） |
| `test_riscv_tests_pipeline` 被判红 | ⚠️ 它**不该**进 CTest ⇒ 若你在 CTest 里看到它 ⇒ CTest 注册被改坏了 |
| 双后端测试随机失败 | `RESOURCE_LOCK "verilator_backend"` ⇒ **并发**跑 verilator 任务 ⇒ 串行重跑 |
| verilator 相关全红 | `build/verilator-install/bin/verilator` 在不在 ⇒ submodule 初始化没做 |
| `run_all_ported_tests.sh` 失败 | ⚠️ **缺失二进制 = 失败**（不是跳过）⇒ 先 `cmake --build build --target all-examples` |

---

## 4. 门禁（四道，层次不同）

### 4.1 核级官方套件（**解释器** · 4 个 CTest 条目）

```bash
ctest --test-dir build -R test_rv32i_official_suite --output-on-failure
```

- DUT = `ch_device<Rv32iCoreTop, itcm, dtcm>` ⇒ 判「**本仓那份 C++**」的行为
- 判据 = **上游 env 的 `tohost` 协议**（`tohost == 1` ⇒ PASS）
- 资产 = 40 份入库镜像（`examples/riscv-mini/tests/riscv-tests/images/`）
- 资产来源 / 改造 / **排除项** / 复现方法 = **单一真相**
  ⇒ `examples/riscv-mini/tests/riscv-tests/PROVENANCE.md`

### 4.2 交付物级官方套件（**真 verilator** · 交付的 Verilog 产物侧）

```bash
bash scripts/verify_rv32i_official_rtl.sh --subset 8   # PR 快通道 ≈ 2 min
bash scripts/verify_rv32i_official_rtl.sh --full       # 全量 40 个 ≈ 10 min
```

逐镜像独立 5 步：

```
tb_rv32i_soc --emit-rtl --firmware <镜像目录> --out <产物目录> --emit-hygiene
                                                            # ① 发射带固件初值的产物（<产物目录>/top.v）
  → verilator --lint-only --top-module top <产物目录>/top.v     # ② lint 门（退出码 0 + 零 %Error）★ 是门禁的一环
  → verilator --binary --timing -CFLAGS -fcoroutines --x-assign unique \
       --top-module tb_rv32i_official --Mdir <产物目录>/obj \
       <产物目录>/top.v examples/riscv-mini/tb_rv32i_official.sv # ③ 编译（TB 模块名 = tb_rv32i_official）
  → <产物目录>/obj/Vtb_rv32i_official +tohost_expected=1        # ④ 跑手写独立 TB
  → 判 tohost（seen=1 value=0x1 ⇒ PASS）                         # ⑤
```

> ⚠️ **② 的 `--top-module` 是 `top`（产物），不是 `tb_rv32i_official`** —— lint 判的是
> **被交付的产物**（`top.v`）；`tb_rv32i_official` 是 ③ 编译步里 **TB** 的模块名。
> 逐镜像实际调用形态 = `scripts/verify_rv32i_official_rtl.sh:117`（①）/ `:131`（②）/
> `:141-143`（③）/ `:149`（④）⇒ ⇒ **照抄上面的形态**才是门禁真正跑的东西。

- **形态**：flat 全 40 个 + **hier 抽样 2 个**（`rv32ui-p-add` / `rv32ui-p-st_ld`）
- **负向对照（防恒绿）**：手写镜像把 **3** 写进 `tohost` ⇒ 门禁 MUST 如实读到 `3`；
  再用 `+tohost_expected=1` 跑同一负镜像 ⇒ MUST **红**（证明判定没被吞）
- **fail-closed**：缺 verilator / 缺发射器 / 缺 TB / 缺镜像 ⇒ **exit 2**（不是 skip、不是绿）
- **CI 落点**：`pr-verilator-fast`（PR 事件 · shard 1 · `--subset 8`）+
  `nightly-verilator`（02:00 UTC cron · `--full`）

⚠️ **为什么必须有第 2 层**（不是冗余）：解释器与 Verilog codegen 由**同一份 C++** 生成
⇒ ⇒ **同构缺陷两后端一致地错** ⇒ ⇒ 第 1 层对 codegen 侧缺陷（初值块 / 端口映射 /
时序形状 / 宽度）**结构性失明**。
**实证**：第 2 层门禁的初版跑出 **40/40 个带固件产物的 lint 全红**（`WIDTHEXPAND`
⇒ 按仓内口径编译直接失败）⇒ ⇒ 而既有 3 个 RTL 护栏 lint 的产物形态**都没有** TCM 初值块
⇒ ⇒ 那个形态**从未被 lint 过**。

### 4.3 RTL 交付环境一键（生成 → lint → 编译 → 运行）

```bash
bash scripts/rv32i_rtl_sim_env.sh
# ⇒ flat / hier 各 RESULT: checks=24 fails=0
```

### 4.4 examples 运行护栏（CI job `examples-runtime`）

```bash
cmake --build build --target all-examples     # 先构建全部被跟踪 example
./run_all_ported_tests.sh
```

- 被跟踪集合的**唯一来源** = `examples/CMakeLists.txt::CH_TRACKED_EXAMPLE_TARGETS`
  （含 `rv32i_phase1/2/3_test` 与 `tb_rv32i_soc`）⇒ ⇒ 同时驱动构建侧的聚合 target
  `all-examples` 与运行侧的本脚本
- ⚠️ **缺失二进制 = 失败**（不是跳过）⇒ ⇒ 只做增量构建的开发者会看到失败
- ⚠️ 护栏 C1–C3 **不含** riscv-mini 的 26 个 Catch2 条目（那些由 ctest 覆盖）⇒
  **两条覆盖面**：Catch2 条目走 `ctest`；带 `main()` 的 example 程序走本护栏

---

## 5. 判据纪律（写新测试前必读）

### 5.1 期望值 MUST **手推**（禁抄仿真输出）

解释器与 codegen 同源 ⇒ ⇒ 双后端 parity 对「同构缺陷」这一族**结构性失明**
⇒ ⇒ 期望值 MUST 从**规范** + **复位值**推导，不能从仿真输出反抄。
⇒ ⇒ 仓内已命中多次「两后端一致地给出与手推不同的值」⇒ ⇒ 那是**推导错**，不是 parity 分歧。

### 5.2 「两后端一致」**不是**正确性证据

同源缺陷 ⇒ 两后端一致地错 ⇒ ⇒ **parity 只能当辅助证据** ⇒ ⇒ 真正的承重判据是
「**手推期望值**」+「**变异**」（§5.3）。

### 5.3 每条新判据配**一条变异**（且变异 MUST 仍能编译）

- 变异 = **还原错误实现** ⇒ 目标判据 MUST **转红** ⇒ ⇒ 才有判别力
- ⚠️ 变异 MUST **先确认仍能编译**（编译失败 ⇒ 跑的是**旧二进制** ⇒ **假绿**）
- ⚠️ 还原错误实现时 MUST 用**原始写法**：`if (!r.valid) return;` 与
  `if (r.valid) { … }` **不等价**（`!` 是**硬件算子** ⇒ 产出节点 ⇒ 建图期恒假
  ⇒ 永远不 return）⇒ ⇒ 该类变异**恒绿**
- ⚠️ **无效变异 MUST 如实登记**（恒绿 ⇒ 不改变可观测行为 ⇒ 不是判别力缺口）
- 逐条登记在 change 的 `design.md` 的**变异矩阵**段（⚠️ **段号各 change 不同**
  ⇒ ⇒ 不写死 §N ⇒ 例：`rv32i-ecall-ebreak-wfi-2026-11` 的 §8 是变异矩阵，
  而 `rv32i-trap-entry-2026-11` 的 §8 是「实施期订正」）

### 5.4 未驱动的输入端口 = **未定义值**

`ch_in` **没有** `operator=` ⇒ `io().X = value` 走 `CHERROR` + **什么都不做**
⇒ ⇒ 端口保持未驱动（实测读到**全 1** 或 0）⇒ ⇒ 测试会「**碰巧**通过」而跨后端行为可能分叉。
⇒ ⇒ 驱动方式 MUST 用 `sim.set_input_value(port, <纯整数>)`（`ch_bool::operator uint64_t`
是 `explicit` ⇒ 不能直接传 `ch_bool`）。
⇒ ⇒ 两条护栏：`scripts/verify_input_port_additions.sh`（增量 · 只列不判）·
`scripts/verify_input_port_required_registry.sh`（登记表双向校验）。

### 5.5 硬件值 MUST NOT 参与 C++ 控制流

`describe()` 在**建图期**执行 ⇒ ⇒ `if (io().x)` / `x ? A : B` / `switch (io().x)`
/ `while (io().x)` / early `return` / 传给 `bool` 形参 ⇒ **全部在建图期求值**
⇒ ⇒ 条件恒假 ⇒ ⇒ 分支体从不发射 ⇒ ⇒ 输出**恒为未驱动**（读到 0）⇒ ⇒
组件「功能上是死的」却**零报错**。
⇒ ⇒ 条件 MUST 落在**表达式的取值**里（`select(...)` / `&` / `|` / `!`）⇒ ⇒
且**每个 `ch_out` MUST 被无条件驱动**。
⚠️ 只把 `if` 换掉**不够** —— 驱动常被注释掉 ⇒ 会发出「地址 0、无写使能」这类
「看起来合法」的请求 ⇒ ⇒ **比「恒未驱动」更危险**。
⇒ ⇒ 权威 = `include/cpu/AGENTS.md` + `include/chlib/state_machine.h` 的「5 条纪律」。

### 5.6 组件生命周期（`ch_module` vs `ch_device`）

`ch::ch_module<T>` **MUST** 在 `Component::describe()` **内部**创建（需要有效的
`Component::current()` 父节点）⇒ ⇒ 在 standalone 测试函数里用 ⇒ **静默失败 + SIGSEGV**。
⇒ ⇒ standalone 用 `ch::ch_device<T>`；组件内用 `ch::ch_module<T>`。
⇒ ⇒ 详见 `docs/developer_guide/patterns/COMPONENT-LIFECYCLE-GUIDE.md`。

---

## 6. 护栏矩阵（谁判什么）

| 护栏 | 判什么 | 域 |
|---|---|---|
| `scripts/verify_example_test_wiring.sh` | **归属守恒**（测试源有没有被真的执行到 · 退出码 0 · 日志无 error/fatal） | `examples/**` 的 `.cpp/.cc/.cxx/.C` 测试源 |
| `scripts/verify_example_assertion_quality.sh` | **断言质量**（被执行到的测试有没有真的断言语义输出） | 同上（Catch2 断言） |
| `scripts/verify_examples_rtl_sim.sh` | **RTL 仿真覆盖**（独立 Verilog TB 的产物级判据） | `.sv` 独立 TB（**不在**上一条的域） |
| `scripts/verify_rv32i_official_rtl.sh` | **官方套件在 RTL 产物上全过**（+ lint 门 + 负向对照） | 交付物级 |
| `scripts/rv32i_rtl_sim_env.sh` | 生成 → lint → 编译 → 运行（flat / hier）· ⚠️ **本地一键脚本 · 不在 CI 门内**（不在可达性台账里） | 交付物级 |
| `scripts/verify_hierarchical_rtl.sh` | 层次化产物 **R1–R8 + R8′ + R9**（另有 R0 前置 / R1a 命令行面）· R8′ = RV32I 产品语料的顶层端口表 == 手推期望集 **53** 根 | 交付物级 |
| `scripts/verify_riscv_soc_core_ports.sh` | 核 IP **53** 端口契约（V1 逐字逐序 · V2 = 53 · V3 C6 快照 51） | 契约面 |
| `scripts/verify_no_duplicate_rv32i_headers.sh` | 单源化（同名头双副本 · C5 = `include/cpu/AGENTS.md` 的 STRUCTURE / KEY FILES 表与磁盘一致 + 自述计数 · **C6 = 51 端口快照**） | 结构面 |
| `scripts/verify_dual_backend_parity.sh` | **双后端覆盖不缩面**（源文件数 + pair 数**双锚点**） | 结构面 |
| `scripts/verify_test_counts.sh` | 计数锚点（`CTEST_TOTAL` / `VERILATOR_E2E_TOTAL` / `EXAMPLES_TEST_SOURCES`） | 结构面 |
| `scripts/verify_input_port_additions.sh` · `verify_input_port_required_registry.sh` | 未驱动输入端口 | 契约面 |
| `scripts/verify_openspec_delta.sh` | **archive 前的实际门**（`openspec validate` **不在** CI 门内） | 规范面 |
| `run_all_ported_tests.sh` | examples **运行护栏**（带 `main()` 的程序逐条 exit 0） | 运行面 |

⚠️ 每个护栏的**文件头**都写了自己的「判据 / 反向判据 / 判别力自证」⇒ ⇒ **不要**在本篇复述。
⚠️ 护栏可达性的**唯一真相** = `.rddf/checks/guard-reachability.md`（与 `ci.yml` 双向 1:1 校验）。

---

## 7. 计数锚点（改动测试后 MUST 同步）

| 锚点 | 值 | 含义 | 校验者 |
|---|---|---|---|
| `CTEST_TOTAL` | **284** | ctest 条目总数 | `scripts/verify_test_counts.sh` |
| `VERILATOR_E2E_TOTAL` | **81** | 带 `verilator_e2e` 标签的条目数 | 同上 |
| `DUAL_BACKEND_PARITY_TOTAL` | **67** | 含 `DUAL_BACKEND_TEST_PAIR(` 的测试**源文件**数 | `scripts/verify_dual_backend_parity.sh` |
| `DUAL_BACKEND_PARITY_PAIRS` | **297** | 同上的 **pair 调用数**（深度维度） | 同上 |
| `EXAMPLES_TEST_SOURCES` | **26** | `examples/**` 下的测试源文件数（**全域** = riscv-mini **24** + axi4 **2**）⚠️ 与 §2.1 的「**26 个 CTest 条目**」**是两个不同的 26**（后者 = 本目录 24 源 → 26 条目） | `scripts/verify_example_test_wiring.sh`（C2） |

⚠️ **两个 parity 锚点都要**：只钉「文件数」的话，从**含多个 pair** 的文件里删一个 pair
⇒ 该文件仍被计数 ⇒ ⇒ 护栏**看不见**（实测：变异 M1 恒绿）⇒ ⇒ 深度锚点补上这个盲区。
⚠️ 增删 MUST **同批**更新锚点 + 跑校验器（锚点 = 可审计的行为，不是注释）。

---

## 8. 已知边界与退役记录

### 8.1 `test_riscv_tests` 大部分用例会 SKIP

49 个用例里 **6 passed / 43 skipped** ⇒ ⇒ 43 个依赖**仓外** riscv-tests 的**预编译二进制**
⇒ ⇒ 需 `RISCV_TESTS_PATH` 指向一份 checkout ⇒ ⇒ CI（无该 checkout）里恒为跳过。
⚠️ 与 §4.1 的 40 份**入库镜像**不是一回事（后者不依赖仓外资源）。

### 8.2 `test_riscv_tests_pipeline` **不进 CTest**

46 个用例全依赖仓外二进制 ⇒ 全部 SKIP ⇒ Catch2 v3 退出码 **4**（= 没有真正跑起来的用例）
⇒ ⇒ **CTest 会判红** ⇒ ⇒ 只构建不注册（`examples/disabled_examples.txt` 已登记 + 归位条件）。
⚠️ 它**从未 `add_test`**（声明走 `riscv_mini_catch_test_noctest`）⇒ ⇒ `ctest -N`
**列不出它**（实测 0 命中）⇒ ⇒ 反过来：若哪天 `ctest -N` 里出现它 ⇒ 说明被误改回
`riscv_mini_catch_test` ⇒ ⇒ C3 会因 exit 4 判红。

### 8.3 官方套件的 2 个排除项

| 测试 | 排除理由 |
|---|---|
| `fence_i` | 测 `Zifencei` 扩展（`fence.i`）⇒ **本核是 RV32I，无该扩展** ⇒ **能力对齐**，不是「跑不过」 |
| `ma_data` | 测**非对齐** load/store ⇒ ⇒ 本核 load/store 的**车道选择忽略低地址位** ⇒ 非对齐访问被**静默对齐**（既不处理也不 trap）⇒ ⇒ **能力对齐** |

⇒ ⇒ 权威登记 = `examples/riscv-mini/tests/riscv-tests/PROVENANCE.md` §3（**不得**静默缩小）。

### 8.4 已知缺口（**如实登记 · 不静默留尾**）

| # | 缺口 | 状态 |
|---|---|---|
| 1 | `rv32i-privilege` 的 `#### Scenario: 6 种 funct3 的 csr_wdata 语义`（P6-2 的**纯表格式** Scenario · 无 `THEN` 行）⇒ ⇒ 结构检查器会点它 | **既有** · 已由 P6-3 的验收报告登记 ⇒ **须独立立项**（不在 P6-4 范围） |
| 2 | `AxiDma.rst_n` **无行为覆盖**（必需端口登记表里已登记的已知缺口） | 已登记 |
| 3 | 层次化逐拍等价只覆盖**顶层可见行为** ⇒ 成层子树的 `ch_out` 被移出顶层 ⇒ 「子树内部算错但顶层输出恰好相同」**结构性测不出** | 权威 = capability `rtl-hierarchical-output` |
| 4 | 未驱动输入端口**没有通用护栏**（通用状态型判定会误报 ⇒ 死结）⇒ ⇒ 义务仍留在**人工核对 + code review** | 已知设计边界 |

### 8.5 退役记录

- **11 条已删除 target**（8 个测试相关 + 3 个发射薄壳）⇒ `examples/removed_examples.txt`
  （理由 + 归位条件）
- **3 个发射薄壳退役**（`emit_rtl.cpp` / `emit_rtl_fw_init.cpp` / `emit_rtl_hier.cpp`）
  ⇒ ⇒ 发射入口并入 `tb_rv32i_soc` 的 `--emit-rtl` / `--emit-rtl --firmware-init` /
  `--emit-rtl --hierarchical` 三个模式 ⇒ ⇒ **产物逐字节不变**
- **根 `tests/test_rv32i_pipeline.cpp` 与 `examples/riscv-mini/tests/test_rv32i_pipeline.cpp`
  不是副本** ⇒ ⇒ 后者注册为 `test_rv32i_pipeline_example`

---

## 9. 导航

| 想找什么 | 去哪 |
|---|---|
| **本目录的运行期速查 + 接线表** | [`examples/riscv-mini/AGENTS.md`](../../examples/riscv-mini/AGENTS.md) §TEST |
| **核 IP 53 端口契约表 / IP 集成契约** | `examples/riscv-mini/AGENTS.md`（「核 IP 的 53 端口契约表」+「IP 集成契约」两节） |
| **RTL 交付流程 + Verilator 仿真环境** | [`docs/developer_guide/rv32i-rtl-delivery-and-sim.md`](../developer_guide/rv32i-rtl-delivery-and-sim.md) |
| **verilator submodule 集成 / 平台支持** | [`docs/developer_guide/verilator-integration.md`](../developer_guide/verilator-integration.md) |
| **跑 ELF（3 层链路 / 装载约束 / 排障）** | `examples/riscv-mini/RUNNING-ELF.md` |
| **官方套件资产来源 / 改造 / 排除项 / 复现** | `examples/riscv-mini/tests/riscv-tests/PROVENANCE.md` |
| **组件生命周期（`ch_device` vs `ch_module`）** | [`docs/developer_guide/patterns/COMPONENT-LIFECYCLE-GUIDE.md`](../developer_guide/patterns/COMPONENT-LIFECYCLE-GUIDE.md) |
| **RV32I 微架构契约（权威细节）** | `docs/architecture/rv32i-microarchitecture.md` |
| **ISA 语义 capability** | `openspec/specs/rv32i-isa-semantics/spec.md` |
| **特权级 capability（CSR / trap / WARL）** | `openspec/specs/rv32i-privilege/spec.md` |
| **架构决策正本** | `docs/adr/`（76 篇 · **决策以它为准**） |
| **每个 change 的验收报告（逐条 AC + 变异登记）** | `.rddf/reviews/*-verify.md` |
| **护栏可达性台账** | `.rddf/checks/guard-reachability.md` |
| **RTL 仿真覆盖清单（E4 名单）** | `.rddf/checks/examples-rtl-sim-targets.tsv` |

---

## 10. 维护纪律

- 本篇是**入口** ⇒ ⇒ 新增/退役测试 ⇒ ⇒ **同批**更新：§2 清单（**含「规模」列**）· §7 锚点 ·
  `examples/riscv-mini/AGENTS.md` 的 §TEST 接线表 · 根 `AGENTS.md` 的计数锚点。
- ⚠️ **「规模」列的刷新办法**（实测口径）：逐个跑二进制取 Catch2 汇总行 ⇒
  `All tests passed (N assertions in M test cases)` ⇒ ⇒ N/M 即该列的两个数。
- ⚠️ **不要**把变更流水往本篇追加 ⇒ ⇒ 流水归**过程工件**
  （change 的 `design.md` / `tasks.md` · `.rddf/reviews/<change>-verify.md` ·
  `docs/CHANGELOG.md`）⇒ ⇒ 本篇只留**当前态**。
- ⚠️ 本篇**不复述**其它文档正文 ⇒ ⇒ 引用一律链出去 ⇒ ⇒ 避免同一事实两处维护。
