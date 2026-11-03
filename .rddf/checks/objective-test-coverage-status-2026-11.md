# 状态裁决包：`objective-test-coverage-100pct`

- **Date**: 2026-11-03
- **交付方**: build-agent（builder role）
- **裁决方**: **planner**（判据 / 阈值 / `status` 字段都归 planner ⇒ build **不改**）
- **触发**：Go 判据 ②（parity 源文件数 ≥ 60）**已达成并超额**（现 **66**），
  但 objective 仍 `status: active` ⇒ ⇒ 需要一次「关闭 or 重定义判据」的裁决。

> ⚠️ **本文件只交数字与证据**。任何勾选 / 阈值 / 状态变更 **MUST** 由 planner 落
> `objective-test-coverage-100pct.md` 与 §11 台账。

## 1. Go 判据逐条现状（§9.2）

| # | 判据原文 | 现状 | 证据（可直接复跑） |
|---|---|---|---|
| ① | Phase 1-5 全部 ship + archived | ⚠️ **1–4 已归档**；**Phase 5 从未以 change 形式存在**（见 §2） | `ls openspec/changes/archive/ \| grep -E "verilator-parity-all-tests\|dual-backend-cache-tests\|dual-backend-rv32i-integration\|dual-backend-full-coverage"` |
| ② | 双后端 parity 源文件数 ≥ 60 | ✅ **66**（阈值 +6，margin 10%） | `bash scripts/verify_dual_backend_parity.sh` ⇒ `PARITY_COUNT=66` |
| ③ | CTEST_TOTAL 同步 + `verify_test_counts.sh` 绿 | ✅ 但文档里的 **202** 已过时（现 **243**） | `bash scripts/verify_test_counts.sh` ⇒ 全绿；`cd build && ctest -N \| tail -1` ⇒ `243` |

### 1.1 计数现状（2026-11-03，post `wide-shl-truncate-word-carry-2026-11`）

| 量 | 值 | 命令 |
|---|---|---|
| `CTEST_TOTAL` | **243** | `cd build && ctest -N \| tail -1` |
| `VERILATOR_E2E_TOTAL`（带标签的 ctest 条目） | **75** | `ctest -N -L verilator_e2e \| tail -1` |
| non-perf | **232** | `ctest -N -LE perf \| tail -1` |
| **parity 源文件数**（Go 判据口径） | **66** | `scripts/verify_dual_backend_parity.sh` |
| parity macro **调用**数（参考量，非闸门） | **289** | 同上 |
| `ctest -LE perf` | **232/232 绿**（2026-11-03 实测） | `cd build && ctest -LE perf -j4` |

### 1.2 判据 ② 的增长轨迹（本轮新增的两个 —— 都是**缺陷驱动**，不是凑数）

| change | parity | 为什么 |
|---|---|---|
| `mux-slot-width-preservation-2026-11` | 64 → 65 | `instr_mux` 的槽位压窄（拷贝赋值 ⇒ RESIZE）⇒ interpreter ↔ Verilog 分歧 |
| `wide-shl-truncate-word-carry-2026-11` | 65 → 66 | `bv_shl_vector` 跨 word 进位方向反了 ⇒ 多 word 左移错值 ⇒ 同一类分歧 |

⇒ ⇒ 两者都不是「为达阈值而加文件」，而是**缺陷的回归判据**。

## 2. Phase 1–5 与实际归档 change 的映射

| Phase | 原始定义（`.rddf/improvements/verilator-parity-all-tests.md` §7） | 实际落点 | 状态 |
|---|---|---|---|
| 1 | macro + 8-10 test + 3 fixture + ci.yml | `2026-10-19-verilator-parity-all-tests` | ✅ 归档 |
| 2 | cache Sync/Async | `2026-10-19-dual-backend-cache-tests` | ✅ 归档 |
| 3 | Batch 1 | `2026-11-01-dual-backend-parity-batch-2026-11` 等 3 个 batch change | ✅ 归档 |
| 4 | RV32I pipeline + riscv-mini integration | `2026-10-27-dual-backend-rv32i-integration-2026-10` | ✅ 归档 |
| 5 | 「全量 100%」+「64-bit / clock toggle / reset 专项」 | **无同名 change**（见下） | ⚠️ **两半分别收口** |

### 2.1 Phase 5 的两半，各自的实际状态

1. **「64-bit port / clock toggle / reset 专项」** ⇒ ✅ **已落地**（4 个 target 全部注册）：
   `test_dual_backend_64_bit` · `test_dual_backend_clock` · `test_dual_backend_reset`
   · `test_dual_backend_template`（`tests/CMakeLists.txt:133-165`，均走
   `add_dual_backend_test`）。
2. **「100% 单元测试覆盖」** ⇒ ⚠️ **本身不可机械核验** —— objective §2 已明文
   把这一行降级为「愿景陈述，**不**作闸门」，并**用 parity 源文件数代理**
   （分母 = design surface，无权威定义）。

