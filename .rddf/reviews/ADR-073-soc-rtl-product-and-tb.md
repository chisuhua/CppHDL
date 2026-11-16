# ADR-073 审稿记录 —— SoC 层的 RTL 产物形态与 SoC 层 TB 的交付契约

- **对象**: `docs/adr/ADR-073-soc-rtl-product-and-tb.md`（240 行）+ `docs/architecture/2026-11-16-soc-rtl-product-and-tb.md`（236 行）
- **档位**: `arch`
- **轮次**: oracle ×1 → critic ×2（第 2 轮 = 复审）
- **结论**: **Accepted**（oracle 第 1 轮 REJECT ⇒ 3 BLOCKER + 6 SHOULD_FIX 全闭 ⇒ critic 第 1 轮 REJECT ⇒ 2 BLOCKER + 3 SHOULD_FIX + 3 NIT 全闭 ⇒ critic 第 2 轮（复审）**APPROVE** · 0 BLOCKER · `mergeable: yes`）
- **日期**: 2026-11-16

---

## 1. oracle 第 1 轮（对抗式复核 · 会怎么坏）—— **REJECT**（3 BLOCKER + 6 SHOULD_FIX）

### 1.1 BLOCKER（3 条 · 全闭）

| # | BLOCKER | 证据 | 闭合位置 |
|---|---|---|---|
| **B1** | **未登记「SoC 有 22（后由 critic 复算为 **30**）个未驱动后代 `ch_in`」** ⇒ ⇒ 它使 **D1 后 SoC 产物发射 fail-closed**（ADR-072 D1-f：后代悬空 ⇒ `ok=false`）⇒ ⇒ 且 **T-2 的 `uart_tx` 断言不可达**（uart 不在总线上）⇒ ⇒ 这是 **J-1…J-5 全部判据的隐藏前置** | `examples/axi4/axi_soc_example.cpp`（`grep 'uart\.io()'` 全文件仅 3 命中：`:258`/`:259`/`:264`）· `include/axi4/peripherals/axi_uart.h:54-76`（11 个 AXI 从机 `ch_in`）· `include/axi4/axi4_lite_matrix.h:66-88`（`m1_*` 11 个）· ⇒ ⇒ **解法已有**：tie-off 惯用法 = `examples/axi4/phase3a_complete.cpp:259-302` | 新增 **事实 ⑤** + **D2**（SoC 可发射性前置：所有后代 `ch_in` MUST 有驱动源 = 接通 或 tie-off）+ **§3 选项 ⑧**（被拒）+ 架构主题文档 **G-3′** + **§2.1′** |
| **B2** | **D3/V2 对「冒泡端口」的归因错误** ⇒ ⇒ 层次化产物把**成层子树的 `ch_out` 移出顶层** ⇒ ⇒ 成层 IP 的 `ch_out` **从不冒泡** ⇒ ⇒ 唯一冒泡源是**未驱动后代 `ch_in`** ⇒ ⇒ 「去掉一个 `ch_layer`」这个负向对照**恒绿** ⇒ ⇒ **V2 零判别力** | `openspec/specs/rtl-hierarchical-output/spec.md:324-325`（排他句）+ ADR-073 自身的代价 ④ | **D4** 改写（过渡期 = 先按 D2 消掉未驱动输入 ⇒ 顶层自然收敛到 14 ⇒ **不需要 sink**）+ **V2 负向对照**改为「**恢复一根未驱动输入** ⇒ 必红」 |
| **B3** | **「SoC 没有 `main()` 形态入口」不成立**（`axi_soc_example.cpp:283` 有 `int main()`；`examples/CMakeLists.txt:69` 的 tracked 集合**含**它）⇒ ⇒ 且 **`REGISTERED_TARGETS` 锚点冲突未裁决**（「首批 10」由谁守）⇒ ⇒ 属**把未决写成已决** | `examples/axi4/axi_soc_example.cpp:283` · `examples/CMakeLists.txt:69` · `.rddf/checks/examples-rtl-sim-targets.tsv:24` · `scripts/verify_examples_rtl_sim.sh:92-99` | **D8** 改写（删错误理由；**锚点按 kind 分桶**的**裁决**：`REGISTERED_TARGETS: 10` 守 `kind ∈ {target, dut}` + **新增** `REGISTERED_SOC: 1` 守 SoC 数 + 护栏 ⓪ 按 kind 分桶）⇒ ⇒ 「首批 10」**仍有人守** |

### 1.2 SHOULD_FIX（6 条 · 全闭）

