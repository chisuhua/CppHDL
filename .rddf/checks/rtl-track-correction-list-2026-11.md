---
id: rtl-track-correction-list-2026-11
日期: 2026-11-12
提出者: planner（对 B0/B1/B2 落地后的 7 份 RTL 轨草案做只读审查时发现）
状态: ✅ **已审查通过**（oracle 第 1 轮 13 条全采纳 · critic 第 1 轮 REJECT → 第 2 轮 **APPROVE**）· **修订已执行**（见 §9）· ⚠️ **2026-11-12 复核轮**：§9.2 的「全绿」有 **5 处不实**（见 §9.5）⇒ ⇒ 已订正
审稿记录:
  - oracle 第 1 轮（2026-11-12）⇒ 13 条建议，**全部采纳并已并入本文**
      （含 1 处事实错误：CLI **无** `--top` · 5 处覆盖缺口 · 3 条脆弱判据）
  - critic 第 1 轮 ⇒ 见 §8
关联:
  - .rddf/improvements/{rtl-emit-product-tooling,port-naming-ratchet,firmware-load-chain,memory-timing-contract,rv32i-dead-component-consolidation,rv32i-isa-gap-closure,examples-rtl-sim-coverage}-2026-11.md
  - .rddf/roadmap/phase-6-rtl-productization.md（批次表 · §0.0 状态词表 · §4 独立立项清单）
  - .rddf/roadmap/phases/phase-6.md（任务状态唯一真相 · B0–B4 状态格 · 🚪 入口门）
  - .rddf/roadmap/phase-6-next-steps.md（R0–R11）
  - .rddf/reviews/b0-batch-verify-2026-11.md · b1-batch-verify-2026-11.md · phase-6-b2-midterm-review-2026-11.md
  - AGENTS.md（CI 锚点段）
  - improvement-suggestions.md（索引表）
---

# 订正清单：RTL 轨 7 份草案的**状态 / 簿记漂移**（5 条）

> ⚠️ **本清单不含任何实现缺陷**。5 条全部是「**记录与现实脱节**」：
> 实现本体（B0/B1/B2）已实测绿（`ctest` **263/263** · `verify_flat_rtl_emit.sh` /
> `verify_port_naming.sh` / `verify_examples_rtl_sim.sh` / `verify_hierarchical_rtl.sh` /
> `verify_test_counts.sh` / `verify_public_api_visibility.sh` / `verify_roadmap_fragments.sh` /
> `verify_ledger_evidence.sh` 全部 PASS · `update_test_count_anchors.sh --check` 5/5 一致）。
> ⇒ ⇒ 漂移的后果**不是**「功能坏了」，而是「**读文档的人（含下一个会话的 builder）会得出错误结论**」。
>
> ⚠️ **本清单是 planner 域的记录订正** ⇒ ⇒ **MUST NOT** 修改任何已签批 ADR 的决策内容、
> **MUST NOT** 改任何 `src/**` / `include/**` / `tests/**`。

---

## 0. 总览

| # | 漂移 | 严重度 | 影响谁 | 归属 |
|---|---|---|---|---|
| **1** | 7 份草案的 `- **Status**` 行全部仍是 `📝 draft（待用户拍板…）`，与 frontmatter `状态:`、`进度:`、索引表**四方不一致** | 🔴 **高** | 下一个会话的 builder 会以为「还没批准」 | planner |
| **2** | 7 份草案的 AC 复选框共 **141** 条（137 行 + 4 条 `<br>` 内联），勾选 **3** 条（且 3 条都是「撤回式勾选」）⇒ ⇒ **已验收的成果在 AC 上看不出来** | 🔴 **高** | 无法据此判断「哪些真做完、哪些没开工」 | planner（证据来自 verifier 报告） |
| **3** | 根 `AGENTS.md` 的 `CTEST_TOTAL` 注释块**重复段 + 算术缺口**（解释到 262，锚点是 263） | 🟠 **中** | 下次改计数的人会算错 / 漏记 `test_rtl_emit_contract` | planner |
| **4** | `port-naming-ratchet` 仍带一条**不可满足**的 AC（`kRejectDuplicates` 下抛异常），且**同文件 4 处**需改（3 处互相矛盾 + 1 处不可满足） | 🔴 **高** | 该 AC **永远无法勾选** ⇒ 验收永远无法完成 | planner |
| **5** | `examples-rtl-sim-coverage` 的 **14 处**仍写旧口径（**5 处文案** + **E4′ 表 9 行 `⬜ 待补`**），实际 **10/10 verified** | 🟠 **中** | 低估已完成度 ⇒ 重复劳动 / 错误排期 | planner |

---

## 1. 草案 `Status` 四方不一致

### 1.1 现象

每份草案的**正文** `- **Status**` 行全部为 `📝 **draft**（待用户拍板 ⇒ 拍板后转 builder 流程）`，
但 **frontmatter `状态:`**、**frontmatter `进度:`** 与 **`improvement-suggestions.md` 的状态列**
各自不同。

### 1.2 证据（2026-11-12 实测）

| 草案 | frontmatter `状态:` | 正文 `- **Status**` | 索引表状态列 |
|---|---|---|---|
| `rtl-emit-product-tooling` | **`implemented`** | `📝 draft` | `📝 draft（metis 第 1 轮已处置…）` |
| `port-naming-ratchet` | **`implemented`** | `📝 draft` | `📝 draft（metis…）` |
| `firmware-load-chain` | **`approved`** | `📝 draft` | `📝 draft（metis…）` |
| `memory-timing-contract` | `draft` | `📝 draft` | `📝 draft（metis…）` |
| `rv32i-dead-component-consolidation` | **`in-progress`** | `📝 draft` | `📝 draft（metis…）` |
| `rv32i-isa-gap-closure` | `draft` | `📝 draft` | `📝 draft（metis…）` |
| `examples-rtl-sim-coverage` | **`implemented`** | `📝 draft` | `📝 draft（metis…）` |

- 复核命令：`grep -n '^状态:' …` / `grep -n '^进度:' …` / `grep -m1 -E '^- \*\*Status\*\*' …` /
  `grep <name> improvement-suggestions.md`。
- **5 份**存在「frontmatter ≠ 正文」（I1 / I2 / I3 / I5 / I7）；**2 份**一致（I4 / I6 —— 都是未开工的 `draft`）。
- ⚠️ **`进度:` 是第 4 个状态通道，且维护得最好**（7/7 都有，含「B3 未开工」「T2 待 architect 裁决」
  「前置门 = 公开面登记未做 ⇒ 不得开工」等**门状态**）⇒ ⇒ 本清单**必须**把它纳入一致性判据，
  否则修完还是一处漂移。

### 1.3 根因

- 批次表 `phase-6-rtl-productization.md` 的 **§0.0 状态词表**于 **2026-11-12** 才新增 `in-progress`，
  并**只回填了 frontmatter 的 `状态:` 与 `进度:`**（机器可读 / 明细通道）。
- 正文 `- **Status**` 行是 **2026-11-10 起草时**写的，回填时没人扫它。
- `improvement-suggestions.md` 的状态列同样停留在起草时的措辞（只被批量改成「metis 第 1 轮已处置」）。

### 1.4 修正方案

1. **唯一真相 = frontmatter `状态:`**（依据批次表 §0.0 词表）⇒ ⇒ 正文 `- **Status**` 与索引表状态列
   **MUST 与它同档**，并附「批次 + 日期」指针。
2. **`进度:` 不被 `状态:` 覆盖**（它是明细通道）⇒ ⇒ 但 **MUST NOT 与 `状态:` 矛盾**，
   且 MUST 在缺口存在时**指向批次表 §4 的登记行**（见 1-b）。
3. 三处文案统一为如下形态（示例）：
   - `implemented` → `✅ **implemented**（B1 已验收 · 2026-11-11）`
   - `approved` → `📋 **approved**（已裁决 · 未开工 ⇒ 归 B3）`
   - `in-progress` → `🟡 **in-progress**（B0 已验收 · C1–C4 未开工 ⇒ 归 B4）`
   - `draft` → `📝 **draft**（未开工 ⇒ 归 B3 / B4）`
4. **⚠️ 1-b（需裁决，不是纯文案）**：`port-naming-ratchet` 的 frontmatter 写 `implemented`
   （`进度:` 写「N1–N6 全部落地」），但本项自己的 AC 明文写着
   「⚠️ **本项 MUST NOT** 声称「存量已全部带名」⇒ ⇒ 豁免表非空时该项**未完成**」
   —— 实测 `.rddf/checks/port-naming-registry.md` 豁免 **1484 / 1484**（`verify_port_naming.sh` R1），
   且「首批消化对象 = `examples/riscv-mini/src/rv32i_pipeline_top.h`」的**32 个端口仍全无名**
   （`ch_in`/`ch_out` 声明处无显式名）。
   ⇒ ⇒ **裁决 = (b)**（保持 `implemented` + 把「首批消化」登记为独立立项），**但必须同时满足**：
   - **(b)-①** 改写 AC 里那句自反的措辞（`examples-rtl-sim-coverage` 之外的
     `port-naming-ratchet-2026-11.md:179`）为**登记式**：
     「豁免表非空时「存量全带名」的**声称**不成立 ⇒ ⇒ 缺口 MUST 登记于批次表 §4（已登记：§4 第 X 行）」
     ⇒ ⇒ **不得保留**「该项未完成」这种与 `implemented` 字面互斥的表述（否则把「三方不一致」
     换成了「草案内部不一致」）。
   - **(b)-②** `进度:` 追加一节指向 §4 登记行（含对象文件 + 端口数 + 触发条件），
     **照 N5 第 2 步的粒度**（`phase-6-rtl-productization.md:174` 是既有先例）。
   - **(b)-③** 批次表 §4「独立立项」清单**新增一行**（照 N5 行粒度）。
   - **(b)-④（critic 第 1 轮 BLOCKER 的连带项）** 批次表 **§0.0 状态词表**的 `implemented`
     定义文本 MUST **扩充**：现状只写「**不可做**的部分已显式登记为独立立项」
     （`phase-6-rtl-productization.md:26`），而「首批消化 32 端口」是**可做但明确移交**的增量工作
     ⇒ ⇒ 若不改词表，修完 7 份草案后**批次表自己的规则行**会与新 §4 行构成新的自相矛盾。
     ⇒ ⇒ 改为：「本项全部**本轮承诺**的 work item 已落地；**未纳入本轮**的部分
     （**不可做** / **可做但显式移交**）MUST 已登记为独立立项（§4）」。
   ⇒ ⇒ **为什么不选 (a) `in-progress`**：与 B1 第 2 轮 APPROVE（`b1-batch-verify-2026-11.md`）
   和批次表 §B1「11/11」既成事实直接冲突，且会让 `in-progress` 先失去语义。
   ⇒ ⇒ **「implemented 是否失去约束力」的答案**：不会 —— §0.0 词表（`phase-6-rtl-productization.md:26`）
   本就写了「不可做的部分已**显式登记为独立立项**」，且 **N5 第 2 步已有完成同型的先例**
   （`进度:` + §4 登记，状态仍 `implemented`）。真正的风险是「登记即遗忘」⇒ ⇒ 由 (b)-②/(b)-③ 的
   **指针**兜住。

