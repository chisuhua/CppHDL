# 审查报告：T3-005（`hierarchical-rtl-equivalence-2026-11`）实现完成后的遗留缺陷与债务

- 日期：2026-11-06
- 角色：architect（实施后审查 · **只审，不改实现**）
- 触发：用户口令「切 architect 审查是否有遗留缺陷或债务，通过后再走 V1 归档门」
- 范围：T3-005 的**实现侧产物**（backend 新入口 · 映射表 · 等价性 harness · 5 个 DUT · 判据）
  + 归档前的一致性（delta / proposal / tasks / ADR / 台账 / 护栏）
- 结论：**4 项须在归档前收口（均为「判据/措辞」级 · 成本低）** + **1 项裁决**（已记录）
  ⇒ ⇒ **不得直接进 V1 归档门** ⇒ ⇒ 先由 builder 收口 F1–F4，再走 V1。

## 1. 逐条核对结果（判据来源 → 现状 → 裁决）

### F1（**须收口**）proposal 的 `O` 下限措辞与实现冲突

- **判据来源**：`proposal.md` §Acceptance「**观测集合 `O` 的下限**：`O` MUST ⊇ DUT 的**全部** `ch_out`」
- **现状**：成层 DUT（`EqTop` / `EqDeepTop` / …）的**子 module 的 `ch_out` 在层次化顶层不可观测**
  ⇒ ⇒ 实现只观测「两侧顶层都可见」的那部分（ADR-058 §4 风险 6 的盲区）⇒ ⇒
  proposal 这条验收**按字面未被满足**。
- **对照**：`delta` 的 ADDED Requirement ④ 只要求「`O`（**非空**且**至少一个随激励变化**）」
  ⇒ ⇒ **delta 是落地契约**（归档时并入主 spec），**proposal 的措辞更严** ⇒ 二者**不自洽**。
- **裁决**：proposal 的该条 MUST 订正为「`O` MUST ⊇ DUT 的**全部** `ch_out`；其中
  **在层次化顶层不可观测**的那部分（成层子树的 `ch_out`）MUST NOT 声称已比对
  ⇒ ⇒ 与 delta ④ + ADR-058 §2.2 ② + §4 风险 6 三处对齐」。
  ⇒ ⇒ 归属：builder（change 文档）。

### F2（**须收口**）观测盲区缺**机械**声明

- **判据来源**：`delta` 的 Scenario「观测盲区 MUST 不被当成「已全覆盖」」→
  「判据 MUST **显式声明**它不在观测面内（MUST NOT 声称「全部输出已比对」）」
- **现状**：测试代码里**只有注释**声明盲区（`tests/test_hierarchy_equivalence.cpp:137-139`）
  ⇒ ⇒ 没有**机械断言** ⇒ ⇒ 判据可以被「悄悄把 `O` 缩到一个恒 0 信号」而不被发现。
- **裁决**：补一条机械判据：对**成层** DUT，MUST 断言「DUT 的 `ch_out` 数量 **>** `O` 的数量」
  ⇒ ⇒ 即「盲区**确实存在**且被**显式**声明」⇒ ⇒ 归属：builder（测试）。

### F3（**须收口**）映射表「覆盖被观测的全部信号」缺机械断言

- **判据来源**：`proposal.md` §Acceptance「**映射表覆盖被观测的全部信号**（MUST 是**四元组**）」
- **现状**：实现只断言了「映射表的 `node_id` ↔ ctx 名表逐字相同」（`[equivalence][layered]`）
  ⇒ ⇒ **没有**断言「每个被观测信号的 `node_id` ∈ 映射表」。
- **裁决**：补断言 —— 每个被观测信号 ⇒ ① 其 `node_id` ∈ 映射表 ∧ ② 其 `hier_top_port_name` **非空**
  （= 它在层次化顶层 ⇒ ⇒ 可观测 ⇒ ⇒ 该断言同时把「可观测性」与「映射表」绑在一起）
  ⇒ ⇒ 归属：builder（测试）。

