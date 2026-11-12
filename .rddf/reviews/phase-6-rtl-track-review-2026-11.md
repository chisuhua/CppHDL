---
id: phase-6-rtl-track-review-2026-11
日期: 2026-11-12
对象: docs/architecture/2026-11-12-phase-6-rtl-track-review.md
状态: ✅ **已闭**（oracle ×1 · APPROVE + critic ×1 · APPROVE_WITH_CONDITIONS · **0 BLOCKER**）
审稿链: oracle → critic（顺序不可颠倒）⇒ ⇒ 另加 planner 的 metis ×1（任务清单刷新）
---

# 审稿记录 · Phase 6 / RTL 轨复盘 #3（architect 域 · 2026-11-12）

> ⚠️ **本轮的产出物**（3 个文件，**零 `src/**` · `include/**` · `tests/**` 变更**）：
> ① `docs/adr/ADR-047-rv32i-data-forwarding.md` **§5.5 dated addendum**（追加 ≠ 改写）
> ② `docs/architecture/rv32i-microarchitecture.md`（+3 行 dated 注记）
> ③ `docs/architecture/2026-11-12-phase-6-rtl-track-review.md`（**新建** · 主题文档 · 自称**非决策文档**）

---

## 1. 审查链

| 轮 | 审查者 | 结论 | 处置 |
|---:|---|---|---|
| 1 | **oracle** | ✅ **APPROVE**（附**一项强制前置 P0-1**）· 8 条建议（P0×2 · P1×4 · P2×2） | **8 条全部采纳并并入**（v2）⇒ ⇒ P0-1 在 planner 执行 T-a **之前**已订正 |
| 2 | **critic** | ✅ **APPROVE_WITH_CONDITIONS** · `mergeable: yes` · `scope_exceeded: false` ⇒ **0 BLOCKER** · 1 SHOULD_FIX + 2 NITS | **1 SHOULD_FIX + 2 NITS 全部采纳**（v3） |
| 3 | **metis**（planner 域 · 任务清单刷新） | ✅ **APPROVE**（附**强制前置 P0-1**）· 19 条（P0×5 · P1×14 · P2×5 → 实际 P1×9 · P2×5） | **P0 5 条全处置** · **P1 9 条处置 8 条** · **P2 5 条处置 4 条** · **未处置 2 条登记进 backlog**（不静默丢弃） |

---

## 2. oracle 第 1 轮（APPROVE · 8 条）的落点

| # | finding | 落点 |
|---:|---|---|
| **P0-1** | 🔴 **复盘 §3.5 的计数口径错误**：AC 计数 MUST 用「**行 + `<br>` 内联**」双计（`rtl-track-correction-list-2026-11.md:152`）⇒ ⇒ 复盘第一稿只按行首计 ⇒ ⇒ 把 `next-steps.md` §8.3 的**正确**的 22 误判为「多写 4」⇒ ⇒ **若 planner 照第一稿去「修」⇒ ⇒ 会把对的改错，且 4 条内联 work item 从索引永久脱落** | ✅ **已订正**：§3.5 改为「漏列 2 份 ⇒ 合计 70 vs 真实 **74**」· §2.5 表改为 `0/22` · 总数 **74** · B4 = **32** · **护栏化项（§6 ⑥）写入「行+内联」口径** ⇒ ⇒ **plan 的 §8.3 已补那 2 行且保留 22** |
| **P0-2** | 🔴 **addendum 的「3 拍」清扫漏了活文档**（根 `AGENTS.md:275-276` = 每会话注入面 · `docs/architecture/rv32i-microarchitecture.md:89` · 草案 `rv32i-isa-gap-closure:154`） | ✅ **§5.5.3.1 活文档清扫 5 行**（含根 `AGENTS.md` ⇒ **planner 域**·带**前向注记**而非改值）· 根 `AGENTS.md` 已加 dated 前向注记 |
| **P1-3** | §5.5.1「决策内容零变更」措辞**过宽**（与 §5.5.2 的「D4/D11 的源必须重指」字面互斥） | ✅ 改为「**语义**零变更（≠ 实现零改动）」+ 显式警告段 · critic 的 NIT-1 再把「决策内容零变更」就地改成「语义零变更」（句序修正） |
| **P1-4** | 复盘**漏登记** 5 处漂移（索引两行 · `firmware-load-chain` 进度行 · `next-steps:39` 的「仍欠」从句 · R 表的 R0/R1/R2/R10 无完成标记） | ✅ **新增「发现 8」**（5 处）+ 并入 T-a 范围 |
| **P1-5** | ⚠️ **两条令字面冲突**（ADR-069 §6③「与 acceptance 同批」vs critic SF-3「与 MODIFIED 同批」）⇒ ⇒ 实际 = acceptance 之后单独补 ⇒ ⇒ **两条令均未字面满足** | ✅ **登记为「已登记的偏离」**（复盘 §4.1.1）⇒ ⇒ 明写「**不得被后续会话当作未做而重做**」 |
| **P1-6** | 发现 4 的措辞不准（`CHANGELOG.md` 全文只有一处提 ADR-069 ⇒ ⇒ 矛盾对象 = **该文件 vs 现实**，不是文件内自相矛盾） | ✅ 措辞已改 |
| **P2-7** | 「不要误改」两行缺**机制依据** | ✅ 已补（退休计数抽头在 MEM 级输出 · A0 不动它 ⇒ 填充拍数不变 · 分支冲刷纯前端） |
| **P2-8** | 复盘的 stray backtick + §2.5 未标「快照 · 计数真相在草案」 | ✅ 已修 + §2.5 加口径注 |

