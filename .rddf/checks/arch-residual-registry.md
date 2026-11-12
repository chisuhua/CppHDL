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
> · `状态`（枚举：`已收口` / `已裁决` / `不做` / `已知` / `未决`）· `日期`（`YYYY-MM-DD`
> ⇒ ★ **该状态在本台账**首次落账**之日**（★ **不是**归一化日期；★ 底层 ADR / 裁决**可能更早**
> ⇒ ⇒ 行 7 / 8 即此类 ⇒ ⇒ 证据列指向该 ADR）⇒ ⇒ 逐行锚点见
> `.rddf/reviews/arch-residual-registry-normalization-2026-11.md` §2）
> · `后缀`（★ **终态行 MUST 非空** ⇒ 否则同态同日的多行会生成**逐字节相同**的规范句 ⇒ 碰撞
> ⇒ ★ **无加粗 · 无反引号 · 不含证据路径**）
> · `闭口证据`（★ `path1,path2`：**半角逗号 · 无空格 · 无反引号 · 无空段 · 不重复**；
> archive 类 MUST 含 `proposal.md`）· `复核条件`（★ `未决` 行 MUST 非空）· `归属`（终态行填 `—`）
>
> ★ **键 = (台账, section, id)** ⇒ 同一台账的不同残留表里出现同号**不会**误配。

★ **architect 裁定（2026-11-07 · 归一化）**：模板 = `✅ 已收口（<日期> · <后缀>）` / `✅ 已裁决（…）`
/ `不做 ⇒ <后缀>` / `⚠️ 已知 ⇒ <后缀>` / `⚠️ 未决 ⇒ <后缀>` ⇒ ★ **行 14 = `已裁决`**（架构问题已由
ADR-062 §2.5 决策⑤ 裁决；★ **下游实现待办** ⇒ 故 `归属 = 下游` ⇒ ★ 这是「终态行 `归属` 填 `—`」的
**唯一例外**，已登记以免被当成格式错误改回）。
★ **行 20 的例外已消解（2026-11-10 · P5 归档）**：行 20 曾因「ADR-066 决策 ⑦ 不纳入 mem 端口
实参 ⇒ 归下游独立立项」而带 `归属 = 下游`（第二例外）⇒ ⇒ **P5 已实施并归档**
（`openspec/changes/archive/2026-11-10-mem-port-arg-cross-layer-2026-11`）⇒ ⇒ 行 20 改记
`已收口` · `归属 = —` ⇒ ⇒ ★ 本段（第二例外的登记）**仅作历史留痕** ⇒ ⇒ **行 20 已不再是例外**
⇒ ⇒ 若有工具按「行 20」判定例外 ⇒ ⇒ **该判定 MUST 一并订正**。