| # | finding | 闭合位置 |
|---|---|---|
| S1 | **根自有端口数 = 12，不是 13** ⇒ ⇒ 框架 2 + 12 = **14**；且 `timer_irq` 是**零消费死端口** | 全文 13 → 12 / 15 → 14；**事实 ⑭** 登记 `timer_irq`；**V11** 残留登记 |
| S2 | 架构主题文档 §1.1 的 **130 / 68 是 RV32I 的数字** ⇒ ⇒ 不是 SoC 的 | 改为 SoC 自己的数（**44 / 14**）+ 明写「130/68 是 RV32I 的」 |
| S3 | **D8 理由③（`ch_mem` 跨模块禁令）过强** ⇒ ⇒ ADR-066 v1.2 §2.7 决策 ⑦ 已把禁令**收窄**为「mem **本体**跨 module」；「仅实参视线跨层」是**已支持**的形态 | **事实 ⑬** + **D9** 删理由③（只留「无 BFM」+「顶层展平策略未定义」两条）+ 架构主题文档 §4 ⑥ 同步订正 |
| S4 | **V9 只改登记、不改漂移源头** ⇒ ⇒ 「+ Timer」的源头 = `axi_soc_example.cpp:4` 的源码头注释 + `:24` 的**零使用** include | **V10** 扩到「源头 + 登记」两处 |
| S5 | **J-4 的观测集与激励设计未落**（`no-varying-signal` 豁免风险） | **D7 的 J-4** 写死观测集 = {`rdata`, `done`, `gpio_out`, `uart_tx`} + 激励 **≥4 拍且值不同** + 「≥2 个随激励变化」 |
| S6 | 「`kind=soc` 的行没有 `target`」错 ⇒ ⇒ `kind=dut` 的行本就填自定标识 | **D8** 明确「`target` 列填 `axi_soc_example`」 |

---

## 2. critic 第 1 轮（arch 档 · 冲突审稿）—— **REJECT**（2 BLOCKER）

### 2.1 BLOCKER（2 条 · 全闭）

| # | BLOCKER | 内容 | 闭合位置 |
|---|---|---|---|
| **B1** | **第三类（配套文档自相矛盾）** —— 配套架构主题文档仍有 **6 处 "13"**（§1.3 / §2.2 两处 / §2.4 J-1 / §3 第 4 项 / §4 ⑤），且同篇 §1.1 写「顶层 14」⇒ ⇒ **J-1 验收值在两份配套文档间 = 14 vs 15** ⇒ ⇒ builder 照 §3 写 TB 会写出 13 根线 | oracle S1 **只修进了 ADR-073，主题文档未同步** ⇒ ⇒ 根因未闭合 | 主题文档 **6 处 13 → 12**（J-1 = 框架 2 + 12 = **14**）⇒ ⇒ 与 §1.1 对齐 |
| **B2** | **兜底第 ① 类（事实错误向下游传染）** —— 「22 个」实为 **30**：① `matrix.m1_*` 11 ② `uart` AXI 从机输入 11 ③ **`matrix.s1_*` 的 8 个响应侧 `ch_in`**（整个文件**零** `s1_*` 连线 —— gpio 占 `s0_*`）⇒ ⇒ 「D1 前 = 14 + 22 = 36」实为 **44**；⇒ ⇒ **§2.4 D4「先 tie-off 掉那 22 个 ⇒ 顶层收敛到 14」照字面执行漏掉 8 个 ⇒ ⇒ J-1 必红**；⇒ ⇒ V3 应为 30；⇒ ⇒ 后果①「~25 行」实为 **30 行** | 逐端口复算：11 + 11 + 8 = **30**；2 + 12 + 30 = **44** | **事实 ⑤**（拆三组 + 列全名 + 「11+11+8=30」+「零 `s1_*` 连线」）· **事实 ⑰**（44）· **§2.1 表**（30 冒泡 = 44；前提 D10 → **D2**）· **§2.2 D2 处置表**（3 行 + 计数 + 「19 根同时消掉 uart 的 11 与 s1 的 8 ⇒ 11+19=30 全覆盖」）· **§2.4 D4**（30）· **§4.2 ①**（~30 行）· **V3**（30 + 三组）· **选项 ⑧**（30）· 主题文档 §1.1 / G-3′ / §2.2 表 / §2.4 J-1 / §3 第 4 项 / §4 ⑤ 同步 |

### 2.2 SHOULD_FIX（3 条 · 全闭）+ NIT（3 条 · 全闭）

