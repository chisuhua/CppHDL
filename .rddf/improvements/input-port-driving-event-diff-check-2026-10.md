---
name: input-port-driving-event-diff-check-2026-10
priority: P1
source: input-port-driving-guard-2026-10（只读 spike 的决策门结论）
phase: phase-5
category: testing
type: chore
created_at: 2026-10-28
---

# Improvement: 新增输入端口 MUST 被显式驱动 ⇒ 用**事件驱动（git diff）增量检查**守

- **Date drafted**: 2026-10-28
- **Drafted by**: build-agent (builder role)
- **Status**: 📋 5-segment draft，**决策权归 planner**（build 只登记，不定方案）
- **来源**：`input-port-driving-guard-2026-10` 的 spike 结论
  ⇒ 交付物 = `openspec/changes/archive/2026-10-28-input-port-driving-guard-2026-10/design.md`
  §2 / §3.4 / §4.1

## 1. Why (背景与动机)

`AGENTS.md §ANTI-PATTERNS` 登记的义务：「**新增**组件输入端口 ⇒ 全部实例化点
MUST 显式驱动」⇒ 未驱动端口读到**未定义值**（实测 `strbe` 读全 1、`rst_n` 读 0）
⇒ 依赖默认值会让测试「**碰巧**通过」而跨后端行为可能分叉。

⚠️ **spike 实测推翻了「通用状态型护栏」这条路线**（不是做不到，是**不该做**）：

| 实测 | 结论 |
|---|---|
| 运行期**无法识别端口身份**（节点名恒 `unnamed_input`、`source_location` 恒 `include/core/io.h:219`、Verilog 名 `dut_unnamed_input_N`） | 运行期检查的诊断**不可行动** ⇒ 该路线**关闭** |
| **per-TEST-CASE 是必需的粒度**（文件粒度会让「一个 DUT 的驱动掩盖另一个 DUT」⇒ 现有护栏头注释已述）⇒ 而只跑**写**路径的用例**理应**不驱动读通道 ⇒ 平规则在该粒度**必然**误报 | ⇒ **死结**（结构论证，非测量）⇒ ⇒ ⇒ ⚠️ 该论证**不依赖**任何逐 DUT 的计数表（初稿那份表因测量脚本的正则 bug 已作废） |
| 驱动藏在 **29 个文件作用域 helper**（213 处调用）+ **186 次宏**（133 次驱动在 lambda 体内）里 | 只扫 `TEST_CASE` 体内会**全部误报** ⇒ 需**过程间分析** ⇒ 成本与误报双高 |

⇒ ⇒ **痛点是「**新增**端口忘了驱动」** ⇒ 用**事件驱动**的检查正好对准**
⇒ ⇒ 实测负载：近 **100** 个 commit 的新增 `ch_in` 分布 =
**92×0 / 5×1 / 1×2 / 1×4 / 1×85**（唯一的大额是 `e015c027` 的一次性单源化重构）
⇒ ⇒ 典型增量负载 = **0–2 个新端口/commit** ⇒ ⇒ **成本趋零、误报趋零**。

## 2. What Changes (变更范围)

⚠️ **方案未定**（planner 裁决），但 spike 已给出**推荐形态**：

1. **判定口径** = **半自动**：脚本**只列出**
   「本次变更新增的 `ch_in` 端口」+「该端口的**全部实例化点**」
   +「各实例化点处是否出现该端口名的驱动」⇒ **由人判定** ⇒
   ⇒ 护栏**不**直接判违规（否则就退回「平规则 ⇒ 高误报」的老问题）；
2. **新增端口的判定** = git diff 的**新增行**里匹配 `ch_in<...> name;`
   ⇒ ⚠️ 必须同时支持两种声明形态：裸 `ch_in<...> x;` 与 `__io(...)` 宏块内
   （仓内 `__io(...)` 用 **267** 次）；
