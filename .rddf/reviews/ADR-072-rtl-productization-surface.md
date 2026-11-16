# ADR-072 审稿记录 —— 「RTL 生成能力产品化」顶层端口面收敛 + 产品边界与验收契约

- **对象**: `docs/adr/ADR-072-rtl-productization-surface-and-boundary.md`
- **档位**: `arch`
- **轮次**: oracle ×1 → critic ×2（第 2 轮 = 复审）
- **结论**: **Accepted**（oracle 第 1 轮 REJECT ⇒ 3 BLOCKER 全闭 ⇒ critic 第 1 轮 REJECT / 1 BLOCKER ⇒ 闭合 ⇒ critic 第 2 轮 **APPROVE** · 0 BLOCKER）
- **日期**: 2026-11-16

---

## 1. oracle 第 1 轮（对抗式复核 · 会怎么坏）—— **REJECT**

**判定**：REJECT（3 BLOCKER + 5 SHOULD_FIX）。**3 BLOCKER 均为「范围/契约完整性」缺口，不是对用户已拍板两条分叉的反对** ⇒ ⇒ 修完即可转 APPROVE，**不需重开决策**。

### 1.1 BLOCKER（3 条 · 全部已闭）

| # | BLOCKER | 证据 | 闭合位置 |
|---|---|---|---|
| **B1** | **Verilator backend 的端口面由 ctx 全量推导** ⇒ ⇒ D1 后引用**不存在的 Vtop 字段** ⇒ ⇒ `libVtop.so` 编译必炸 ⇒ ⇒ 全部 auto-mode `verilator_e2e`（**77**）转红 ⇒ ⇒ 「原子落地」排程是假的 | `src/core/verilator_backend.cpp:857-867`（`build_port_access_table` 收全部 `type_output`）+ `:238-343`（`emit_sim_main_postlude` 给每个 `type_output` 生成 `v-><vname>` 访问 case） | §2.1 表 **①** + §5 **V5**（含负向对照） |
| **B2** | **既有 capability 与 D1 逐字相反** ⇒ ⇒ change 过不了归档合并 | `openspec/specs/rtl-hierarchical-output/spec.md:48-50`：「祖先 = RTL 顶层 ⇒ 照扁平规则（任意 `ch_out` ⇒ 顶层 `output`，**包括已被父层消费的** ⇒ **MUST NOT** 降成纯内部 wire）」 | §2.1 表 **②** + §5 **V6** |
| **B3** | **ctx-only 公开 API 无根可推导** ⇒ ⇒ 旧 API 在 D1 后**何去何从没有答案** ⇒ ⇒ 要么保留旧行为 = **永久的第二种产物形态**（正是选项 ③ 以「长期维护两个产物形态」为由拒掉的东西），要么全部迁移 —— **两条都没写** | `include/core/context.h:83-247`（全类**无** Component 注册表）⇒ 「无根」成立；但 `src/rtl_emit/cli.cpp:144-161` 证明**产品路径 100% 持根** ⇒ ⇒ 「拿不到根」是**半真相** · 仓内 **49** 处活跃 ctx-only 调用点 | §2.1「ctx-only 公开 API 的处置」段（**已写死 (a)**）+ 同批必改 **⑩** + §5 **V1** |

### 1.2 SHOULD_FIX（5 条 · 全部已闭）