| # | finding | 闭合位置 |
|---|---|---|
| S1 | 主题文档 §2.2「⇒ **example 源码零改动**」与 §3 第 1 项 / ADR-073 后果 ① 矛盾 | 改为「**发射入口零额外副本**（无需搬组件定义）⇒ ⇒ **但 D2 的连线改造仍落在 example 源码**（~30 行）⇒ ⇒ **MUST NOT** 读成「example 不用动」」 |
| S2 | 排除表行的处置未裁决（SoC TB 交付后移除还是保留）⇒ ⇒ 与 tsv 新增 `kind=soc` 行并存 = **双重登记** | **D8** 增补：「SoC TB 交付后该行 MUST **移除**（理由 =「缺 TB」⇒ ⇒ 交付后理由消失）⇒ ⇒ **禁**保留 ⇒ ⇒ 该行的「Timer」漂移随之消失 ⇒ ⇒ **但源码头注释 + 零使用 include 仍 MUST 单独订正**（V10）」 |
| S3 | 选项 ⑨（缩到 2 IP）被拒后无对应决策指针 | 选项 ⑨ 末尾增补「**承接的决策 = D1 + J-2 / V1**（module 数 == 成层 IP 数 + 1 ⇒ ⇒ 缩到 2 IP ⇒ ⇒ 期望 module 数变 3 ⇒ ⇒ 判据随之降级）」 |
| N1 | 「tie-off ⇒ 不是悬空」的链条只引了**发射侧**（`codegen_verilog.cpp:432-441`）⇒ ⇒ 缺**分析侧**判据键 | **D2** 的「tie-off 的机理」段补 `src/hierarchy_analysis.cpp:404-415`（**判据键 = `num_srcs() == 0`**）⇒ ⇒ 两侧都引（复审轮 NIT 再补主题文档侧） |
| N2 | D8 的「target 值可机械核对」表述过强（护栏**只**做行数守恒，**无** target 列存在性比对） | **D8** 补「⚠️ **该约束目前只是「约定」**」+ 指出机械比对在 `verify_example_test_wiring.sh:151-156` 且**不读** kind 列 |
| N3 | 焦点 3（J-4 的语义）—— critic 已核实 **D6 无需改**（A 侧 = ctx 自生成的扁平产物；比对**只经** `inputs`/`observations` 落根端口 ⇒ ⇒ 端口面差异**结构性不影响**判定） | **不报 finding** ⇒ ⇒ **事实 ⑪** 已写该链条 ⇒ ⇒ 维持 |

### 2.3 critic 焦点 1 / 2 的核实结论（**结论成立**，仅 NIT）

- **焦点 1（D1-f 会不会仍判 tie-off 为悬空）**：**不成立** —— `src/hierarchy_analysis.cpp:404-415` 的悬空判据键 = `num_srcs() == 0`；tie-off 置 driver ⇒ `num_srcs() > 0` ⇒ ⇒ **不进 `pending_bubbles`** ⇒ ⇒ **D2 的推理链成立**。
- **焦点 2（D7 锚点分桶冲突）**：**不成立** —— 护栏 `verify_examples_rtl_sim.sh` **全脚本无** `CH_TRACKED_EXAMPLE_TARGETS` 机械比对（该比对在 `verify_example_test_wiring.sh:151-156`，且**不读** tsv 的 kind 列）⇒ ⇒ `kind=soc` 行填 `axi_soc_example` **不会误判**任何现有检查；⇒ ⇒ ADR-072 D5 的「首批 10 = 9 target + 1 dut」与 D8 新口径在现 tsv（9+1）上**逐字一致**。

---

## 3. critic 第 2 轮（**复审**）—— **APPROVE · 0 BLOCKER**

- **BLOCKER-1 闭合** ✅ —— 主题文档 6 处逐处复核：§1.3（`AxiSocTop = 12 个`）· §2.2 表两处（`框架 2 + 根自有 12 + 30 冒泡 = 44` / `框架 2 + 根自有 12 = 14（D2 后）`）· §2.4 J-1（`框架 2 + 根自有 12 = 14`）· §3 第 4 项（`14 个顶层端口（框架 2 + 根自有 12）`）· §4 ⑤（`框架 2 + 根自有 12 = 14`）· §1.1（`顶层 44` / `顶层 14`）⇒ ⇒ **全篇 14 一致**；独立复算根端口 = 8 `ch_in` + 4 `ch_out` = **12** ⇒ 框架 2 ⇒ **14** ⇒ ⇒ 与 ADR-073 事实 ②⑰ / D4 / V2 一致。
- **BLOCKER-2 闭合** ✅ —— 两份文档 grep `\b22\b` / `\b36\b` / `~25` **零命中**；30 的三组独立复算一致（`m1_*` 11 · `s1_*` 响应侧 8 · `uart` AXI 11 ⇒ 文件总 `ch_in` 13 里 `uart_rx`/`rst_n` 已接）；**零 `s1_*`/`m1_*` 连线**（grep 确认）；D2 处置表的接线数学 = 11 + (11 + 8) = **30 全覆盖**，且**方向/名对逐字核验通过**（s1 请求侧 11 out ↔ uart 从机 11 in；uart 响应 8 out ↔ s1 响应 8 in）。
- **本轮增量无新 BLOCKER** —— 修正引入的全部数字（30 / 44 / 14 / 19 / 11 / 8）经独立复算一致。
- **2 条 NIT（可选润色，已顺手闭合）**：① tie-off 机理段补分析侧判据键 `src/hierarchy_analysis.cpp:404-415（num_srcs()）` ② D8 补「该约束目前只是约定」。