### F4（**建议收口** · 成本极低）delta 要求的「两次 emit 共享同一 ctx」缺**显式**断言

- **判据来源**：`delta` 的 Scenario「映射表 MUST 由 emitter 同次产出」→
  「harness MUST 断言「两次 emit 共享同一 ctx」」
- **现状**：现状是**构造上保证**（同一个 `ch_device`）+ **隐含**（`[determinism]` 逐项比对
  `node_id` ⇒ 不同 ctx 的 `node_id` 无可比性 ⇒ 逐项相同即等价于「同 ctx」）。
- **裁决**：现状**实质满足**，但 MUST 断言是**显式**要求 ⇒ ⇒ 补一句显式断言
  （比较两次发射的 ctx 指针 / 或就地把「逐项 `node_id` 相同 ⇒ 同 ctx」写成具名断言 + 注释）
  ⇒ ⇒ 归属：builder（测试）。

### F5（**裁决已记录** · 非本 change 的债）盲区「三处登记」的时序

- **判据来源**：ADR-058 §4 风险 6 ⇒ 「MUST 在 **capability spec + 用法指南**同步登记」；
  roadmap 的 **P0-4** ⇒ 「归档前人工核对三处齐」。
- **现状**：ADR ✓ / capability spec ✓（随本 change 的 delta 归档落地）/ **用法指南 ✗**
  （`docs/usage_guide/**` 零处「盲区」提及 ⇒ 该处归 **T3-006 的 D1**）。
  ⇒ ⇒ **P0-4 的「归档前核对三处齐」与「用法指南归 T3-006」自相矛盾**（本 change 不可能同时满足）。
- **裁决（architect · 2026-11-06）**：**登记时序** =
  「ADR + capability spec 两处**随 T3-005 归档**；用法指南那处**随 T3-006 的 D1**」
  ⇒ ⇒ 理由：两份用户面文档**不得被两个 change 同时改**（冲突面）⇒ ⇒
  **P0-4 的「归档前核对三处齐」MUST 改读为「核对 ADR + capability spec 两处已齐，
  并确认用法指南那处已登记在 T3-006 的 D1 且未被遗漏」**。
  ⇒ ⇒ 归属：roadmap 口径 = planner；change 侧记账 = builder。

## 2. 本次审查**同时收口**的文档债（architect 自有产物）

| # | 位置 | 问题 | 处置 |
|---|---|---|---|
| D1 | `docs/adr/ADR-058-hierarchical-rtl-output.md` §4 风险 3 | 「层次化产物**不被** backend 消费 ⇒ 两套真值长期存在」⇒ ⇒ **已被 T3-005 证伪** ⇒ 归档后主 ADR 会带一条**假**风险 | ✅ 已改写为「**T3-005 已收口**」+ **残留收窄**为「常规 parity 路径仍只喂扁平」⇒ 归 P8 |
| D2 | `docs/architecture/2026-11-02-hierarchical-rtl-output.md` §7 残留 1 | 同上（「扁平产物仍是 backend 的唯一输入 ⇒ 未决」） | ✅ 已改写为「主体已收口 + 残留归 P8」 |

⇒ ⚠️ 这两条属**同步性**修订（不改变任何裁决方向）⇒ 按 architect 的审查配对走 **oracle**（arch 档）。

## 3. 已核对为「无债 / 已登记」的项（免得重复劳动）

