# Proposal: 端口名的 C++ 成员可解析性契约（`port-naming-member-parity-contract-2026-11`）

> ⚠️ **本 change 是 `port-naming-ratchet` 的 N4「规范落点」** ⇒ ⇒ **该 Requirement 的唯一承载**
> （`.rddf/roadmap/phase-6-rtl-productization.md:88`）⇒ ⇒ ⇒
> `port-name-literal-parity` 的 P2 **只是契约化提案** ⇒ ⇒ ⇒
> **MUST NOT 另立一个 change 承载同一条 Requirement**（否则归档后主 spec 会出现两条同名 Requirement）。

## Why

### 缺陷（实测 · 可复算）

用户在 `port-naming-ratchet-2026-11.md` 提出的原始诉求是「生成的 RTL **顶层端口**希望保持和 C++ 侧一样的命名」。

三探针实测结论（scratch · 未进仓 ⇒ 结论已固化进 `.rddf/improvements/port-name-literal-parity-2026-11.md` A.1）：

| 探针 | 结果 |
|---|---|
| 根实例名置空 | 节点名 `unnamed.prog_addr` ⇒ Verilog `unnamed_prog_addr` ⇒ **不是**裸 `prog_addr` |
| 同名兄弟实例 | `top_dup_prog_addr` / **`top_dup_prog_addr_1`** ⇒ ⇒ 「逐字相同」当前不成立 |
| 层次化分析层 | **复用**扁平全局表（`src/hierarchy_analysis.cpp:195-201`）⇒ ⇒ **不**提供端口名消歧 |

⇒ ⇒ 在当前扁平发射器下，「全体组件字面同名」**不成立**；「根组件端口字面同名」**有条件成立**，但要它成立需改公开 API 语义 + 全仓产物字节变更
⇒ ⇒ ⇒ **裁决为「当前不做 + 触发条件式结案」**（用户 2026-11-10 拍板）⇒ ⇒ ⇒ 改以「**机器可解析**」的契约兑现诉求。

⇒ ⇒ **本 change 的问题陈述** = 「顶层端口名与 C++ 成员名之间**没有**任何机器可解析的契约」⇒ ⇒ 下游工具（独立 TB 生成器 / 端口文档生成器 / 下游集成）**只能靠猜**。

### 为什么现在做

① **它是 manifest 的规范前提** —— `rtl-emit-product-tooling` 的 C4 定义了 `cpp_member` / `is_top_io` / `owner` 字段 ⇒ ⇒ 但**字段语义若不在规范里钉死** ⇒ ⇒ 每个下游各自解释 ⇒ ⇒ ⇒ 必然分叉；
② **它是「零序号」承诺的兑现点** —— 用户看到的痛点是「端口名带 `_1` `_2` 后缀」⇒ ⇒ 而「显式命名 ⇒ 零序号」需要一个**规范依据**；
③ **它是 B1 批的地基** —— 批次表把「manifest schema（C4）」与「N4 规范落点」都排在 B1 ⇒ ⇒ ⇒ 顺序已在批次表钉死。

## What Changes

1. **`rtl-hierarchical-output` ADDED ×1** —— Requirement「顶层端口名的 C++ 成员可解析性 MUST 按端口三类分别规定」：
   - **三类互斥划分**（**按序判定**：先 A ⇒ 再 B ⇒ 剩余为 C）
     - **A · 基础设施**（`role` ∈ {`clock`, `reset`}）⇒ ⇒ **MUST NOT** 被要求提供 `cpp_member`
     - **B · 显式命名**（在命名棘轮登记表中有条目）⇒ ⇒ **MUST** 提供，且其值 MUST 等于登记表中的显式名
     - **C · 未显式命名** ⇒ ⇒ **MUST NOT** 被要求提供（走兼容去重路径）
   - **归属空洞排除** —— `owner == nullptr` 且 `role` 非 `clock`/`reset` 的端口 **MUST NOT** 适用本 Requirement ⇒ ⇒ 由分析层 `CHWARN` 单独处置
   - **字面同名的 SHOULD + 不可能条件** —— 用户诉求的**剩余部分**在此兑现（**带前提的 SHOULD**，不是不可满足的 MUST）

   ⇒ ⇒ **delta 正文逐字取自** `.rddf/checks/decision-request-port-naming-parity-2026-11.md` §2 R1-a 的围栏内容（该围栏已纯化 ⇒ ⇒ 内含**零条**审稿日志）。

2. **零代码改动** —— 本 change **只落规范** ⇒ ⇒ `git diff --stat` MUST NOT 含 `src/**` 与 `include/**`（**机械判据**）。

## 不做（Non-Goals）