---

## 4. 逐条应答汇总

| 来源 | # | finding | 处置 |
|---|---|---|---|
| oracle | B1 | 22（后复算 30）个未驱动后代 `ch_in` 未登记 | ✅ **已修** —— 事实 ⑤ + **D2**（新增决策）+ 选项 ⑧ + 主题文档 G-3′/§2.1′ |
| oracle | B2 | 冒泡归因错误 ⇒ V2 零判别力 | ✅ **已修** —— D4 + V2 负向对照改为「恢复一根未驱动输入」 |
| oracle | B3 | 「SoC 无 main()」错 + 锚点冲突未裁决 | ✅ **已修** —— D8 改写 + 锚点按 kind 分桶的**裁决** |
| oracle | S1 | 端口 13 → 12；`timer_irq` 死端口 | ✅ **已修** |
| oracle | S2 | 130/68 是 RV32I 的数字 | ✅ **已修** —— 主题文档 → 44 / 14 |
| oracle | S3 | D8 理由③（mem 禁令）过强 | ✅ **已修** —— 事实 ⑬ + D9 删理由③ |
| oracle | S4 | V9 只改登记不改源头 | ✅ **已修** —— V10 扩到源头 |
| oracle | S5 | J-4 观测集/激励未落 | ✅ **已修** —— D7 的 J-4 |
| oracle | S6 | `kind=soc` 行该填 target | ✅ **已修** —— D8 |
| critic | B1 | 主题文档 6 处 "13" 未同步 | ✅ **已修** —— 6 处 13 → 12 |
| critic | B2 | 22 → 30；36 → 44；D4/V3/后果① 连带 | ✅ **已修** —— 7 处 + 主题文档 6 处 |
| critic | S1 | 「example 源码零改动」与 D2 矛盾 | ✅ **已修** |
| critic | S2 | 排除表行处置未裁决 | ✅ **已修** —— D8 |
| critic | S3 | 选项 ⑨ 无承接指针 | ✅ **已修** |
| critic | N1 | tie-off 机理缺分析侧判据键 | ✅ **已修**（复审轮再补主题文档侧） |
| critic | N2 | 「target 可机械核对」过强 | ✅ **已修** |
| critic | N3 | J-4 语义 | ⛔ **不适用**（critic 自身核实 D6 无需改） |

⇒ ⇒ **全部 finding 已逐条应答并处置** ⇒ ⇒ **无「列出但不改」的 finding** ⇒ ⇒ **无「未应答的 BLOCKER」**。

---

## 5. 本 ADR 的完成标志核对（`arch` 档）

| # | 标志 | 状态 |
|---|---|---|
| ① | 四要素齐（元数据 / 选项对比表 ≥ 3 行 / 独立「后果·代价」章 / 前瞻性 Verification 章） | ✅（**9 行**对比表 · §4 · §5 的 **V1–V12** 全未勾） |
| ② | oracle + critic 双审；critic 报过的 BLOCKER 全闭并**经复审确认** | ✅（oracle ×1 REJECT + critic ×2 · 复审 **APPROVE** · 0 BLOCKER · `mergeable: yes`） |
| ③ | Status 已转已决 | ✅ **Accepted**（2026-11-16） |
| ④ | 「关联 change」已落库或已登记 | ✅ `backlog: soc-rtl-product-and-tb-2026-11`（**尚未立项** ⇒ ⇒ 立项后由 planner 回填指针 + AC 映射） |
| ⑤ | 审稿报告已落 `.rddf/reviews/` | ✅ 本文件 |

⚠️ **自指审查声明**：本 ADR 的 **D2** 与 **ADR-072 D1-f** 耦合（SoC 侧 tie-off 是 D1-f 的逃生舱）⇒ ⇒ **ADR-072 的在途 delta 与本 ADR 的 D2 MUST 同批协调**（D10 的顺序硬约束）⇒ ⇒ 该协调**不是**本轮审查能替代的 ⇒ ⇒ 落地时以**两个 change 的实施顺序**为准。