| # | finding | 闭合位置 |
|---|---|---|
| S1 | **D3 的 `illegal_reason` 落后于 HEAD**（实为 **4 态**：0/1/2/**3 保留组合**） | §1.2 事实 ⑤（4 态）· §2.3 D3（4 态）· 仓内漂移登记 = §6 ⑩ |
| S2 | **D5 的缓解方向错误** —— 「同款但独立实现」会制造**第二份必然漂移的解析**（本仓前例：tsv 列漂移致 10 个发射器失去归属） | §4.3 ③ 改为「**单一共享解析函数**（`scripts/lib/`，`run_all_ported_tests.sh` 与新判据共同 source）+ 空解析 fail-closed + 投毒自证」+ §5 **V14** |
| S3 | **D7 缺作用域** —— 「默认」有两个落点（CLI 默认 vs **库层默认** `EmitOptions{}` / `verilogwriter(ctx)`）⇒ ⇒ 翻库层 = 改 77 个 `verilator_e2e` 的编译形态 + 缓存 key + 文本断言基线 ⇒ ⇒ 「F1-a/F1-b/F8 结构不变 ⇒ 零新增护栏面」只在 CLI 层为真 | §2.7 D7 加作用域句（**只翻 CLI 默认 · 库层不动**）+ §5 **V13**（库层默认未被误伤） |
| S4 | **axi4_lite 的 S8 场景**（oracle 说 4 条） | ⚠️ **architect 实测 = 8 条**（`examples/axi4/tb_axi4_lite_example.sv:393-401`）⇒ ⇒ `checks` 45 → **37**（不是 41）· `scenarios` 8 → 7 ⇒ §1.2 事实 ⑰ + §2.1 表 ④ |
| S5 | **§4.3③ 的 V9 → V12 交叉引用错** + **§6 ⑨「诉求 ⑤ 在顶层闭环」**需加限定 | §5 **V14**（投毒在 V14）+ §6 ⑨ 加「闭环 = 成员名可推导 + `top_` 前缀稳定，**不是**字面同名」 |

### 1.3 oracle 的「未能核实项」（如实登记）

oracle **无执行权限**（`./build/.../emit_rtl_rv32i_soc` 被拒），故「扁平 130 / manifest 213 / `is_top_io:true`=32 / 层次化 68」**未由其亲测**。
⇒ ⇒ **architect 已亲测**（2026-11-16）：发射器实跑 + 只读探针（`/tmp/opencode/hier_probe`）⇒ ⇒ 数字与仓内两个独立来源吻合
（`examples/riscv-mini/tb_rv32i_soc.sv:60` 自述「130（6 输入 + 124 输出 · 2026-11-16 实测）」+ 台账 id=23 的沿革）。

### 1.4 oracle 排除的替代方案（architect 采信）

- **推翻用户已拍板的两条分叉** —— 无失效证据 ⇒ ⇒ 反而**加强**（F1 证明收敛在产品路径可行；F5 证明 10 项口径与 tracked 现实一致）。
- **从 ctx 用名字前缀启发式推导根**（免 API 变更）—— 排除：名字碰撞 / 未登记子树 ⇒ ⇒ **静默**错分类（与 fail-closed 文化相反）。
- **D1 只改扁平、层次化维持 68 端口** —— 排除：破坏「默认全 inline ≡ 扁平」的一致性 ⇒ ⇒ 制造选项 ③ 式双档。
- **D7 翻库层默认** —— 排除（见 S3）。

---

## 2. critic 第 1 轮（冲突审稿 · arch 档）—— **REJECT · 1 BLOCKER**

**判定**：REJECT（1 BLOCKER）· `mergeable: no` · `scope_exceeded: false`。

### 2.1 BLOCKER

> **D1 推翻的成文契约跨 2 份 capability + 2 份 Accepted ADR，而修订/取代流程的范围不闭合**（BLOCKER 第 ① 类：与既有成文契约冲突且未走取代流程）。

**四组证据**（全部经 architect 独立复核成立）：

| 组 | 内容 | 实测原文 |
|---|---|---|
| **(a)** | **`openspec/specs/rtl-emit-portability/spec.md` 整份 capability 被遗忘**：「`is_top_io` 的筛选是消费方的选择 …… **AND MUST NOT 成为发射器的默认行为**（⇒ ⇒ **发射器 MUST NOT 因 `is_top_io` 而改变端口面或端口顺序**）」⇒ ⇒ **与 D1 逐字相反**，且它是**残差 id=23「保持现状」的成文依据** | `rtl-emit-portability/spec.md:57-62`（实测 :61-62 确为该句）· 残差引用 = `.rddf/checks/arch-residual-registry.md:59` + `docs/architecture/2026-11-02-hierarchical-rtl-output.md:277` |
| **(b)** | manifest MUST 携带 `rtl-emit-manifest-v1` 的**全部 10 个字段** ⇒ ⇒ 新增字段 ⇒ ⇒ **schema 版本变更**无伴随条款 | `rtl-emit-portability/spec.md:8-34` · `tests/test_rtl_manifest.cpp:114` 钉 `schema == "rtl-emit-manifest-v1"` |
| **(c)** | `rtl-hierarchical-output` 内部**另有三处**直接矛盾句（② 的「顶层端口表枚举句」措辞含糊 ⇒ ⇒ 实施方无法判断 delta 边界） | `:40`（端口表收缩 MUST **只发生在成层子树**）· `:190-214`（悬空子输入 MUST 向上**冒泡为顶层 input** + 「两份产物**都**暴露为顶层可驱动输入」）· `:321-328`（**排他句（承重）**：层次化顶层 MUST 含**全部** inline 后代的 `ch_out`） |
| **(d)** | `ADR-058:63-64` + `ADR-061:132` 的**决策内容**（写死的枚举「任意 `ch_out` ⇒ 顶层 `output`」，**不是**活指针）被实质修订但无取代/追认动作 ⇒ ⇒ 双权威 | 两处实测确为写死枚举 · 仓内先例 = ADR-060 v1.20 / ADR-062 v1.7（判据收窄时**同步修订 ADR 本体**） |

**critic 的定性（architect 采信）**：D1 的**方向**本身与 ADR-046 D10-rev（冻结的正是根自有 32 个 ⇒ D1 是**对齐**而非冲突）及用户裁决一致；问题只在**取代流程的清单不完整** ⇒ ⇒ 照原清单立项，归档时两份主 spec + 两份 Accepted ADR 仍会冻着与新产品**逐字相反**的条款。

### 2.2 critic 的 SHOULD_FIX（3 条 · 已闭）

| # | finding | 闭合位置 |
|---|---|---|
| C-S1 | **「8 项 MUST 原子落地」的清单不闭合** —— 遗漏承重测试载体：`tests/test_rtl_manifest.cpp:114,228-240`（schema 名 + JSON 字段形状）· `tests/test_rtl_tb_stub.cpp:64-78`（TB 骨架「只连顶层端口」的索引来源 = 产品端口表）· `examples/riscv-mini/tests/test_rv32i_hier_rtl.cpp:218-225`（「层次化端口行 == 扁平端口行**前缀**」判据 —— 扁平变 34 后该前缀关系仍成立与否**未被任何判据覆盖**）· `tests/test_rtl_emit_contract.cpp` · `tests/test_hierarchy_equivalence.cpp`（精确期望集公式 `:198,258`） | §2.1 表 **⑪**（新增行） |
| C-S2 | **写死旧端口规则的用户面/架构文档未进清单** ⇒ ⇒ 「扁平规则」从此**一分为二** ⇒ ⇒ 双权威漂移 | §2.1 表 **⑫**（新增行）：`docs/usage_guide/05-component-design.md:552,680` · `docs/developer_guide/patterns/HIERARCHICAL-RTL-AUTHORING-GUIDE.md:49-55,88` · `PORT-NAMING-GUIDE.md:78` |
| C-S3 | **事实 ⑮ 的「60 处」与实测不符**（critic 口径 ≈ 44）⇒ ⇒ 且 `cli.cpp` 证明**产品路径今天走的就是 ctx-only** ⇒ ⇒ (a) 的论述依赖「新增带根重载 + CLI 切换」，而该动作**未在同批必改中显式落项** | §1.2 事实 ⑮ 订正为 **49**（口径：排除注释行与头文件声明）· 同批必改 **⑩**（CLI 切带根重载）⇒ ⚠️ architect 口径说明：**49** = 含 `src/` 的 5 处；**44** = 不含 `src/` ⇒ ⇒ 两个数**都对**，口径不同 |

### 2.3 critic 的 NIT（2 条 · 已闭）

| # | finding | 闭合位置 |
|---|---|---|
| C-N1 | §5 V12 写「F2–F7」但 `verify_flat_rtl_emit.sh` 的清单是 F1–F6 + F8（**无 F7**） | §5 **V12** 改为「F1–F6 + F8 逐条」 |
| C-N2 | §5 V1 标题「残余探针」与内容不匹配（事实 ⑯ 已判「拿得到根」） | §2.1 条件式块 + §5 **V1** 改写为「**落地确认**」+ 明写「V1 **不是**未知量探针」 |

### 2.4 critic 的裁定（architect 采信，均**不**构成 BLOCKER）

- **ADR-058「层次化顶层端口序复刻扁平序」在 D1 后仍成立** ⇒ ⇒ 无需修订该条款。
- **ADR-070「扁平产物逐字节不变 = 结构性恒真的跳线」未被推翻** ⇒ ⇒ 其范围限「成层声明只写标志位」，D1 不动粒度声明 ⇒ ⇒ **无需修订行**。
- **D1 的条件式形态可接受**（显式标注未决，非「把未决写成已决」）⇒ ⇒ 但 (a)/(b) 应写死 ⇒ ⇒ 已写死 (a)。
- **§6 ⑨「诉求 ⑤ 在顶层闭环」成立** ⇒ ⇒ 闭环定义 = 成员名可推导 + `top_` 前缀稳定 ⇒ ⇒ 与残差 id=21 的 2026-11-15 裁决一致 ⇒ ⇒ 本 ADR **不重开**。
- **§4.3 ⑤ D6「可能降级为文档约定」不成 BLOCKER** ⇒ ⇒ 它**如实标注**未决并要求登记 + 绑定 V15 ⇒ ⇒ 是反方向（承认未决），不是伪装已决。

---

## 3. critic 第 2 轮（**复审**）—— **APPROVE · 0 BLOCKER**

**判定**：**APPROVE** · `mergeable: yes` · `scope_exceeded: false`。

### 3.1 上轮 BLOCKER 的逐组闭合复核（critic 实测原文后确认）

| 上轮证据组 | critic 实测原文位置 | 闭合位置 | 判定 |
|---|---|---|---|
| (a) `rtl-emit-portability:57-62` | 实测 :61-62 确为该句 | §2.1 ② 子项 ①（逐句点名 + 目标措辞） | ✅ **闭合** |
| (b) manifest 10 字段 / schema 版本 | 实测 `:8-34` 在位；`tests/test_rtl_manifest.cpp:114` 钉 v1 | §2.1 ② 子项 ②（v1→v2 + 测试同步） | ✅ **闭合** |
| (c) 三处遗漏 spec 句 | 实测 `:40` / `:190`+`:201-207` / `:324-325` —— 与事实 ⑲ 的引用**逐行吻合** | §2.1 ② 子项 ③④⑤（各附目标措辞） | ✅ **闭合** |
| (d) ADR-058/061 决策内容 | 实测两处确为写死枚举 | §2.1 表 ⑨ + §6 ② 的同批顺序 | ✅ **闭合** |

⇒ ⇒ 上轮 BLOCKER 的根因（「修订范围不闭合」）**已消除** ⇒ ⇒ §2.1 表扩为 **12 行逐句清单**（①–⑫），四组证据各有落点与目标措辞。

### 3.2 本轮新 diff 的新 BLOCKER 判定

**无**。critic 复核了：事实 ⑲ 的六组引用（**行号与内容全部准确**）· D1 条件式（未触 BLOCKER ④ 类）· ctx-only 处置 (a)（无就地扩权口）· 同批必改 ⑩（事实链成立）· V6 的计数一致性 · §6 ② 的顺序含 ⑨。

### 3.3 第 2 轮的 NIT（1 条 · 已闭）

- `src/rtl_emit/cli.cpp` 的 `toVerilog(...)` 实际在 **:181**（architect 原写 :183 = `filesystem::exists` 检查）⇒ ⇒ **已订正** 3 处引用。

---

## 4. 逐条应答汇总（oracle + critic 全 finding）

| 来源 | # | finding | 处置 |
|---|---|---|---|
| oracle | B1 | backend 端口面 ⇒ 77 个 `verilator_e2e` 全红 | ✅ **已修** —— §2.1 表 ① + §5 V5 |
| oracle | B2 | capability 与 D1 逐字相反 | ✅ **已修** —— §2.1 表 ②（**后由 critic 扩为 5 句 + schema**）+ §5 V6 |
| oracle | B3 | ctx-only API 无根 ⇒ 旧 API 无处置 | ✅ **已修** —— §2.1 ctx-only 处置段（**写死 (a)**）+ 表 ⑩ + §5 V1 |
| oracle | S1 | `illegal_reason` 3 态 → 4 态 | ✅ **已修** —— 事实 ⑤ / D3 / §6 ⑩ |
| oracle | S2 | D5 的「独立实现第二份解析」方向错 | ✅ **已修** —— §4.3 ③ + §5 V14 |
| oracle | S3 | D7 缺作用域（CLI 默认 vs 库层默认） | ✅ **已修** —— §2.7 作用域句 + §5 V13 |
| oracle | S4 | axi4_lite S8 判据数 | ✅ **已修（并订正 oracle 的数字）** —— 实测 **8** 条 ⇒ 45 → **37** |
| oracle | S5 | V9→V12 交叉引用 + §6 ⑨ 措辞 | ✅ **已修** —— §5 V14 + §6 ⑨ |
| critic | BLOCKER | 取代流程清单不闭合（(a)–(d) 四组） | ✅ **已修** —— §2.1 表 ② 扩为 5 句 + schema、表 ⑨ |
| critic | C-S1 | 承重测试载体遗漏 | ✅ **已修** —— §2.1 表 ⑪ |
| critic | C-S2 | 写死旧端口规则的文档未进清单 | ✅ **已修** —— §2.1 表 ⑫ |
| critic | C-S3 | 事实 ⑮ 计数 + CLI 切换未落项 | ✅ **已修** —— 订正为 **49** + 表 ⑩ |
| critic | C-N1 | V12 的「F2–F7」无 F7 | ✅ **已修** —— 「F1–F6 + F8」 |
| critic | C-N2 | V1 标题与内容不匹配 | ✅ **已修** —— 「落地确认」 |
| critic（复审） | NIT | `cli.cpp` 行号漂移（:183 → :181） | ✅ **已修** —— 3 处引用已订正 |

⇒ ⇒ **全部 finding 已逐条应答并处置** ⇒ ⇒ **无「列出但不改」的 finding** ⇒ ⇒ **无「未应答的 BLOCKER」**。

---

## 5. 本 ADR 的完成标志核对（`arch` 档）

| # | 标志 | 状态 |
|---|---|---|
| ① | 四要素齐（元数据 / 选项对比表 ≥ 3 行 / 独立「后果/代价」章 / 前瞻性 Verification 章） | ✅（9 行对比表 · §4 · §5 全未勾） |
| ② | oracle + critic 双审完成；critic 报过的 BLOCKER 全部闭合并经 **1 次复审**确认 | ✅（oracle ×1 · critic ×2 · 复审 `mergeable: yes`） |
| ③ | Status 已转已决状态 | ✅ **Accepted**（2026-11-16） |
| ④ | 「关联 change」已落库或已登记 | ✅ `backlog: rtl-productization-surface-2026-11`（**尚未立项** ⇒ ⇒ 立项后由 planner 回填指针 + AC 映射） |
| ⑤ | 审稿报告已落 `.rddf/reviews/` | ✅ 本文件 |

⚠️ **自指审查声明**：本 ADR 的 **D1** 触及 `ADR-058` / `ADR-061` 的决策内容 ⇒ ⇒ 其 dated addendum（同批必改 ⑨）**按仓内先例走复审**（ADR-060 v1.20 / ADR-062 v1.7）⇒ ⇒ 那一步**不是**本轮审查能替代的。

---

# 6. **v2 轮 · oracle 第 1 轮**（对抗式复核 · v2 增量）—— **REJECT**（3 BLOCKER + 3 SHOULD_FIX）

**触发**：planner 的 **metis 第 1 轮**（7 P0 + 3 P1 + 2 P2）+ change 的 **critic 第 1 轮**（plan 档 `APPROVE_WITH_CONDITIONS`）实测推翻 v1 的 7 处 ⇒ ⇒ ADR-072 **v2**。

## 6.1 BLOCKER（3 条 · 全部已闭）

| # | BLOCKER | 证据 | 闭合位置 |
|---|---|---|---|
| **B1** | **R-6 的「D1 下顶层不再有 C 类」= 事实错误** —— 冒泡端口只是顶层 C 类的**一个**来源；另一个来源 = **根组件自有的 legacy 未具名端口**，D1 下**仍是顶层端口** | `examples/axi4/axi4_lite_example.cpp:20-31`（`AxiLiteTop` 的 `__io` 全部 legacy 未具名）· `.rddf/checks/port-naming-exempt.tsv:36-55`（20 个端口登记为「既有债·基线豁免」⇒ C 类）· `.rddf/checks/examples-rtl-sim-targets.tsv:40`（`axi4_lite_example` = 首批 10 个 **verified** 产品目标之一）⇒ ⇒ 照原稿写进 delta ⇒ **已登记在册的 verified 产品立刻违反主 spec** | **撤回 R-6** ⇒ ⇒ **`:637` 不需 delta**（三类判定与「冒泡端口是否存在」**正交**）⇒ ⇒ 列进「不需 delta 的四条」 |
| **B2** | **「inline 折叠进根」的悬空 `ch_in` 未裁决** —— 现行 spec 要求**放行**，D1 契约要求**拒绝** | `openspec/specs/rtl-hierarchical-output/spec.md:192-194`（「折叠进根的悬空 `ch_in` 在扁平里本就是顶层 input ⇒ ⇒ **仍 MUST 产出**」）· Scenario `:213-214`（「inline 折叠进根 ⇒ MUST NOT 触发拒绝」）· `tests/test_hierarchy_verilog.cpp:2181-2192`（`REQUIRE(r.ok)` + 注释「拒绝它们 = 拒绝合法设计」） | **D1-f 扩写**：「inline 折叠进根 ⇒ **拒绝**」+ 明写**取代** `:192-194`/`:213-214`（旧理由「在扁平里本就是顶层 input」的**前提已不成立**）+ 新增 **V18** + 同批必改 ④ 补 `:2181-2192` 与 4 个 `[bubble]` 用例 |
| **B3** | **Status 曾写「Accepted · v2」而 §7 不存在** ⇒ ⇒ 「把未决问题写成已决」（`arch` 档 BLOCKER 第四类） | ADR `:3` / `:380` 引用 §7 · 文档到 §6 就结束 · `.rddf/reviews/` 无任何 v2 记录 | **退回 Draft · v2 待审稿**（本章即其自证） |

## 6.2 SHOULD_FIX（3 条 · 全部已闭）

| # | finding | 闭合位置 |
|---|---|---|
| S1 | **fail-closed 的爆炸半径被低估**，且「同批改期望集」**措辞误导**（fail-closed 后**产物不存在** ⇒ 无表可改）· 漏 6 个同步点（含 `tests/test_component_granularity.cpp:248-254` 的 `static_assert` 把 `Bubble` 钉为 canonical 默认 ⇒ 一退役即编译失败） | 措辞改为「语料**重设计**为拒绝断言或删除」+ 同批必改 ④ 补全（`:2044` / `:2096` / `:2162` / `:2239` / `:2181-2192` / `:2638-2639` / `hier_rtl_products.cpp:114-141,476` / `verify_hierarchical_rtl.sh:501-520` / `test_component_granularity.cpp:248-254`） |
| S2 | **R-4 消灭冒泡 vs R-5 保留「冒泡端口」来源 + `Bubbled` 值** ⇒ 内部矛盾 + 默许死代码 | **R-5 改「五类 → 三类」**（删「inline 后代的 `ch_out`」+「冒泡端口」）+ **新增 D1-h**（`UndrivenPolicy::Bubble` / `*Origin::Bubbled` / `ensure_bubbled_port` / **`BubbledInput`** **MUST 退役 · 时点 = 同批**）+ **V19** |
| S3 | D1-a 的「（多出来的是输出）」**以偏概全** | 明写「**对 RV32I** 成立」+ 「**不得外推为一般性不变量**」（含后代悬空 `ch_in` 的设计在产品路径上是**拒绝**，不是「变成 output」） |

## 6.3 核实通过项（v2 增量无问题的部分）

- **backend 130 不变成立**：`src/core/verilator_backend.cpp:708-709` 走 `verilogwriter writer(ctx)`；`:849-879` 的 `build_port_access_table` 只依赖 `ctx_->get_eval_list()`，且 `:874-878` 跳过有 driver 的 `type_input` —— 与 `src/codegen_verilog.cpp:432-441` **逐字同构** ⇒ ⇒ D1 **不引入新变化**。
- **「只增不改」可行**：`ModuleEmitter(context*, ModuleView)`（`include/codegen_verilog.h:156`）与 `ModuleView`（`:90-147`）**无需新增字段**；新 `toVerilog` 可绕 `verilogwriter` 直接用 `ModuleEmitter`。**重载歧义不成立**（全仓 `toVerilog("...", nullptr|NULL|0)` **0 命中**；`ch_device` 只暴露 `context()` 访问器）。
- **`:98` 的标题确实中性**：三个条款 + 唯一 Scenario（`:104-111`）**均无**「用户直建节点的值可经顶层端口观测」⇒ ⇒ **R-3 是 MODIFIED，不是 R+A**。
- **delta 标题判据对反引号不敏感**：`scripts/verify_openspec_delta.sh:155` 用 `grep -qxF`（固定串）；且新标题是 **ADDED**（R3 只查 MODIFIED）。
- **`:478` 不需 delta 成立**：`:478-482` 的「既有路径」**直指**「既有 `toVerilog()`」（两个 ctx 形参重载）；Scenario `:492-496` 把「签名不变 + 产物逐字节不变」钉在该入口上。
- **`′` 编号有先例**：`docs/adr/ADR-059-*.md:156` 的 `D2′`。
- **冒泡机制在 ctx-only 路径零耦合**（v2-critic 复核）：`src/codegen_verilog.cpp` **0 处** `Bubble/Bubbled/UndrivenPolicy` 引用 ⇒ ⇒ 物理删 `Bubble` **不改变 130 端口产物的任何字节** ⇒ ⇒ **V5′ 可满足**。

---

# 7. **v2 轮 · critic 第 1 轮**（arch 档 · 冲突审稿）—— **REJECT**（2 BLOCKER）

## 7.1 BLOCKER（2 条 · 全部已闭）

| # | BLOCKER | 内容 | 闭合位置 |
|---|---|---|---|
| **B1** | **第三类（文档内自相矛盾）** | §2.8 **R-4 要点④** 写「`UndrivenPolicy::Bubble` 对后代悬空 `ch_in` 不再有效（**枚举是否退役 = 实施期可选简化**）」，而 **D1-h** 写「**MUST 退役** ⇒ 时点 = 本 change 同批（**MUST NOT** 留作『实施期可选简化』）」，**V19** 又把「零残留」列为 🔴 ⇒ ⇒ **builder 无法决定 delta 里该写「退役」还是「保留」** | R-4 要点④ 改为「**MUST 退役（见 D1-h）· 时点 = 同批**」+ 明写「**MUST NOT 留可选项的余地**」 |
| **B2** | **第三类 + 冻错契约** | 「§2.8 = **5 条** delta 的唯一权威输入 · MUST 照抄」与 **V6 负向判据（旧措辞零残留）** 及 **D1-h** **不能同时成立** —— 主 spec 另有 **4 处**旧契约面不被 5 条 delta 覆盖：<br>**(a)** `:240` 的 Scenario `:336-344` 末句「**未驱动的子 input MUST 冒泡成顶层 input**」（**R-5 自己的目标 Requirement 内部** ⇒ MODIFIED 是整条替换）<br>**(b)** `:408` 的「悬空输入的策略 MUST 由 `UndrivenPolicy`（**`Bubble` / `Reject`**）承载」⇒ D1-h 删 `Bubble` 后**指称不存在的符号**<br>**(c)** capability Purpose `:12-13`「悬空子输入 MUST 向上**冒泡**为顶层输入」⇒ delta 不合并 Purpose ⇒ 归档后**与新 R-4 相反**<br>**(d)** `rtl-emit-portability:64-69`「manifest 不定义端口面与命名规则」 vs R-1 要点③ 的**标题级张力** | **(a)** R-5 补**第 4 处**改法 + 新增「**MODIFIED 的完整性义务**」段（整条替换含全部 Scenario · `:240` 有 8 个 · R3 只查标题 ⇒ **抓不到正文漏改** ⇒ 必须人工逐 Scenario 对照）<br>**(b)** 新增 **R-6**（`:402` 的 MODIFIED）⇒ ⇒ **delta 5 → 6 条**<br>**(c)** 新增「**Purpose 的两处落点**」表<br>**(d)** R-1 补要点⑥（该条约束的是**产物不变**，不是「字段不能描述端口面」） |

## 7.2 SHOULD_FIX（1 条已闭）+ NIT（2 条已闭）

| # | finding | 闭合位置 |
|---|---|---|
| S1 | **退役清单漏 `BubbledInput` 结构与其消费者**（`include/hierarchy_analysis.h:121,152` · `src/hierarchy_verilog.cpp:144,1261` 的 `dangling` 收集）⇒ 退役后残留死公开结构 ⇒ 与 D1-h 自我矛盾 | D1-h / 同批必改 ④′ / V19 **三处**补入 + V19 加「删后 MUST 全量重建（禁只重建被点名 target）」 |
| N1 | R-4 **新标题**无「后代」限定（正文 ① 又放行「根自己的」） | 新标题改为「悬空**后代**子输入 MUST 被响亮拒绝…」 |
| N2 | R-5 要点 2 的「根的端口集合 MUST == 自身 `ch_in` ∪ `ch_out`」**未排除框架 clock/reset** | 限定为「根的**自有 IO 段**（不含框架 clock/reset）」⇒ 与 D1-d 的「34 = 2 + 32」逐字对齐 |
| N3 | 「`tb_stub` 会把 **124** 个 output 连到 34 端口」不可复算 | ⚠️ **architect 复核认为 critic 的算术有误**（124 = 28 根自有 out + 96 冒泡 ✔）⇒ ⇒ 仍改为**派生表述**（= `ports[]` 中 `dir==output` 的条目数 · **禁**写死） |

## 7.3 核实通过项（v2 增量的其余部分）

- **R-3 维持 MODIFIED 成立**（同 §6.3）。
- **R-5 的「五类 → 三类」逻辑一致**（与 R-2 / R-4 无冲突）。
- **D1-h 判定成立**：冒泡唯一生产者 = `src/hierarchy_analysis.cpp:404-416`；**ctx-only 遗留路径零耦合**；层次化公开入口本来就收 `Component& root`（`include/hierarchy_verilog.h:144`）⇒ ⇒ **不存在「ctx-only 层次化路径用冒泡」的形态**。
- **D1-f × D1-g 不冲突**：分类键同为 `get_parent()`，一个 `!= nullptr` 一个 `== nullptr` ⇒ 天然不相交。
- **ADR-vs-spec 的时序不构成 BLOCKER**：取代载体（delta）已立项且排在实施**同批** ⇒ 不是绕过；Status = Draft ⇒ 未把未决写成已决。
- **V5′ 可满足**（同 §6.3）。
- **登记**：`src/hierarchy_analysis.cpp:412-416` 有 `owner == nullptr` 的 `type_input` 防御分支 ⇒ ⇒ 该形态**归 D1-g 管辖**（不拒绝）⇒ ⇒ 已在 D1-f 的表中新增该行。

---

# 8. **v2 轮 · critic 第 2 轮（复审）** —— **APPROVE · 0 BLOCKER**

- **BLOCKER-1 闭合** ✅：`ADR:274` 现为「MUST 退役（见 D1-h · 时点 = 本 change 同批）…MUST NOT 留可选项的余地」；全文 grep「可选 / 实施期简化」的 4 处命中**全部是**历史行或闭合否定句本身，**无残留活体矛盾**；R-4 / D1-h / V19 三处**方向一致** ⇒ builder 无第二种读法。
- **BLOCKER-2 闭合** ✅（四组证据逐条核实）：
  (a) R-5 要点 4 给出 `:336-344` 末句的**逐字改写** + 「漏它的后果」；「MODIFIED 的完整性义务」段成文；
  (b) 新增 R-6 逐字给出 `:408` 的改写文本 + 「不改 ⇒ 冻错契约」；delta 5 → 6 及**原因**成文；
  (c) Purpose 两处落点表成文（含 `:12-13`「与新 R-4 方向相反」）+ §6 ②/⑨ 登记为 builder 任务；
  (d) R-1 要点⑥ 给出 `:64-69` 的**显式解读** + MUST 补的逐字句子。
- **本轮新 diff 无新 BLOCKER**：退役清单三处一致；delta 条数全文一致（`:367` 的「缩到 5 条」是**历史裁决句**，其后 R-6 已把总范围重新定为 6）；Status 仍 Draft。
- **SHOULD_FIX（1 条）**：**V6 的 grep 范围未圈定** —— 「冒泡 / `Bubble` / `Bubbled` 全部命中 MUST 清零」与「Purpose `:12-13` 不被 delta 合并」并存 ⇒ verifier 无法判定 `:13` 算红还是豁免。⇒ ⇒ **已补范围裁定**：「清零范围 = 6 条 Requirement 的正文与 Scenario；**Purpose 不在该范围内**（走 §6 ②/⑨ 通道）」+ 列出 8 处命中中**仅 `:13` 走 Purpose 通道**。
- **NIT**：D1-f 边界行（`:142`）与「`ch_in` 端口节点恒 `owner != nullptr`」（`:157`）的张力 ⇒ **降为 NIT**（「不可能发生 vs 发生时归谁」的裁决**唯一** ⇒ 不构成决策二义）。
- **结论**：**`mergeable: yes` · `scope_exceeded: false`**。

---

# 9. 本 ADR v2 的完成标志核对（`arch` 档）

| # | 标志 | 状态 |
|---|---|---|
| ① | 四要素齐（元数据 / 选项对比表 ≥ 3 行 / 独立「后果·代价」章 / 前瞻性 Verification 章） | ✅（**13** 行对比表 · §4 · §5 的 **V1–V19** 全未勾） |
| ② | oracle + critic 双审；BLOCKER 全闭并**经复审确认** | ✅（v1 轮 oracle 1 + critic 2 · **v2 轮 oracle 1 + critic 2（复审 APPROVE）**） |
| ③ | Status 已转已决 | ✅ **Accepted · v2** |
| ④ | 「关联 change」已落库 | ✅ `openspec/changes/rtl-productization-surface-2026-11/`（**已立项** · planner 2026-11-16）· AC 映射 = 该 change 的 AC1–AC15 + T1–T14 |
| ⑤ | 审稿报告已落 `.rddf/reviews/` | ✅ 本文件（§1–§9） |

⚠️ **自指审查声明**：`ADR-058` / `ADR-061` 的 dated addendum（同批必改 ⑧）**不是**本轮审查能替代的 ⇒ ⇒ 那一步**按仓内先例走独立复审**（ADR-060 v1.20 / ADR-062 v1.7）。
