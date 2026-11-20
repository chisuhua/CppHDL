# Proposal: 删掉 SoC 顶层零消费的 `timer_irq` 占位端口（台账 id=27 落地）

- **状态**: **立项（2026-11-21 · builder · 用户 2026-11-21 批准执行）** · 待归档
- **性质**: **实施侧**（**决策已定**，本 change **不改任何决策**）
- **裁决依据**: `docs/adr/ADR-074-rtl-verification-single-source.md` **§7.7**
  （台账 id=27 ⇒ 裁决 = **删除该占位端口**，`ext_clk` 同型先例 · 2026-11-14 用户改判）
  + **2026-11-20 用户追认**（采纳推荐）
- **上一轮显式移交**: `openspec/changes/archive/2026-11-20-axi4-soc-verif-migration-2026-11/proposal.md:91`
  ——「删除根自有 `timer_irq` 占位端口（台账 id=27）⇒ ⇒ **独立 change 收口**（ADR-074 §7.7）」
  ⇒ ⇒ 本 change 就是那个独立 change。

---

## Why

SoC 顶层（`examples/axi4/axi_soc_example_top.h`）的根 `__io` 里有一个 `timer_irq` 输入端口：

- **DUT 从不读它**（全仓 grep：声明处 1 命中 + 消费者 0 命中）⇒ ⇒ 它**在顶层端口面里但没有语义**；
- 它不是「另一个 IP 的 IRQ」⇒ ⇒ 语义**不可混用**（ADR-073 事实⑭ 已把它登记为死端口）；
- 它**不是**库层 `AxiLiteUart::irq`（那是 id=26 的裁决对象 ⇒ **不删**）⇒ ⇒ 两者不可混为一谈。

⇒ ⇒ 后果有三：

1. **端口面撒谎**：对外交付的 SoC 端口表里有一根「看起来有效、实则无意义」的输入 ⇒ ⇒
   下游会以为「接上就能收中断」⇒ ⇒ **MUST NOT** 假装它可观测（ADR-073 §D2 表末两行）；
2. **验证面被它绑架**：TB 必须为它接一个常量（否则 verilator 严格 flags 下
   `%Warning-PINMISSING` 致命）⇒ ⇒ 「端口存在」被误当成「端口有义务」；
3. **它还占着等价性 harness 的输入面**：根的 `ch_in` 全集被当成「必须逐个驱动的输入面」
   ⇒ ⇒ 一根死端口逼着 stimulus 表多一列（无信息量的一列）。

⇒ ⇒ 台账把它登记为 **id=27**（状态 `已知`），ADR-074 §7.7 裁决 = **删**，
⇒ ⇒ 只差一次实施。

## What Changes

### ① 删端口（设计层）

`examples/axi4/axi_soc_example_top.h` 删 `ch_in<ch_bool> timer_irq{"timer_irq"};`
⇒ ⇒ 根自有 `__io` **12 → 11**（8 in + 4 out → **7 in + 4 out**）⇒ ⇒ 顶层端口 **14 → 13**。

### ② 验证面同步（三处 + 一次重生成）

| 处 | 改什么 |
|---|---|
| `examples/axi4/tb_axi_soc_example.verif` | 删 `SET timer_irq 0`（否则解释器侧渲染会撞「未知端口」） |
| `examples/axi4/tb_axi_soc_example.cpp` | ① `make_inputs()` 删该输入项（根 `ch_in` 全集 8 → 7）· ② `make_stimulus()` 每行删最后一列（14 行 × 1 列）· ③ `kExpectedTopPorts` 14 → 13 |
| `examples/axi4/tb_axi_soc_example.sv` | **由 `.verif` 重新渲染**（单一真相源 ⇒ **禁**手改生成物；护栏 ⑥' 逐字节比对） |

### ③ 期望源与清单同步

- `.rddf/checks/soc-rtl-expectations.md`：§1 端口表删行 10（并重编号 11–14 → 10–13）·
  §5 输入常量删 `timer_irq` 行 · §5b 场景出处表删 `timer_irq` · §5c 盲区段删「`timer_irq` 是零消费死端口」那条；
- `.rddf/checks/examples-rtl-sim-targets.tsv`：SoC 行 note 的「顶层端口 14 / 根自有 12」→ 13 / 11。

