# riscv-mini 双后端（interpreter + verilator）**覆盖缺口盘点**

> **建档**：2026-11-26 · **触发**：用户「审查 `docs/riscv-mini/test-architecture.md`」⇒
> 双后端覆盖现状盘点
> **本篇的角色**：回答**一个**问题 —— 「`examples/riscv-mini/tests/` 的 26 个条目里，
> **哪些**值得做双后端化、**按什么顺序**做」。
> **权威分层**：契约在 `openspec/specs/verilator-rv32i-e2e/spec.md` +
> `dual-backend-test-coverage/spec.md`；**本篇是排期依据**（不是契约，也不是现状快照 ——
> 现状快照见 `docs/riscv-mini/test-architecture.md` §2）。

---

## 0. 结论速览

| 档 | 条目 | 判定 |
|---|---|---|
| **P0** | ✅ **P0-1 已收口**（2026-11-26 · AXI-Lite 11 根进向量）· ✅ **P0-2 已收口**（2026-11-27 · trap / CSR 的跨后端 golden） | ✅ 均已完成 |
| **P1** | `test_rv32i_stall_integrity`（反压期操作数完整性） | 做（可搭 P0 的 MMIO 多拍车） |
| **P2** | 9 个 `test_pipeline_*` / `test_wrapper_*` / `test_ifstage_device` + `test_csr_unit` + `test_interrupt_flow` + 官方套件 4 条 + `test_riscv_tests` | **不做**（理由逐条见 §6） |
| **P3** | `test_address_decoder` / `test_axil_mmio_master` / `test_soc_hello_world` / `test_riscv_tests_pipeline` | **结构性不可行 或 零价值**（§7） |

**一句话**：`examples/riscv-mini/tests/` 的 **26 个条目里只有 3 个有 verilator 证据**
（`test_rv32i_soc_dual_parity` · `test_rv32i_hier_rtl` · `riscv_mini_cache_smoke`）
⇒ ⇒ **缺口不在「数量」而在「那 3 个的判别面」**：核级语义（trap / CSR / 反压）与
AXI-Lite 通道都不在其中。

---

## 1. 排序模型（**先读这节，否则排序看着像拍脑袋**）

```
价值 = 判据面风险 R × 现有 verilator 覆盖缺口 (1 − V) ÷ 改造成本 C
```

**根前提**：双后端 parity 是**同构盲区** —— interpreter 与 Verilog codegen 由**同一份 C++**
生成 ⇒ ⇒ 它**抓不到**「两边一致地错」。⇒ ⇒ 它的**唯一不可替代价值** = 抓 **codegen 侧独有**
问题（初值块 / 端口映射 / 时序形状 / 宽度 / 未驱动输入）。

⇒ ⇒ 推论（决定 P2/P3 的边界）：

| 判据形态 | 双后端价值 | 例 |
|---|---|---|
| 固件驱动 + 多拍时序 + 端口握手 | **高** | CSR 指令 · trap 入口 · 反压 · AXI-Lite |
| 组件级纯逻辑（组合/时序） | **中**，但**仓根已覆盖** ⇒ 边际 ≈ 0 | HazardUnit · 各 stage |
| 纯常量 / 查表 / 结构断言 | **≈ 0**（同构风险高、影响面小） | 地址译码表 |
| 已在**交付物级** verilator 上跑过 | **≈ 0** | 官方 ISA 套件 |

`R` 高 / `V` 低 / `C` 低的项排前面。

---

## 2. 现状矩阵（26 条目 · 2026-11-26 实测）