| 台账 | section | id | 状态 | 日期 | 后缀 | 闭口证据 | 复核条件 | 归属 |
|---|---|---|---|---|---|---|---|---|
| docs/architecture/2026-11-02-hierarchical-rtl-output.md | 7 | 1 | 已收口 | 2026-11-07 | P8 公共 helper 归档 | openspec/changes/archive/2026-11-07-p8-hierarchical-equivalence-public-helper-2026-11 | — | — |
| docs/architecture/2026-11-02-hierarchical-rtl-output.md | 7 | 1b | 已收口 | 2026-11-07 | 语料级回归归档 | openspec/changes/archive/2026-11-07-hierarchical-corpus-regression-2026-11 | — | — |
| docs/architecture/2026-11-02-hierarchical-rtl-output.md | 7 | 2 | 已裁决 | 2026-11-02 | ADR-060 §2.1 第四行 | docs/adr/ADR-060-hierarchical-port-policy-and-module-reuse.md | — | — |
| docs/architecture/2026-11-02-hierarchical-rtl-output.md | 7 | 3 | 已裁决 | 2026-11-02 | ADR-058 §2.1 v2.3 | docs/adr/ADR-058-hierarchical-rtl-output.md | — | — |
| docs/architecture/2026-11-02-hierarchical-rtl-output.md | 7 | 4 | 已收口 | 2026-11-06 | T3-005 实施后收口 | .rddf/reviews/T3-005-equivalence-debt-review.md | — | — |
| docs/architecture/2026-11-02-hierarchical-rtl-output.md | 7 | 5 | 已收口 | 2026-11-06 | 端口命名护栏 | openspec/changes/archive/2026-11-06-port-naming-contract-guard-2026-11,scripts/verify_hierarchical_port_naming.sh | — | — |
| docs/architecture/2026-11-02-hierarchical-rtl-output.md | 7 | 6 | 不做 | 2026-11-02 | 先用「每类型一份 module」 | — | — | — |
| docs/architecture/2026-11-02-hierarchical-rtl-output.md | 7 | 7 | 不做 | 2026-11-02 | 框架本身单时钟域（ADR-008） | docs/adr/ADR-008-single-clock-domain-constraint.md | — | — |
| docs/architecture/2026-11-02-hierarchical-rtl-output.md | 7 | 8 | 不做 | 2026-11-02 | ADR-024 已评估，另案 | docs/adr/ADR-024-blackbox-ip-integration-evaluation.md | — | — |
| docs/architecture/2026-11-02-hierarchical-rtl-output.md | 7 | 9 | 已知 | 2026-11-02 | Verilator 等价性验证的排除项 | docs/adr/ADR-035-verilator-backend.md | — | — |
| docs/architecture/2026-11-02-hierarchical-rtl-output.md | 7 | 10 | 不做 | 2026-11-02 | 需真实综合工具，超出本仓范围 | — | — | — |
| docs/architecture/2026-11-02-hierarchical-rtl-output.md | 7 | 14 | 已裁决 | 2026-11-02 | ADR-062 决策⑤ | docs/adr/ADR-062-hierarchical-unit-interface-typing.md | — | 下游 |
| docs/architecture/2026-11-02-hierarchical-rtl-output.md | 7 | 12 | 已收口 | 2026-11-02 | C0-0 spike 提取器 | scripts/lib/verilog_tree_extract.py | — | — |
| docs/architecture/2026-11-02-hierarchical-rtl-output.md | 7 | 13 | 已收口 | 2026-11-06 | 随 T3-005 关闭 | openspec/changes/archive/2026-11-06-hierarchical-rtl-equivalence-2026-11 | — | — |
| docs/architecture/2026-11-02-hierarchical-rtl-output.md | 7 | 11 | 已收口 | 2026-11-07 | P6 + P8 双立项归档 | openspec/changes/archive/2026-11-07-p6-hierarchy-friendliness-diagnosis-2026-11,openspec/changes/archive/2026-11-07-p8-hierarchical-equivalence-public-helper-2026-11 | — | — |
| docs/architecture/2026-11-02-hierarchical-rtl-output.md | 7 | 15 | 已知 | 2026-11-08 | 三档 PortPolicy 在可合成形态上语义未分化 | — | 三档差异化处置（Reject 拒可合成形态 / SynthesizeFatal 先合成后失败）且默认档 SynthesizeWarn 的 CHWARN + 产物注释义务分别落地后复核 | builder |
| docs/architecture/2026-11-02-hierarchical-rtl-output.md | 7 | 16 | 已裁决 | 2026-11-08 | ADR-066 决策 ① 落点语义裁定 | docs/adr/ADR-066-hierarchical-parent-side-landing-semantics.md | — | — |
| docs/architecture/2026-11-02-hierarchical-rtl-output.md | 7 | 17 | 已收口 | 2026-11-08 | 语料登记归因与 tripwire 同批修订 | openspec/changes/archive/2026-11-09-hierarchical-parent-side-landing-implementation | — | — |
| docs/architecture/2026-11-02-hierarchical-rtl-output.md | 7 | 19 | 已收口 | 2026-11-08 | 路径 B 父侧落点网 + 产物可编译 | openspec/changes/archive/2026-11-09-hierarchical-parent-side-landing-implementation | — | — |
| docs/architecture/2026-11-02-hierarchical-rtl-output.md | 7 | 20 | 已收口 | 2026-11-10 | P5 实施归档（三桶判定 + (ii) 支持 + en fail-closed + 归因分桶） | openspec/changes/archive/2026-11-10-mem-port-arg-cross-layer-2026-11 | — | — |
| docs/architecture/2026-11-02-hierarchical-rtl-output.md | 7 | 21 | 未决 | 2026-11-10 | route (c) 字面同名当前不做 · 触发条件 = 用户在看到 manifest 产物后仍要求人眼可读 | — | 用户在看到 manifest 产物后仍要求人眼可读 ⇒ ⇒ 重开 route (c) 裁定 | architect |
| docs/architecture/2026-11-02-hierarchical-rtl-output.md | 7 | 21b | 未决 | 2026-11-10 | 根端口裸名目标钉 · 随 route (c) 一并处置 | — | route (c) 重开时一并处置 | architect |
| docs/architecture/2026-11-02-hierarchical-rtl-output.md | 7 | 22 | 未决 | 2026-11-12 | 同父同名实例 ⇒ 七处索引表 last-wins ⇒ 实例连错 module + module 不可达（合法 Verilog ⇒ R1 lint 盲区） | — | 同父下出现同名实例 ⇒ ⇒ 重开「实例名唯一性」裁决（分析层前置校验新增一条响亮拒绝） | architect |
