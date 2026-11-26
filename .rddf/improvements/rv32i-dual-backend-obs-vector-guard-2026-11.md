---
name: rv32i-dual-backend-obs-vector-guard-2026-11
priority: P1
phase: phase-6
category: testing
type: feature
状态: implemented
进度: "**立项（2026-11-26 · 用户指令 · 主会话 builder）** ⇒ ⇒ 本项只做**步 1 = 护栏先行**（护栏 + 缺口登记表 + spec 契约），**不**补向量；缺口本体（11 根 `axil_*` 进跨后端向量）归**步 2 = 独立 change**（`rv32i-obs-vector-axil-2026-11`） ⇒ ⇒ ✅ **步 1 已归档**（change `2026-11-26-rv32i-dual-backend-obs-vector-guard-2026-11` · MODIFIED `verilator-rv32i-e2e` 已合并进主 spec）"
主题: dual-backend 覆盖守恒 —— 跨后端观测向量 vs 核顶层端口面（`axil_*` 缺口）
依赖: change `rv32i-soc-mmio-channel-2026-11`（**S4a** 给核加 AXI-Lite 通道 · 19 端口 · 已归档 2026-11-24）· ADR-046 D10（核顶层端口面冻结）· `scripts/verify_dual_backend_parity.sh`（既有 parity 计数护栏 · 本项在其内加第二判据）
来源: 用户 2026-11-26「审查 docs/riscv-mini/test-architecture.md」⇒ 双后端覆盖盘点 ⇒ 采纳「步 1 = 护栏先行」+ builder 实测（向量 28 ≠ 核顶层 ch_out 39）
生成时间: 2026-11-26
evidence: probe:examples/riscv-mini/tests/test_rv32i_soc_dual_parity.cpp
---

# Improvement: 跨后端观测向量守恒护栏（RV32I）

**主题**: dual-backend 全量覆盖残余

> 具体对象 = **核顶层 `ch_out` 端口面**与**跨后端逐拍比对向量**之间的**守恒**。
> （frontmatter 的 `主题:` 是完整表述；本行取**路线图主题**以过 `add-improve` 的 HARD-GATE 匹配。）

---

## 架构依据

| # | 权威 / 实测事实 | 对本项的约束 |
|---|---|---|
| ① | 核顶层 `examples/riscv-mini/src/rv32i_core_top.h` 的 `__io` 有 **39 个 `ch_out`**（28 → S4a 加 **11 根 `axil_*`**） | 端口面是**被冻结的契约**（ADR-046 D10）⇒ ⇒ 判据必须**对着端口面**判，不能对着测试自己判 |
| ② | 跨后端观测向量 = `test_rv32i_soc_dual_parity.cpp` 的 `kObsNames[]` ⇒ **28 条**，且 `static_assert(kObsCount == 28)` | 向量**只与自己自洽** ⇒ ⇒ **S4a 加 19 个端口时没有任何护栏转红**（实测：向量与端口面差值 = 11） |
| ③ | 同一 fixture **不驱动 8 根 `axil_*` 输入**（只驱动 `ext_rst` / `prog_*`） | 按根 `AGENTS.md` 铁律「未驱动输入 = 未定义值」⇒ ⇒ 该通道在双后端跑的是**未定义输入** ⇒ ⇒ 「没比对」+「输入未定」= 双重无证据 |
| ④ | `scripts/verify_dual_backend_parity.sh` 已有**双锚点**（文件数 / pair 调用数），但两者都只数**宏调用** | 计数锚点**看不见**「向量 vs 端口面」这类**语义覆盖**缩面 ⇒ ⇒ 需要**第三判据**（本项） |
| ⑤ | 仓内既有「登记表 + 双向校验」范式：`scripts/verify_input_port_required_registry.sh` + `.rddf/checks/input-port-required-registry.md` | 缺口 MUST **登记**（带归位条件）⇒ 编造豁免 / 过期豁免 **双向**都可判 |

---

## 范围

- **In Scope**：
  - `scripts/lib/rv32i_obs_vector.py`（新逻辑载体 · 4 条判据 + fixture 自检）
  - `scripts/verify_dual_backend_parity.sh`（挂第二判据 + 自检串联）
  - `.rddf/checks/rv32i-obs-vector-registry.md`（缺口登记表 · 11 条 `axil_*`）
  - `openspec/specs/verilator-rv32i-e2e` 的契约修正（「全部 27 个输出观测值」已陈旧）
  - `docs/riscv-mini/test-architecture.md` §6 指针
- **Out of Scope**（逐条给出归属）：
  - **补向量**（11 根 `axil_*` 进向量 + 驱动 8 根输入 + MMIO 事务固件）⇒ ⇒ **步 2** 独立 change
  - 其余 23 条解释器-only 条目的双后端化 ⇒ ⇒ 盘点结论是「P2/P3 不做」（见 `docs/riscv-mini/test-architecture.md` §2 的判据面分类）
  - 核顶层端口面变更（ADR-046 D10 冻结 ⇒ 本项**零端口变更**）

---

## Why

S4a（2026-11-24）给核加了 AXI-Lite 通道（8 in + 11 out）之后，**跨后端逐拍比对的观测面**与**核顶层端口面**之间出现了一条**无人看守的缝**：