### 1.5 判据（可机械判定）

- **C1-a（四通道同档）**：对 7 份逐份比对 `^状态:` 的档位词、`^- \*\*Status\*\*` 的档位词、
  索引行的档位词 ⇒ ⇒ **0 处不一致**（`进度:` 不要求同档，见 C1-c）。
- **C1-b（缺口有落点）**：`port-naming-ratchet` 的「首批消化」缺口在**两处之一**有落地记录
  （frontmatter `状态:` = `in-progress` **或** 批次表 §4 有对应行）⇒ ⇒
  `grep -n "首批消化" .rddf/roadmap/phase-6-rtl-productization.md` 必须命中（在 §4 段内）。
- **C1-c（`进度:` 不与 `状态:` 矛盾）**：若 `状态:` = `implemented` 且 `进度:` 含「未开工 / 全部落地」
  之外的不确定词（`待` / `未` / `缺口`）⇒ ⇒ `进度:` MUST 含 `§4` 或 `独立立项` 字样。
  ⇒ ⇒ 反例：现状 `port-naming-ratchet-2026-11.md:8`（含「N5 第 2 步 = 独立立项」✅）
  但**缺**首批消化 ⇒ ⇒ 必须补。
- **C1-d（`进度:` 的 B3 门可见性）**：`memory-timing-contract` / `firmware-load-chain` 的 `进度:`
  必须含**不可开工的理由**（architect 裁决 / 公开面登记）⇒ ⇒ 因为 §7.3-Q5 的答案源就是它。

### 1.6 归属 / 依赖

- **归属**：planner。
- **依赖**：无（可独立先做）；但 1-b 的裁决结果会**决定项 2 里 I2 的勾选口径**。

---

## 2. AC 复选框未回填（已验收的成果在 AC 上看不出来）

### 2.1 现象

7 份草案的 `## Acceptance` 段共 **141** 条复选框（**按行计 137 条 + `<br>- [ ]` 内联 4 条**，
全部来自 `rv32i-isa-gap-closure`），其中**已勾选 3 条**，
且这 3 条**全部是「撤回式勾选」**（`- [x] ~~原文~~ 🔴 已撤回/已作废` ⇒ ⇒ 表达的是「这条判据不成立」，
**不是**「这条判据已满足」）。
⇒ ⇒ **没有任何一条「已实施并验收」的 AC 被勾选**。

### 2.2 证据（2026-11-12 实测）

计数口径：`grep -cE '^\s*- \[[ xX]\]'`（行）+ `grep -oE '<br>- \[[ xX]\]' | wc -l`（内联）。

| 草案 | 行内 | `<br>` 内联 | 合计 | 已勾选 | 已勾选的性质 |
|---|---:|---:|---:|---:|---|
| `rtl-emit-product-tooling` | 20 | 0 | 20 | 3 | 全部撤回式 |
| `port-naming-ratchet` | 17 | 0 | 17 | 0 | — |
| `firmware-load-chain` | 18 | 0 | 18 | 0 | — |
| `memory-timing-contract` | 17 | 0 | 17 | 0 | — |
| `rv32i-dead-component-consolidation` | 25 | 0 | 25 | 0 | — |
| `rv32i-isa-gap-closure` | 18 | **4** | **22** | 0 | — |
| `examples-rtl-sim-coverage` | 22 | 0 | 22 | 0 | — |
| **合计** | **137** | **4** | **141** | **3** | 3 / 3 撤回式 |

- 佐证（B1 验收报告已给出逐条结论，但**没有投影回草案**）：`.rddf/reviews/b1-batch-verify-2026-11.md`
  「✅ 通过（7 项）」表 + 「❌ 不通过（2 项）」段；返工后由同文件第 2 轮 APPROVE 段闭合。
- 佐证（B2）：`phase-6-rtl-productization.md` §B2 行已写 **「E4′ 已验证 = 10 / 待补期望源 = 0」**，
  而草案 `examples-rtl-sim-coverage` 的 AC 仍全空。

### 2.3 根因

- 验收报告落在 `.rddf/reviews/`，而 **verifier 角色边界禁止改 improvement**（
  `verifier` 只写报告，不改实现、不改草案）⇒ ⇒ 报告的结论**没有回填通道**。
- 逐条勾选需要「AC ↔ 证据」的映射，起草时未预留「证据指针」位置 ⇒ ⇒ 回填成本高 ⇒ ⇒ 一直没做。

### 2.4 修正方案

1. **逐条**处理 141 条（**含 `<br>` 内联的 4 条** ⇒ ⇒ 只按行 grep 会漏），分三档，**禁止一次性全勾**：
   | 档 | 判定 | 写法 |
   |---|---|---|
   | **已实施并验收** | 有验收报告 / 护栏 PASS / 实测证据 | `- [x] <原文> ⇒ 证据: <报告节 / 脚本 / 测试名>` |
   | **未开工** | 属 B3 / B4（`phase-6.md:75-76` 标 ⬜） | `- [ ] <原文> ⇒ 归属: B3 / B4`（**保持未勾**） |
   | **不可满足 / 已撤回** | 判据与事实冲突（已有 3 条先例） | `- [x] ~~原文~~ 🔴 <作废理由>`（沿用既有形态） |

   ⚠️ **第四种形态（不许误转）**：`rv32i-isa-gap-closure` 的 `:151/:154/:170` 等处是
   「**已按裁决改写、但 AC 本体仍是活的未勾判据**」（例：`:151` 尾部填 NOP 判据待 B4 验收）。
   ⇒ ⇒ 这类 MUST 保留为 `- [ ]` + `🔵 裁决注记: …`，**MUST NOT** 转成 `[x] ~~原文~~ 🔴`
   （转了就撞 C2-b 的「B4 checked == 0」）。
2. **每条 checked 的 AC MUST 带证据指针**（见 C2-a）⇒ ⇒ 无证据指针的勾选**不算回填**。
3. **⚠️ 2-a（必须处理「按原文与实现不符」的条目）**。**至少四处**，且**范围不止 AC**：
   - **① CLI 形态**（`rtl-emit-product-tooling`）：草案 `## What Changes` C1 的 `:109`
     （示例命令 `--top Rv32iPipelineTop --module-name rv32i_soc --strict`）、
     `:114`（`CH_EMITTABLE_COMPONENT` **注册表**）、`:130-131`（manifest 示例的
     `root_component` / `module_name`）**全部描述未实现的机制**。
     ⇒ ⇒ **实测形态**：CLI 选项只有 `--out` / `--manifest` / `--verilog` / `--no-verilog` /
     `--no-manifest` / `--emit-tb-stub` / `--tb-stub-module` / `--tb-stub-hint` /
     `--tb-clock-half-period` / `--emit-hygiene` / `--allow-diagnostics`（`src/rtl_emit/cli.cpp:30-74`）
     ⇒ ⇒ **无 `--top`、无 `--module-name`、无 `--strict`、无注册表**；
     设计由 **`DesignFactory` 回调**注入（`include/rtl_emit/cli.h:75,80` +
     `tools/cpphdl-emit-rtl/main.cpp:87`）。
     ⇒ ⇒ **AC 第 1 条 MUST 改为**：
     「`cpphdl-emit-rtl -o <dir> --manifest <file>`（工具自带 `Top` 设计）产出 `.v` + manifest；
     `.v` 过 `verilator --lint-only`；退出码 `0` / `1` / `2` 实测符合契约」+
     「RV32I 路径由 `examples/**` 各自的发射器承担（B2 已落地 **10** 个）」。
     ⇒ ⇒ **同时**把 `:109/:114/:130-131` 同步为实测形态（**不许**只改 AC 留正文漂移）。
   - **② F6 条**：AC 写「backend 产物 == 产品产物（逐字节）」，而护栏现值是
     **F6 = 不适用**（工具设计无寄存器 ⇒ backend 不产 `top.v`）⇒ ⇒ AC MUST **对齐护栏现值**
     （「不适用 + 解除条件：被测设计含寄存器时 MUST 恢复逐字节比对」），**不许**新造「条件式逐字节」。
   - **③ 测试数写死**：**范围 = 全文件的「当前态断言」**（`## Acceptance` + 正文 +
     `## Capabilities` / `## Impact` 表），**排除**文件末尾的
     `## metis 第 1 轮 gap 分析与处置（2026-11-10）` 段（那是**历史引用**，MUST 原样保留）。
     ⇒ ⇒ 实测当前态命中 **9 处**，逐处 MUST 改为**引用锚点**（「`CTEST_TOTAL` == `ctest -N`」）：

     | 文件 | 行 | 段 |
     |---|---:|---|
     | `rtl-emit-product-tooling` | **165** | `## What Changes` · C6 复位写法段末（「判别力：… 既有 257 个 ctest 全绿」） |
     | 同上 | **198** | `## Acceptance` · 不回归 |
     | 同上 | **226** | `## Impact` 爆炸半径表「既有 257 个 ctest」行 |
     | `port-naming-ratchet` | **115** | `## What Changes` · N1「零变化保证」 |
     | 同上 | **162** | `## Acceptance` · 框架层（括号内） |
     | 同上 | **183** | `## Acceptance` · 不回归 |
     | `memory-timing-contract` | **168** | `## Acceptance` · 不回归 |
     | `rv32i-isa-gap-closure` | **175** | `## Acceptance` · 不回归 |
     | `examples-rtl-sim-coverage` | **232** | `## Acceptance` · 不回归 |

     ⇒ ⇒ **关键**：只改 AC 不够（`:165/:226/:115` 在正文与 Impact 表里）⇒ ⇒
     否则 §7.2 的 C2-c 命令**必然仍红**（critic 第 1 轮 BLOCKER-2）。
   - **④ `port-naming-ratchet` 的不可满足 AC** ⇒ **见项 4**（单独一条，含同文件 4 处需改）。
4. **⚠️ 2-b（未勾项必须有归属）**：每条 `- [ ]` MUST 标明 `⇒ 归属: B3` / `B4` / `独立立项（§4 第 N 行）`
   ⇒ ⇒ 这是 §7.3-Q3 可答性的**承重件**（否则「没做完的那些条」等于没有信息）。

### 2.5 判据（可机械判定）

