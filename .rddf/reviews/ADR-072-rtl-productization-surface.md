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
