---
id: decision-request-port-face-2026-11
日期: 2026-11-13
提出者: planner（2026-11-13 独立复核 · **护栏实测 + 源码 grep + 产物实测**三路交叉 · 不采信 Status 行自述）
状态: **部分已闭**（R2 已落地 2026-11-13 · R3 早已由 D1 裁决闭 · **R1 待用户裁决**）
关联: .rddf/improvements/rtl-emit-product-tooling-2026-11.md · .rddf/improvements/examples-rtl-sim-coverage-2026-11.md · .rddf/checks/rtl-track-correction-list-2026-11.md · openspec/specs/rtl-emit-portability/spec.md
---

# 决策请求单：RTL 轨的**端口面残留**登记 + **两处判据缺口**（2026-11-13）

> ⚠️ **本单的定位**（与 `.rddf/checks/decision-request-input-port-guards-2026-11.md` 同体例）：
> planner 侧把**实测证据做齐了**（可复现命令 + 数值）⇒ ⇒ **裁决权在用户 / architect**
> ⇒ ⇒ ⇒ **本单只列「事实 / 选项 / 代价 / 建议」，不做决定**。
>
> ⚠️ **本单不是「7 份草案的完成度报告」** —— 那份清单是
> `.rddf/checks/rtl-track-correction-list-2026-11.md`（2026-11-12 · 已闭：8 个护栏 exit 0 · HARD-GATE 7/7）。
> ⇒ ⇒ **本单只收「那份清单里没有、且需要人拍板」的 3 条**。

---

## 0. 一句话背景

2026-11-13 的 planner 独立复核（**不采信 Status 行自述**）实测了 RTL 轨的落地情况 ⇒ ⇒
**已落地部分质量高于预期**（`mem_is_load` 泄漏已闭合 · 10/10 独立 TB 全绿 · hygiene 默认 off 保住层次化字节不变），
但暴露 **3 条需要人拍板的残留/缺口** ⇒ ⇒ 其中 **1 条是 planner 上一轮的口头审计错误**（已如实订正，见 §4）。

---

## 1. R1 —— 扁平产物的顶层端口面仍暴露 **80 个内部 `ch_out`** ⇒ 请求**登记为已知残留**

### 1.1 事实（2026-11-13 实测 · 可复现）

```bash
build/examples/riscv-mini/emit_rtl_rv32i_soc --out /tmp/x
sed -n '/^module top (/,/^);/p' /tmp/x/top.v | grep -cE '^\s+(input|output)'   # → 114
```
| 量 | 实测 | 口径 |
|---|---:|---|
| `.v` 顶层端口面 | **114** | 7 input + 107 output |
| 根组件自有 IO（`is_top_io == true`） | **32** | 5 input + 27 output |
| ⇒ ⇒ **暴露在顶层的内部 `ch_out`** | **80** | 107 − 27 |
| manifest `ports[]` 全量 | **191** | 84 input + 107 output ⇒ 比 `.v` 宽 **77** |
| 「未驱动 且 非 `is_top_io`」的 input | **0** | ⇒ ⇒ **C3 的悬空泄漏已闭合 ✅** |

### 1.2 为什么仍要登记（尽管它**已被裁决**）

⚠️ **先说清事实**：**C2（owner 口径的端口选择）不是「漏做」，而是「已撤回」** ——
`rtl-emit-product-tooling` 的 frontmatter 明写「**C2 已撤回（2026-11-11 用户裁决）**」，
草案 §63 记「**与已签批契约正面冲突** ⇒ 撤回 C2 · 保留已签批契约」。
⇒ ⇒ **`rtl-emit-portability` capability 的 Scenario（`:57-62`）把「`is_top_io` 的筛选是消费方的选择」
· 「MUST NOT 成为发射器的默认行为」规范化了** ⇒ ⇒ **现状是自洽的**。

⇒ ⇒ **仍请求登记的理由只有一条**：这条**已被裁决**的现状 ⇒ ① **不在残差台账里**
（`.rddf/checks/arch-residual-registry.md` grep「端口面 / 内部输出 / is_top_io」**零命中**）
⇒ ⇒ ② **只存在于规范与草案里** ⇒ ⇒ ③ 但它是**用户原始诉求 ⑦（RTL 卫生）**里的一条
⇒ ⇒ ⇒ **台账是「已知残留」的唯一真相源**（本仓纪律）⇒ ⇒ ⇒ **不在台账 ⇒ 缺口会静默留存**。