- **C2-a（checked 必有**指针形态**的证据）** —— **两个独立条件**（各自一行判定 · 撤回式条目**双重豁免**）：
  - **① 证据标记**：`- [x]` 行 MUST 含 `证据[:：]` **或** `证据（…）：`；
  - **② 可定位 token**：同行 MUST 含 ≥1 个 —— (a) 反引号内**含 `/`** 的路径 ∨
    (b) 反引号内**以已知扩展名结尾**的文件名（`.py|.sh|.cpp|.h|.md|.sv|.json|.yaml|.tsv`）∨
    (c) 反引号内的**命令片段**（`--<option>` 或 `ctest …`）⇒ ⇒ **不接受**「见报告」「见上文」这类无指向文本。
  - ⚠️ **2026-11-12 复核订正（判据自身不可满足）**：原正则 `- \[x\].*证据[:：]\s*[\w./#-]+`
    **对本清单自己规定的写法恒不命中** ⇒ 三重原因：
    ① `证据[:：]` 要求冒号**紧跟**「证据」⇒ 实际写法 `证据（**静态**）:` 不命中；
    ② `\s*` 之后要求路径首字符 ∈ `[\w./#-]` ⇒ 实际写法 `证据: \`path\`` 的**反引号**不命中；
    ③ 按**行** grep ⇒ 证据落在**续行**的条目必漏。
    ⇒ ⇒ 实测：原判据在 73 条 checked AC 上命中 **0** ⇒ ⇒ §9.2 的「73/73 合规」**不实**（见 §9.5）。
  - 路径类指针 MUST **存在性检查**（`test -e <path>` 或 `git grep -l <symbol>` 命中）。
  - ⚠️ **明示局限**：形式判据只能保「形似」，保不了「指针真的支撑结论」⇒ ⇒
    S6 增加**抽样复核**（随机 5 条 checked AC 人工核对证据强度）。
    **不得**把「C2-a 绿」当成「AC 回填可信」的充分条件。
  - 撤回式条目豁免（形态 = `~~…~~` + `🔴` + `撤回|作废|删除`）。
- **C2-b（未开工项不得被勾）**：B3（`firmware-load-chain` / `memory-timing-contract`）与
  B4（`rv32i-isa-gap-closure` 全部、`rv32i-dead-component-consolidation` 的 C1–C4）的 AC
  ⇒ ⇒ checked 数 == 0（**已被验收的 C5/C6/C7/C8/⑧ 除外**，它们归 B0）。
  ⚠️ **分区依据 MUST 写明**（否则半人工不可复核）⇒ ⇒ **按 work item 归属**分区，**不按小节标题**：
  - `rv32i-dead-component-consolidation`：`差分` 小节的 **首条**（「🔴 **SLTU 修复 MUST 自带判据**」）
    属 **B0 ⑧**（**可勾** · 证据 `tests/test_rv32i_alu.cpp`）⇒ ⇒ 该小节**其余 4 条** ⇒ **B4**（C1）；
    `接入` 小节**全部 6 条** ⇒ **B4**（C3/C4）；`退役` / `诚实化` / `不回归` / `悬空输入 tie-off` ⇒ **B0**（C5–C8，可勾）。
    ⇒ ⇒ 合计：**B4 = 10 条**（checked MUST == 0）· **B0 = 15 条**（可勾）。
  - `rv32i-isa-gap-closure`：**全部 22 条** ⇒ B4。
  - ⚠️ **2026-11-12 复核订正（判据自身不可满足）**：原表述「按**小节标题**分区 · `差分`/`接入` ⇒ C1–C4」
    把 `差分` 小节的**首条**（B0 ⑧）也划进 B4 ⇒ ⇒ 该小节 checked ≠ 0 ⇒ ⇒ 原判据**必然红**
    ⇒ ⇒ 与 §8.2 的 B-2 **同类**（「验收命令不可能通过」）⇒ ⇒ 改为上面的**按 work item 归属**分区。
- **C2-c（「当前态」不许写死测试数）**：**逐文件**跑
  ```bash
  awk '/^## metis 第 1 轮 gap 分析与处置/{exit} {print}' <file> \
    | grep -cE '257\s*个\s*ctest|ctest.{0,10}257|既有\s*\*{0,2}257'
  ```
  ⇒ ⇒ 命中数 MUST **== 0**。
  ⚠️ **一个已知盲区（critic 第 2 轮 NIT）**：该正则只钉数字 **257** ⇒ ⇒
  若把 9 处改写成**另一个写死数字**（如「263 个 ctest」）⇒ ⇒ 命令照样全零通过，
  而意图（MUST 改为 `CTEST_TOTAL` 引用锚点）被绕过 ⇒ ⇒
  「改成锚点引用」这一条**由 §2.4-3③ 的 MUST 指令 + C7-e 抽样复核承重**，不由 C2-c 判定。
  ⚠️ **两个约束缺一不可**：
  - **必须排除 metis 段**（`awk … exit`）—— 裸 `grep 257` 会误伤
    `memory-timing-contract:232` 的**行号引用** `hazard_unit.h:256-257`，
    以及 `firmware-load-chain:235` 等 metis 处置表里**对原文的引用**
    （critic 第 1 轮实测：不排除 metis 段则至少仍报 5 命中 ⇒ 命令必红）。
  - **范围必须覆盖正文与 Impact 表**（见 §2.4-3③ 的 9 行清单）—— 只改 AC 则 C2-c 仍红。
- **C2-d（勾选率合理）**：`implemented` 的 3 份（I1/I2/I7）与非 `implemented` 的 4 份相比，
  checked 数 MUST 不同（禁止「全部 0」或「全部满」两种退化形态）。
- **C2-e（未勾必有归属）**：每条 `- [ ]` 的**同行或紧随行**含 `归属[:：]\s*(B3|B4|独立立项)`
  ⇒ ⇒ 缺失数 == 0（`独立立项` 者 MUST 附 §4 行号）。

### 2.6 归属 / 依赖

- **归属**：planner（回填执行）· **证据来源** = verifier 报告 + 护栏输出（planner **不得**自行发明证据）。
- **依赖**：项 1（Status 先对齐，才知道哪些该勾）；项 4 / 项 5（那两条的 AC 会被改写，先改再勾）。

---

## 3. 根 `AGENTS.md` 的 `CTEST_TOTAL` 注释块：重复段 + 算术缺口

### 3.1 现象

`AGENTS.md` 的 `- CTEST_TOTAL:` 块内：

1. **「判别力实测」出现两次**（块内第 5 行与第 9 行，措辞不同、内容重复）。
2. **块内叙事只解释到 `262`**，而 **anchor 与实测都是 `263`**
   ⇒ ⇒ 缺 `test_rtl_emit_contract` 的 **+1**（B1 的 C5 判据落地时新增，`ctest` 编号 `Test #194`）。

### 3.2 证据（2026-11-12 实测）

```
$ awk '/^- CTEST_TOTAL/,/^<!-- CI anchor: .ctest -L/' AGENTS.md | grep -c "判别力实测"
2
$ awk '/^- CTEST_TOTAL/,/^<!-- CI anchor: .ctest -L/' AGENTS.md | grep -oE '\*\*[0-9]{3}\*\*'
**261**
**262**
$ grep -nE '^- CTEST_TOTAL:' AGENTS.md          → 452:- CTEST_TOTAL: 263
$ cd build && ctest -N | tail -1                 → Total Tests: 263
$ ctest -N | grep -E 'test_port_name|test_rtl_'
  189 test_port_name_named_ctor   190 test_port_name_parity
  191 test_rtl_manifest            192 test_rtl_emit_hygiene
  193 test_rtl_tb_stub             194 test_rtl_emit_contract     ← 未被注释块解释
```

⇒ ⇒ 6 个新测试（189–194）对应 **263 − 257 = +6**，而注释块只解释了 **+4**（→261）与 **+1**（→262）⇒ ⇒ **差 1**。

### 3.3 根因

`test_rtl_emit_contract` 是在 B1 的 C5 落地**之后**补的（它是 C5 的判据），
但锚点注释块的追加习惯是「按 commit 批」⇒ ⇒ 那一笔只跑了
`scripts/update_test_count_anchors.sh` 改了**数字**，**没补注释段**（脚本只动数字，不动散文）。

### 3.4 修正方案

1. **合并重复段**为**一段**逐条判别力记录（保留信息更全的那版措辞：M1/M2/M3 逐条 + 还原复绿）。
2. **补 +1 段**：`test_rtl_emit_contract`（B1 · C5 判据）⇒ ⇒ `263`。
3. **使块内数字链可核**：`257`（B1 前）→ `261`（+4）→ `262`（+1）→ `263`（+1），
   每一步 MUST **点名新增的 `test_*`**（这是结构性要求，不是枚举要求 —— 见 C3-b）。
4. ⚠️ **字节预算**：`AGENTS.md` 实测 **57,088 B**，`verify_agents_size.sh` 的 WARN = **57,344 B**
   ⇒ ⇒ **余量只有 256 B** ⇒ ⇒ 本项 MUST **净删字节**（合并两段刚好做到）⇒ ⇒
   **禁**「在末尾追加一段」的写法（会直接越 WARN 线）。
5. ⚠️ **遵守批次表 ★1 约束**「**锚点编辑权 = 最后合入者**」⇒ ⇒ 本次无在途批次 ⇒ planner 可改；
   改完 MUST 跑 `scripts/update_test_count_anchors.sh --check`（5/5）与
   `scripts/verify_test_counts.sh`（PASS）与 `scripts/verify_agents_size.sh`（**不得转 FAIL**）。

### 3.5 判据（可机械判定）

- **C3-a**：块内 `判别力实测` 出现次数 **== 1**。
- **C3-b（结构性，不是枚举）**：块内**每个** `**NNN**` 数字 MUST **在同一句内**
  点名**≥1 个 `test_*`**（同句 = 该数字与测试名之间无句号/换行段落边界）+ 块内数字**严格递增**
  ⇒ ⇒ **不**要求等于某个固定集合（改判据后，未来加测试**无需**回头改本条）。
  ⚠️ 「同句」是为了堵「段内随便塞一个测试名」的绕过（critic 第 1 轮 NIT）。
- **C3-c**：块内**最末一处**数字 == `- CTEST_TOTAL:` 的值 == `ctest -N` 的 `Total Tests`。
- **C3-d**：`scripts/verify_test_counts.sh` PASS ∧ `update_test_count_anchors.sh --check` 5/5 ✓。
- **C3-e（对齐护栏语义）**：`verify_agents_size.sh` **不得转 FAIL**（**WARN 只是告警**，
  见 `scripts/verify_agents_size.sh:17`）⇒ ⇒ 且本项修改后**字节数 MUST 减少**
  （`stat -c %s AGENTS.md` 前后比较）。

### 3.6 归属 / 依赖

- **归属**：planner（文档）。
- **依赖**：无。⚠️ 但若项 2 的修订**新增/删除测试**（本清单不新增测试 ⇒ 预计不变），
  则 MUST 在本项之后重跑锚点检查。

---

