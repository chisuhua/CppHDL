# phase-6 **P2 / P2b** —— roadmap 事实核对（planner · 2026-11-09）

- **任务**：`phase-6.md` 的 **P2**（roadmap 事实核对）+ **P2b**（Windows-2022 CI 不做登记）
- **判据来源**：① 用户指令「继续下一步」⇒ ② `phase-6.md` 的 P2 / P2b 出口判据
  （P2：feature 与 phase 状态**逐条**与 `AGENTS.md` / capability / 归档件一致 ·
  `verify_roadmap_fragments.sh` 绿 · **phase-5 可据实转 `done`**；
  P2b：phase-5 完成条件里该项标为「不做（附裁决日期）」，**不动 CI**）
- **铁律**：**不采信** `phase-6.md` 那份「已实测的漂移清单」—— 它是**快照**，
  本轮**逐条复跑**（结果：其中 **2 条已不成立** ⇒ 见 §2）

---

## 1. 结论（3 行）

1. **phase-5 转 `done`**（9 条完成条件全部有结论：8 ✅ + 1 ⛔ 不做）。
2. **P2b 完成**：Windows-2022 CI 标为「不做（2026-11-03 裁决）」，**CI 文件零改动**。
3. **phase-6 的 P2 漂移清单有 2 条已过时**（① 与 ② 的附注）⇒ 已在 `phase-6.md` 里订正，
   并**补上它漏列的第 ⑥ 条**（`phase-5.md` 的**概览表**）。

---

## 2. 逐条核对（**每条都有工具证据**）

### 2.1 原清单 ① —— `feat-rv32i-isa-completeness` 的 ④⑤「待做」

| 检查 | 命令 | 结果 |
|---|---|---|
| 文件里还有「待做 / ⏳ / 🟡 / 未完成」吗 | `grep -n "待做\|⏳\|未完成\|🟡" .rddf/roadmap/features/feat-rv32i-isa-completeness.md` | **零命中** |
| ④ 是否已归档 | `ls openspec/changes/archive/ \| grep fetch-skid` | `2026-10-27-rv32i-fetch-skid-buffer-2026-10` ✅ |
| ⑤ 是否已归档 | `ls openspec/changes/archive/ \| grep load-byte` | `2026-10-28-rv32i-load-byte-lane-2026-10` ✅ |
| 何时修的 | `git log --oneline -1 -- <该文件>` | `ce683e2 chore(roadmap): feat-rv32i-isa-completeness → completed（五项 change 全部归档）` |
| front-matter | `head -4` | `status: completed` ✅ |

⇒ ⇒ **不是漂移**（`ce683e2` 已修）。**本轮仍做了一处订正**：该文件的**概述审计表**
「后果」列是 **2026-10-26 快照**，其中「固件 MUST 在停顿点后手工插 NOP」在 ④ 之后**已不成立**
⇒ 表下加了明确的状态注记（7 条全部消除 + ⑤ 补的读侧 + 现在只剩 CSR/ECALL/FENCE）。

### 2.2 原清单 ② —— `objective-test-coverage-100pct` 的旧「≥ 100」

| 检查 | 命令 | 结果 |
|---|---|---|
| objective 状态 | `grep -m1 '^status:' .rddf/roadmap/objectives/objective-test-coverage-100pct.md` | `status: completed` ✅ |
| 完成判据现为 | `sed -n '38,45p'` | ⭐「**双后端 parity 源文件数 ≥ 60**（**取代**旧的 `ctest -L verilator_e2e ≥ 100`）」 |
| 旧口径是否残留为**活判据** | 同上 + `grep -n "≥ 100"` | 只剩 **两处带日期的历史快照**（§1 的 2026-10-26 状态、§2 的「为什么必须改」）⇒ 均已明示 superseded |
| 实测达成 | `bash scripts/verify_dual_backend_parity.sh` | `parity 源文件数 = 66` · `macro 调用数 = 289` · **两锚点均匹配** ✅ |

⇒ ⇒ **不是漂移**（旧口径已显式作废，历史快照按仓内惯例原样保留）。
原清单写「objective 文件正文仍有旧的『≥ 100』口径 ⇒ 一并核对」——核对结论 = **合规**。