### 1.3 选项 / 代价 / 建议

| 选项 | 内容 | 代价 |
|---|---|---|
| **A（建议）** | **登记为已知残留**（台账 + 登记表 + `docs/architecture/` 同步）⇒ 一行写明「扁平顶层端口面 = 全部 `ch_out`（契约）⇒ ⇒ **80 个内部输出暴露** ⇒ ⇒ 消费方 MUST 按 `is_top_io`/`role` 过滤（`tb_stub.cpp` 已这么做 ✅）」 | 台账 +1 行 + 登记表 +1 行 ⇒ ⇒ **零代码改动** ⇒ ⇒ **不推翻任何已裁决内容** |
| B | 修订 `rtl-hierarchical-output` 的端口面契约（推翻 C2 撤回）⇒ ⇒ 走 ADR 取代流程 | 高 ⇒ ⇒ 且与已签批契约冲突 ⇒ ⇒ **不建议**（已裁决过一次） |
| C | 不登记（靠规范 + 草案两处承载） | 零成本 ⇒ ⇒ **但台账无行 ⇒ ⇒ 与本仓「台账 = 唯一真相源」纪律冲突** ⇒ ⇒ 且未来窗口可能重提 |

⇒ ⇒ **建议 A**。

---

## 2. R2 —— `verify_flat_rtl_emit.sh` 的 **F6 与 I1 的 AC 不一致** ⇒ 护栏**未实现 AC**

### 2.1 事实

| 位置 | 内容 |
|---|---|
| `rtl-emit-product-tooling` 的 AC（`:204`） | `~~verilator 后端的 work_dir/top.v 与产品产物**逐字节相同**（F6）~~` ⇒ ⇒ 🔴 **判据恒绿且形态不符**（两者是**同一个** `verilogwriter`）⇒ ⇒ 改为「**`ctest -L verilator_e2e`（77 条）全绿 ＋ `build_port_access_table()` 对每个 `port_access_` 条目都能取到非空 Vtop 字段**」 |
| `scripts/verify_flat_rtl_emit.sh` 实测输出 | `::notice F6 **当前不适用**（不是 pass）：本仓工具的设计**无寄存器** ⇒ ⇒ verilator backend 不产 top.v ⇒ ⇒ 无可比对象` ⇒ ⇒ 解除条件 = 「工具的设计里**加一个寄存器**」 |

⇒ ⇒ **落差**：**AC 已经改成可判别形态，护栏却仍停在「不适用」** ⇒ ⇒
⇒ **backend 的端口表与产品端口面是否同步，当前**无任何判据**（`port_access_` 与 `emit_sim_main_postlude`
是否与 flat 的 114 端口面一致 ⇒ 无护栏 ⇒ ⇒ 未来改端口面时**无人报警**）。

### 2.2 选项 / 代价 / 建议

| 选项 | 内容 | 代价 |
|---|---|---|
| **A（建议）** | **护栏实现 AC 的新 F6**：① `ctest -L verilator_e2e` 全绿（**注意**：该 job 需 `BUILD_VERILATOR=ON`，CI 的 `pr-verilator-fast` 才有 ⇒ ⇒ **护栏须在无 verilator 环境下 fail-closed 或显式跳过**）② 或**改用不依赖 verilator 的等价判据**：**给工具的自测设计加一个 `ch_reg`** ⇒ ⇒ backend 就会产 `work_dir/top.v` ⇒ ⇒ 「逐字节比对」**重新变得可判**（此时两者**不是**同一个函数的两次调用 ⇒ ⇒ **不是恒绿**） | 加一个寄存器 ⇒ ⇒ 工具的产物端口面变化 ⇒ ⇒ 护栏 F3/F8 的期望集需同步 |
| B | 维持「不适用」，但在 AC 上标注「**该条不适用**」（与护栏一致）⇒ ⇒ 承认 backend 端口表**无判据** | 零成本 ⇒ ⇒ **缺口留存**（未来改端口面无报警） |
| C | 把 F6 从护栏里删掉 | 零成本 ⇒ ⇒ 与 AC 又不一致 ⇒ ⇒ 不建议 |