⇒ ⇒ **Phase 5 没有可执行的剩余工作量**：一半已完成，另一半的定义已被 §2 的
判据修订**取代**。

## 3. 未 dual 的 138 个 `add_catch_test` —— 它们**不是** Phase 5 的余额

`tests/CMakeLists.txt` 里 `add_dual_backend_test` = **59**、`add_catch_test` = **138**。
但 138 **不等于**「还差 138 个要做」：其中大量命中 capability 的 **R6 排除清单**
（编译期 / 裸 context / 无端口 / SECTION 重入 / 契约锁定的单后端参考实现）。

- 权威分类见 `openspec/changes/archive/2026-10-28-dual-backend-full-coverage-inventory-2026-10/design.md`
  （逐条 4 问裁决）。
- ⚠️ **该分类是 2026-10-28 的快照**，此后新增了 ~30 个 dual 文件与若干新单后端测试
  ⇒ ⇒ 若 planner 要一个「按值」的口径（例如「每个**可 dual 而尚未 dual** 的文件
  必须有书面豁免」），**需要一次新的盘点**（不是本包的范围）。

## 4. 三个选项（planner 选一个）

### 选项 A：**直接关闭** objective（`status: completed`）

- 依据：判据 ② 已超额（66 ≥ 60）；Phase 5 无可执行余额（§2.1）；
  判据 ③ 只是文档里的旧数字待同步。
- 代价：判据 ① 的措辞「Phase 1-5 全部 ship」**在字面上**不成立（Phase 5 无 change）
  ⇒ ⇒ 关闭时 MUST 同批把 ① 改写为「Phase 1–4 ship + Phase 5 的两半按 §2.1 收口」。
- 风险：把「全量」这件事**永久关闭** ⇒ ⇒ 将来若发现某个可 dual 却未 dual 的
  重要文件，没有 objective 承接（⇒ 只能走 improvement）。

### 选项 B：**关闭判据 ②，保留 objective**（转为「按值」的持续目标）

- 把 Go 判据 ② 改成「**每个可 dual 的文件都必须 dual 或有书面豁免**」（按值，不按数）
  ⇒ ⇒ 需要一次新盘点（§3 的 ⚠️）才能判定是否达成。
- 代价：新盘点 ≈ 一次只读普查（历史经验：`2026-10-28-...-inventory` 花了一轮完整
  会话）⇒ ⇒ 且「可 dual」的判定仍需 4 问 ⇒ **主观性回到判据里**。

### 选项 C：**保持 active，但把 Go 判据 ② 标为已达成并停止推进**

- 即：objective 继续 `active`（作为「背景约束」存在），不再以「爬数字」为工作项
  ⇒ ⇒ 后续只做「缺陷驱动的 dual 判据」（本轮 `mux` / `bv_shl` 就是这种形态）。
- 代价：`active` 且判据已达成 ⇒ ⇒ 台账里会出现「达成但仍 active」的长期悬置
  ⇒ ⇒ 下一位 planner 必须重读本包才知道为什么。

## 5. build 的建议（仅建议，裁决权在 planner）

**推荐选项 A + 把 ① 改写**，理由：

1. 判据 ② 是 §2 明文声明的**唯一**可机械核验的完成标准 ⇒ 它已超额达成；
2. Phase 5 的两半**都没有剩余可执行项**（§2.1）；
3. 「剩下的 138 个单后端测试」的性质**已经在 capability 的 R6 排除清单里**被
   制度化 ⇒ ⇒ 不是 objective 的余额，而是**契约的边界**；
4. 本轮两个 change 证明了一个更强的结论：**parity 的真正价值是「抓缺陷」而不是
   「覆盖数」**（`mux` 压窄 · `bv_shl` 进位，两个都是既有 232 个测试**全绿**时
   仍然存在的真 bug）⇒ ⇒ 继续按数字推进的边际收益低，按缺陷推进的收益高
   ⇒ ⇒ 这也是「关闭数字目标、保留作为背景约束」的实质理由。

⚠️ 若选 A，**同批 MUST**：
- 改 §9.2 判据 ① 的措辞（Phase 5 无 change 这一事实）与判据 ③ 的 `CTEST_TOTAL`
  （202 → **243**）；
- 在 §11 台账追加一行（裁决 + 依据 = 本文件的路径）；
- 检查 `AGENTS.md` 与本 objective 的 feature fragment（`feat-dual-backend-coverage`）
  是否需要同步（`bash scripts/verify_roadmap_fragments.sh` 会抓）。

## 6. 复跑清单（审阅本包时可一键复核）

```bash
cd build && ctest -N | tail -1                       # 243
cd build && ctest -N -L verilator_e2e | tail -1      # 75
cd build && ctest -N -LE perf | tail -1              # 232
bash scripts/verify_dual_backend_parity.sh           # PARITY_COUNT=66 / PAIRS=289
bash scripts/verify_test_counts.sh                   # 全绿
bash scripts/verify_roadmap_fragments.sh             # 全绿
```
