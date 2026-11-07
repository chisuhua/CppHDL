---
# ⚠️ 本文件是**活账**（★ 架构台账「已知残留」的**唯一真相**）⇒ ⇒ ⇒ 护栏
#    `scripts/verify_arch_ledger_residuals.sh` 对它做**双向 + 1:1**核对。
evidence: probe:scripts/probe_arch_ledger_residuals.sh
---

# 残留状态登记表（唯一真相）

> **权威**：本表是「哪些残留已闭口 / 未闭口 / 不做 / 已知」的**唯一登记处** ⇒ ⇒ ⇒
> 台账 §7 的状态格 MUST 以本表**确定性生成**的规范句开头 ⇒ ⇒ ⇒ 散文保留在规范句之后。
>
> ★ **列语义**：`台账` · `section`（台账内章节锚）· `id`（台账 `#` 列的**标签**，★ **不是**物理行号）
> · `状态`（枚举：`已收口` / `已裁决` / `不做` / `已知` / `未决`）· `日期`（`YYYY-MM-DD`）
> · `后缀`（★ **终态行 MUST 非空** ⇒ 否则同态同日的多行会生成**逐字节相同**的规范句 ⇒ 碰撞）
> · `闭口证据`（★ `path1,path2`：**半角逗号 · 无空格 · 无反引号 · 无空段 · 不重复**；
> archive 类 MUST 含 `proposal.md`）· `复核条件`（★ `未决` 行 MUST 非空）· `归属`（终态行填 `—`）
>
> ★ **键 = (台账, section, id)** ⇒ 同一台账的不同残留表里出现同号**不会**误配。

| 台账 | section | id | 状态 | 日期 | 后缀 | 闭口证据 | 复核条件 | 归属 |
|---|---|---|---|---|---|---|---|---|
| docs/architecture/2026-11-02-hierarchical-rtl-output.md | 7 | 1 | 已收口 | 2026-11-07 | P8 公共 helper 归档 | openspec/changes/archive/2026-11-07-p8-hierarchical-equivalence-public-helper-2026-11 | — | — |
| docs/architecture/2026-11-02-hierarchical-rtl-output.md | 7 | 1b | 已收口 | 2026-11-07 | 语料级回归归档 | openspec/changes/archive/2026-11-07-hierarchical-corpus-regression-2026-11 | — | — |
| docs/architecture/2026-11-02-hierarchical-rtl-output.md | 7 | 2 | 已裁决 | 2026-11-07 | ADR-060 §2.1 第四行 | docs/adr/ADR-060-hierarchical-port-policy-and-module-reuse.md | — | — |
| docs/architecture/2026-11-02-hierarchical-rtl-output.md | 7 | 3 | 已裁决 | 2026-11-07 | ADR-058 §2.1 v2.3 | docs/adr/ADR-058-hierarchical-rtl-output.md | — | — |
| docs/architecture/2026-11-02-hierarchical-rtl-output.md | 7 | 4 | 已收口 | 2026-11-07 | T3-005 实施后收口 | .rddf/reviews/T3-005-equivalence-debt-review.md | — | — |
| docs/architecture/2026-11-02-hierarchical-rtl-output.md | 7 | 5 | 已收口 | 2026-11-06 | 端口命名护栏 | scripts/verify_hierarchical_port_naming.sh | — | — |
| docs/architecture/2026-11-02-hierarchical-rtl-output.md | 7 | 6 | 不做 | 2026-11-07 | 每类型一份 module | — | — | — |
| docs/architecture/2026-11-02-hierarchical-rtl-output.md | 7 | 7 | 不做 | 2026-11-07 | 框架单时钟域 | docs/adr/ADR-008-single-clock-domain-constraint.md | — | — |
| docs/architecture/2026-11-02-hierarchical-rtl-output.md | 7 | 8 | 不做 | 2026-11-07 | ADR-024 已评估 | docs/adr/ADR-024-blackbox-ip-integration-evaluation.md | — | — |
| docs/architecture/2026-11-02-hierarchical-rtl-output.md | 7 | 9 | 已知 | 2026-11-07 | 等价性验证 MUST 排除 >64bit 端口 | docs/adr/ADR-035-verilator-backend.md | — | — |
| docs/architecture/2026-11-02-hierarchical-rtl-output.md | 7 | 10 | 不做 | 2026-11-07 | 需真实综合工具 | — | — | — |
| docs/architecture/2026-11-02-hierarchical-rtl-output.md | 7 | 14 | 已裁决 | 2026-11-07 | ADR-062 决策⑤ | docs/adr/ADR-062-hierarchical-unit-interface-typing.md | — | 下游 |
| docs/architecture/2026-11-02-hierarchical-rtl-output.md | 7 | 12 | 已收口 | 2026-11-02 | C0-0 spike 提取器 | scripts/lib/verilog_tree_extract.py | — | — |
| docs/architecture/2026-11-02-hierarchical-rtl-output.md | 7 | 13 | 已收口 | 2026-11-06 | 随 T3-005 关闭 | docs/adr/ADR-058-hierarchical-rtl-output.md | — | — |
| docs/architecture/2026-11-02-hierarchical-rtl-output.md | 7 | 11 | 已收口 | 2026-11-07 | P6 + P8 双立项归档 | openspec/changes/archive/2026-11-07-p6-hierarchy-friendliness-diagnosis-2026-11,openspec/changes/archive/2026-11-07-p8-hierarchical-equivalence-public-helper-2026-11 | — | — |