⇒ ⇒ **建议 A**（② 的成本最低且能真正闭合；① 依赖 CI 环境 ⇒ ⇒ 需先确认护栏在无 verilator 环境下的失败语义）。

---

## 3. R3 —— `gpio_example` 的断言强度不足（`spec_checks = 0`）

### 3.1 事实（`scripts/verify_examples_rtl_sim.sh` 实测输出）

```
── gpio_example（target · emit_rtl_gpio · examples/spinalhdl-ported/gpio/tb_gpio_example.sv）
  ⚠️  ②'' spec_checks=0 < 3 ⇒ ⇒ impl-contract only（只能防漂移，不能证明符合规范）
  ✅ gpio_example：命中缓存（245652e62990b313a701c90d3d0af6cfcc5ace3e）
  ✅ gpio_example：RESULT: checks=9 fails=0（scenarios=7 · spec_checks=0）
```

⇒ ⇒ 10 项里 **9 项**有 `spec_checks ≥ 3`（⇒ ⇒ 断言**能证明符合规范**），
**唯独 `gpio_example` = 0** ⇒ ⇒ 它的 9 个 check 只能证明「产物没漂移」，**不能**证明「产物符合规范」。

### 3.2 选项 / 代价 / 建议

| 选项 | 内容 | 代价 |
|---|---|---|
| **A（建议）** | **补 spec 层断言**：把 `gpio_example` 的期望来源从「impl-contract」升级到「spec/impl 分型」⇒ ⇒ 判据来源指向仓内可引用的规范（如同 `axi4_lite` 那样「判据直接引仓内 AXI capability」）⇒ ⇒ 需在 `.rddf/checks/examples-rtl-expectations.md` 补 spec 侧条目 | 中（需找到 gpio 的规范依据；若仓内无 ⇒ ⇒ 需先有规范） |
| B | **接受现状**，但在期望登记文档里**显式标注**「`gpio_example` = impl-contract only ⇒ 只防漂移」⇒ ⇒ 让缺口可见而非隐含 | 零成本 ⇒ ⇒ 缺口**留存**但**可见** |
| C | 把 `gpio_example` 从首批名单里**移出**（首批 = 9 项）⇒ ⇒ 名单口径变更 ⇒ ⇒ 「首批 10 项」这个数会变 | 低 ⇒ ⇒ 但会削弱覆盖面 |

⇒ ⇒ **建议 A**；若仓内确实没有 gpio 的规范依据 ⇒ ⇒ 退 **B**（显式标注，**不**用 C 削覆盖面）。

---

## 4. R4 —— planner **上一轮的口头审计错误**（如实订正，**不重复立单**）

⚠️ **2026-11-13 的 planner 口头汇报**把「`ext_clk` 是死端口」列为「本次审出的新问题」
⇒ ⇒ **有误**：该条**已于 2026-11-12 09:51 立单** ⇒
`.rddf/checks/decision-request-rv32i-ext-clock-dead-port-2026-11.md`（**状态：待裁决** · 建议 A2 = 删端口或注释说明 · 零行为变更）
⇒ ⇒ **本单不重复立单**，此处只做**指针**，避免两份真相源。

⇒ ⇒ **同源的另外两条**（planner 上一轮亦列为「新发现」）**同样已在册**：
| 上一轮口头列为「新发现」 | 实际已在册于 |
|---|---|
| `ext_clk` 死端口 | `decision-request-rv32i-ext-clock-dead-port-2026-11.md`（2026-11-12 09:51） |
| 端口命名的规范追加 + 残差登记 | `decision-request-port-naming-parity-2026-11.md`（2026-11-12 11:43 · R1-a/R1-b/R2） |
| 命名采用率 0（1484 全豁免） | `.rddf/checks/port-naming-registry.md` §「为什么不能一次全改」+ `rtl-track-correction-list` 的 C1-b + `phase-6-rtl-productization.md` §4「N2 补遗 · 首批端口消化（32 端口）」 |