### ④ 残留登记两侧同批转「已收口」

- `.rddf/checks/arch-residual-registry.md` 的 **id=27** 行：状态 `已知` → **`已收口`**，
  日期 = 状态首次落账日（**2026-11-21**）· 后缀 · 闭口证据（归档 change 的 `proposal.md`
  + 删端口的源文件）· 归属 `—`；
- `docs/architecture/2026-11-02-hierarchical-rtl-output.md` §7 的 **行 27** 状态格
  MUST 以登记表**生成的规范句**开头（`✅ 已收口（2026-11-21 · …）`）⇒ ⇒ **两侧同批**。

## 判据

| 判据 | 命令 |
|---|---|
| SoC 产品面（⑧ 端口表 / ⑥' 新鲜度 / ⑥'' 双向键 / ⑥‴ 无残留 / ⑧'' 生成物形态 / ⑨ 等式） | `bash scripts/verify_examples_rtl_sim.sh` |
| 残留登记 ↔ 台账 1:1 | `bash scripts/verify_arch_ledger_residuals.sh` |
| examples 全量构建 + 运行护栏 | `cmake --build build --target all-examples` + `./run_all_ported_tests.sh` |
| 库测试不回归 | `ctest -L base` |

## Out of Scope

- **不**动库层 `AxiLiteUart::irq`（台账 **id=26** 的裁决 = **不删**：它是 in-force spec
  `dual-backend-test-coverage` 强制的行为覆盖载体）⇒ ⇒ 本 change 只处理**根自有**的
  `timer_irq` ⇒ ⇒ 两者的语义**不可混用**。
- **不**改 ADR-073 事实⑭（那是**决策史**：它记录的是 2026-11-16 当时的状态）⇒ ⇒
  现状由残留登记表 + 台账承载。
- **不**改 SoC 的**功能行为**：删的是零消费端口 ⇒ ⇒ 四个场景的期望值逐字不变
  （`VERIF-CHECKS: 15` 不变）。
- **不**引入新的 Timer 外设（那是 id=27 的「复核条件」，属另案）。

---

## 用户可见变更

> ⚠️ **本段是 2026-11-21 补写的**（归档时遗漏 ⇒ `scripts/verify_public_api_visibility.sh`
> 的 **R1**（归档 change MUST 带「## 用户可见变更」段 · 机制生效日 2026-11-09）会报红
> ⇒ ⇒ 补的是**归档流程的产物要求**，不改动本 change 的任何决策与实施内容）。

| 项 | 前 | 后 |
|---|---|---|
| SoC 顶层端口表（`examples/axi4/axi_soc_example_top.h`） | **14** 根（框架 2 + 根自有 12）⇒ ⇒ 含一根**零消费**的 `top_timer_irq` 输入（下游会误以为「接上就能收中断」） | **13** 根（框架 2 + 根自有 **11**）⇒ ⇒ `top_timer_irq` **物理删除**（台账 id=27 · ADR-074 §7.7 裁决 = 删 ⇒ 用户 2026-11-20 追认） |
| SoC 的 TB / 验证源 / 生成物 | `.verif` 含 `SET timer_irq 0` · TB 输入面 8 根 + stimulus 8 列 · `.sv` 含 `top_timer_irq` 连线 | 三者**同步**（`.verif` 删该 SET · 输入面 7 根 + stimulus 7 列 · `.sv` **由 `.verif` 重新渲染** ⇒ 逐字节可复现） |
| SoC 的场景期望值 | `VERIF-CHECKS: 15` · `RESULT: checks=15 fails=0` | **逐字不变**（删的是零消费端口 ⇒ ⇒ 行为不变） |
| 台账 **id=27**（根自有 `timer_irq` 零消费） | 状态 `已知` | 状态 **`已收口`**（登记表 + 台账 §7 **两侧同批** ⇒ 闭口证据 = 本归档 change） |
| 库层 `AxiLiteUart::irq`（台账 **id=26**） | `ch_out` · 零消费者 | **不动**（id=26 的裁决 = **不删**：它是 in-force spec `dual-backend-test-coverage` 强制的行为覆盖载体）⇒ ⇒ 与 `timer_irq` 语义**不可混用** |