| 项 | 结论 |
|---|---|
| `delta` 的 ADDED Requirement ①–⑤ | ① 映射表 ✓（B1）· ② 输入集合相等 ✓（B2 + 护栏 R8）· ③ 输出 = 精确期望集 ✓（B2 + R8）· ④ 逐拍全等 + 随激励变化 ✓（B5/B6）· ⑤ 盲区声明 ⇒ **文字已声明，机械断言缺 ⇒ F2** |
| `proposal` 的其余验收项 | 5 个 DUT 清单 ✓（B6）· ForceInline/ForceLayer ✓（B6）· 多拍激励 + 寄存器 ✓ · 同一 `node_id` 名字逐字相同 ✓ · 判别力自证 ≥2（实测 3）✓ · 确定性 ✓（B7）· lint 门 ✓（护栏 R1）· 既有 `initialize` 逐字节不变 ✓（B3 的 prelude IDENTICAL）· 零回归 + 锚点 ✓（250/76/66/289） |
| ADR-058 §5 效果判据 | 映射表载体 ✓ · backend 消费路径 ✓ · 编译输入文件名（v2.16）✓ · 观测盲区登记 ⇒ 文字 ✓ / 机械断言 ⇒ **F2** |
| Non-Goals（P6 / P8 公共 harness） | ✅ 均**独立立项**登记（P8 的形态约束已由 T3-005 满足 + P8 守卫 `scripts/verify_hierarchical_p8.sh`） |
| 台账未决项 C2（多份产物拼接）· 端口命名静态护栏 | ✅ 已登记，**不阻塞**本 change 归档 |
| 用法指南盲区登记 | ⏳ 归 **T3-006 D1**（roadmap 明文）⇒ ⇒ **F5** 的时序裁决 |
| AGENTS.md「WHERE TO LOOK」补层次化入口 / backend 新入口 | ⏳ 归 **T3-006 D2**（roadmap 明文）⇒ 非本 change 的债 |
| 遗留清扫（TODO / 半截实现 / 注释代码 / 临时日志 / skip 测试 / 未替换 mock） | ✅ 本次改动文件内**零命中**（`mkdtemp` 模板与既有 `std::cerr` 属存量代码） |
| P8 守卫的 CI 覆盖 | ⚠️ R4（公开 API 面 ⊆ 4 个头）**需 git 历史** ⇒ CI 的浅克隆**明确 NOTE 跳过**（脚本内已写明）⇒ ⇒ R1–R3 仍是 CI 可跑的形态 ⇒ 非隐藏债 |

## 4. 处置清单（交回 builder）

| # | 动作 | 归属 |
|---|---|---|
| F1 | `proposal.md` 的 `O` 下限措辞订正 | builder（change 文档） |
| F2 | 补「盲区**确实存在**且被声明」的机械断言（成层 DUT：`ch_out` 数 **>** `O` 数） | builder（测试） |
| F3 | 补「每个被观测信号 ∈ 映射表 ∧ `hier_top_port_name` 非空」的断言 | builder（测试） |
| F4 | 补「两次 emit 共享同一 ctx」的**显式**断言 | builder（测试） |
| F5 | roadmap P0-4 的「三处齐」口径改读（planner 产物 ⇒ 由 builder 代记账并标注来源） + change 侧登记时序 | builder（记账） |

⇒ ⇒ 上述 5 项收口 + 验证门全绿 ⇒ ⇒ **再走 V1 归档门**（主 spec 两条 grep → 0 / 1）
⇒ ⇒ `openspec archive hierarchical-rtl-equivalence-2026-11`（**禁** `--skip-specs`）。

---

## 5. oracle 审查（arch 档）的追加裁决（2026-11-06 · architect 处置）

> oracle 结论：5 条 finding 的**方向全部正确**（无一判反）⇒ 但**三处细化** + **两处我方措辞过强**
> + **发现 1 个我们自己的交付物缺陷** ⇒ ⇒ 本节逐条处置。