⚠️ **教训（如实登记）**：planner 在做「独立复核」前 **MUST 先扫 `.rddf/checks/decision-request-*.md`
与 `.rddf/checks/*-correction-list-*.md`** ⇒ ⇒ 本次是**先做审计、后查台账** ⇒ ⇒ 顺序错了 ⇒ ⇒
⇒ **本单的 §1–§3 是补扫后的净新增**（逐条已核对既有 8 份 decision-request + 订正清单 ⇒ **零重叠**）。

⚠️ **本单是 2026-11-13 审计时点的快照** —— 同日 R17.2（首批端口消化）落地后，
**命名棘轮基线由 1484 变为 1452**（`scripts/verify_port_naming.sh` 的 `EXPECTED_EXEMPT_PORTS`）
⇒ ⇒ **§5 底账里「棘轮锚点 1484」是审计时点的值** ⇒ ⇒ **当前值 = 1452**
⇒ ⇒ ⚠️ 但**本单 §1–§4 的结论均不受影响**（R1 讲端口面 · R2 讲 F6 判据 · R3 讲 gpio 断言强度 · §4 讲立单纪律）
⇒ ⇒ ⇒ **R1 的「扁平顶层暴露 80 个内部 `ch_out`」也不受影响**（改名不改端口面）。

---

## 5. 复核底账（2026-11-13 实测 · 供复核者复算）

| 项 | 命令 | 结果 |
|---|---|---|
| 扁平产物护栏 | `bash scripts/verify_flat_rtl_emit.sh` | **PASS**（F1·F2·F3·F4·F5·F8 · **F6 = 不适用**） |
| 命名棘轮护栏 | `bash scripts/verify_port_naming.sh` | **PASS**（R1 逐端口双向 · R2 锚点 1484） |
| examples 独立仿真护栏 | `bash scripts/verify_examples_rtl_sim.sh` | **PASS**（10/10 · **gpio 标 impl-contract only**） |
| 层次化护栏 | `bash scripts/verify_hierarchical_rtl.sh` | **PASS**（R1–R8） |
| 计数 | `ctest -N` | **263**（与锚点一致） |
| 端口面 | `build/examples/riscv-mini/emit_rtl_rv32i_soc --out /tmp/x` | `.v` **114** 端口（7 in + 107 out）· manifest **191** · `is_top_io=true` **32** · **未驱动且非 top_io = 0** |
| 具名端口采用率 | `grep -rhoE 'ch_(in\|out)<[^;]*>[[:space:]]*[A-Za-z_][A-Za-z0-9_]*[[:space:]]*\\{"' include/ examples/ tools/` | **5 处，全部在 `tools/cpphdl-emit-rtl/main.cpp`**（工具自测载体）⇒ ⇒ **产品组件零采用**（**已在册**，见 §4） |

---

## 6. 建议汇总

| # | 建议 | 类型 | 成本 |
|---|---|---|---|
| **R1** | 端口面残留**登记为已知残留**（台账 + 登记表）⇒ ⇒ **不推翻任何已裁决内容** | 登记 | 极低 |
| **R2** | ✅ **已闭（2026-11-13）** ⇒ ⇒ 护栏的 F6 从「硬编码 `note`」改为**真检查**（**元判据**：承载判据的测试 / 它的 CTest 标签 / 跑它的 CI job 三者俱在）⇒ ⇒ **AC 的内容判据已由既有 ctest 承载**（`tests/test_verilator_backend.cpp` 的 `port_access_snapshot()` 用例）⇒ ⇒ ⚠️ **原「逐字节比对」形态恒绿**（两者出自**同一个** `verilogwriter`）⇒ ⇒ **不可用** ⇒ ⇒ 自检 M6（副本投毒 ⇒ 必红）已加 | 实施 | 低 |
| **R3** | ✅ **早已闭（2026-11-12 · D1 裁决）** —— 期望登记的 gpio 段**已显式标注**「`spec_checks` **保持 0**（只判 impl-contract）」⇒ ⇒ **不是待裁决项** ⇒ ⇒ ⚠️ planner 本轮差点把它当新发现（同一纪律第二次生效）⇒ ⇒ 无需再改产品 | 实施 | 零 |
| **R4** | （**无需动作**）planner 上一轮口头错误的订正已记录 | 登记 | 零 |
