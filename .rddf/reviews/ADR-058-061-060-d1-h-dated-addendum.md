# 审稿报告：ADR-058 / ADR-060 / ADR-061 / ADR-062 / ADR-066 的 dated addendum（ADR-072 D1-f / D1-h 对齐）

- 产出角色：**architect**（主会话 · 用户 2026-11-17 口令「我现在切 architect」）
- 关联 change：`rtl-productization-surface-2026-11`（**T8**）
- 审稿轮次：**oracle 1 轮 + critic 1 轮**（arch 档 · 按 ADR-069 §4 的 dated-addendum 纪律）
- 审查对象：`docs/adr/ADR-058-hierarchical-rtl-output.md` · `ADR-060-hierarchical-port-policy-and-module-reuse.md` ·
  `ADR-061-hierarchical-granularity-user-control.md` ·
  `ADR-062-hierarchical-unit-interface-typing.md` ·
  `ADR-066-hierarchical-parent-side-landing-semantics.md`（各追加一个 dated 段 + 元数据 / 变更记录行）
- 结论：**critic = APPROVE_WITH_CONDITIONS（mergeable: yes · scope_exceeded: false · 0 BLOCKER）**
  ⇒ ⇒ **oracle 第 1 轮的 1 BLOCKER + 2 SHOULD_FIX 已全部闭合**，critic 的 5 SHOULD_FIX + 1 NIT 已全部处置。

---

## 1. 意图检查点（architect 路径 · 判定前提）

| 项 | 内容 |
|---|---|
| **意图陈述** | ADR-072 D1-f / D1-h 已 Accepted 且**实施落地**（后代 `ch_out` 不上任何 module 的端口表；悬空后代 `ch_in` 改 fail-closed；冒泡机制物理退役）⇒ ⇒ 本批**只追加** dated addendum，把五份 Accepted ADR 中与该退役**冲突或陈旧**的陈述**逐字保留**并加 dated 指针，使仓内 Accepted ADR 之间不再存在「一份说冒泡、一份说不冒泡」的**持久矛盾** |
| **范围边界** | ① 只允许「追加 dated 段 + Status 版本号 + 关联 change 指针 + 审稿指针 + 变更记录新增一行」；② **禁**改写任何既有决策句；③ **禁**新增 / 删除决策；④ **禁**改 Status 的 Accepted 判定；⑤ **禁**碰 capability 文本（T7 范围） |
| **歧义枚举** | metis 未先行 —— 意图由**用户裁决**（AC18 口径）+ **ADR-072 的 D1-f / D1-h 决策本体**唯一确定，**不存在 ≥2 种合理解读** ⇒ ⇒ 直接进入 oracle |
| **审查域** | **其他审查**（项目内 `docs/adr/**`；非 `~/.config/opencode/**` 治理面） |

## 2. oracle 第 1 轮（对抗式：会怎么坏）

**判定 = REJECT**（结构手段正确，但**带一条新矛盾**落盘 ⇒ ⇒ 不得收口）。

### 2.1 BLOCKER-1 —— ADR-060 §7.2 与 ADR-072 D1-f 界定表 / change AC17 直接矛盾

- **原文（我写的）**：ADR-060 §7.2 写「`Reject` 的适用范围」那段**仍然成立**，且
  「① 根 module 自己的 `ch_in` 与 ② **inline 折叠进根 module** 的悬空 `ch_in` **不触发拒绝**」，
  同时又在同段末尾写「产品路径里仍**必须**显式 tie-off（否则**拒绝**）」⇒ ⇒ **同段自相矛盾**。
- **冲突对象**：ADR-072 §2.1.7 的 D1-f 界定表（`inline` 折叠进根的后代 `ch_in` ⇒ `owner` ≠ 根 ⇒
  🔴 **拒绝**）+ change proposal **AC17**（`RootUndrivenTop` 用例 MUST `ok == false`）。
- **处置**：**已改**（ADR-060 §7.2 改为「v1.13 适用范围 = **半退役**」的两行表：① 根自有 ⇒ 放行；
  ② inline 折叠进根 ⇒ **产品路径 MUST 拒绝**）⇒ ⇒ 并**写明旧裁决退役的理由**（v1.13 的理由是
  「扁平里它本就是顶层 input」⇒ ⇒ 该**前提**在 D1-f 下不成立）⇒ ⇒ 与 D1-f 界定表逐字对齐。

### 2.2 SHOULD_FIX-1 —— 三份 ADR 内的**从属句**漏收（处置清单不全）

