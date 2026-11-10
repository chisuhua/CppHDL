# critic 第 3 轮审查结论（2026-11-10 · 端口命名 parity 批）

> ⚠️ **本文件是 critic 第 3 轮的输出誊写**（审查者只读 ⇒ ⇒ 由 planner 代为落盘）
> ⇒ ⇒ **用途 = 第 4 轮复审的仓内基准**（「上轮 BLOCKER 闭合」的逐条依据行）
> ⇒ ⇒ **不是** planner 的自检，**不是** architect 的自检。
> ⚠️ **scope**：本轮由**用户明确批准**（critic 预算 2/2 已用满后开闸）⇒ ⇒ 书面 scope 限定为
> 「① 上轮 7 条 BLOCKER 是否闭合 ② `1dc1d98..HEAD` 新 diff 是否引入新 BLOCKER」
> ⇒ ⇒ **禁**重开已裁项（B1–B4 / G3 / G4 / ⑨ / oracle 建议 1/2/3 / 2026-11-10 用户拍板项）。
>
> **审查者**：critic · **轮次**：3 · **profile**：`plan, arch`
> **结论**：`REJECT` · `mergeable: no` · `scope_exceeded: true`

---

## 1. 上轮 7 条 BLOCKER —— 逐条闭合判定（**7/7 闭合**）

| # | 上轮项 | 判定 | 依据（现状 file:line） |
|---|---|---|---|
| 1 | **B1** C2 与已签批 spec 冲突 | **闭合** | `rtl-emit-product-tooling:62`（C2 撤回段 · 引 `rtl-hierarchical-output/spec.md:309-316`）· `:174`（AC 改为 `is_top_io` 集合 == 7 in + 27 out）· `:181`（`mem_is_load` 判据反转）· `:185`（删恒假 AC）· `:287`（待裁决段改标已裁决）。契约原文 `openspec/specs/rtl-hierarchical-output/spec.md:309-316` 与草案口径一致 ⇒ ⇒ 无二义。⚠️ 实质闭合内容在基线 `1dc1d98` 已存在，本 diff 新增的只是 `:287` 的「已裁决」标注。 |
| 2 | **B2** ⑥「剥根前缀」落点不可实现 ⇒ 两步走 | **闭合** | `port-naming-ratchet:69`（N5 = 两步走 · 第 1 步 manifest 双字段 · 第 2 步独立立项 + MUST NOT 改 `build_verilog_name_table`）· `:261`（已裁决）· 批次表 `:76`（第 1 步在 B1）· `:83`（第 2 步不在 B1）· `:141`（§4 独立立项）。⚠️ `:141` 复制了被 sister `:47` 证伪的回退条件 ⇒ ⇒ 另见 SHOULD_FIX-2（**不构成「闭合但引入新矛盾」**：route (c) 的结论未被改动）。 |
| 3 | **B3** C5 缺 C6 ⇒ 编译必红 | **闭合** | `rv32i-dead-component-consolidation:71`（C5 MUST 与 C6 同批 + `hazard_detection_unit.h:19` include `rv32i_forwarding.h` ⇒ 单删必红 + C6 里先断 include 再删头） |
| 4 | **B4** T2′(iii) 写可见性无判据 ⇒ T6 | **闭合** | `memory-timing-contract:62`（T6 定义 · 只能 `read-new`）· `:150`（紧邻 `SW`/`LW` 同址判别性 AC + 登记「现固件隔 4 字 ⇒ 零判别力」）· 批次表 `:99` |
| 5 | **G4** G1 未编排 ⇒ 批内顺序 | **闭合** | `rv32i-isa-gap-closure:60`（③ 尾部填 NOP → G1+G4 → 其余 · 含「反序 ⇒ golden 改两次」论证）· `:263` · 批次表 `:31`（依据③）· `:110` |
| 6 | **E6** 裁决未传到 AC | **闭合** | `examples-rtl-sim-coverage:65`（E6 行）· `:178`（已裁标记）· **`:216`（AC 已落地）** · `:219`（墙钟分项） |
| 7 | **dtcm size 差 4 倍** | **闭合** | `firmware-load-chain:104`（`1 << (kTcmAddrWidth + 2)` = 16 KB，注明 `kTcmAddrWidth = 12` 是字索引宽度）· 批次表 `:101` |

⇒ ⇒ **上轮 7 条全部闭合**，无「未闭合」、无「闭合但引入新矛盾」。

---

## 2. 🔴 新引入的 BLOCKER（5 条）

### BLOCKER-1 —— R1-a 把**常态违约**的 MUST 冻进主 spec

**问题**：R1-a（`decision-request-port-naming-parity-2026-11.md` §2 R1-a）要求「顶层 module 端口表中的**每个**端口 MUST 由 manifest 的 `cpp_member` 字段承载」。

**反证**：顶层端口表**必含** `default_clock` / `default_reset`（`openspec/specs/rtl-hierarchical-output/spec.md:312`：「顶层端口表 MUST 是 clock → reset → 无 driver 的 input → **全部** `ch_out`」）⇒ ⇒ 二者是 **ctx 级**基础设施节点（`include/core/context.h:245-246` 的 `core::clockimpl *default_clock_` / `core::resetimpl *default_reset_`）⇒ ⇒ **无 C++ 成员** ⇒ ⇒ **无 `cpp_member`**。