| 条目 | 用例/断言 | DUT 层级 | 观测面（顶层可见？） | verilator |
|---|---|---|---|---|
| `test_rv32i_soc_dual_parity` | 18 / 60311 | 核顶层 | **39 / 39** 根 `ch_out`（含 11 根 `axil_*`） | ✅ 双路逐拍（含 trap / CSR） |
| `test_rv32i_hier_rtl` | 4 / 188 | 产物（flat vs hier） | Verilog 产物端口表 | ✅ 产物级 |
| `riscv_mini_cache_smoke` | 12 / 78 | cache wrapper | 组件 `ch_out` | ✅ 双后端 |
| `test_rv32i_trap_entry` | 11 / 1196 | **核顶层** | `dbg_*` + 固件写回 | ❌ |
| `test_rv32i_csr_instructions` | 8 / 31 | **核顶层** | `dbg_wb_*` + `dbg_illegal_reason` | ❌ |
| `test_rv32i_stall_integrity` | 3 / 21 | **核顶层** | `dbg_wb_*` / `dbg_id_pc` | ❌ |
| `test_rv32i_core_mmio` | 7 / 50 | **核顶层** + AXI-Lite stub | `obs_data_write_*` | ❌ |
| `test_axil_mmio_master` | 5 / 58 | 核内组件 `AxilMmioMaster` | 组件端口 | ❌ |
| `test_interrupt_flow` | 18 / 68 | 组件（`Clint` / `CsrBank` / `ExceptionUnit`） | 组件端口 | ❌ |
| `test_csr_unit` | 15 / 60 | 组件 `CsrBank` | 组件端口 | ❌（仓根 `test_rv32i_arch_dual` 已覆盖同面） |
| `test_address_decoder` | 10 / 132 | 组件 `AddressDecoder` | 组件端口 | ❌ |
| `test_soc_hello_world` | 4 / 11 | 核 + TCM + UART MMIO | UART 写捕获（核外） | ❌ |
| `test_rv32i_official_suite` g1–g4 | 各 11 / 22 | **核顶层** | `tohost` | ❌（交付物级已有 `verify_rv32i_official_rtl.sh`） |
| `test_riscv_tests` | 49（6 passed / 43 skip）/ 10 | **核顶层** | `tohost` | ❌（同上） |
| `test_pipeline_exec` | 16 / 69 | 组件/组合 | 内部节点 / 顶层 | ❌（仓根 `test_rv32i_pipeline_behavior` 等覆盖） |
| `test_rv32i_pipeline_example` | 6 / 25 | 同上 | 同上 | ❌（同上） |
| `test_pipeline_debug` / `_minimal` / `_wrapper` / `test_wrapper_simple` / `_ifstage` / `_pipeline` / `test_ifstage_device` | 1–9 | 组件 | 组件端口 | ❌（同上） |
| `test_riscv_tests_pipeline` | 46（全 skip · 只构建） | 流水线 + 仓外镜像 | — | ❌（依赖仓外二进制 ⇒ 与后端无关） |

**仓根源侧的既有双后端资产**（P2 判「边际 ≈ 0」的依据）：
`tests/test_rv32i_pipeline_behavior.cpp`（**20** pair · Batch 1/2/3/4/5）·
`test_rv32i_arch_dual`（4）· `test_rv32i_stage_dual`（5）· `test_rv32i_timing_dual`（3）·
`tests/chlib/test_forwarding_dual.cpp`（1）。

---

## 3. P0-1 · AXI-Lite 通道的跨后端逐拍证据 —— ✅ **已收口（2026-11-26）**

> 收口 change = `rv32i-obs-vector-axil-2026-11`：向量 28 → **39**（+11 根 `axil_*`）·
> 8 根输入变成显式驱动（`AxilSlave` 默认**立即应答** · 全常量 ⇒ 逐拍输入相同）·
> 新增 `Rv32iSoc - AxilMmioCrossBackend`（MMIO 事务的 AXI 语义 golden + 非 MMIO 程序
> 的「请求线全程为 0」）· 登记表 11 行**归零**。
> ⇒ ⇒ 以下保留**原始分析**（照相机式记录「为什么当时值得做」）。

**现状（三条独立事实指向同一个洞）**

| 事实 | 证据 |
|---|---|
| 核顶层有 **39** 个 `ch_out` | `examples/riscv-mini/src/rv32i_core_top.h` 的 `__io`（28 旧观测 + **11 `axil_*`**） |
| 跨后端向量只覆盖 **28** | `test_rv32i_soc_dual_parity.cpp` 的 `kObsNames[]` |
| fixture **不驱动** 8 根 `axil_*` 输入 | 只驱动 `ext_rst` / `prog_*` ⇒ 未驱动 = 未定义值（根 `AGENTS.md` 铁律） |