| 漏收位置 | 处置 |
|---|---|
| ADR-060 §2.4 默认值段（`undriven_policy = Bubble`） | 已补进 §7.4 处置清单（**订正** = 默认 `Reject`） |
| ADR-060 §4 负面（顶层会出现「子模块的悬空输入」端口） | 已补进 §7.4 处置清单（**退役**） |
| ADR-060 §5 **效果判据**（悬空子输入 MUST 表现为顶层可驱动输入） | 已补进 §7.4 处置清单（**退役** · fail-closed 后无产物可驱动） |
| ADR-061 §3 选项 9/10/11 · §4 风险 3 · §5 判据 | 已补 **§7.5 处置清单**（原 §7.5 零变更 → §7.6，Verification → §7.7） |
| ADR-061 §2.2 ⓘ「子 `input` MUST **冒泡**成顶层 input」 | 已补进 §7.1「连带失效」（并注明「MUST NOT 空连接」那半句**仍成立**） |
| ADR-058 §2.2 ③① 括注 / §2.1 括注（遗留端口面口径） | 已补进 §7.3 末 + §7.4 措辞澄清（**不改写**括注） |

### 2.3 SHOULD_FIX-2 —— ADR-062 / ADR-066 仍有指向已删符号 / 已退役章节的**活指针**

- **证据**：ADR-062 的 §5 引用 `PortProvenanceOrigin` 的 `Bubbled` / `InlineFolded`（**已物理删除**）
  + §2 落点表与 §5「五类来源」判据建立在「后代 `ch_out` 上端口」+「悬空输入冒泡」上；
  ADR-066 的 §4「沿既有向上**冒泡** / 拒绝路径」+ §2 的「**不是** ADR-060 的**冒泡章节**」否定式
  区分（该章节已整体退役 ⇒ ⇒ **失去指向对象**）。
- **处置**：**已扩展覆盖**（ADR-062 §7 · ADR-066 §6）⇒ ⇒ 扩展理由写在各段触发说明里
  （⚠️ **这是对 change proposal / ADR-072 T8 清单的扩展** —— 原清单只列 058/061/060）。

### 2.4 oracle 其余判定

- 「只追加」形态与 ADR-069 §4 / ADR-047 §5.5.2.1 先例一致；四个被否的替代方案**全否得对**
  （仅对方案②补了一条 nuance：扩展到 060 之后，**同理**应扩展到 062/066 —— 已采纳）。
- ⚠️ **oracle 的权限拒绝如实登记**：本轮两次只读 shell 核验被拒 ⇒ ⇒ 「仓内 49 处调用点」
  「审稿文件存在性」「`test_pipeline_dual.cpp` 的 tie-off 实例」「ADR-066 是否已有自己的 dated
  addendum」四项**未经 oracle 独立核实** ⇒ ⇒ 已由 critic 第 1 轮 + architect 本人复核补齐
  （见 §4 的「独立复核」表）。

## 3. critic 第 1 轮（arch 档）

**结论 = APPROVE_WITH_CONDITIONS**（`mergeable: yes` · `scope_exceeded: false` · **0 BLOCKER**）。

### 3.1 critic 认定的通过项（摘录）

- 走的是 ADR-069 §4 确立的 dated-addendum 纪律（追加 ≠ 改写 · 原文逐字保留 · Status 保持 Accepted
  · 零决策改写）⇒ ⇒ **不属于**「与已签批 ADR 冲突未走流程」；
- ADR-060 §7.2 的**半退役**写法与 ADR-072 D1-f 界定表**逐字一致**（oracle BLOCKER-1 确已闭合）；
- **无误标退役**（`__if_`/`__x_` 合成端口规则 · G0 chain builder · ADR-061 §2.2 第一条 ·
  ADR-062 §7.4 本体 · ADR-066 §6.1 结论）⇒ ⇒ 退役面切分正确；
- **横向一致**：「ctx-only 遗留路径仍逐字节不变」措辞一致；「backend 端口面 ≠ 产品端口面」无被误读面；
- **残留指针已清**：全仓 `docs/adr/**` 核毕 —— ADR-059/063/064/065/067/068/069/070/071 对
  `冒泡|Bubble|Bubbled|InlineFolded|port_table` **零命中**；**ADR-073** 的 7 处「冒泡」引用逐条核对
  = **历史态描述**或**「零冒泡端口」判据**（其 J-1 / V2 断言的是「零冒泡」）⇒ ⇒ **不构成活指针**；
  ADR-072 的引用是 D1-h 的**决策本体** ⇒ ⇒ 无需 addendum；
- **扩展到 062/066 恰当**（两文件确有指向已删符号 / 已退役章节的活指针）。

### 3.2 critic 的 SHOULD_FIX + NIT —— 逐条处置