### 2.3 原清单 ③ —— RV32I ISA 语义边界

| 检查 | 结果 |
|---|---|
| ① `rv32i-store-byte-enable-2026-10` | `2026-10-26-…` ✅ |
| ② `rv32i-jump-sra-2026-10` | `2026-10-27-…` ✅ |
| ③ `rv32i-forwarding-decision-2026-10` | `2026-10-27-…` ✅ |
| ④ `rv32i-fetch-skid-buffer-2026-10` | `2026-10-27-…` ✅ + `docs/adr/ADR-048-rv32i-fetch-skid-buffer.md` 存在 |
| ⑤ `rv32i-load-byte-lane-2026-10` | `2026-10-28-…` ✅（★ 原清单只写 ①–④ ⇒ **实为 ①–⑤**） |

⇒ ⇒ 已在 `phase-5.md` 勾选，并把口径订正为 **①–⑤**。

### 2.4 原清单 ④ —— riscv-mini RTL 缺陷

| 检查 | 结果 |
|---|---|
| ① `riscv-mini-cache-wrapper-live-logic-2026-10` | `2026-10-28-…` ✅ |
| ② `clint-mtime-autoincrement-2026-10` | `2026-10-30-…` ✅ + `docs/adr/ADR-049-clint-mtime-semantics.md` |
| feature 状态 | `.rddf/roadmap/features/feat-riscv-mini-rtl-debt.md` ⇒ `status: completed` ✅ |

### 2.5 原清单 ⑤ / **P2b** —— Windows-2022 CI

| 检查 | 命令 | 结果 |
|---|---|---|
| CI 是否真的只有 Ubuntu | `grep -n "runs-on:" .github/workflows/ci.yml` | 5 处字面 `ubuntu-22.04` + 1 处 `${{ matrix.os }}` |
| 那个矩阵的取值 | 解析 `ci.yml` 的 `build-and-test.strategy.matrix` | `os: ['ubuntu-22.04']` ⇒ **无 Windows/macOS** ✅ |
| 裁决有落点吗 | `grep -n "2026-11-03" .rddf/roadmap/features/feat-platform-matrix.md` | 主题行 + §正文 + 矩阵表 + Verilator 段，共 5 处 ✅ |

⇒ ⇒ **P2b 的处置 = 只登记 + 关闭 → 已完成**（`phase-5.md` 完成条件标为
`- [x] ~~Windows-2022 CI 启用~~ ⇒ ⛔ 不做（2026-11-03 用户裁决）`）· **CI 文件零改动** ✅

### 2.6 ★ 原清单**漏列**的第 ⑥ 条 —— `phase-5.md` 的**概览表**

P2 原清单只核对「完成条件」与两个 feature 文件 ⇒ **漏了 `phase-5.md` 自己的概览表**
（4 行状态）。实读结果：3 行仍 🟡/⏳、平台矩阵仍「⏳ 计划中」⇒ 已全部改为终结态
（✅×3 + ⛔×1）。

---

## 3. feature 与 phase 的状态**逐条**核对（P2 出口判据 ①）

### 3.1 phases（6 个）

| phase | status | 核对 |
|---|---|---|
| phase-1 / 2 / 3 / 4 | `done` | — |
| **phase-5** | `active` → **`done`** | 本轮转（9 条完成条件全部有结论） |
| **phase-6** | `active` | 本窗口 ⇒ 转后它是**唯一** active ✅ |

### 3.2 features（9 个 · 与根 `AGENTS.md` AUTO 块逐一对照）