| # | oracle 的发现 | 处置 |
|---|---|---|
| **O1**（高） | 我写的「两套真值的**同源校验已建立**」**过强**：等价性判据比较的是**两份 Verilog 产物**（同源 ⇒ 共享发射底座 ⇒ 底座算错 ⇒ 两边一起错 ⇒ 照样全绿）⇒ ⇒ 只能判「层次化**变换**引入的分叉」 | ✅ **已改写**（ADR-058 §4 风险 3 · v2.17）：改为「『层次化 ≡ 扁平』的**逐拍一致性判据已建立**」+ **点名判别力边界**（不判别共享底座算错 ⇒ 后者仍归既有 parity + 手推期望值义务）⇒ ⇒ 台账残留 4 同步 |
| **O2**（高） | 残留打包错：P8 只覆盖**用户面缺口**（无公共 helper ⇒ 用户设计零等价性证据）⇒ **不**覆盖**语料级回归缺口**（层次化路径在既有语料上零覆盖 · `DUAL_BACKEND_TEST_PAIR` 不参数化粒度）⇒ ⇒ 并进 P8 ⇒ 按 P8 的 done 判据交付后该缺口**静默留存** | ✅ **已拆两条**（台账 §7 残留 1 = 用户面缺口 → P8；残留 **1b** = 语料级回归缺口 → **独立立项** + 显式「MUST NOT 并入 P8」）⇒ ADR §4 风险 3 同步点名 |
| **O3**（中） | 我改了 ADR §4 却**没补 §6 版本行 / header** ⇒ 违反该 ADR 逐版登记的惯例；台账也缺 dated 注记 | ✅ **已补**：ADR-058 **v2.17**（§6 新行 + header 版本）· 台账 §7 加 **2026-11-06** 日期注记 |
| **O4**（中） | **F2 的修法偏弱**：`ch_out` 数 > `O` 数只是 count-tripwire ⇒ **没有**把 delta Scenario 的「盲区信号 MUST **出现在映射表**里（`hier_top_port_name` 为空）」变成机械断言 ⇒ ⇒ 该契约条目仍无载体 | ✅ **F2 升级**：断言做成**对偶** ——「被观测侧」：每个被观测信号 ⇒ `node_id` ∈ 映射表 ∧ `hier_top_port_name` **非空**；「盲区侧」：每个**未**被观测的 `ch_out` ⇒ `node_id` ∈ 映射表 ∧ `hier_top_port_name` **为空** ⇒ ⇒ 与 delta Scenario 逐字对齐 |
| **O5** | **F4 定级不自洽**：delta 写的是「harness **MUST 断言**」⇒ 属 spec 合规缺口 ⇒ 应为「须收口」，而报告标题写「建议收口」⇒ 与总述「4 项须收口」矛盾 | ✅ **F4 升为「须收口」** ⇒ ⇒ 合计 **5 项须收口**（F1–F5）+ 2 项新增（F6/F7） |
| **O6**（中） | ⚠️ **我们自己的交付物里有判据空转**：`tests/test_hierarchy_equivalence.cpp` 的 `deep_obs(..., all_visible=true)` push 了**两条字符级相同**的 `mid.dout`（第二条本意是兄弟叶的 `a/b.dout` ⇒ 早前因 `ch_module` 不暴露孙成员而被替换 ⇒ ⇒ 变成**死重观测** ⇒ 零增量判别力）⇒ ⇒ 且 `[deep]` 的 ForceInline 子用例注释声称「观测面可含**全部** `ch_out`」⇒ 而兄弟叶的 `dout` **从未被观测** | ⇒ ⇒ **新增 F6（须收口）**：第二条改为**兄弟叶的真实输出**（`root.mid.instance().a.io().dout` / `.b` —— `ch_module::instance()` 可达孙成员）⇒ ⇒ 修完 MUST 复跑 `[equivalence][deep]`：若转红 ⇒ 说明兄弟边此前**确实**观测不足（那本身就是这条判据的价值证明）⇒ 若仍绿 ⇒ 记录「此前是死重」 |
| **O7**（低） | F1 的修法措辞仍别扭（既然「⊇ 全部」就又含不可观测信号）⇒ 应直接引用 tasks 2.2 的已登记口径 | ✅ **F1 修法改为**：直接引用 tasks 2.2 的口径「**两侧顶层都可见**的 `ch_out`」+ 盲区**显式登记**（MUST NOT 声称「全部输出已比对」） |
| **O8**（低） | `tasks.md` 的验收证据引用 `/tmp/opencode/cmp_prelude5.py` ⇒ **不可复算** ⇒ 违反「证据必须可复算」 | ⇒ ⇒ **新增 F7（须收口 · 低severity）**：把该证据改成**可复算**形态（既有路径**行为**逐字节不变由 `ctest -LE perf` 全绿 + 缓存 key 不变（ctest 时长与基线同量级）承担；prelude 文本的逐字比对**不入仓** ⇒ 只作过程记录，不作判据引用） |