## 4. `port-naming-ratchet` 的不可满足 AC（含同文件 4 处需改）

### 4.1 现象

草案 `## Acceptance` →「框架层」段仍有一条：

```
- [ ] 同名端口在 `kRejectDuplicates` 策略下**创建点抛异常**
     （复用 `core-ctx-node-name-uniqueness`；默认策略仍宽松 ⇒ 既有代码零影响）
```

⇒ ⇒ 该 AC **字面上永远无法满足**，且与草案**自己的** A.1⑥ / N3 订正**互相矛盾**。
⚠️ 需改的**不止这一条** —— `kRejectDuplicates` 在文件内出现 **7 处**，其中 **4 处仍按「复用」口径**（`:53` / `:134` / `:165` / `:196`）。

### 4.2 证据（2026-11-12 实测 · `grep -n kRejectDuplicates`）

| 行 | 段 | 现状 | 判定 |
|---:|---|---|---|
| `:39` | A.1⑥ | 已带 metis 订正（「**不可能**用于产品设计」） | ✅ 保留（否定说明） |
| **`:53`** | **A.3 表** | 「直接复用，**不**新造重名报错机制」 | 🔴 **矛盾**（清单第 1 版漏了这一处） |
| `:68` | N3 | 已订正（「**不能**复用」） | ✅ 保留（否定说明） |
| **`:134`** | **What Changes · N3 段** | 「同名 ⇒ **报错**（复用 `kRejectDuplicates`）」 | 🔴 **矛盾** |
| **`:165`** | **Acceptance** | 「在 `kRejectDuplicates` 策略下**创建点抛异常**」 | 🔴 **不可满足（本项目标）** |
| **`:196`** | **Capabilities 表** | 「本项 MUST **复用**其 `kRejectDuplicates` 策略」 | 🔴 **矛盾** |
| `:255` | metis 处置历史 | 「会被 `ch_reg` 默认名触发 ⇒ 不可能用于产品设计」 | ✅ 保留（历史） |

- **实现现状**（已按「端口作用域」落地）：`include/codegen_naming.h:195-200`（第一遍占位）
  + `:210-218`（第二遍发名）⇒ ⇒ 重名时发 `CHERROR("端口命名冲突：…")` + **后者退回计数器路径**
  拿唯一名 ⇒ ⇒ **不抛异常**、**不拒绝整份设计**、**产物永远合法**。
- **判别力缺口（顺带发现）**：`tests/test_port_name_named_ctor.cpp` /
  `tests/test_port_name_parity.cpp` 中 `grep -cE "重名|duplicate|kRejectDuplicates"` == **0**
  ⇒ ⇒ 该 `CHERROR` 分支**零测试覆盖**。

### 4.3 根因

metis 第 1 轮订正只改了 **A.1 / N3 正文**，**没有回扫** `## Acceptance` 段、
`## Capabilities` 表、**A.3 表**与 `## What Changes` 的 N3 段
⇒ ⇒ 「改了一半」：正文说「不复用」，另外三处仍说「复用」。

### 4.4 修正方案

1. **删除** `:165` 的 AC，替换为**与实现一致**的两条：
   - `- [x] 同一组件内两个**显式命名**端口 sanitize 成同一 Verilog 名 ⇒ MUST 报 `CHERROR` 点名冲突，
     且**后者退回计数器路径**拿到唯一名（产物仍**合法**，整份设计**不被拒绝**）
     ⇒ 证据（**静态**）: `include/codegen_naming.h:195-200,210-218` ⇒ ⇒
     ⚠️ **静态证据只支撑代码形态**；**运行期判别力 = 下一条（当前未建）** —— 不得混为一谈。
   - `- [ ] 该冲突路径 MUST 有单元测试**正向**（构造重名 ⇒ 断言 `CHERROR` 被触发 + 两者名字不同）
     ∧ **反向**（不重名 ⇒ 断言**无** `CHERROR`）⇒ 判别力自证：删掉 `CHERROR` ⇒ 正向用例必红
     ⇒ 归属: **独立立项**（批次表 §4 第 N 行 · 见 §7 的 **S1.5**）`
2. **同步** `:196`（Capabilities 表）：把「MUST **复用**其 `kRejectDuplicates`」改为
   「**明确不复用**（该策略作用于 ctx 全部节点，会被 `ch_reg` 默认名触发）⇒ ⇒
   端口重名用**端口作用域**局部判定（`codegen_naming.h`）」。
3. **同步** `:53`（A.3 表）：删掉「直接复用，不新造重名报错机制」，改为
   「⚠️ **不复用** `kRejectDuplicates`（理由见 A.1⑥）⇒ ⇒ 用 `codegen_naming.h` 的端口作用域判定」。
4. **同步** `:134`（What Changes · N3 段）：把「报错（复用 `kRejectDuplicates`）」改为
   「报 `CHERROR` + 后者退回计数器路径（**端口作用域**判定 ⇒ 不拒绝整份设计）」。
5. **`:39` / `:68` / `:255` 不动**（它们是否定说明 / 历史记录 ⇒ ⇒ 抹掉会破坏证据链）。

### 4.5 判据（可机械判定）

- **C4-a（全文件扫描 + **内容**白名单，不用行号）**：`kRejectDuplicates` 的**每一处**命中，
  该行 MUST 属于下列**两类之一**：
  - **白名单**（3 类特征串，**用内容锚定而非行号** —— 行号会随编辑漂移）：
    ① 同句含 `metis 订正`（A.1⑥）；② 同句含 `N3` **且**含 `不能/不复用`；
    ③ 位于 `## metis 第 1 轮 gap 分析与处置` 段内（处置历史）。
  - **改写后**：同句含 `不复用` / `不作` / `作废` / `不可能` 之一。
  ⇒ ⇒ 反例数 == 0（覆盖 `:53/:134/:165/:196`，**不是**段内扫描）。
- **C4-b**：`## Acceptance` 内不存在「…在 `kRejectDuplicates` 策略下…抛异常」形态的**未撤回**条目。
- **C4-c**：存在一条**端口作用域**的重名 AC（含正向 + 反向两半）。
- **C4-d**：该 AC 的 checked 状态 MUST 为**未勾**（单测尚未写），且 `⇒ 归属: 独立立项（§4 第 N 行）`
  ⇒ ⇒ **裁决写死为「独立立项」**（不是「B4 或独立小项」—— 见 §7 的 S1.5）。

### 4.6 归属 / 依赖

- **归属**：planner 改文档（`4.4` 的 1–5）；**新增单测**属 builder（**不在本清单执行**，只登记）。
- **依赖**：**无前置**（C4-d 的归属已**写死**为「独立立项」⇒ ⇒ 不依赖项 1 的 Status 裁决；
  执行顺序见 §7.1 的 **S1**）。但 MUST 与 **S1.5** 同批（把单测登记进批次表 §4）
  ⇒ ⇒ 否则它是一条「可满足但无处可去」的僵尸 AC。

---

## 5. `examples-rtl-sim-coverage` 的 AC 已被现实超越（**14 处**）

### 5.1 现象

草案内 **14 处**仍写旧口径（**5 处文案** + **E4′ 表 9 行状态**）：

| 行 | 段 | 现状 |
|---:|---|---|
| `:64` | `## What Changes` · E4 行 | `已验证 = 1` · `待补期望源 = 9` |
| **`:145`** | **`### E4′` 节标题** | 「**「待补期望源」清单（10 项逐条：缺什么）**」 |
| **`:147`** | **E4′ 的「口径」行** | `**\`已验证\` = 1**` · 「其余 9 项为 `待补期望源`」 |
| **`:153`–`:161`** | **E4′ 表 9 行**（`counter` … `axi4_lite`） | 状态列 = `⬜ 待补` |
| `:209` | `## Acceptance` →「独立 Verilog 仿真」AC 本身 | `**\`已验证\` = 1**` · `**\`待补期望源\` = 9**` |
| `:309` | 文末「已裁决」段 | `已验证 = 1` · `待补期望源 = 9` |

⚠️ **`:147` 与 `:209` 带反引号**（`` **`已验证` = 1** ``）⇒ ⇒
**朴素 `grep "已验证 = 1"` 会漏掉这两处**（实测：朴素 pattern 只命中 2 处，
稳健 pattern 命中 4 处文案）⇒ ⇒ 这是 critic 第 1 轮 BLOCKER-1 的连带发现。

### 5.2 证据（2026-11-12 实测）

```
$ awk -F'\t' '$0!~/^#/ && NF>1 {print $7}' .rddf/checks/examples-rtl-sim-targets.tsv | sort | uniq -c
     10 verified                       # 权威表：10/10 全 verified（无 pending）
$ awk -F'\t' '$0!~/^#/ && NF>1 && $2=="target"' .rddf/checks/examples-rtl-sim-targets.tsv | wc -l
9                                      # 9 个 kind=target + 1 个 kind=dut = 10 项
$ bash scripts/verify_examples_rtl_sim.sh
✅ verify_examples_rtl_sim: PASS（10 项 · 登记数守恒 · status 合规 · TB 存在 · 编译通过 · 跑通 · 断言 ≥ 下界）
   quadrature_encoder_example：RESULT: checks=7 fails=0
   axi4_lite_example：RESULT: checks=45 fails=0   （… 每项均有 checks=N fails=0）
```
- 批次表 §B2 行**已订正**为「**E4′ 已验证 = 10 / 待补期望源 = 0**」。
- 草案 frontmatter `进度:` **已**写「**E3 = 10/10 verified**」⇒ ⇒ 只有**正文与 E4′ 表**没跟上。
- 期望源登记：`.rddf/checks/examples-rtl-expectations.md`（**30,366 B**）·
  落选名单 `.rddf/checks/examples-rtl-sim-exclusions.md`（**6,682 B**）。

### 5.3 根因

E4′ 的期望源补齐发生在 **B2 内部**（commits `73f182b` 期望源独立性 / `926dfb2` 勘误），
批次表、验收报告、frontmatter `进度:`、`examples-rtl-sim-targets.tsv` 都回填了
⇒ ⇒ **草案正文 + E4′ 表没回填**（同项 2 的根因：报告 → 草案正文无通道）。

### 5.4 修正方案

1. **5 处文案**（`:64` / `:145` / `:147` / `:209` / `:309`）统一改为：
   `已验证 = 10 · 待补期望源 = 0（2026-11-12 订正 · 明细见 .rddf/checks/examples-rtl-sim-targets.tsv）`，
   并**保留**原裁决语义（「收窄声明而非收窄目标」+ 准则 ③ 不可放宽）。
   - `:145` 节标题改为：`### E4′ — 10 项的期望源登记（已全部 verified）`
   - `:147` 口径行改为：`口径：10 项**全部**已验证（明细见 tsv）⇒ ⇒ 每项的期望源出处与 RESULT 下界见下方表`