---

## 3. critic 第 2 轮（APPROVE_WITH_CONDITIONS · 0 BLOCKER · 1 SF + 2 NITS）的落点

| # | finding | 落点 |
|---:|---|---|
| **SF** | 🔴 **三处「3 拍」原文现场无前向指针** ⇒ ⇒ 只读 §2-D6 标题就开工的实施方，要读到 200 行外的 §5.5 才发现拍数已修订 ⇒ ⇒ 力度与 D10 表 / `rv32i-microarchitecture.md` 不一致 | ✅ **三处各加一条 dated 指针**（**不改原文一字**）：§1 现状表（表**之后**加 ⇒ ⇒ 不破坏表结构）· §2 **D6 标题**（下方）· §3 Consequences（条目内）⇒ ⇒ **验证：ADR-047 原文零删除（`git diff | grep -c '^-[^-]'` == 0）** |
| **NIT-1** | §5.5.1 句序（先说「决策内容零变更」· 两段后才限定） | ✅ 已就地改（**该段是本轮新增文本** ⇒ ⇒ 不触碰任何原文 ⇒ ⇒ 无流程成本） |
| **NIT-2** | 复盘 §3 标题写「7 条」实有 8 条 | ✅ 已改为 **8 条**（并标注「发现 8 = oracle P1-4 新增」） |

> ⚠️ **critic 对 0 BLOCKER 的理由**（留痕）：
> ① **focus 1**（持久矛盾）= **已消解** ⇒ ⇒ ADR 内部两个拍数并存**不构成** BLOCKER ③（§5.5.1 逐条标注 + 机械路径）；
> ② **focus 2** = **契约层无冲突**（§5.5.2 用 MUST 表钉死 + §5.5.1 警告段堵住误读）；
> ③ **focus 3** = §5.5.3 是**前瞻判据**（非实测日志）+ 正反两面俱在；
> ④ **focus 4**（复盘形状）= **合规**（裁决①不是「自己派活」= 执行 ADR-069 §6③ 的既有授权 · 裁决④派的全是载体非决策）；
> ⑤ **focus 5**（74 复核）= **实测命中**（0+0+2+2+3+10+18+17+22 = **74** ✓ · B3=35 ✓ · B4=32 ✓ · 独立立项=7 ✓）；
> ⑥ **偏离**（两条令互斥）= **可接受**（回滚 ADR-069 到 Draft = 重开一个已 Accepted 的决策 ⇒ ⇒ **代价 ≫ 收益** ⇒ ⇒ **不回滚**）。

---

## 4. metis 第 3 轮（planner 域 · 任务清单刷新）的处置

⇒ ⇒ 完整逐条处置记录 = `.rddf/roadmap/phase-6-next-steps.md` **§7.0**（19 条）⇒ ⇒ **P0 5 条全处置**：

| # | finding | 处置落点 |
|---:|---|---|
| **P0-1** | R13.1「两个 change」与 R14.1「T1+T3 同批 ⇒ 不得拆批」**直接冲突** | ✅ **裁决：两个 change · 同一窗口 · 归档顺序无中间态**（R13.1） |
| **P0-2** | 两份 MODIFIED 的 delta 被放到**实施期**（OpenSpec delta 必须在提案期写） | ✅ **落点改回 R13.2c** · R14.5 降为「核对 + 归档合并」· 判据改为「归档后主 spec `grep -c "实测 4 拍"` == 1」 |
| **P0-3** | 🔴 **ADR-069 V12 指向 `tests/chlib/test_hazard_complete.cpp` —— 该文件不存在**（B0 C5+C6 已退役）⇒ ⇒ **判据不可满足** | ✅ **architect 域已修**（ADR-069 V12 加 2026-11-12 订正）⇒ ⇒ 落点改为「**新建** `HazardUnit` 组件级停顿链真值表用例」+「`test_phase4_integration.cpp` 当前**只有实例化 smoke** ⇒ ⇒ 禁只断言能实例化」 |
| **P0-4** | ADR-068 **V8**（`non_retired == 21` 不变）与 ADR-069 **§3.2-②**（随之变）**互斥** | ✅ **architect 域已归一**（ADR-068 §5.5 补行）⇒ ⇒ R14.6 / R15.4 改为引用「B3 后的基线」· **禁**写死 21 |
| **P0-5** | L1–L6 的位置：批次表「本项先做」vs 草案「依赖 memory-timing」**方向相反** | ✅ **裁决：L1–L2 先行 · L3–L6 随后 · T1 与 L1–L2 同 commit 不可拆**（R14.4） |