| feature | status | 核对证据 | 结论 |
|---|---|---|---|
| `feat-cache-rewrite` | completed | `ls include/cpu/cache/ \| grep -i stub` = **空**（stub 物理删除） | ✅ 一致 |
| `feat-dual-backend-coverage` | completed | objective `completed` + parity **66 ≥ 60**（护栏实测） | ✅ 一致 |
| `feat-hdl-logical-deprecation` | active | `[[deprecated]]` **8 个 overload**（`operators_logic.h` 6 + `io_logical.h` 2）· `verify_deprecation_calendar_gate.sh` PASS（**仍在锁期内** · 最早 2027-04-17）· `verify_deprecation_callsites.sh` PASS（0 新增） | ✅ 一致（**必须**保持 active） |
| `feat-hierarchical-rtl-output` | active | phase-6 的 P1 ✅ / **P5 · P7 · P10 未做** | ✅ 一致 |
| `feat-platform-matrix` | completed | `ci.yml` 全 `ubuntu-22.04`（2026-11-03 裁决） | ✅ 一致 |
| `feat-riscv-mini-convergence` | completed | `2026-10-25-rv32i-canonical-convergence-2026-10` 等归档 + ADR-046 | ✅ 一致 |
| `feat-riscv-mini-rtl-debt` | completed | 两项归档（2026-10-28 / 2026-10-30） | ✅ 一致 |
| `feat-rv32i-integration` | completed | `2026-10-27-dual-backend-rv32i-integration-2026-10` 等归档 | ✅ 一致 |
| `feat-rv32i-isa-completeness` | completed | ①–⑤ 归档 | ✅ 一致 |

### 3.3 objectives（4 个）

`objective-cache-stub-retired` / `objective-riscv-mini-convergence` / `objective-rv32i-e2e` /
`objective-test-coverage-100pct` ⇒ **全部 `completed`** ✅

---

## 4. 改动清单

| 文件 | 改动 |
|---|---|
| `.rddf/roadmap/phases/phase-5.md` | `status: active → done` · 概览表 4 行（✅×3 + ⛔×1）· 完成条件 3 条勾选 + 1 条标「不做（附裁决日期）」· 概览加收口注记 · **§计划能力 第 4 条**（`4 个 change` → **5 个**、补 ⑤ 行、删「④ ⏳ 唯一剩余项」、订正「仍不覆盖」清单）· **依赖图**（`只剩 ④` → **①–⑤ 全部已归档**） |
| `.rddf/roadmap/features/feat-rv32i-isa-completeness.md` | 概述审计表下加**状态注记**（7 条已消除 + ⑤ 补的读侧 + 只剩 CSR/ECALL/FENCE） |
| `.rddf/roadmap/phases/phase-6.md` | P2 漂移清单：① 标「已无漂移」· ② 标「不是漂移」· ③ 订正为 ①–⑤ · **新增 ⑥**（概览表）· P6 的映射格订正为「仅 ISA 领域归属，CSR/ECALL/FENCE 独立立项」· P2 / P2b 行 ✅ |

### 4.1 ★ **P2 审计的补漏（同日第二轮 · 用户裁决「一起修」）**

P2 首轮只核对了 `.rddf/roadmap/**` 的 fragment + 根 `AGENTS.md` ⇒ **漏了两处** ⇒ 用户裁决
「一起修」⇒ 本轮补齐：

| # | 漏项 | 实测 | 处置 |
|---|---|---|---|
| **⑦** | **`.rddf/roadmap.md` = 第二份 feature / phase / objective 索引** ⇒ 与 fragment front-matter 漂移 | ① Phase Skeleton 表 phase-5 仍 `active`（fragment 已 `done`）· ② **无 phase-6 行**（fragment 2026-11-09 就有）· ③ Objectives 段 `objective-test-coverage-100pct` 仍 `active`（fragment 已 `completed`）· ④ `feat-platform-matrix` 主题仍是「CI 平台矩阵扩展（macOS-14 已启用 → Windows-2022）」⇒ 与 AGENTS.md AUTO 块的「仅 Ubuntu」**不一致** · ⑤ 元信息「当前阶段」仍是 phase-5 | ★ **AUTO-INDEX 段由官方生成器刷新**（`roadmap_refresh_fragments.sh` ⇒ 一次性补齐 ①②③④ + 新增 Objectives 段）⇒ **不手工改**自动段 · **Phase Skeleton 表与元信息**（手工维护段）⇒ phase-5 → `done` / `2026-11` + **补 phase-6 行** + 当前阶段 → phase-6 + 最后更新 → 2026-11-09 |
| **⑧** | ★ **「`rddf roadmap --update-agent-md` 的 CLI dispatch 在本仓缺失」这条结论已过时**（phase-6 的 G1 行 + 护栏头注释 + CI 注释共 **3 处**同一断言） | dispatch **存在**（工具链 **rdd-workflow v4.0.0** @ `d5376087`）⇒ 但它按**项目根**解析 `skills/roadmap/scripts/…` ⇒ 本仓**未 vendor** `skills/` ⇒ 报「script not found」⇒ ⇒ **直接调用全局 payload 可用**，且**产出与已提交 AUTO 块逐字节一致** | 三处断言**统一订正** + 记录再验证（护栏的「rdd-workflow 升级 ⇒ MUST 重跑 oracle」触发条件**已满足并已复跑** ⇒ **vendored 渲染规则仍等价** ⇒ 护栏**无需改动**） |