### 5.1 修订后的处置清单（交回 builder）

| # | 动作 | 归属 | 状态 |
|---|---|---|---|
| F1 | `proposal.md` 的 `O` 下限措辞 ⇒ 引用 tasks 2.2 的口径 | builder | ⏳ 待办 |
| F2 | 盲区断言做成**对偶**（被观测侧 + 盲区侧，均绑到映射表） | builder | ⏳ 待办 |
| F3 | 「每个被观测信号 ∈ 映射表 ∧ `hier_top_port_name` 非空」 | builder | ⏳ 待办 |
| F4 | 「两次 emit 共享同一 ctx」的**显式**断言（升为须收口） | builder | ⏳ 待办 |
| F5 | roadmap P0-4 的「三处齐」口径改读 + change 侧登记时序 | planner / builder | ⏳ 待办 |
| **F6** | **修 `deep_obs` 的死重观测**（第二条 ⇒ 兄弟叶真实输出）+ 补 `obs.size()` 断言 | builder | ⏳ 待办（**新发现**） |
| **F7** | `tasks.md` 的 prelude 证据引用改为**可复算**形态（去掉 /tmp 引用作判据） | builder | ⏳ 待办（**新发现**） |
| D1 | ADR-058 §4 风险 3 改写（含判别力边界点名）+ §6 v2.17 + header | architect | ✅ **已落** |
| D2 | 台账 §7 残留 1 拆两条 + 残留 4 收口 + 日期注记 | architect | ✅ **已落** |

⇒ ⇒ 共 **7 项待 builder 收口** ⇒ ⇒ 收口 + 验证门全绿 ⇒ ⇒ **再走 V1 归档门**。

---

## 6. critic 审查（arch 档）的处置（2026-11-06）

> critic 结论：**APPROVE_WITH_CONDITIONS** · `mergeable: yes` · `scope_exceeded: false` · **0 BLOCKER**
> ⇒ ⇒ 「ADR §4 风险表 + §6 版本史 + header 一致」「决策内容零变更属实」「残留拆分属债务归属记账，
> 不构成未走流程的架构决策」「『MUST NOT 并入 P8』的论证成立，是真实边界而不是拆碎」。

| # | finding | 处置 |
|---|---|---|
| **S1**（should fix） | 台账 §11.11.4 第 3 行仍写「P8 **待 T3-005 harness 落地**」⇒ ⇒ 前置条件**已越过**（harness 已落地）⇒ 与本轮「同步到事实状态」同病 | ✅ **已改**（architect 自有产物）：改为「P8 独立立项 = A4 · 形态约束 = 公共 helper · 本期不在 `include/` 暴露 ⇒ ⇒ **归位条件已达成** ⇒ 待 planner 立项；P6 仍待立项」 |
| **S2**（should fix） | ADR §4 风险 3 与台账 §7 残留 4 变成**近逐字重复**的两份全文 ⇒ 未点名权威 ⇒ ⇒ 下一轮只改一边 = 新漂移源 | ✅ **已改**：ADR 该格**收敛为决策级摘要 + 权威指针**（「残留登记的**权威**在台账 §7」）⇒ ⇒ 台账保留全文 |
| **N1**（nit） | 残留 1b（语料级回归缺口）只在台账有登记 ⇒ feature 文件的 backlog 段没有对应条目 ⇒ planner 扫 feature 立项时可能看不见 | ⇒ ⇒ **登记为 F9**（feature 文件属 planner 域 ⇒ 由 builder 代记账 + 标注来源） |
| **N2**（nit） | 台账 §7 表尾编号顺序 …10, 14, 12（14 排在 12 前） | ⇒ 存量形态，**非本轮引入** ⇒ 仅记录，**本轮不改**（改编号会牵动全文引用） |

