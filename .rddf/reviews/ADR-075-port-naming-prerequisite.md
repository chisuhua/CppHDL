# 审稿记录 · ADR-075（产品面顶层端口具名化）

## review_profile

`arch`（产物路径 = `docs/adr/**` ⇒ 架构决策面）

## 轮次

| 轮 | 审查者 | 结论 | 本 ADR 相关 BLOCKER | 闭合 |
|---|---|---|---|---|
| 1 | **oracle** | **REJECT**（同一批次 6 BLOCKER，其中 **B1** 直接命中本 ADR） | **1** | ✅ 全闭（v2） |
| 1 | **critic** | **REJECT**（4 BLOCKER，其中 **B3** 直接命中本 ADR 的上游文档） | **1** | ✅ 全闭（v2） |
| 2 | **critic（复审 · 只验闭合）** | **APPROVE_WITH_CONDITIONS · 0 BLOCKER · `mergeable: yes`** | **0** | — |

> ⚠️ 本 ADR 与 **ADR-074** 同批受审（同一 oracle / critic 调用）⇒ ⇒ 完整 finding 表见 `.rddf/reviews/ADR-074-rtl-verification-single-source.md`；本文件只登记**与本 ADR 直接相关**的项。

## 逐条应答

| # | finding（← ADR-074 记录里的编号） | 对本 ADR 的处置 | 落点 |
|---|---|---|---|
| **B1（oracle）** | 「产品顶层 = 55 端口」错 | ✅ 改为 **32**（`AxiLiteTop` 20 + `AxiSocTop` 12）；**23** 条 `AxiLiteTestMaster` 端口**显式移出范围**（子 module 端口面 ⇒ 另一类产品可见变更） | §1.2④⑤ · §2.1 · 选项 ③ 拒 · AC10 |
| 派生（oracle S/实测） | 豁免表数据行是 **1447** 而非 1450；剩余 **1415**（= 1447 − 32） | ✅ 三处数字已改 | §1.2③ · §2.4 · §2.5 · AC4 |
| 派生（oracle B5） | 身份链「≡」自相矛盾 + 映射无载体（`port_table[]` 无 `cpp_member`） | ✅ 本 ADR 承载**承诺边界**（E2：可解析 ≠ 字面相同）+ **映射载体**（E3：两表 join + 1:1 fail-closed，**不新增 schema**） | §2.2 · §2.3 |
| **B3（critic）** | ADR-073 `:107` 仍写 35（自相矛盾） | ✅ 已改 12 并点名 23 条为子 module 端口 ⇒ **本 ADR 的 32 不再与上游冲突** | `ADR-073:107` |
| N3（critic） | 具名示例的端口宽度不符 | ✅ 改 `ch_in<ch_uint<32>> awaddr{"awaddr"};` / `ch_in<ch_bool> start{"start"};` | §2.1 |
| N2（critic） | `top_<成员名>` 被当既成事实 | ✅ 改为「产物名由 `port_table[].name` **读取**；预期形态 `top_<成员名>`」+ §2.2 禁字符串拼接 | §2.2 · §4.1 P2 |
| 实测（critic 前） | 手写映射表**当前已漂**（注释 `awprot=input_1` vs 连线 `.top_unnamed_input_1(arprot)`） | ✅ 已作为本 ADR 的核心动机（§1.2 未单列编号，见 ADR-074 事实④） | ADR-074 事实④ |

## 未闭合 / 已知边界

| # | 项 | 处置 |
|---|---|---|
| 1 | **复审未跑**（预算） | 由 planner 立项时的 metis 复核 |
| 2 | 子 module 端口名仍不可自解释（23 条） | **已知边界** · §4.2 N5 · AC10 反向判据（未变） |
| 3 | 全仓 **1415** 条债 | 棘轮只减不增 · AC9 |
| 4 | `PORT-NAMING-GUIDE.md` / `include/core/AGENTS.md` 正文改写 | **不在 architect 写集** ⇒ 交 planner/builder（§6） |