| # | critic finding | 处置 |
|---|---|---|
| S1 | ADR-062 版本号**碰撞**（新增 v1.5 与既有 v1.5 重复；且 Status「当前 v1.5」**低于** changelog 已有的 v1.6 / v1.7） | **已改**：新增版本改为 **v1.8**（Status + §7 头 + 变更记录行三处同步） |
| S2 | ADR-061 版本号**碰撞**（新增 v1.6.5 与既有 v1.6.5 重复） | **已改**：改为 **v1.6.6**（三处同步） |
| S3 | 五份 §7/§6 头引用的 `.rddf/reviews/ADR-058-061-060-d1-h-dated-addendum.md` **当时不在工作树** ⇒ ⇒ 5 个 Accepted ADR 会悬空 | **已落**：本文件即该路径（**文件名与仓内惯例一致**，critic 已核对） |
| S4 | change proposal 的 **T8 文本与实际范围不符**（仍写「ADR_058 / ADR_061」）⇒ ⇒ 归档门「外部依赖可追踪」会判漏项 | **已处置（跨角色）**：proposal 属 planner 域 ⇒ ⇒ **architect 不改** ⇒ ⇒ 由 **builder** 在同批提交里同步 T8 措辞（见 §5 的交接项） |
| S5 | ADR-062 §7.2 的「四类」计数与自列判据不自洽（产品路径实际枚举 ①③⑤ 三类，② 恒空） | **已改**：写明「**有效来源 = ①③⑤ 共三类**（② 在产品路径下**恒空**，其『遗留路径来源』身份保留但**不计入**产品路径的来源计数）」 |
| N1 | 变更记录新增行插在表头**之前** ⇒ markdown 表断 | **已改**：四份 ADR 的新增行移到表头分隔行**之后** |

### 3.3 critic 未提、但 architect 自查发现并已修的一处

- **ADR-058 §7.1 的「49 处调用点」是**旧口径数字**（加带根入口**之前**、路径集更窄）⇒ ⇒ 实测
  `grep -rn "toVerilog(" src/ tests/ samples/ examples/ include/` = **71** 行（含声明 / 定义 / 注释）
  ⇒ ⇒ 带根重载占 **3** 行 ⇒ ⇒ ctx-only 相关 **68** 行 ⇒ ⇒ **已改为不写死数字**（判据 =「这 68 行
  签名零改动」⇒ ⇒ 可用 `git diff` 逐行核对）⇒ ⇒ 避免下一个读者按旧数字对账失败。

## 4. 独立复核（architect 本人 · 补 oracle 权限拒绝留下的四个空缺）

| 空缺项 | 复核结果 | 手段 |
|---|---|---|
| ctx-only 调用点计数 | **71** 处出现（含声明/定义/注释）· 带根 3 · ctx-only 68 | `grep -rn "toVerilog(" src/ tests/ samples/ examples/ include/` |
| 审稿文件存在性 | 本文件落盘后，五份 §7/§6 头的引用路径**全部命中** | `ls .rddf/reviews/ADR-058-061-060-d1-h-dated-addendum.md` |
| `test_pipeline_dual.cpp` 的 tie-off 实例 | **存在**（`:121-124` · `:171-172`）⇒ ⇒ ADR-060 §7.2 的引用**属实** | `sed -n` 读该文件 |
| ADR-066 是否已有自己的 dated addendum | **无** ⇒ ⇒ 本批新建 §6 无版本碰撞 | `grep -n "^## " docs/adr/ADR-066-*.md` |

## 5. 收口状态（ADR 完成的标志逐条）

| 标志 | 状态 | 证据 |
|---|---|---|
| 四要素齐（元数据 / 选项对比 / 后果 / Verification） | ✅ | 既有四要素**未动**；本批各 dated 段自带 **Verification + AC 映射** |
| oracle + critic 双审完成 | ✅ | oracle 1 轮（REJECT → 闭合）+ critic 1 轮（APPROVE_WITH_CONDITIONS · 0 BLOCKER） |
| BLOCKER 全闭合并复审确认 | ✅ | oracle BLOCKER-1 已闭合（critic 逐字核对 D1-f 界定表 + AC17 后判定通过） |
| Status 从 Draft 转已决 | ➖ **不适用** | 五份 ADR 均为 **Accepted**；dated addendum **零决策变更** ⇒ ⇒ Status **保持** Accepted（**不是** Draft ⇒ ⇒ 无需转换） |
| 关联 change 已落库 | ✅ | 五份元数据均登记 `rtl-productization-surface-2026-11`（**T8**） |
| 审稿报告已落 `.rddf/reviews/` | ✅ | 本文件 |

## 6. 交接给 builder 的两项（**跨角色**，不改 architect 产物）

1. **change proposal 的 T8 措辞同步**（critic S4）—— proposal 属 planner 域 ⇒ ⇒ architect **不改**
   ⇒ ⇒ 由 builder 在同一批提交里把「ADR_058 / ADR_061」改为实际交付的**五份**
   （058 / 060 / 061 / 062 / 066）⇒ ⇒ 归档门「外部依赖可追踪」才判得过。
2. **change tasks.md 的 B10 勾选与实测数字回填**（oracle 1 + critic 1 · 0 BLOCKER · 5 SHOULD_FIX 闭合）。

## 7. 遗留（**不在本批范围**，登记而不静默清理）

- **ADR-073** 的 7 处「冒泡」引用：经 critic 逐条核对 = **历史态描述**或**「零冒泡端口」判据**
  ⇒ ⇒ **不构成活指针** ⇒ ⇒ **无需** addendum（**结论已记入本报告**，避免下一轮重复排查）。
- **ADR-072** 自身对冒泡机制的引用 = D1-h 的**决策本体**（不是残留）⇒ ⇒ 无需改。