- 向量仍是 28 条（`kObsCount == 28`），端口面已是 39 条 ⇒ **11 根 `axil_*` 输出从未跨后端比对**；
- fixture 连那 8 根 `axil_*` **输入都没驱动** ⇒ 即使比对也是比两个「未定义输入下的输出」。

⇒ ⇒ 结果是：**交付的 Verilog 里 AXI-Lite 通道的逐拍行为没有任何第二后端证据**，而这条通道恰好是「多拍握手 / VALID-READY 相位 / WSTRB 车道」这类 **codegen 最易算错**的形态。而唯一的推导性证据（`test_axil_mmio_master` / `test_rv32i_core_mmio`）**只在解释器跑**。

⇒ ⇒ 本项**不修那个洞**（那是步 2），只做一件事：**让这个洞第二次出现时必须有人签字** —— 把「向量 == 端口面 − 已登记缺口」变成可机械判定的不变量。

---

## What Changes

| # | 内容 | 载体 |
|---|---|---|
| **C1** | **守恒判据**：`向量条数 == 核顶层 ch_out 数 − 已登记缺口数` | `rv32i_obs_vector.py --check` |
| **C2** | **登记有效性**：登记表每条端口名 MUST 真实存在于核顶层 `ch_out`（防编造豁免） | 同上 |
| **C3** | **登记不 stale**：登记表每条 MUST **不在**向量里（补齐后必须清账 ⇒ 否则红） | 同上 |
| **C4** | **归属判据**：核顶层每个 `ch_out` MUST ∈（向量 ∪ 登记）⇒ 新增端口**不可能**静默失同步 | 同上 |
| **C5** | **自检**：5 类投毒（删向量项 / 编造登记 / 删登记 / 加端口不进向量 / 登记与向量重复）⇒ 逐条必红 ⇒ 还原复绿 | `--self-test` |
| **C6** | **缺口登记表**：11 条 `axil_*` 输出（带「为什么未纳入」+「归位条件」） | `.rddf/checks/rv32i-obs-vector-registry.md` |
| **C7** | **契约修正**：`verilator-rv32i-e2e` 的「全部 **27** 个输出观测值」⇒ 「核顶层**全部** `ch_out` 观测端口（当前缺口登记在册）」 | spec delta |

---

## Acceptance

- [ ] `scripts/verify_dual_backend_parity.sh` 主路径跑出 **PASS**，且输出里含第四判据的 4 行结论（守恒 / 登记有效 / 不 stale / 归属）
- [ ] `--self-test` 5 类投毒**逐条必红**、还原复绿（实测输出贴进验收报告）
- [ ] 缺口登记表 11 条与核顶层 `axil_*` 输出**逐字一致**（正反向都可判：多一条 / 少一条都红）
- [ ] 篡改实验（手工）：把 `kObsNames[]` 删一项 ⇒ 主路径**必红**；把登记表删一行 ⇒ **必红**；把某一根 `axil_*` 加进向量但不删登记 ⇒ **C3 必红**
- [ ] `openspec/specs/verilator-rv32i-e2e` 的「27 个输出观测值」被修正（archive 后主 spec 无该陈旧数字）
- [ ] `docs/riscv-mini/test-architecture.md` §6 的 parity 行含新判据指针
- [ ] 既有护栏回归：`verify_guard_reachability.sh` / `verify_docs_links.sh` / `verify_test_counts.sh` 全绿

---

## Capabilities

新增 / 收紧的契约：

| capability | 变化 | 内容 |
|---|---|---|
| `verilator-rv32i-e2e` | **MODIFIED** | 双路逐周期 parity 的观测面 MUST == 核顶层 `ch_out` 端口面（缺口 MUST 登记 + 带归位条件 + 补齐后 MUST 清账） |
| （结构面） | **新增护栏判据** | `verify_dual_backend_parity.sh` 从「只数宏调用」扩到「宏调用 + 观测向量守恒」 |

---

## Impact

| 面 | 影响 | 缓解 |
|---|---|---|
| **CI** | `build-and-test` / `pr-verilator-fast` 的两个 parity step **各 +1 条 python 判据**（毫秒级） | 无新增 job / 无新增脚本 ⇒ **guard-reachability 台账 1:1 不变** |
| **CI 计数锚点** | `CTEST_TOTAL` / `VERILATOR_E2E_TOTAL` / `DUAL_BACKEND_PARITY_*` / `EXAMPLES_TEST_SOURCES` **全部不变** | 本项不加测试、不加宏调用 |
| **产品代码** | **零改动**（只加护栏 + 登记表 + spec 契约） | — |
| **后续（步 2）** | 补齐 11 根后 MUST 删 11 行登记（C3 会强制） | 步 2 的 change 必须把「删登记」写进 AC |
| **误报风险** | 若将来**有意**缩观测面（例如某个输出不可比）⇒ 必须走登记 | 登记表要求「为什么未纳入」+「归位条件」⇒ 缩面会被看见 |
| **风险** | 判据只覆盖**这一个**消费点（RV32I SoC 的向量） | 归属判据 C4 是通用的（端口面一侧）；其它 DUT 的向量可按同法扩展 |