### 6.1 critic 的验证建议 ⇒ 已执行

- **grep 复核旧断言**（`不被 Verilator backend 消费` / `两套真值` / `待 T3-005` / `待 T3-⑤`）
  ⇒ 命中 6 处 ⇒ 逐处判定：
  - `ADR-058:317` / `:404` / `:534` / `:551` · 台账 `:277` ⇒ **本轮已改或在版本史里**（历史行不动 ✓）
  - **`docs/developer_guide/patterns/HIERARCHICAL-RTL-AUTHORING-GUIDE.md:240`**（**P7** 痛点行：
    「层次化产物**不被 Verilator backend 消费** ⇒ 用户**不能**直接对自己的设计跑双后端」）
    ⇒ ⇒ **陈旧**：技术入口已具备（T3-005）⇒ 但用户面仍缺（P8）⇒ ⇒ **归 T3-006 的 D1**
    （该指南由 D1 改写）⇒ ⇒ **登记为 F8**（**不得**在本 change 里改 ⇒ 避免两个 change 同改一份用户面文档）
  - **`.rddf/roadmap/features/feat-hierarchical-rtl-output.md:351`**（「P8 待 T3-⑤ 的 harness 落地后立项」）
    ⇒ ⇒ 前置条件**已越过** ⇒ ⇒ **登记为 F9**（feature 文件属 planner 域 ⇒ builder 代记账 + 标注来源）

### 6.2 最终处置清单（交回 builder · 共 9 项）

| # | 动作 | 归属 | 状态 |
|---|---|---|---|
| F1 | `proposal.md` 的 `O` 下限措辞 ⇒ 引用 tasks 2.2 的口径 | builder | ⏳ |
| F2 | 盲区断言做成**对偶**（被观测侧 + 盲区侧，均绑到映射表） | builder | ⏳ |
| F3 | 「每个被观测信号 ∈ 映射表 ∧ `hier_top_port_name` 非空」 | builder | ⏳ |
| F4 | 「两次 emit 共享同一 ctx」的**显式**断言 | builder | ⏳ |
| F5 | roadmap P0-4 的「三处齐」口径改读（planner 域 ⇒ builder 代记账 + 标注来源） | builder（代 planner） | ⏳ |
| **F6** | **修 `deep_obs` 的死重观测**（第二条 ⇒ 兄弟叶真实输出）+ 补 `obs.size()` 断言 + 复跑 `[equivalence][deep]` | builder | ⏳（oracle 发现） |
| **F7** | `tasks.md` 的 prelude 证据引用改为**可复算**形态（去掉 /tmp 引用作判据） | builder | ⏳（oracle 发现） |
| **F8** | 用法指南 **P7 痛点行**的同步 ⇒ **交 T3-006 D1**（本 change **不改**该文档 ⇒ 避免两 change 同改一份用户面文档） | T3-006 | ⏳（登记） |
| **F9** | feature 文件：① 残留 1b 的指针 ② P8 归位条件已达成 ⇒ 待 planner 立项 | builder（代 planner） | ⏳（oracle/critic 发现） |

⇒ architect 侧（D1/D2 + S1/S2）**已全部落地** ⇒ ⇒ 剩 **9 项 builder 侧** ⇒ ⇒
收口 + 验证门全绿 ⇒ ⇒ **再走 V1 归档门**。