⇒ ⇒ **本次修的是「G1 的结论」，不是「G1 的缺口」** —— 缺口本身（关联没有机械载体）**仍在**，
只是**同步手段**从「手工 + 待办」变成「**一条命令**（且已有）⇒ ⇒ 缺口**降级**为
「有工具，但需知道怎么调」⇒ ⇒ 已在 phase-6 的 G1 行写明。

**根 `AGENTS.md` 的 AUTO 块**：本轮**未改任何 feature 的 `status` / `phase_refs` / `主题`**
⇒ AUTO 块**无需变更**（`verify_roadmap_fragments.sh` 的 C1 会证明这一点）。
⚠️ 这解释了 phase-6 的「P2 收尾必须包含 AUTO 块同步」为何**本轮无动作** —— 该义务是
**条件触发**（只在 feature front-matter 变了才需要），不是「每轮必须改一次」。

---

## 5. 未做 / 遗留（如实登记）

| 项 | 说明 | 处置 |
|---|---|---|
| `objective-test-coverage-100pct` §1 的 2026-10-26 历史快照仍写「保持 active」 | 与 `status: completed` 字面冲突，但**带日期**且下一段明示 superseded | **不改**（仓内「历史快照原样保留」惯例） |
| `phase-5.md` 概览里 `ctest -L verilator_e2e = 37` 等历史数字 | 已是历史链；现行判据改为 parity 源文件数 | **不改**（同上，且已有「2026-10-30 判据已修订」块说明） |
| 表格「反引号内含未转义 `\|`」的同类缺陷另见 25 个 md 文件 | 存量、多为归档件 | **不改**（已登记在 P1 的审稿记录 §I） |
| ★ `.rddf/roadmap.md` 的 Current Sprint 表里「CLINT `mtime` 自增」行的 `` `hi\|lo` `` **未转义** ⇒ 该行 6 列（表头 5 列）⇒ 渲染溢出 | **存量**（`git show HEAD:.rddf/roadmap.md` 已存在 · HEAD 的 :54 行 = 现在的 :64 行）⇒ 与 phase-6 P11 行是**同一缺陷类** | **已修**（本轮顺手 · 转义为 `hi\|lo`）—— 理由：该文件本轮已在改，且与 phase-6 P11 行同类 ⇒ 留着一条**已知坏行**在刚编辑过的文件里更差 ⇒ ⇒ 已修并在此登记（**不是**静默清理） |

---

## 6. 验证

| 项 | 结果 |
|---|---|
| `bash scripts/verify_roadmap_fragments.sh` | **PASS**（C1 AUTO 块守恒 + C2/C3 完成度一致） |
| `bash scripts/verify_roadmap_fragments.sh --self-test` | **PASS** |
| `bash scripts/verify_test_counts.sh` | **PASS** |
| `bash scripts/verify_dual_backend_parity.sh` | **PASS**（66 / 289） |
| `bash scripts/verify_deprecation_calendar_gate.sh` / `verify_deprecation_callsites.sh` | **PASS** |
| 6 个 phase 的 `status` | phase-1..5 = `done` · phase-6 = `active`（唯一） |
| `phase-6.md` 表格结构 | 列数全部一致（含新增的 ⑥ 行） |
