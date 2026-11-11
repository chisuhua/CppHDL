# Proposal: 端口清单（manifest）schema `rtl-emit-manifest-v1`（`rtl-emit-portability` 的第一块地基）

> **来源**：`rtl-emit-product-tooling-2026-11.md` 的 **C4**（B1 批 · 第一项）
> ⚠️ **顺序硬约束**：C4 MUST 先于 C1（CLI）· C5（产物护栏）· N5(a)（manifest 双字段）· N3（名字稳定性）
> —— 这四项**全部**消费本 schema ⇒ ⇒ 否则下游各自定义字段 ⇒ ⇒ 必然漂移。
> ⚠️ **本 change 是纯规格 change** ⇒ ⇒ `src/**` 与 `include/**` **零改动** ⇒ ⇒ 落地方式 = **归档**。

## Why

### 缺陷（实测 · 可复算）

1. **端口身份无权威载体** ⇒ ⇒ 下游要回答「这个端口**是谁的**、**多宽**、**是不是顶层 IO**、
   **它有没有被驱动**」时，只能去翻产物文本或节点 id ⇒ ⇒ 每次都要重新推导。
   ⇒ ⇒ 典型症状：层次化产物里 `top_unnamed_input` 这类名字**不可自解释**（用户要写 TB
   必须查 port map —— planner 本轮就是靠 node-id 对照才查出来的）。

2. **「没被驱动」这个状态今天**既不**可观测**也**不可区分**：
   - **节点级**「有没有被消费」—— 有判据（`diagnose_drive()` 已在仓内）；
   - **功能级**「驱动它是否产生**可观测行为**」—— **没有**判据。

3. ⚠️ **一个 `driven: bool` 表达不了第三类端口**（`rtl-emit-product-tooling` 的 planner 复核订正）：
   `ext_clk` 是**有连线**的（`rv32i_pipeline_top.h:137` 接到了 `pipeline.io().clk`）
   ⇒ ⇒ 节点级**有消费者**；**但** `include/cpu/pipeline/**` 对 `io().clk` **零消费者**
   （grep 无命中）⇒ ⇒ **变异 tie 1 ⇒ 零行为影响** ⇒ ⇒ 它属于**第三类：「有连线但无功能」**
   —— 既不是「活端口」，也不是「悬空端口」。
   ⇒ ⇒ 故 MUST 拆成**两个布尔**：`driven`（节点级）+ `effective`（功能级）
   ⇒ ⇒ `ext_clk` 成为 `driven=true ∧ effective=false` 的**标准样例**。

4. **端口面本身今天**已经是「正确的」（`rtl-hierarchical-output/spec.md:309-316` 的顶层端口表
   = clock → reset → 无 driver 的 input → 全部 `ch_out`）⇒ ⇒ 本 change **不动端口面**
   ⇒ ⇒ 只让下游**能自己筛**（例：TB 生成器可选「只连 `is_top_io == true`」）
   ⇒ ⇒ ⚠️ **该筛选是消费方的选择，MUST NOT 成为发射器的默认行为**。

### 为什么现在做

B1 的**每一项**都建立在 manifest 之上（CLI 要写它、护栏要比它、棘轮要读它）
⇒ ⇒ 先把 schema 定死，后面四项才不会各自发明字段。

## What Changes

1. **新 capability `rtl-emit-portability`**（本 change 是它的**第一个** Requirement）
   —— **端口清单（manifest）** 的 schema `rtl-emit-manifest-v1`：

   | 字段 | 类型 | 语义 |
   |---|---|---|
   | `name` | string | 端口名（**RTL 全名**，即产物里的名字） |
   | `dir` | enum | `input` / `output` |
   | `width` | uint | 位宽 |
   | `role` | enum | `clock` / `reset` / `data` |
   | `driven` | bool | **节点级**：该端口**是否被消费**（有无 driver / 有无 sink） |
   | `effective` | bool | **功能级**：驱动它**是否产生可观测行为** |
   | `owner` | string | **所属组件路径**（组件实例全路径） |
   | `is_top_io` | bool | 是否**根组件自有 IO**（**不是** inline 后代输出） |
   | `cpp_member` | string | **C++ 成员路径**（由 `port-naming-ratchet` 第 1 步消费） |
   | `semantic` | string? | **可选** · **v1 无生产者** |

   ⚠️ **`role: data_unused` 已删除**（metis 订正）—— 该取值**无判定原语**
   ⇒ ⇒ 改用 `driven: bool` 表达。

2. **实施体为空** ⇒ ⇒ 本 change **只落规格**（归档即合并 delta 进主 spec）
   ⇒ ⇒ **不产** CLI / 护栏 / 发射器改动（那是 C1 / C5 / C6）。

## 不做（Non-Goals）

- **不改端口面**（不增删端口、不改顺序）⇒ ⇒ 端口选择语义归 `rtl-hierarchical-output`。
- **不改命名规则**（`name` / `cpp_member` 只是被**消费**）⇒ ⇒ 命名规则归 `rtl-hierarchical-output`。
- **不实现 CLI**（C1）· **不实现护栏**（C5）· **不实现 codegen 卫生**（C6）。
- **不定义 `semantic` 的取值**（v1 **无生产者** ⇒ ⇒ 只留字段位）。
- **不做 `role` 的新取值**（`clock` / `reset` / `data` 三值封顶）。