2. **E4′ 表 9 行**（`:153`–`:161`）的**状态列** `⬜ 待补` → `✅ **已验证**`，
   **「缺什么」列**改为「—（期望源已登记，见 `examples-rtl-expectations.md`）」。
   ⇒ ⇒ ⚠️ 这 9 行**不是**「回填」，是**事实订正**（tsv 权威表已 10/10 verified）。
3. `:209` 是 **AC 条目本身** ⇒ ⇒ 改完后**同时**按项 2 的规则勾选（它是 3 份 `implemented` 之一，
   勾选率应**最高**）。
4. 「每一项 MUST 列出「缺什么」」⇒ ⇒ 改为**已完成档**表述：
   「每项的期望源出处 + `RESULT: checks=N` 下界」已登记在 `examples-rtl-expectations.md`。

### 5.5 判据（可机械判定）

- **C5-a（稳健 pattern，**必须容忍反引号**）**：
  ```bash
  grep -cE '已验证`*\s*=\s*1([^0-9]|$)|待补期望源`*\s*=\s*9([^0-9]|$)|⬜ 待补' <file>
  ```
  ⇒ ⇒ 命中数 MUST **== 0**。
  ⚠️ **pattern 的三个盲区（人工承重，不许当作 C5-a 已覆盖）**：
  ① **`:145` 节标题**旧形态是「「待补期望源」**清单**（10 项逐条：缺什么）」——
     `待补期望源` 后紧跟 `」` 而非 `=` ⇒ ⇒ **pattern 看不见它** ⇒ ⇒ 由 §5.4-1 的 MUST 指令覆盖；
  ② 9 行表格状态列的 `⬜ 待补` 由第三分支覆盖，但**改后的新文案**（「—（期望源已登记…）」）
     不再可判「改对了没有」⇒ ⇒ 由 §7.2 的**目视抽样**覆盖；
  ③ E4′ 表若被改成 `✅ 已验证` 以外的形态（如 `✅ verified`）⇒ ⇒ pattern 不报错 ⇒ ⇒ 同样靠目视。
  ⚠️ **禁用** `grep "已验证 = 1"`：
  ① 它是 `已验证 = 10` 的**子串** ⇒ ⇒ 修订后必红（critic BLOCKER-1）；
  ② 它**漏掉** `:147` / `:209`（带反引号）⇒ ⇒ 修订前也测不全。
