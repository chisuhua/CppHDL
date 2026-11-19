# `docs/architecture/` —— 索引与当前态权威

> **本目录只放「当前态」架构主题文档**（2026-11-19 重组）。
> 历史方案快照 / 阶段复盘 / `phases/` 规划已移至 [`../archive/architecture-history/`](../archive/architecture-history/)。

## 1. 当前态权威（**按主题**）

| 主题 | 当前态权威文档 | 决策面（ADR） |
|---|---|---|
| **RTL 验证的单一真相源 / 生成式 TB / 端口具名化 / 发射面收敛** | [2026-11-17-rtl-verification-single-source.md](2026-11-17-rtl-verification-single-source.md) | **ADR-074** · **ADR-075** |
| **层次化 RTL 输出** | [2026-11-02-hierarchical-rtl-output.md](2026-11-02-hierarchical-rtl-output.md) | ADR-058 · ADR-060 · ADR-061 · ADR-066 · ADR-070 |
| **RV32I 微架构** | [rv32i-microarchitecture.md](rv32i-microarchitecture.md) | ADR-046 · ADR-047 · ADR-068 · ADR-069 |
| **文档治理口径**（分级 · 引用有效性 · 死链处置 · 自动化门） | [2026-11-19-document-governance.md](2026-11-19-document-governance.md)（**Draft** · oracle APPROVE 附 6 项必改 · 已落） | ——（**不是**决策正本；规范正本 = [`../DOCUMENT-ARCHITECTURE.md`](../DOCUMENT-ARCHITECTURE.md) v2.1） |

⚠️ **冲突时以 ADR 为准** —— 主题文档承载推导与快照，**不新增**架构义务。

### 已移入归档的「曾在此处」文档

| 文档 | 现在位置 | 状态 |
|---|---|---|
| SoC 层 RTL 产物与 TB | [`../archive/architecture-history/2026-11-16-soc-rtl-product-and-tb.md`](../archive/architecture-history/2026-11-16-soc-rtl-product-and-tb.md) | **历史方案**（当前态见 2026-11-17 那篇；决策面 ADR-073） |
| 父侧消费子 `ch_out` 的落点裁定 | [`../archive/architecture-history/2026-11-08-hierarchical-parent-side-landing.md`](../archive/architecture-history/2026-11-08-hierarchical-parent-side-landing.md) | 已裁决（ADR-066）⇒ 历史输入文档 |
| Phase 6 复盘 #1–#5 · N4/N6 证据集 | [`../archive/architecture-history/`](../archive/architecture-history/) | 过程记录 |
| 早期方案快照（2026-03-30 ×3 · PHASE6-ARCHITECTURE · SYNC-REPORT） | [`../archive/architecture-history/`](../archive/architecture-history/) | 内容已被取代 |
| `phases/` 规划（13 篇）· `phase1-tasks.md` | [`../archive/architecture-history/phases/`](../archive/architecture-history/phases/) | 2026-04 规划 |

## 2. 判读纪律（不变）

1. **决策看 ADR**（[`../adr/`](../adr/)）⇒ ⇒ 主题文档只承载推导 / 快照 / 逐文件裁决。
2. **过期内容用 dated addendum 追落**（在**对应条目行尾**标注「日期 + 变更指针 + 若上游被否则回滚」）⇒ ⇒ **不改写历史正文**（仓规「存量文档不追溯」）。
3. **新增主题文档时 MUST 在本表 §1 登记** ⇒ ⇒ 否则「当前态权威」再次失焦（**这正是 2026-11-17 建本索引的动因**）。
4. **本目录不做历史堆积** —— 新写的方案一旦被取代，**移到 `archive/architecture-history/`**，不留在活区。

---

- **建档**: 2026-11-17 · architect role（build agent）
- **重组**: 2026-11-19（历史文档移出 · 32 篇 → 5 篇）
- **关联**: [`../adr/ADR-074-rtl-verification-single-source.md`](../adr/ADR-074-rtl-verification-single-source.md) · [`../adr/ADR-075-port-naming-prerequisite.md`](../adr/ADR-075-port-naming-prerequisite.md)