- **不做**「顶层端口名与 C++ 成员名**逐字相同**」 ⇒ ⇒ 已由 oracle 建议 1 裁决为**不可满足**（结构上：唯一性要求 + `sanitize` 非单射）⇒ ⇒ 改以带前提的 SHOULD 兑现。
- **不做**「根实例名清空」 ⇒ ⇒ 探针实测得 `unnamed_prog_addr` ⇒ ⇒ route (c) 在当前命名机制下**不可实现** ⇒ ⇒ 已登记为未决残差（台账 §7 行 21 / 21b）。
- **不做** manifest schema 本身 ⇒ ⇒ 那归 `rtl-emit-product-tooling` 的 C4（本 change **只引用**字段语义，**MUST NOT** 重定义）。
- **不做** 命名棘轮表 / 护栏 ⇒ ⇒ 那归 `port-naming-ratchet` 的 N2（**必须先于本 change 落地** ⇒ ⇒ 否则 C 类的登记义务当场不可满足）。
- **不做** 归属空洞类的处置 ⇒ ⇒ 独立立项（由分析层 `CHWARN` 承担）。

## 依赖与顺序

| # | 依赖 | 理由 |
|---|---|---|
| 1 | **`port-naming-ratchet` N2（基线棘轮）MUST 先落地** | C 类「MUST 被登记在棘轮豁免表」依赖该表**已存在** ⇒ ⇒ 否则条款当场不可满足 |
| 2 | `rtl-emit-product-tooling` C4（manifest schema） | `cpp_member` / `role` / `is_top_io` / `owner` 的字段语义来源 |

⇒ ⇒ **本 change 的实现体量极小（纯文档）**，但**顺序约束是硬前置**。

## 验收标准（AC）

- [x] `openspec/changes/port-naming-member-parity-contract-2026-11/specs/rtl-hierarchical-output/spec.md` 的 delta 与 `.rddf/checks/decision-request-port-naming-parity-2026-11.md` §2 R1-a 围栏内容**逐字一致**
- [x] delta 用 `## ADDED Requirements`（**不是** `MODIFIED`）⇒ ⇒ 否则 `scripts/verify_openspec_delta.sh` R3 会**静默不合并**
- [x] `bash scripts/verify_openspec_delta.sh port-naming-member-parity-contract-2026-11` 绿
- [x] `openspec validate --specs`（**非 strict**）**不新增**红项
- [x] 本 change 的 `git diff --stat` **不含** `src/**` 与 `include/**`
- [x] 主 spec `rtl-hierarchical-output/spec.md:309-316` 的**既有** Scenario（顶层端口表 = clock → reset → 无 driver input → 全部 `ch_out`）**未被触碰**（本 delta 是**追加**，与它**正交**：那条定**端口集合**，本条定**元信息可解析性**）
- [x] `component-ownership-and-naming` 的那条 Requirement 由**另一个** change（`node-name-prefix-path-contract-2026-11`）承载 ⇒ ⇒ 本 change **MUST NOT** 出现它（**一份 Requirement · 一处落点**）

### ✅ 验收记录（verifier · 2026-11-11）

> **验收报告**：`.rddf/reviews/b0-batch-verify-2026-11.md`（B0 批六项 · 本 change 的实施体在 B0 内完成）
> **逐条证据**见该报告 §4（`node-name-prefix-path-contract` 8/8 · `port-naming-member-parity-contract` 7/7）
> ⚠️ **归档注**：归档 MUST **禁** `--skip-specs` ⇒ ⇒ 本 delta 会被合并进主 spec
> ⇒ ⇒ ⇒ B1 的 **N4「规范落点」** = **归档本 change**（不是另写一份规范）

---

## 用户可见变更

> 本段按 `.rddf/checks/public-api-surface.md` 的载体机制填写（机制生效日 = 2026-11-09）。

- **库代码 / 公开 API：无变更**（`include/**` 与 `src/**` **零改动** ⇒ ⇒ 无行为、无 ABI 影响）。
- **规范（下游相关，**值得知道**）**：`openspec/specs/rtl-hierarchical-output/spec.md` 新增 **1 条 Requirement**
  「顶层端口名的 C++ 成员可解析性 MUST 按端口三类分别规定」
  ⇒ ⇒ **下游若实现端口命名 / 端口映射工具 MUST 按该三类分别规定**（不许用一个判据套三类）。
  ⇒ ⇒ ⇒ **这是 B1 的 N4「规范落点」** —— 命名规则的**唯一承载**就是这条 Requirement
  （`hierarchical-port-naming-guard` **MUST NOT** 复述）。
- **公开 API 移除 / 改名：无** ⇒ ⇒ **不走** P14 的移除登记。