⇒ ⇒ ⚠️ **P1-8**（ADR-068 **V7**「`data_ready` 恒 1 · 不是握手」vs B3 草案 T3 的三条「真反压」**互斥**）
⇒ ⇒ ✅ **architect 域裁决（V7 胜出）** + **planner 域已改草案**（T3 三条降级为**登记型**判据 + 原三条**逐字保留**为历史记录）
⇒ ⇒ ⇒ R13.3 的 design pins 补第 ⑥ 条。

⇒ ⇒ **未处置的 2 条** = `next-steps.md` **§7.0.1 的 backlog-1 / backlog-2**（负面约束两份载体重叠 · 「禁写死数字」只对 `CTEST_TOTAL` 执行）⇒ ⇒ **登记不丢弃**。

---

## 5. 复核命令（**可复跑**）

```bash
cd /workspace/project/CppHDL
# ---- ADR-047 的 addendum：原文零删除 + 三处指针就位 ----
git diff HEAD -- docs/adr/ADR-047-rv32i-data-forwarding.md | grep -c '^-[^-]'   # ⇒ 0（纯追加）
grep -c "2026-11-12 dated 指针" docs/adr/ADR-047-rv32i-data-forwarding.md          # ⇒ 3
grep -c "^| load-use 停顿 \*\*3 拍\*\*" docs/adr/ADR-047-rv32i-data-forwarding.md  # ⇒ 1（原文在）
grep -c "^### D6\. 保守边界" docs/adr/ADR-047-rv32i-data-forwarding.md            # ⇒ 1（原文在）
grep -c "^- \*\*load-use 停顿仍在\*\*（3 拍" docs/adr/ADR-047-rv32i-data-forwarding.md  # ⇒ 1（原文在）
# ---- 护栏（12 个）+ 锚点 + ctest ----
bash scripts/verify_ledger_evidence.sh        # R0 ⇒ PASS
bash scripts/verify_arch_ledger_residuals.sh  # ⇒ PASS（23 行）
bash scripts/verify_public_api_visibility.sh  # ⇒ PASS
bash scripts/verify_roadmap_fragments.sh      # ⇒ PASS
bash scripts/verify_test_counts.sh && bash scripts/update_test_count_anchors.sh --check
(cd build && ctest -N | tail -1)             # ⇒ Total Tests: 263
# ---- plan 的全判据自检（§6，已重写）----
bash -c "$(python3 -c "import pathlib;t=pathlib.Path('.rddf/roadmap/phase-6-next-steps.md').read_text();print(t.split('## 6. 判据自检')[1].split('## 7.')[0].split('\`\`\`bash')[1].split('\`\`\`')[0])")"
```

---

## 6. 本轮的状态

| # | 事项 | 状态 |
|---|---|---|
| ① | oracle 两轮 findings | ✅ **全部采纳并并入** |
| ② | critic 的 SF + NITS | ✅ **全部采纳并并入** |
| ③ | metis 的 P0 | ✅ **5 条全处置**（其中 3 条跨 architect/planner 两域） |
| ④ | metis 的 P1 / P2 | ✅ **处置 12 条** · **2 条登记进 backlog** |
| ⑤ | ADR-069 | ✅ **Accepted**（本轮仅修 **V12 的落点** = 事实性订正 · **不触碰决策**） |
| ⑥ | ADR-047 / ADR-068 | ✅ **dated addendum**（追加 ≠ 改写 · 原文逐字保留 · 原文零删除已验证） |
| ⑦ | 复盘 #3 | ✅ **主题文档**（决策内容零变更 · 只指路不复制状态真相） |
| ⑧ | `next-steps.md`（planner 域） | ✅ **已刷新**（R 表 + 实施路径 + 判据自检）⇒ ⇒ ⚠️ **完成态 = 待提交**（判据只看工作区 ⇒ ⇒ 提交后须回填 `commit <sha>`） |
| ⑨ | B3 的两道🚪门 | ✅ **R5 + R6 已全闭** ⇒ ⇒ **只差「立项」（R13）** |