## 依赖与顺序

| # | 依赖 | 理由 |
|---|---|---|
| 1 | **无** | 纯规格 ⇒ ⇒ 可**立即**归档 ⇒ ⇒ 为 B1 其余四项解锁 |

⇒ ⇒ ⚠️ **归档 MUST 禁 `--skip-specs`**（否则 delta 静默不合并 ⇒ ⇒ capability 不存在）。

## 验收标准（AC）

- [x] 本 change 的 delta 用 `## ADDED Requirements` ⇒ ⇒ 否则 `scripts/verify_openspec_delta.sh` R3 会**静默不合并**
- [x] `bash scripts/verify_openspec_delta.sh rtl-emit-manifest-schema-2026-11` 绿
- [x] `openspec validate --specs`（**非 strict**）**不新增**红项
- [x] 本 change 的 `git diff --stat` **不含** `src/**` 与 `include/**`（**纯规格**）
- [x] 归档后主 spec 出现 capability `rtl-emit-portability`，其 Requirement 含**全部 10 个字段**
      （`name` / `dir` / `width` / `role` / `driven` / `effective` / `owner` / `is_top_io` /
      `cpp_member` / `semantic`）⇒ ⇒ **零字段缺失**（字段表是「可数」的 ⇒ 护栏可点算）
- [x] Requirement 明写 `role` 的取值**只有** `clock` / `reset` / `data` ⇒ ⇒ **不含** `data_unused`
- [x] Requirement 明写 `driven`（节点级）与 `effective`（功能级）**是两个独立字段**
      ⇒ ⇒ 且 `ext_clk` 是 `driven=true ∧ effective=false` 的**标准样例**
- [x] Requirement 明写 `is_top_io` 的筛选是**消费方的选择**，MUST NOT 成为发射器的**默认行为**
- [x] Requirement 明写 `semantic` 在 v1 **无生产者**
- [x] `rtl-hierarchical-output` 的**既有** Requirement **未被触碰**（本 delta 是**追加**）
- [x] `core-node-driver-visibility` 的**既有** Requirement **未被触碰**
      （`driven` MUST 复用 `diagnose_drive()`，**MUST NOT** 另造驱动状态判定）
- [x] `docs/CHANGELOG.md` + `.rddf/checks/public-api-surface.md` 的「**行为变更（非符号移除）**」
      节各有一行（**无公开 API 移除 / 改名** ⇒ ⇒ 不走 P14 移除登记）

## Capabilities

| capability | 动作 | 说明 |
|---|---|---|
| **`rtl-emit-portability`**（**新**） | ADDED | 端口清单（manifest）schema `rtl-emit-manifest-v1`（**本 change 的唯一承载**） |
| `rtl-hierarchical-output` | **不动** | 端口面与命名规则的规范所有者 ⇒ ⇒ 本 change MUST NOT 复述 |
| `core-node-driver-visibility` | **不动** | `driven` 复用 `diagnose_drive()` ⇒ ⇒ 本 change **零改动** |

---

## 用户可见变更

> 本段按 `.rddf/checks/public-api-surface.md` 的载体机制填写（机制生效日 = 2026-11-09）。

- **库代码 / 公开 API：无变更**（`include/**` 与 `src/**` **零改动** ⇒ ⇒ 无行为、无 ABI 影响）。
- **规范（下游相关，**值得知道**）**：新增 capability **`rtl-emit-portability`**，
  其第一条 Requirement 钉住**端口清单（manifest）** 的 schema `rtl-emit-manifest-v1`
  ⇒ ⇒ **下游若实现 RTL 生成器 / TB 生成器 / 端口命名工具，MUST 按这 10 个字段产出 manifest**
  ⇒ ⇒ 尤其：`driven`（**节点级**「是否被消费」）与 `effective`（**功能级**「是否产生可观测行为」）
  是**两个独立字段** —— 「有连线但无功能」（如 `ext_clk`）这一类**单布尔表达不了**。
- **端口面不变**：本 change **不改**任何端口、不改顺序、不定义命名规则
  ⇒ ⇒ 下游现有的产物解析**不受影响**。
- **公开 API 移除 / 改名：无** ⇒ ⇒ **不走** P14 的移除登记。

### ✅ 验收记录（planner · 2026-11-11）

> **纯规格 change** ⇒ ⇒ 实施体 = **归档即合并 delta** ⇒ ⇒ 无代码面。
> **逐条证据**：`openspec validate rtl-emit-manifest-schema-2026-11` = valid ·
> `bash scripts/verify_openspec_delta.sh rtl-emit-manifest-schema-2026-11` **R1–R4 全绿** ·
> `openspec validate --specs`（非 strict）**44 passed / 0 failed**（**未新增**红项，且
> **未新增** long-requirement warning —— Requirement 正文 249 字符 < 500 阈值）·
> 本 change 仅 3 个文件（`.openspec.yaml` / `proposal.md` / `specs/…/spec.md`）⇒ ⇒
> **零** `src/**` 与 `include/**` 改动 · `rtl-hierarchical-output` 与
> `core-node-driver-visibility` 的既有 Requirement **均未被触碰**。
> ⇒ ⇒ **B1 的 C4 已解锁**：C1（CLI）· C5（护栏）· N5(a) · N3 现在有唯一 schema 可依赖。