**为什么值得**：AXI-Lite 是「多拍握手 / VALID-READY 相位 / WSTRB 车道」—— codegen 最易算错的
形态；而唯一覆盖它的两个判据（`test_axil_mmio_master` / `test_rv32i_core_mmio`）**只在解释器跑**。

**改法**：① 驱动 8 根输入（范式：`test_rv32i_trap_entry.cpp` 的 tie-0）② 固件加一条 MMIO 事务
（否则 11 根线恒 0 ⇒ 比对**空转**）③ 11 根进 `kObsNames[]` ⇒ `static_assert` → 39
④ 删掉登记表的 11 行（护栏 C3 强制）。

**成本**：中（1 个 change）· **完成判据**：向量 39/39 · MMIO 事务在两后端逐拍一致 ·
**1 条变异**（去掉一拍 `wvalid` ⇒ 必红）。

> ⚠️ **护栏已就位**：`.rddf/checks/rv32i-obs-vector-registry.md` 登记了这 11 行，
> `scripts/verify_dual_backend_parity.sh` 的第二判据（C1–C4）保证「不补齐就必须留账」。

---

## 4. P0-2 · trap 入口 / CSR 指令的跨后端 golden —— ✅ **已收口（2026-11-27）**

> 收口 change = `rv32i-trap-csr-crossbackend-2026-11`：新增 `Rv32iSoc - TrapEntryCrossBackend`
> 与 `Rv32iSoc - CsrInstrsCrossBackend`（逐拍 diff 39 根 + 手推写回 golden + 2 条变异）。
> ⚠️ **如实登记**：本项只覆盖「未知 opcode」一条触发源；`ECALL`/`EBREAK`/`SRET` 与中断
> 的跨后端仍归后续（前者由解释器判据覆盖，后者核顶层无 `irq` 端口）。
> ⇒ ⇒ 以下保留**原始分析**。

**现状**：两个核级判据（11 用例 / 1196 断言 + 8 / 31）**只有解释器证据**。
dual parity 覆盖的是「非法指令**不退休 + 可观测**」与「flush 优先于 trap」，
**没有**覆盖：

- trap 入口的**写回语义**（`mepc` = 自身 PC · `mcause` = 2/11/3 · PC → `mtvec`）；
- CSR 指令的 `csr_wdata` 计算（`CSRRC` MUST 是「按位取反后 AND」而非 XOR）。

**为什么值得**：这族是**交付语义**（ISA 合规），且**期望值可完全手推**（规范 + 复位值）
⇒ ⇒ 「手推 + 双后端」是唯一能同时抓「两边一起错」与「codegen 算错」的组合。

**改法**：按 spec 要求用「同一 TEST_CASE 内 2×`ch_device` + 2×`Simulator`」（**不能用**
`DUAL_BACKEND_TEST_PAIR` 宏做逐拍 diff）；判据 = 写回序列 + `dbg_illegal_reason` + 退休计数，
**两后端逐拍一致 + 终值 == 手推**。

**难点（真实存在，别低估）**：CSR 读回靠固件 `csrrs` ⇒ 读回序列在两后端可能**拍数不同**
⇒ PATH 逐拍 diff MUST 按**事件拍**对齐（不能按绝对拍）。

**成本**：中-高（1–2 个 change）。

---

## 5. P1 · 反压 / 多拍握手

`test_rv32i_stall_integrity`（3 / 21）判「MEM 级反压期间被冻结在 EX 的指令的**操作数完整性**」
—— 只有解释器证据。dual parity 的固件里有 D-L1 冷行 miss（4-beat burst ⇒ 多拍冻结），
但判的是**事件拍数**，**不是**反压期操作数完整性。

**成本**：中（可搭 P0-1 的 MMIO 事务车）。

---

## 6. P2 · 边际价值 ≈ 0（**不做**，附理由以防反复被提）