3. **实例化点的枚举** = `grep` `ch_device<X>` / `ch_module<X>` / `DUAL_BACKEND_TEST`
   ⇒ ⚠️ 须处理**模板别名**（`using Full32 = axi4::Axi4Slave<...>;` ⇒ `Full32`）
   —— 现有 `verify_rst_n_wiring.sh` 已有该逻辑，**可复用**；
4. **驱动判定** = `set_input_value(...<port>...)` 的文本匹配 + **helper 识别**
   ⇒ ⚠️ 必须**排除** `io().X <<= Y`（**内部**驱动，不是 TB 驱动；仓内 **682** 处）
   ⇒ ⚠️ 必须**点名** `io().X = value`（落在 `ch_in` 上 ⇒ **静默失效** ⇒ **真缺陷**）
   ⇒ ⚠️ **无法用正则区分 `ch_in` / `ch_out`** ⇒ ⇒ 要么解析 DUT 的 `io_type`
   （= 候选 B 的成本来源），要么**只列不判**（推荐）；
5. **CI 挂载点** = **PR 事件**（base branch → HEAD 的 diff）⇒ push/schedule 事件
   **跳过**（自比无意义）⇒ **必须**打印 `::notice`；
   ⚠️ CI 的 `git fetch` 失败 ⇒ **硬失败**（不得静默跳过 ⇒ 闸门形同虚设）
   —— 与 `scripts/verify_perf_baseline_anchors.sh` 同一纪律；
6. **MUST 带 `--self-test`**（合成 fixture 至少 5 种形态：新增 1 端口 / 新增 0 端口 /
   驱动在 helper 内 / 只有 `io().X = value` / 批量重构（量大 ⇒ 只列不判））
   ⇒ 每条判据各自要有判别力。

⇒ **前置**：**无框架改动、无 ADR**（不改契约、不改 API）⇒ 是仓内**最轻**的一类护栏。

## 3. Acceptance (验收标准)

- [ ] 脚本对**真实历史**跑出与 §1 实测一致的分布（92×0 / 5×1 / 1×2 / 1×4 / 1×85）
- [ ] 对**任一**合成 fixture 都能正确列出「新增端口 + 实例化点」
- [ ] `--self-test` 全绿，且每条判据**单独**变异即转红
- [ ] CI 在 **PR 事件**生效；push/schedule 事件**跳过**并打印 `::notice`
- [ ] CI 的 `git fetch` 失败 ⇒ **硬失败**（不得静默跳过）
- [ ] 报告里**不含**「直接判违规」的逻辑（半自动 ⇒ 人判）
- [ ] `scripts/verify_test_counts.sh` 等既有护栏仍全绿；**计数锚点不变**
      （本 change 不新增 ctest 条目 ⇒ `CTEST_TOTAL` 202 / `EXAMPLES_TEST_SOURCES` 17）

## 4. Capabilities (涉及的能力维度)

- `dual-backend-test-coverage` —— **可能 MODIFIED**（若「新增端口 MUST 显式驱动」
  的**核验方式**要写进契约；当前契约只规定了 R6 排除清单，**未**规定驱动纪律的
  核验手段）⇒ **待实施时判定**
- ⚠️ 与「必需端口注册表」（另一个 change）**互补**：本 change 守「**新增**端口」，
  登记表守「**已知**的必需端口（存量）」

## 5. Impact (影响面分析)

- **改动文件**：新增 `scripts/verify_input_port_additions.sh`（+ fixture 自测）
  ⇒ **无** `src/**` / `include/**` / `tests/**` 改动
- **计数**：**不变**
- **风险**：**低** ⇒ 半自动（不判违规）⇒ 最坏情况是「列出来了但没人看」
  ⇒ ⇒ 缓解：CI 日志用 `::warning` 显眼输出（不 `::notice`）
- **与既有护栏的关系**：与 `scripts/verify_rst_n_wiring.sh`（守**存量** `rst_n`）
  **互补不重叠**