⇒ ⇒ 后果：**每个设计都违反该 MUST**（常态违约 = 不可违反的 MUST ⇒ 永久技术债）。
⇒ ⇒ **自证**：本仓自己写的 manifest 样例（`rtl-emit-product-tooling:132`）`{"name": "default_clock", "dir": "in", "width": 1, "role": "clock"}` **就没有 `cpp_member`**。

---

### BLOCKER-2 —— 「空名哨兵语义 MUST 明写」在两份草案间**冲突**

**问题**：`port-name-literal-parity` 要求把「空名哨兵（`"unnamed"`）的语义 MUST 明写」写成契约条款（`:86` P2 ② · `:147` · `:217`）；而 `node-name-prefix-consistency:141` **明令禁**加（metis finding 15 已判定该 MUST 不可违反 ⇒ 属 AI slop）⇒ ⇒ 同一纪律在同一 capability 上一留一删。

⇒ ⇒ 附带：sister 自己的 AC「契约里无不可满足条款」**被自己的 P2 违反**。

---

### BLOCKER-3 —— delta 形态三方不一致 ⇒ ⇒ **静默不合并**

**问题**：`port-name-literal-parity:216`/`:217` 的 Capabilities 表写 **`MODIFIED`**；`node-name-prefix-consistency:186` 写 **`ADDED`**；`decision-request` 规定 **`ADDED`**。

⇒ ⇒ 后果：照 sister 的表写 delta ⇒ `scripts/verify_openspec_delta.sh` R3 要求 MODIFIED 标题与主 spec **逐字一致** ⇒ ⇒ 否则**静默不合并**（最坏的失败模式：CI 不报错，但 spec 永远不变）。

---

### BLOCKER-4 —— 同一义务**三处落点**且文本**不等价**

**问题**：

| 落点 | 独有子句 |
|---|---|
| `port-naming-ratchet:139`（N4） | 「未显式命名者 MUST 走**兼容去重路径**（且 MUST 被登记在棘轮豁免表）」 |
| `port-name-literal-parity:86`（P2） | 「**成立条件 / 不可能条件 MUST 明写**」 |
| `decision-request` R1-a（§2） | **两者都缺** |

⇒ ⇒ 后果：破 `port-naming-ratchet` 自己写的「**规则只此一份**」⇒ ⇒ 且「兼容去重 + 豁免表」这条**机制义务**在唯一落地文本里**消失**。

---

### BLOCKER-5 —— B0 ⑧ 修 SLTU ⇒ ⇒ B4 的 C1 门槛**不可满足**

**问题**：B0（批次表 `:58`）修掉 `rv32i_alu.h:93` 的 SLTU 缺陷；B4 的 C1（`rv32i-dead-component-consolidation:163`）要求「差异清单 MUST 至少含 SLTU 借位 ⇒ ⇒ **缺 SLTU 视为清单不合格**」⇒ ⇒ 到 B4 时 SLTU **已不是差异** ⇒ ⇒ **卡在 B4**。

---

## 3. SHOULD_FIX（6 条）

| # | 项 |
|---|---|
| **1** | 批次表 §0 的 12 条重建依据里 **6 条 file:line 错位** |
| **2** | 批次表 `:141` **复制了已被证伪的 N5(c) 回退条件**（探针实测得 `unnamed_prog_addr` = **第三形态**，既非裸 `prog_addr` 也非 `_prog_addr`）⇒ ⇒ 该回退条件**已被证伪** |
| **3** | **A7 已落地但 3 处叙述未回填**：sister `:286-296` 仍写「仅剩 1 项」「PASS/20 行」· 批次表 `:172` 仍写「Blocked · 待切 architect」· `improvement-suggestions.md` 索引两行过时 |
| **4** | R2 段「逐字可执行」已被 architect 落地时改写（丢了 `spec:62` 定位）⇒ ⇒ decision-request `:123` 与台账 `:299` 措辞已漂移 |
| **5** | `decision-request:92` 引用的复核记录文件名**缺 `-10`**（实为 `...-2026-11-10.md`） |
| **6** | ① B0 准则「零交付风险」与 **F-A 例外**未同处声明（F-A = 下游可见行为变更）· ② ratchet「共用写点」依据存疑 · ③ B3 的**公开面前置门**未登记 |

---

## 4. NITS / 验证建议（critic 给出，不计入 BLOCKERS）

- 超出本轮 diff 范围的问题（历史遗留 / 其它草案 / 仓内代码现状）已标 `scope_exceeded: true`
- 批次表 `:141` 的伪回退条件应改为「**已实测 = `unnamed_prog_addr`**」⇒ ⇒ 独立立项的前置条件 = 需要**新的命名机制**（非清空实例名）
- 护栏实测口径见 `.rddf/roadmap/phase-6-rtl-productization.md` §5

---

## 5. 修复轮次登记

| 轮次 | 时间 | 结论 | 处置 |
|---|---|---|---|
| 1 | 2026-11-09 | REJECT（5 条 BLOCKER） | 全部修复（metis/oracle 轮） |
| 2 | 2026-11-10 | REJECT（2 条新增 BLOCKER） | 全部修复 + 新增 2 份草案 + 批次表 + decision-request |
| **3** | **2026-11-10** | **REJECT（上轮 7/7 闭合 · 新引入 5 条 BLOCKER）** | **本文件落盘后进入第 4 轮复审**（需用户再批准） |

⇒ ⇒ **第 4 轮的书面 scope** = 「① 上轮 5 条 BLOCKER 是否闭合（逐条给依据行）② 第 4 轮新 diff 是否引入新 BLOCKER」⇒ ⇒ **禁**全量重审、**禁**开放式找漏洞。
