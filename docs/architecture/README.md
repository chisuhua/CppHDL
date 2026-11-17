# `docs/architecture/` —— 索引与当前态权威

> ⚠️ **本目录是「架构主题文档 + 评审记录」的混合目录** ⇒ ⇒ **文件多 ≠ 有多个权威** ⇒ ⇒
> 本索引给出**当前态权威**与**历史快照**的判读口径（2026-11-17 建）。

## 1. 当前态权威（**按主题**）

| 主题 | 当前态权威文档 | 决策面（ADR） |
|---|---|---|
| **RTL 验证的单一真相源 / 生成式 TB / 端口具名化 / 发射面收敛** | `2026-11-17-rtl-verification-single-source.md` | **ADR-074** · **ADR-075** |
| **SoC 层的 RTL 产物形态与 SoC 层 TB** | `2026-11-16-soc-rtl-product-and-tb.md`（⚠️ **历史方案**：其 dated 注已声明「当前态见 `2026-11-17-…`」；**决策面以 ADR-073 为准**） | ADR-073（**Accepted**） |
| **层次化 RTL 输出** | `2026-11-02-hierarchical-rtl-output.md` · `2026-11-08-hierarchical-parent-side-landing.md` | ADR-058 · ADR-060 · ADR-061 · ADR-066 · ADR-070 |
| **RV32I 微架构** | `rv32i-microarchitecture.md` | ADR-046 · ADR-047 · ADR-068 · ADR-069 |

⚠️ **冲突时以 ADR 为准**（主题文档承载推导与快照，**不新增**架构义务）。

## 2. 历史快照 / 评审记录（**按日期读**）

- `2026-0*-*.md`（2026-03-30 ~ 2026-04-10）· `PHASE6-ARCHITECTURE.md` —— **早期方案快照**，**内容已被后续文档取代** ⇒ ⇒ **只作历史参考**，**不得**作为实现依据。
- `2026-11-0*`/`2026-11-1*phase-6-*review*.md` —— **阶段评审记录**（review record）⇒ ⇒ 记录当时的判断与残留登记，**不是**当前态声明。
- ⚠️ **`SYNC-REPORT.md`（停在 2026-03-30）已失效** ⇒ ⇒ **不得**据此判断当前同步状态。
- `phases/` · `plans/` —— 在 `arch-docs.json` 的 `exclude_paths` 内 ⇒ ⇒ **不属于**架构文档面。

## 3. 判读纪律

1. **决策看 ADR**（`../adr/`）⇒ ⇒ 主题文档只承载推导 / 快照 / 逐文件裁决。
2. **过期内容用 dated addendum 追落**（在**对应条目行尾**标注「日期 + 变更指针 + 若上游被否则回滚」）⇒ ⇒ **不改写历史正文**（仓规「存量文档不追溯」）。
3. **新增主题文档时 MUST 在本表登记** ⇒ ⇒ 否则「当前态权威」再次失焦（**这正是 2026-11-17 建本索引的动因**）。

---

- **建档**: 2026-11-17 · architect role（build agent）
- **关联**: `docs/adr/ADR-074-rtl-verification-single-source.md` · `docs/adr/ADR-075-port-naming-prerequisite.md`