- **C5-b**：草案内出现「已验证 = 10」与「待补期望源 = 0」（**允许反引号穿插**，即
  `已验证`[`]*\s*=\s*10）⇒ ⇒ 各 **≥1** 次。
- **C5-c**：`scripts/verify_examples_rtl_sim.sh` PASS ∧ tsv 的 `status` 列 **10 行全 `verified`**
  ∧ `kind=target` 行数 == 9 ⇒ ⇒ 与草案文案**互不矛盾**。

### 5.6 归属 / 依赖

- **归属**：planner。
- **依赖**：**文案订正在 S2**、**AC 勾选在 S4**（同一份文件的两次动作）⇒ ⇒
  不阻塞彼此，但 MUST 同一**原子提交**（见 §7.1 的原子性约束）。

---

## 6. 范围边界（**本清单不做**，逐条写明以免被当成遗漏）

| # | 不做的事 | 理由 / 归属 |
|---|---|---|
| 1 | **不实现任何 B3 / B4 的 work item**（D-TCM `sread` / 装载链路 / `illegal_instr` / 组件接入） | 归 builder；本清单只是**记录订正** |
| 2 | **不补任何测试**（含项 4 提到的重名单测） | 归 builder；本清单只**登记**该缺口（登记落点 = 批次表 §4 · 见 **S1.5**） |
| 3 | **不裁决 B3 的入口门**（R5 = ADR-069 / R6 = 公开面前置门） | 归 architect + 用户（`phase-6-next-steps.md` R5/R6） |
| 4 | **不改 `phase-6.md` 的 B0–B4 状态格** | 那些格**已正确**（B0/B1/B2 ✅ · B3/B4 ⬜），是本清单的**对照基准**，不是待改对象 |
| 5 | **不改批次表 §B1/§B2 的内容列** | 已订正（E4′ 已写 10/0）⇒ ⇒ 本清单只改**草案本体**；**例外** = §4「独立立项」清单**新增 2 行**（见 S1.5 与项 1-b） |
| 6 | **不追溯改写 B0/B1/B2 的验收报告** | 报告是**证据**，只被引用、不被改写 |
| 7 | **不动 `include/cpu/AGENTS.md` 的组件定位说明** | B0 的 C7 已落地且 `verify_ledger_evidence.sh` PASS |
| 8 | **不新增/删除 ctest 条目** | 本清单是纯文档修订 ⇒ ⇒ `CTEST_TOTAL` 应保持 **263** 不变（项 3 只改注释，不改数字） |
| 9 | **不抹除 `kRejectDuplicates` 的历史记录**（`:39/:68/:255`） | 历史只该被新批注覆盖，不该被擦除（会破坏证据链） |
| 10 | ⚠️ **已知超范围：其余草案的同型四通道漂移** | 实测同型者 = **2 份**：`port-name-literal-parity-2026-11`（fm=`implemented` / 正文 `📝 draft`）· `node-name-prefix-consistency-2026-11`（同上）。<br>对照：`mem-port-arg-cross-layer-2026-11`（两者均 `implemented` ✅）· `hierarchical-port-landing-matrix-2026-11`（两者均 draft ✅）⇒ ⇒ **无漂移**。<br>⇒ ⇒ 本清单**只修 7 份 RTL 轨**；上述 2 份 MUST 另开清单（否则 `improvement-suggestions.md` 会半新半旧 —— 正是本清单要治的病）。 |

---

## 7. 执行顺序与整体验收

### 7.1 顺序（有依赖，不许乱序）

| 步 | 动作 | 为什么在这个位置 |
|---|---|---|
| **S1** | 项 4（删不可满足 AC + 同步 `:53/:134/:196`） | 它改的是**判据本身** ⇒ 必须先于勾选 |
| **S1.5** | **把项 4 的重名单测登记进批次表 §4**（载体 = `port-naming-ratchet` N3 补遗，照 N5 行粒度：对象 + 判别力自证要求 + 触发条件） | 否则它是一条「可满足但无处可去」的僵尸 AC ⇒ 归属裁决写死为「**独立立项**」（不写「B4 或」） |
| **S2** | 项 5（I7 的 **14 处**：`:64/:145/:147/:209/:309` 文案 + `:153`–`:161` **9 行状态列**） | 同上（改判据文案 + 事实订正） |
| **S3** | 项 1（7 份四通道对齐 + 1-b 的 (b)-**①②③④** 落地，含 §0.0 词表扩充） | 决定了「哪些 AC 该勾」 |
| **S4** | 项 2（逐条回填 **141** 条 AC + 2-a 的**四处**改写**含 9 行正文/Impact 表的 257** + C2-e 的归属标注） | 依赖 S1–S3 |
| **S5** | 项 3（`AGENTS.md` 注释块**合并**并补 +1，**净删字节**） | 独立，可并行；但 MUST 在 S4 **之后**跑锚点检查（防 S4 引入测试数变化） |
| **S6** | 全量复验（见 §7.2）+ **C2-a 抽样复核 5 条** | 收口 |

⚠️ **原子性（critic 第 1 轮 SHOULD_FIX）**：S1–S6 MUST 作为**单一原子提交单元**入库。
中途任何一步失败/中止 ⇒ ⇒ **不得部分提交** —— 否则会入库「AC 已改但 Status 未对齐」
的中间态文档，与本清单要治的病**同型**。
（若确需分批提交，则每批 MUST 自带「该批不引入新的三方/四方不一致」的复验证据。）

⇒ ⇒ **无循环依赖**（S1 → S1.5 → S2 → S3 → S4 → S5 → S6 严格单向）。

### 7.2 整体验收判据（全部 MUST 绿）

```bash
# 文档一致性
# ① C5-a（容忍反引号 + 覆盖 E4′ 表 9 行；朴素 grep "已验证 = 1" 是 "= 10" 的子串 ⇒ 必红）
grep -cE '已验证`*\s*=\s*1([^0-9]|$)|待补期望源`*\s*=\s*9([^0-9]|$)|⬜ 待补' \
  .rddf/improvements/examples-rtl-sim-coverage-2026-11.md                          # 必须 0
# ② C2-c（逐文件、排除末尾 metis 处置段 ⇒ 否则历史引用会误伤）
for f in .rddf/improvements/*-2026-11.md; do
  n=$(awk '/^## metis 第 1 轮 gap 分析与处置/{exit} {print}' "$f" \
        | grep -cE '257\s*个\s*ctest|ctest.{0,10}257|既有\s*\*{0,2}257')
  printf "%-70s c2c-hits=%s\n" "$f" "$n"
done                                                                               # 全部必须 0
# ③ C3-a / C3-c
awk '/^- CTEST_TOTAL/,/^<!-- CI anchor: .ctest -L/' AGENTS.md | grep -c "判别力实测"   # 必须 1
stat -c %s AGENTS.md                                                              # 必须 < 57088（净删）
# ④ C2-a（2026-11-12 复核订正版）：**不合规的 checked 行数 == 0**
#    判据：`- [x]` 行 MUST 同时含「证据标记」与「可定位 token」（**任一顺序**）⇒
#          撤回式条目（`~~…~~` + 🔴 + 撤回|作废|删除）**豁免**
cat > /tmp/c2a_mark.txt <<'P'
证据[:：]|证据（[^）]*）[:：]
P
cat > /tmp/c2a_tok.txt <<'P'
(`[^`]*[/][^`]*`|`[^`]*\.(py|sh|cpp|h|md|sv|json|yaml|tsv)`|`(--[a-z-]+|ctest [^`]*)`)
P
for f in .rddf/improvements/rtl-emit-product-tooling-2026-11.md \
         .rddf/improvements/port-naming-ratchet-2026-11.md \
         .rddf/improvements/firmware-load-chain-2026-11.md \
         .rddf/improvements/memory-timing-contract-2026-11.md \
         .rddf/improvements/rv32i-dead-component-consolidation-2026-11.md \
         .rddf/improvements/rv32i-isa-gap-closure-2026-11.md \
         .rddf/improvements/examples-rtl-sim-coverage-2026-11.md; do
  b=$(awk '/^## metis 第 1 轮 gap 分析与处置/{exit} {print}' "$f")
  chk=$(printf '%s\n' "$b" | grep -cE '^\s*-\s*\[[xX]\]')
  bad=$(printf '%s\n' "$b" | grep -E '^\s*-\s*\[[xX]\]' \
        | grep -vE '~~.*(撤回|作废|删除)' \
        | grep -vE -f /tmp/c2a_mark.txt \
        | grep -vE -f /tmp/c2a_tok.txt \
        | grep -E -f /tmp/c2a_mark.txt \
        | grep -E -f /tmp/c2a_tok.txt | wc -l)
  printf "%-62s checked=%-3s 不合规=%s\n" "$f" "$chk" "$bad"
done
#    ⇒ 期望：每份「不合规」== 0（撤回式条目已在上一步剔除 ⇒ 不计入 bad）

```

- **C7-a**：上表每条命令的输出符合注释里的期望。
- **C7-b**：7 份草案的 HARD-GATE 复检仍 PASS
  （`bash ~/.agents/skills/rdd-workflow-brainstorm/scripts/pre_create_brainstorm_check.sh <file> --project-root "$PWD"`）。
- **C7-c**：`git status --short` 只含**文档类**路径
  （`.rddf/**` · `improvement-suggestions.md` · `AGENTS.md`）⇒ ⇒ **零** `src/**` / `include/**` / `tests/**` 变更。
- **C7-e（抽样复核 · **确定性抽样**，非「随机」）**：把 7 份草案的全部 checked AC 按
  `<文件路径, 行号>` 升序排序，取**第 1 / 4 / 7 / 10 / 13 条**（共 5 条），
  人工确认其证据指针**真的支撑**结论 ⇒ ⇒ 至少 **4/5** 成立；否则整批回填**不通过**
  （形式判据 ≠ 证据强度）。⚠️ 不用「随机」—— 无 seed 的随机不可复现、不可复核。

### 7.3 交付给下一个会话的 builder 的可用性要求

修订完成后，产物 MUST 支持 builder **只读 improvement + 批次表 + 本文档**就能回答：

1. **本项要做什么** → `## 范围 ✅ 在内` 的编号列表（不含已撤回条目）。
2. **做完了没有** → AC 的勾选状态 + 每条的证据指针（**不含**「无证据的勾」）。
3. **没做完的是哪些、归哪一批** → 未勾 AC 上的 `⇒ 归属:` 标注（C2-e 保证存在）。
4. **判据是什么、怎么自证** → checked AC 指向的测试名 / 护栏名 / 报告节。
5. **能不能现在开工** → 项的 `- **Status**` + **frontmatter `进度:`** + 批次表 §B3/§B4 的 🚪 入口门。

⇒ ⇒ ⚠️ **Q5 的答案源 MUST 含 `进度:`** —— 因为 **R5（T2 / ADR-069 门）当前不在批次表里**
（`phase-6-rtl-productization.md` §B3 的 T2 行依赖列只写「T1」），T2 的门状态只活在
`phase-6.md:75`、`phase-6-next-steps.md`、以及**草案 frontmatter `进度:`**
（`memory-timing-contract-2026-11.md:8` 记着「T2 待 architect 裁决」）。
⇒ ⇒ 二选一（**推荐前者**）：**(i)** 把 `进度:` 纳入 Q5 答案源（C1-d 已保证 B3 门可见）；
**(ii)** 把 R5/R6 当前态**抄进批次表 §B3 行**（会产生第二份副本 ⇒ 有漂移风险）。

⇒ ⇒ **判据 C7-d**：对 7 份草案逐份跑上述 5 问，答案 MUST 能在
**草案 + 批次表 + `进度:` 字段**内找到，**不得**需要读 `.rddf/reviews/**` 才能答出
「做完了没有」（报告只作为**证据指针**被引用）。

---

## 8. 审稿记录

| 轮 | 审查者 | 结论 | 处置 |
|---:|---|---|---|
| 1 | **oracle** | 5 条诊断全部属实；但**清单自身**含 1 处事实错误 + 5 处覆盖缺口 + 3 条脆弱判据 | **13 条建议全部采纳**，已并入本文（见 §8.1） |
| 1 | **critic** | **REJECT** · `mergeable: no` · `scope_exceeded: false` ⇒ **2 条 BLOCKER**（均为「验收命令不可能通过」）+ 4 条 SHOULD_FIX + 5 条 NITS | **2 BLOCKER 已修**（闭合证据见 §8.2）· SHOULD_FIX **全部采纳** · NITS **全部采纳** ⇒ 送第 2 轮（只验 BLOCKER 闭合） |
| 2 | **critic（复审）** | ✅ **APPROVE** · `mergeable: yes` · `scope_exceeded: false` ⇒ **0 BLOCKER** · 0 SHOULD_FIX · **3 NITS**（均不阻断） | **2 条 BLOCKER 独立复核为 CLOSED**：① B-1 —— 新 pattern 在全目录实测恰命中 **13** 处、「已验证 = 10」不命中、反引号形态命中；② B-2 —— 不过滤时 10 命中、`firmware-load-chain:235` 正确排除、余 **9 处与 §2.4-3③ 清单逐一对上**、metis heading 9 份文件各**恰 1 处**。<br>**3 NITS 已全部修**：① `待补期望源 = 9` 分支补 `([^0-9]|$)` 边界；② §5.5 明示 **`:145` / 表格新文案 / 别形态** 三个 pattern 盲区（人工承重）；③ §2.5 C2-c 明示「只钉 257 ⇒ 改写成别的写死数字可绕过」的盲区（由 §2.4-3③ MUST + C7-e 承重）。 |
| — | **总判定** | ✅ **清单通过审查，可进入修订** | **未超范围**（`scope_exceeded: false`）· 事实订正（9 行表格 `⬜ 待补` → `✅ 已验证`）经复核**不越权**（权威源 = `examples-rtl-sim-targets.tsv` 10/10 `verified` + 护栏 PASS + B2 验收事实） |

### 8.1 oracle 第 1 轮的 13 条与其落点

| # | oracle 建议 | 落点 |
|---:|---|---|
| 1 | **重写 2-a**：CLI **无** `--top`（实测 `DesignFactory`），并把 C1 设计段 `:109/:114/:130-131` 纳入同步；F6 对齐护栏现值 | §2.4-3① / ② |
| 2 | **项 4 扩面**：加 `:53`；C4-a/C4-b 改**全文件** + 白名单 `{39,68,255}` | §4.2 / §4.5 C4-a（已进一步改为**内容**白名单，见 §8.2） |
| 3 | **项 5 补 `:209`** | §5.1（critic 后进一步扩到 **14 处**，见 §8.2） |
| 4 | **1-b 维持 (b)**，加 `:179` 改写 + `进度:` 指针 + §4 登记行 | §1.4-4 的 (b)-①②③（critic 后加 (b)-④ 词表扩充） |
| 5 | **新增 C2-e**（未勾必有归属） | §2.5 C2-e |
| 6 | **新增 S1.5**（重名单测登记进 §4，归属写死「独立立项」） | §7.1 S1.5 / §4.5 C4-d |
| 7 | **C2-a 防退化**：指针形态正则 + 存在性 + 抽样复核 + 明示局限 | §2.5 C2-a / §7.2 C7-e |
| 8 | **C3-b 换结构性判据**（每段点名 `test_*` + 严格递增） | §3.5 C3-b |
| 9 | **C3-e 对齐护栏语义**（不得 FAIL）+ 明示 256 B 余量 | §3.4-4 / §3.5 C3-e |
| 10 | **C1-a 扩为四通道**（含 `进度:`） | §1.5 C1-a/C1-c |
| 11 | **§7.3-Q5 / C7-d 修正**：R5 不在批次表 ⇒ 答案源含 `进度:` 或抄进 §B3 | §7.3 |
| 12 | **§2.2 计数口径**：137 → **141**（含 `<br>` 内联） | §0 / §2.1 / §2.2 |
| 13 | **项 4 第一条 AC 的证据措辞**：静态证据 vs 运行期判别力分开写 | §4.4-1 第一条 |

### 8.2 critic 第 1 轮的 BLOCKER 闭合证据（本轮只验这两条）

| # | BLOCKER（critic 原文要点） | 根因 | 闭合证据（**已实测**） |
|---:|---|---|---|
| **B-1** | C5-a/§7.2 的 `grep -c "已验证 = 1\|待补期望源 = 9"` **执行修正方案后必然不通过** —— 「已验证 = 1」是「已验证 = 10」的**子串** | 判据未加数字边界 | ① 实测：`printf '已验证 = 10 …' \| grep -c "已验证 = 1"` ⇒ **1**（必红）· 加边界 `已验证 = 1[^0-9]` ⇒ **0**；<br>② 进一步发现**朴素 pattern 还会漏** `:147`/`:209`（带反引号）⇒ 现用 `已验证\`*\s*=\s*1([^0-9]\|$)`；<br>③ 落点：§5.5 C5-a（新 pattern + 三条禁令）/ §7.2 ① |
| **B-2** | C2-c 的「收窄正则」**未收窄到位** —— 排除范围外仍有 5 处命中（清单自己断言「不误伤」与实测不符） | 只允许改 AC，但正文 / Impact 表也有写死 257；且 metis 段在**引用历史** | ① 实测：`awk '/^## metis…/{exit}'` 过滤后仍有 **9 处**当前态命中（`:165/:198/:226` · `:115/:162/:183` · `:168` · `:175` · `:232`）；<br>② `awk … exit` 过滤 metis 段后 `firmware-load-chain:235` 等历史引用**被正确排除**；<br>③ 落点：§2.4-3③（**9 行逐处清单** + 范围扩到正文/Impact 表）/ §2.5 C2-c（逐文件 + awk 过滤的命令）/ §7.2 ② |

### 8.3 SHOULD_FIX / NITS 的落点

| # | critic 项 | 落点 |
|---:|---|---|
| SF-1 | `implemented` 词表未覆盖「**可做但明确移交**」⇒ 修完会与 §4 新行矛盾 | **§1.4-4 的 (b)-④**（扩充 §0.0 词表文本） |
| SF-2 | §4.6 / §5.6 的依赖注记与 §7.1 的 S 顺序矛盾 | **§4.6**（改「无前置」）· **§5.6**（改为「文案在 S2 / 勾选在 S4」） |
| SF-3 | §6 漏登「其余草案同型漂移」 | **§6 #10**（实测 `port-name-literal-parity` / `node-name-prefix-consistency` 2 份同型；另 2 份无漂移） |
| SF-4 | S1→S6 未规定原子性 ⇒ 可能入库中间态 | **§7.1 的「原子性」约束** |
| N-1 | C7-e「随机」不可复现 | **§7.2 C7-e** 改为确定性抽样（第 1/4/7/10/13 条） |
| N-2 | C4-a 用**行号**白名单 ⇒ 编辑后失效 | **§4.5 C4-a** 改为**内容**白名单（3 类特征串） |
| N-3 | C2-b 的 C1–C4 分区无机械规则 | **§2.5 C2-b** 补「按 `## Acceptance` 小节标题分区」 |
| N-4 | C3-b 可被「段内随便塞一个 `test_*`」绕过 | **§3.5 C3-b** 加「**同句**相邻」 |
| N-5 | `rv32i-isa-gap-closure` 的 `:151/:154/:170` 是「裁决注记 ≠ 撤回式」 | **§2.4** 新增「第四种形态」段 |

---

## 9. 修订执行记录（2026-11-12 · planner）

> ⚠️ **未 commit**（全局纪律：等用户确认）。本次改动**全部是文档**（`git status` 见 §9.4）。

### 9.1 实际改动清单

| 文件 | 改动 | 对应项 |
|---|---|---|
| `.rddf/improvements/rtl-emit-product-tooling-2026-11.md` | C1 段（CLI 形态 · `DesignFactory`）· C4 段（manifest JSON 实测样例）· **20 条 AC 全部回填** · `- **Status**` 对齐 | 项 1 / 项 2 |
| `.rddf/improvements/port-naming-ratchet-2026-11.md` | **4 处** `kRejectDuplicates` 矛盾全改 · **19 条 AC** 回填 · `进度:` 加 §4 指针 | 项 1 / 项 2 / 项 4 |
| `.rddf/improvements/firmware-load-chain-2026-11.md` | **18 条 AC** 全部保持未勾 + `⇒ 归属: **B3**` · `- **Status**` 对齐 | 项 1 / 项 2 |
| `.rddf/improvements/memory-timing-contract-2026-11.md` | **17 条 AC** 全部保持未勾 + `⇒ 归属: **B3**` · `- **Status**` 对齐 | 项 1 / 项 2 |
| `.rddf/improvements/rv32i-dead-component-consolidation-2026-11.md` | **25 条 AC** 回填（**15 勾 / 10 未勾⇒B4**）· `- **Status**` 对齐 | 项 1 / 项 2 |
| `.rddf/improvements/rv32i-isa-gap-closure-2026-11.md` | **22 条 AC** 全部保持未勾 + `⇒ 归属: **B4**` · `- **Status**` 对齐 | 项 1 / 项 2 |
| `.rddf/improvements/examples-rtl-sim-coverage-2026-11.md` | **14 处**旧口径订正（5 文案 + 9 行表状态）· **22 条 AC** 全部回填 · `- **Status**` 对齐 | 项 1 / 项 2 / 项 5 |
| `.rddf/roadmap/phase-6-rtl-productization.md` | §0.0 `implemented` 定义扩充（(b)-④）· §4 **新增 2 行**（N2 补遗 / N3 补遗） | 项 1-b |
| `.rddf/roadmap/phase-6-next-steps.md` | 新增 **§8「新会话 builder 的实施入口」**（读序 4 步 · 权威载体表 · 未完成 work item 逐条索引）⇒ ⇒ 载体 = 本清单 §9 的**下游读法**（§7.3 的 5 问） | 项 1（**§9.4 快照初版漏登 ⇒ 2026-11-12 补**） |
| `AGENTS.md` | `CTEST_TOTAL` 注释块：**合并重复段** + 补 `test_rtl_emit_contract` 的 +1 · **净删 48 B**（57,088 → 57,040） | 项 3 |
| `.rddf/checks/rtl-track-followup-list-2026-11.md` | **新建**（D5 登记义务的兑现）⇒ ⇒ 覆盖另 2 份草案的同型漂移 + **40** 条 AC 回填 ⇒ ⇒ `状态: 待审` ⇒ ⇒ **本轮不执行其修订** | 项 1 / 项 2（**后继**） |
| `improvement-suggestions.md` | 7 行状态列对齐 | 项 1 |

### 9.2 判据实测结果（**全绿**）

| 判据 | 结果 | 证据 |
|---|---|---|
| **C1-a** 四通道同档 | ✅ **12/12** | 7 份 × (frontmatter / 正文 Status / 索引表) 档位词逐一对齐 |
| **C1-b** 缺口有落点 | ✅ | 批次表 §4 新增「N2 补遗 · 首批端口消化（32 端口）」行 |
| **C1-c** `进度:` 不与 `状态:` 矛盾 | ✅ | `port-naming-ratchet` 的 `进度:` 已加「⚠️ 首批端口消化（32 端口）未做 ⇒ 已登记 §4」 |
| **C1-d** B3 门可见性 | ✅ | `memory-timing-contract` / `firmware-load-chain` 的 `进度:` 均含不可开工理由 |
| **C2-a** checked 有证据指针 | ⚠️ **原写「✅ 73/73」不实** ⇒ ⇒ **2026-11-12 复核订正**：当时用的是**宽松扫法**（段内含「证据」二字即算），而 §2.5 的**判据正则**实际命中 **0/73** ⇒ ⇒ 真相 = **7 条证据子句无可定位指针**（已逐条补路径 ⇒ 现 **69/69 合规 + 4 撤回式豁免**）⇒ ⇒ 详见 **§9.5 D1** | 逐条扫（`- [x]` 段内含「证据 / 作废 / 撤回 / 已删除」）—— ⚠️ **该扫法只保「形似」，保不了「指针可定位」** ⇒ ⇒ 判据本体已改（§2.5 C2-a + §7.2 ④） |
| **C2-b** B3/B4 不得被勾 | ✅ **状态正确 · 判据表述已订正** | B3（B3 批 2 份）checked = **0**；B4（`rv32i-isa-gap-closure` 全 22）checked = **0**；`rv32i-dead-component-consolidation` 的 C1–C4（10 条）checked = **0**（B0 的 15 条已勾）⇒ ⇒ ⚠️ **原「按小节标题分区」的表述不可满足**（`差分` 小节首条属 B0 ⑧ ⇒ 已勾）⇒ ⇒ §2.5 C2-b 已改为「按 work item 归属」分区（详见 **§9.5 D2**） |
| **C2-c** 不写死测试数 | ✅ **全 0** | 逐文件 `awk '/^## metis…/{exit}'` 过滤后计数 = 0 |
| **C2-d** 勾选率分布 | ✅ | implemented 3 份 = **20 / 16 / 22** 勾；未开工 2 份 = **0 / 0**；部分 1 份 = **15 / 25** |
| **C2-e** 未勾必有归属 | ✅ **70/70** | 未勾项逐条含 `归属: **B3**` / `**B4**` / 独立立项 |
| **C3-a** 「判别力实测」唯一 | ✅ **1** | `awk` 计数 = 1 |
| **C3-b** 结构性 | ✅ | 数字链 **257 → 261 → 262 → 263** 严格递增，每个数字所在句均点名 `test_*` |
| **C3-c** 末值 == 锚点 == 实际 | ✅ | **263** == `AGENTS.md:452` == `ctest -N` 的 `Total Tests: 263` |
| **C3-d** 锚点护栏 | ✅ | `verify_test_counts.sh` PASS · `--check` **5/5** |
| **C3-e** 字节 | ✅ | **57,040 B** < 基线 57,088 B（**净删 48 B**）· `verify_agents_size.sh` exit 0（未转 FAIL） |
| **C4-a** `kRejectDuplicates` 全文件 | ✅ **7/7 合规** | 每处落在「内容白名单」或含否定说明 |
| **C4-b~d** | ✅ | 无「未撤回的抛异常」条目 · 端口作用域 AC 在位 · 该 AC 未勾 + 归属 §4 |
| **C5-a** 旧口径清零 | ✅ **0** | 稳健 pattern（含反引号容忍）计数 = 0 |
| **C5-b / C5-c** | ✅ | 「已验证 = 10」与「待补期望源 = 0」在位 · tsv `status` **10 行全 `verified`** |
| **C7-a** 全量复验 | ✅ | 8 个护栏全 exit 0（`agents_size` / `test_counts` / `roadmap_fragments` / `ledger_evidence` / `public_api_visibility` / `flat_rtl_emit` / `port_naming` / `examples_rtl_sim` / `hierarchical_rtl`）· `ctest -N` = **263** |
| **C7-b** HARD-GATE | ✅ **7/7** | `pre_create_brainstorm_check.sh` 逐份 PASS |
| **C7-c** 零实现面变更 | ✅ | `git status` 仅 10 个**文档**路径 + 1 个新增 `.rddf/checks/**`；**零** `src/**`·`include/**`·`tests/**`·`examples/**`·`scripts/**` |
| **C7-e** 抽样复核 | ✅ **5/5** | 按 `<文件, 行号>` 升序取第 1/4/7/10/13 条（全部落在 `examples-rtl-sim-coverage`）：`test_rtl_emit_contract` 的 5 个 TEST_CASE + 文件头注释 ✓ · `--emit-tb-stub` 选项 + `test_rtl_tb_stub.cpp` ✓ · tsv 10 行 + 两份登记文件 ✓ · `ci.yml` 的 `pr-verilator-fast` step ✓ · 墙钟 §E6（warm 3 s / cold 61 s）✓ |

### 9.3 修订中**发现并订正**的两处事实错误（清单自身的）

| # | 清单原写 | 实测 | 处置 |
|---:|---|---|---|
| 1 | 「`is_top_io == true` 的集合 == 7 in + 27 out = **34**」 | manifest 实测 = **32**（5 in + 27 out）—— ctx 级 `default_clock`/`default_reset` 的 `owner=""` ⇒ `is_top_io=false`；而**产物 Verilog 顶层端口表** = **34**（含 clock/reset） | I1 的 AC 已**同时写清两个口径**并标注「MUST NOT 混用」 |
| 2 | 「重名判定的 `CHERROR` 分支**零测试覆盖**」 | `tests/test_port_name_parity.cpp` 的 **Ratchet 3** 已覆盖该路径（断言 `top_dup_prog_addr` / `_1`）⇒ ⇒ 真实缺口 = 「**`CHERROR` 的触发未被断言**」（删掉 `CHERROR` 仍全绿） | §4.2 / §4.4 的措辞已改为「行为已覆盖 · **触发未断言**」，并据此写 §4 的新 AC |

⇒ ⇒ 两条都**不改变**任何 work item 的范围，但会影响 builder 对判据的理解 ⇒ ⇒ 故如实登记。

### 9.4 提交状态

```
 M .rddf/improvements/examples-rtl-sim-coverage-2026-11.md
 M .rddf/improvements/firmware-load-chain-2026-11.md
 M .rddf/improvements/memory-timing-contract-2026-11.md
 M .rddf/improvements/port-naming-ratchet-2026-11.md
 M .rddf/improvements/rtl-emit-product-tooling-2026-11.md
 M .rddf/improvements/rv32i-dead-component-consolidation-2026-11.md
 M .rddf/improvements/rv32i-isa-gap-closure-2026-11.md
 M .rddf/roadmap/phase-6-next-steps.md
 M .rddf/roadmap/phase-6-rtl-productization.md
 M AGENTS.md
 M improvement-suggestions.md
?? .rddf/checks/rtl-track-correction-list-2026-11.md
?? .rddf/checks/rtl-track-followup-list-2026-11.md
```

⇒ ⇒ **未 commit**（等用户确认）。若要提交 ⇒ ⇒ 按仓内纪律**逐路径** `git add <path>`（**禁** `git add -A`）。

---

---

## 9.5 ⚠️ **2026-11-12 复核轮**（planner · 只读复核 ⇒ 查出 §9.2 的 5 处不实 ⇒ 已订正）

> **触发**：用户要求「审查本清单内容并据此修订」⇒ ⇒ planner 对 §9 的执行结果做**独立复验**
> （逐条重跑 §7.2 的命令 + 逐份重算 §2.5 的每条判据）⇒ ⇒ **§9.2 的「全绿」有 5 处不实**。
> ⇒ ⇒ 本节记录**复核方法 · 5 个缺陷 · 订正 · 复验结果**；§9.1–§9.4 的**事实错误**已就地订正
> （**保留**原记录 + 加订正指针 ⇒ ⇒ 不抹除历史）。

### 9.5.1 复核方法

| # | 做了什么 |
|---|---|
| ① | 重跑 §7.2 的**全部**命令（原样，不改判据）⇒ ⇒ 全绿 |
| ② | 把 §2.5 的 **C1-a…C7-e 逐条**化成可执行检查，**独立重算**（不复用 §9.2 的结论） |
| ③ | 对每条 checked AC **逐块**解析（`- [x]` 行 + 续行），独立判定「证据标记」与「可定位 token」 |
| ④ | 把 §2.4-2-a 的**全部 4 处**与 §2.4-3③ 的 **9 行清单**逐行核对（不是抽样） |
| ⑤ | 跑全部 9 个护栏 + `update_test_count_anchors.sh --check` + 7 份 HARD-GATE |

### 9.5.2 查出的 5 个缺陷（**全部已订正**）

| # | 缺陷 | 证据 | 处置 |
|---|---|---|---|
| **D1** | 🔴 **C2-a 的正则对其自身规定的写法**恒不命中** ⇒ ⇒ §9.2 的「73/73 合规」**不实** | 原正则 `- \[x\].*证据[:：]\s*[\w./#-]+` 在 73 条 checked AC 上实测命中 **0**：① `证据[:：]` 要求冒号紧跟「证据」⇒ 实际写法 `证据（**静态**）:` 不命中；② `\s*` 后要求路径首字符 ∈ `[\w./#-]` ⇒ 实际写法 `证据: \`path\`` 的**反引号**不命中；③ 按**行** grep ⇒ 证据在**续行**的条目必漏。⇒ ⇒ §9.2 当时用的是**宽松扫法**（「段内含『证据』二字即算」）⇒ ⇒ 那不是 C2-a | ① §2.5 C2-a 改为**两个独立条件**（① 证据标记 ② 可定位 token · 撤回式双重豁免）并写明 token 的三种可判定形态 ② §7.2 加**可执行的 ④**（三数相等）③ **7 条**证据子句补真实路径（`scripts/verify_port_naming.sh` ×2 · `.rddf/reviews/b0-batch-verify-2026-11.md` ×3 · `tests/test_rtl_manifest.cpp` · `scripts/verify_examples_rtl_sim.sh`）⇒ ⇒ 现 **69/69 合规 + 4 撤回式豁免** |
| **D2** | 🔴 **C2-b 的「按小节标题分区」与文件实际不符** ⇒ ⇒ 该判据**必然红** | `rv32i-dead-component-consolidation` 的 `差分` 小节**首条**（「🔴 **SLTU 修复 MUST 自带判据**」）属 **B0 ⑧** 且**已勾**（证据 `tests/test_rv32i_alu.cpp`）⇒ ⇒ 按小节标题判 ⇒ 该小节 checked ≠ 0 ⇒ ⇒ 与 §8.2 的 **B-2 同类**（「验收命令不可能通过」） | §2.5 C2-b 改为**按 work item 归属**分区（逐条列明：`差分` 首条 = B0 ⑧ · `差分` 其余 4 条 + `接入` 6 条 = B4 · `退役`/`诚实化`/`不回归`/`tie-off` = B0）⇒ ⇒ **状态未变**（B4 10 条 checked == 0 · B0 15 条已勾） |
| **D3** | 🟠 **frontmatter `状态: 待审`** 已过期 | critic **第 2 轮 APPROVE**（§8 表第 3 行）⇒ ⇒ 「oracle → critic 通过后才允许修订」早已满足 | frontmatter 改为「✅ **已审查通过**（oracle 1 轮 + critic 2 轮 APPROVE）· **修订已执行**（见 §9）· ⚠️ 复核轮：§9.2 有 5 处不实（见 §9.5）」 |
| **D4** | 🟠 **§9.1 / §9.4 漏登 `phase-6-next-steps.md`** | 该文件**确实被改**（`git status` 显示 M · **+84 行** = §8「新会话 builder 的实施入口」· 读序 4 步 · 权威载体表 · 未完成 work item 逐条索引）⇒ ⇒ 但 §9.1 的 10 行改动表与 §9.4 的 git status 快照**都没有它** ⇒ ⇒ 「§9.1 = 全部改动」的断言**不成立** | §9.1 补一行（含其内容与归属）· §9.4 的快照补 ` M .rddf/roadmap/phase-6-next-steps.md` |
| **D5** | 🟠 **§6 #10 登记的「另开清单」至今未建** ⇒ ⇒ 登记的义务**未履行** | §6 #10 明写「上述 2 份 MUST 另开清单（否则 `improvement-suggestions.md` 会半新半旧 —— 正是本清单要治的病）」⇒ ⇒ 实测该 2 份**仍是同型漂移**（`port-name-literal-parity-2026-11` / `node-name-prefix-consistency-2026-11`：frontmatter `状态: implemented` · 正文 `- **Status**` = `📝 draft` · 索引表 = `📝 draft` · AC **0/25** 与 **0/15** 已勾）⇒ ⇒ 病仍在 | **已建** `.rddf/checks/rtl-track-followup-list-2026-11.md`（**状态 = 待审** · oracle → critic 通过后才允许修订 ⇒ ⇒ **本轮不执行其修订**）⇒ ⇒ 与本清单**同格式、同判据面** |

### 9.5.3 复验结果（**订正后**）

| 判据 | 结果 | 说明 |
|---|---|---|
| **C1-a** 四通道同档 | ✅ **7/7** | 逐份比对 frontmatter `状态:` / 正文 `- **Status**` / 索引表档位词 ⇒ 全部一致 |
| **C1-b** 缺口有落点 | ✅ | 批次表 §4 的「N2 补遗 · 首批端口消化（32 端口）」行在位 |
| **C1-c / C1-d** | ✅ | `进度:` 不与 `状态:` 矛盾；B3 两份的 `进度:` 均含不可开工理由 |
| **C2-a** | ✅ **69/69 + 4 撤回式豁免** | **判据本体已改**（§2.5）+ 可执行命令已加（§7.2 ④）+ **7 条证据子句已补路径** ⇒ ⇒ 复核**用新判据**跑出 **0 缺口** |
| **C2-b** | ✅ **状态正确** | B3 两份 = 0 · `rv32i-isa-gap-closure` = 0/22 · `rv32i-dead-component` 的 B4 10 条 = 0 · B0 15 条已勾 ⇒ ⇒ **判据表述已订正**（按 work item 归属） |
| **C2-c** | ✅ **全 0** | 逐文件（`awk` 排除 metis 段）⇒ 29 份改进草案命中数全 0 |
| **C2-d** 勾选率分布 | ✅ | implemented 3 份 = **20 / 16 / 22** · 未开工 2 份 = **0 / 0** · 部分 1 份 = **15 / 25** |
| **C2-e** 未勾必有归属 | ✅ **70/70** | 逐条含 `归属:` |
| **C3-a…C3-e** | ✅ | 「判别力实测」= **1** · 数字链 **257→261→262→263** 严格递增且每步点名 `test_*` · 末值 == 锚点 == `ctest -N` = **263** · 字节 **57,040** < 基线 57,088（净删 48 B）· `verify_agents_size.sh` exit 0 |
| **C4-a…C4-d** | ✅ | `kRejectDuplicates` 7 处全部落在内容白名单或含否定说明 · 无「未撤回的抛异常」条目 · 端口作用域 AC 在位且未勾 + 归属 §4 |
| **C5-a…C5-c** | ✅ | 旧口径计数 = **0** · 「已验证 = 10」/「待补期望源 = 0」在位 · tsv `status` **10 行全 `verified`** · `kind=target` = 9 |
| **C7-a** 全量复验 | ✅ | 9 个护栏全 exit 0 · `update_test_count_anchors.sh --check` 5/5 · `ctest -N` = **263** · `ctest` 全量 **263/263 PASS** |
| **C7-b** HARD-GATE | ✅ **7/7** | 逐份 PASS |
| **C7-c** 零实现面变更 | ✅ | `git status` 仅 **文档类**路径（`.rddf/**` · `AGENTS.md` · `improvement-suggestions.md`）⇒ ⇒ **零** `src/**` · `include/**` · `tests/**` · `examples/**` · `scripts/**` |
| **C7-e** 抽样复核 | ✅ **5/5** | 沿用 §9.2 的**确定性**抽样（第 1/4/7/10/13 条）⇒ ⇒ 人工核对证据指针支撑结论 ⇒ ⇒ 全成立 |

### 9.5.4 本轮**未**做（**如实登记**）

| # | 未做 | 理由 |
|---|---|---|
| 1 | **未执行**后继清单（`rtl-track-followup-list-2026-11.md`）的修订 | 该清单 `状态 = 待审` ⇒ ⇒ 按其自身门禁（oracle → critic 通过后才允许修订）⇒ ⇒ 本轮**只建清单**，**不动**那 2 份草案的 AC |
| 2 | **未**给那 2 份草案回填 AC（40 条） | 同上 |
| 3 | **未**碰 B3 / B4 的任何 work item | 归 builder / architect |
| 4 | **未**新增/删除 ctest 条目 | 本轮是纯文档修订 ⇒ ⇒ `CTEST_TOTAL` 仍 **263** |
| 5 | **未**复核 `C7-e` 之外的**全部** checked AC 的证据**强度** | C2-a 是形式判据 ⇒ ⇒ 证据强度只能抽样（**明示局限**已在 §2.5 C2-a 写明）⇒ ⇒ 本轮抽 5 条 + 本轮**新补的 7 条**逐条人工核对 |

### 9.5.5 给下一个会话的**一句话**

⇒ ⇒ 本清单的 **5 条漂移已闭**（§9.5.2）⇒ ⇒ 但 **D5 的病根未闭**：另 2 份草案仍是同型漂移 ⇒ ⇒ **下一个会话先做 `.rddf/checks/rtl-track-followup-list-2026-11.md`**（待审 ⇒ 先送 oracle → critic）⇒ ⇒ 再考虑 B3 的两道门（R5 = ADR-069 · R6 = 公开面前置门）。

---