| 条目族 | 理由 |
|---|---|
| `test_pipeline_exec` / `test_rv32i_pipeline_example` / `test_pipeline_debug` / `_minimal` / `_wrapper` / `test_wrapper_simple` / `_ifstage` / `_pipeline` / `test_ifstage_device` | **仓根已有双后端等价物**（见 §2 末的资产表）⇒ 组件级面已在根侧闭合 ⇒ examples 侧重做 = **零增量** |
| `test_csr_unit` | 判 CSR **复位值 + WARL** ⇒ 仓根 `test_rv32i_arch_dual`（双后端）已覆盖同面 |
| `test_interrupt_flow` | ① DUT 是**组件**（`Clint` / `CsrBank` / `ExceptionUnit`），不是核顶层 ② **中断路径在核顶层尚不存在**（`irq` 端口归 S5 · IP v1 = polling-only）⇒ 现在做会**白做** |
| `test_rv32i_official_suite` g1–g4 / `test_riscv_tests` | 官方 ISA 族**已有 verilator 侧证据**：`scripts/verify_rv32i_official_rtl.sh`（40 镜像 · 真 verilator · 交付物级）⇒ 核级再双后端化 = 同一批镜像跑两遍 |

---

## 7. P3 · 结构性不可行 / 零价值

| 条目 | 原因 |
|---|---|
| `test_address_decoder`（10 / 132） | 纯**查表**判据 ⇒ 双后端价值 ≈ 0（查表逻辑同构）；且组件级根侧有同类 |
| `test_axil_mmio_master`（5 / 58） | DUT 是核**内部**组件 ⇒ 若独立双后端化，与 **P0-1（核顶层含该通道）重复**且覆盖更弱 ⇒ **应被 P0-1 吸收** |
| `test_soc_hello_world`（4 / 11） | 观测点在**核外**（UART 写捕获）⇒ 不在核端口面上 |
| `test_riscv_tests_pipeline`（46 · 全 skip） | 依赖**仓外**二进制 ⇒ 与后端无关（卡在 `RISCV_TESTS_PATH`） |

---

## 8. 已收口的护栏缺口（**别再重复发现**）

2026-11-26 之前：跨后端向量与核顶层端口面之间**没有任何护栏** ⇒ S4a 加 19 个端口时
无一处转红。**已由** change `2026-11-26-rv32i-dual-backend-obs-vector-guard-2026-11`
（归档）收口：`scripts/lib/rv32i_obs_vector.py` 的 C1–C4 + 缺口登记表 + spec 契约
（`verilator-rv32i-e2e` 的「观测集合 MUST 对着端口面守恒」）。

⇒ ⇒ **推论**：本盘点的 P0-1 不再是「可选优化」—— 只要不补，登记表就一直在册；
一旦有人想删登记行（C3），就必须同时把端口补进向量。**该推论已于 2026-11-26 兑现**
（P0-1 收口 ⇒ 登记表归零 ⇒ 护栏现在守强形式 `|向量| == |端口面|`）。

---

## 9. 落地顺序

| 步 | 内容 | 前置 | 估 |
|---|---|---|---|
| **1** | 护栏（**已归档** 2026-11-26） | — | ✅ |
| **2** | **P0-1**：AXI-Lite 19 端口进跨后端证据 | 步 1 的登记表 | ✅ **已收口 2026-11-26** |
| **3** | **P0-2**：trap / CSR 的跨后端 golden | 步 2 的 fixture 范式 | ✅ **已收口 2026-11-27** |
| **3b** | **P1**：反压期操作数完整性（`test_rv32i_stall_integrity`） | 步 2 的 fixture 范式 | 待排 |

**CI 成本**：全部落在**已有**的 3 个 `verilator_e2e` 条目内扩 ⇒ **不新增条目**
⇒ `VERILATOR_E2E_TOTAL` / `CTEST_TOTAL` 锚点不动。

---

## 10. 维护纪律

- 本盘点**不是**现状快照 ⇒ ⇒ 现状（条目 / 用例数 / 向量条数）以
  `docs/riscv-mini/test-architecture.md` §2 + 护栏输出为准；两者冲突时**以护栏为准**。
- **新增**核顶层 `ch_out` ⇒ 必须「进向量」或「进登记表」（护栏 C4 强制）⇒ **本盘点的 P0-1
  完成后，登记表归零**，此时 C1 变成 `|V| == |P|` ⇒ 本文件 §3 可整节标记为「已收口」。
- **新增** verilator 条目 ⇒ 同批更新 §2 矩阵 + `docs/riscv-mini/test-architecture.md` §2。
- ⚠️ **不要把变更流水写进本篇**（流水归 change 的 `design.md` / `tasks.md` / `.rddf/reviews/`）。
