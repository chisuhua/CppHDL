# Tasks：把**已过时**的「未收口」陈述从 capability 主 spec 里收口

## C1 —— delta（**只改内容 · 不改标题 · 不新增/不删义务**）

- [x] `specs/chlib-stream-ready-ownership/spec.md`：**MODIFIED ×1**
      （Requirement「组合子 MUST NOT **无条件**忽略消费端的 ready」）
      - 逐条对照 `design.md` §3 的 A-1 / A-1′ / A-2 / A-4 / A-5 / A-6 订正正文
      - **A-7（新增 MUST）不写** ⇒ ⇒ 新增前瞻性义务属另一个 change
      - **A-6 钉死指代**（critic B2）⇒ ⇒ 「**不**属『**未收口**』之列」+
        「**上游侧**硬编码常量 ready **对所有转发类**违规」（指向 B-4 的
        Scenario）+ **如实登记判别力缺口**（「`<<=` + 常量」当前无护栏）
        ⇒ ⇒ **义务强度不变**（不新增超出 B-4 的义务）
      - 组件清单**不含** `stream_arbiter_sequence`（已死）⇒ ⇒ 且**不写死**
        `filters` 的个数
      - **ADR 归属**（critic B3）：转发类收口 = **ADR-054 §3.2 第 2 条**
        ⇒ ⇒ **不是** ADR-055（后者 = builder 链内中间级，全文 0 处提转发类）
- [x] `specs/rv32i-isa-semantics/spec.md`：**MODIFIED ×1**
      （Requirement「`DataTCM.strbe` MUST 被其实例化点显式驱动」）
      - 只改**范围限定 NOTE**（`design.md` §5）⇒ ⇒ **并如实保留边界**
      - **并修正同段计数**（3 → 4 个实例化点，grep `DataTCM<` 可证）
      - Scenario 标题与数量**不变**
- [x] `specs/chlib-stream-ready-ownership/spec.md`：**MODIFIED ×2**（其二）
      （Requirement「转发类组合子的 ready 所有权 MUST 是**默认值 + 可覆盖**」）
      - 逐条对照 `design.md` §3 的 **B-1 … B-4** 订正正文
      - **Scenario 标题逐字保留**（7 个里只改 3 个 ⇒ 另 4 个逐字保留）
      - 「局部常量充当 ready 源」⇒ ⇒ 限定为**上游侧**（输入侧）⇒ ⇒ 并排除
        **形态 E 的输出侧默认驱动**（否则规范按字面禁止已落地的形态 E）
      - 「**可覆盖**」三处**同口径**（critic S4）⇒ ⇒ 并注明它是**人工判据**
        （**R8 只按角色分类** ⇒ ⇒ 判不出可覆盖性）
- [x] **保留项逐段核对**（`design.md` §7 的 **K-1 … K-21**）⇒ ⇒ 全部在 delta 中
      原样出现 ⇒ ⇒ **这是头号风险**（合并 = 整条替换 ⇒ 漏一条即静默丢内容）

## C2 —— 校验

- [x] `openspec validate spec-stale-closure-text-correction-2026-11 --type change --strict` = valid
      （⚠️ 用仓内钉住的 **CLI 1.4.1**；本机 **1.14.3** 的结论**不可**替代）
- [x] `scripts/verify_openspec_delta.sh spec-stale-closure-text-correction-2026-11` 4 条全绿
      （R1 = delta 内无本 change 名 · R2 = 无占位符路径 · R3 = Requirement 标题逐字一致
      · R4 = tasks 的 bash 块语法可达）
- [x] **Scenario 标题集合**比对（归档**前**）：两个 capability 的
      `grep '^#### Scenario:'` 主 spec vs delta ⇒ **集合相等**（数量不可增删）
      ⇒ ⇒ 这是 CLI 拒绝归档的**唯一**前置条件，而 `verify_openspec_delta.sh` **不查**它

## C3a —— 归档（**禁** `--skip-specs`）

- [x] 归档前：`design.md` §4.1–§4.5 的五个「旧」块**仍逐字存在**于主 spec
      （`grep -F`）⇒ ⇒ 若已被他人改动 ⇒ **先重算**替换文本
      ⚠️ §4.5 是**追加**（原文逐字保留 + 首尾 dated 订正）⇒ ⇒ 其「旧」块
      = **整段原文**⇒ ⇒ `grep -F` 命中即通过
- [x] 归档 ⇒ `openspec/changes/archive/<date>-<name>/` ⇒ ⇒ **未**用 `--skip-specs`

## C3b —— 独立 commit：**手改** `## Purpose`（archive **不**合并 Purpose）

- [x] `openspec/specs/chlib-stream-ready-ownership/spec.md` 的 `## Purpose`
      **五处**（精确文本见 `design.md` §4.1 / §4.2 / §4.3 / §4.4 / §4.5）
- [x] §4.2 的新文本 MUST **保留**「判据 #7 仍未达成」的登记（缺陷已修 ≠ 判据已补）
- [ ] **与 C3a 分成两个 commit** ⇒ ⇒ 避免「漏改则除一条 grep 外全绿」

## C4 —— 归档后的核对（**可判定**形式，见 `design.md` §8）

- [x] `awk '/^## Requirements/,0' openspec/specs/chlib-stream-ready-ownership/spec.md | grep -c "直至它们落地\|⚠️ \*\*仍\*\*未收口者\|❌ \*\*仍未收口\*\*"`
      ⇒ **0 命中**（**未决语气**残留 ⇒ ⇒ ⚠️ 面**收窄**到该子集 ⇒ ⇒
      「两端都断」「未收口者」作为 dated / 历史语境引用**合法**
      ⇒ ⇒ ⚠️ **必须限定 Requirements 段**（Purpose 段由 C3b 手改 ⇒ ⇒
      手改前全文件判 0 = **必红**）
- [x] `grep -c "仍属违规" openspec/specs/chlib-stream-ready-ownership/spec.md`
      ⇒ **>= 1**（⚠️ **不是** `== 1`：K-4 正文 + A-6 Scenario **各** 1 处
      ⇒ ⇒ `== 1` 会诱使删掉其中一处来凑数 ⇒ ⇒ **义务静默消失**）
- [x] `grep -F "转发类**（mux / demux / arbiter）" openspec/specs/chlib-stream-ready-ownership/spec.md`
      ⇒ **命中**（K-4 逐字存在）
- [x] `awk '/^## Requirements/,0' openspec/specs/chlib-stream-ready-ownership/spec.md | grep -c "stream_arbiter_sequence"`
      ⇒ **== 0**（⚠️ **Purpose 段允许**点名它 ⇒ ⇒ §4.4 故意写「已物理删除」）
- [x] `grep -n "3 个实例化点" openspec/specs/rv32i-isa-semantics/spec.md`
      ⇒ **0 命中**
- [x] `grep -rnE "ch_module<DataTCM|ch_device<DataTCM|^using [A-Za-z_]+ = riscv::DataTCM<" include/ src/ examples/ tests/`
      ⇒ **4 处**（⚠️ 原判据的模式（以 `riscv::DataTCM<` 收尾）实测命中 **7** 处
      —— 额外 3 处是 `tests/test_rv32i_timing_dual.cpp` 里的**注释**
      ⇒ ⇒ 裸 `grep "DataTCM<"` 更是命中 **12** 处 ⇒ ⇒ ⇒ 二者**均不可复现**
      ⇒ ⇒ ⇒ 判据必须写**实例化形态**：3 处 `ch_module` / `ch_device` **构造**
      + 1 处 `using` **类型别名**（`tests/test_rv32i_timing_dual.cpp:92`
      ⇒ ⇒ 该别名被当 DUT 构造 ⇒ ⇒ 计入）⇒ ⇒ 与前 3 处合计 **4**）
- [x] `grep -n "demux\|fork" openspec/specs/chlib-stream-ready-ownership/spec.md`
      ⇒ **逐条人工过一遍**（每处 MUST 是「已收口 / 现行契约」语境）
- [x] `grep -n "out_ready = true" openspec/specs/chlib-stream-ready-ownership/spec.md`
      ⇒ **0 命中**（侧歧义已消除）
- [x] `grep -n "转发类现状 = 「上游恒不 ready = 死路」" openspec/specs/chlib-stream-ready-ownership/spec.md`
      ⇒ **0 命中**（已标注为**收口前**）
- [x] 行号引用漂移已处理：`.rddf/checks/architect-decisions-pending-2026-11.md:26`
      与 `.rddf/checks/next-execution-review-brief-2026-11.md:19` ⇒ ⇒ 改为**标题引用**
- [x] `grep -n "不属此类\|无例外集合\|不得把本条读作" openspec/specs/chlib-stream-ready-ownership/spec.md`
      ⇒ 命中处**逐条人工确认指代唯一、无两条互斥指令并存**（critic B2）
- [x] **ADR 归属**逐条人工核：转发类 ⇒ ADR-054 §3.2 第 2 条 ·
      builder 链内 ⇒ ADR-055 · demux ⇒ ADR-056 · fork ⇒ ADR-057（critic B3）

## C5 —— 同批的**文档面**状态订正（零逻辑改动）

- [x] `scripts/verify_stream_ready_ownership.sh` **头注释**（只改注释）
- [x] `.rddf/improvements/stream-forward-ready-closure-2026-10.md` 的
      「穷举全部入口」验收项（只加**状态订正**，**不改**其达成判定）
- [x] 同台账的 **Status 头**（critic S1b）：`arbiter×4`（含**已死的** `sequence`）
      · `filters×6` ⇒ ⇒ 改为「`arbiter` 族（`priority` / `round_robin` / `lock`）
      · `join` · `filters` 族 · `combine_with`」⇒ ⇒ **不写死个数**
- [x] `docs/adr/ADR-054-stream-ready-ownership-form.md` §3.2（只加**状态订正**，
      **不改** ADR 的决策内容）

## 验收

- [x] 护栏全绿：`scripts/verify_openspec_delta.sh`（全部 active change）·
      `scripts/verify_stream_ready_ownership.sh`（+ `--self-test`）·
      `scripts/verify_chlib_naming_honesty.sh`（+ `--self-test`）·
      `scripts/verify_input_port_required_registry.sh`（+ `--self-test`）
- [x] **零代码改动 ⇒ 无需跑 `ctest`**（计数锚点不变）
- [x] 计数锚点不变：`CTEST_TOTAL` / `VERILATOR_E2E_TOTAL` /
      `DUAL_BACKEND_PARITY_TOTAL` / `DUAL_BACKEND_PARITY_PAIRS` /
      `EXAMPLES_TEST_SOURCES`
- [x] 归档：**未**用 `--skip-specs`（两个 MODIFIED 已合并进主 spec）

⚠️ **`openspec validate --specs --strict` 的结论**（本机 CLI **1.14.3**；仓库钉
**1.4.1** ⇒ ⇒ `AGENTS.md` 已登记「1.14.x 新增两条 strict 规则」属**版本漂移**）：
归档后失败集 = 基线 **21** 项 **+ 1** 项（新增 `spec/chlib-stream-ready-ownership`，
`before - after` 为空 ⇒ ⇒ 无其它 spec 变化）。新增的那项**唯一**原因是 1.14.3 的
「Requirement 正文 > 500 字符」⇒ ⇒ 合并后该条正文 **469 → 580** 字符。
⇒ ⇒ **不做裁剪**：那 580 字符里的三个引导段正是本 change **K-15 / K-16 / K-17
要求逐字保留**的内容 ⇒ ⇒ 为讨好一个**未固定版本**的规则而删掉受保护的义务，
与本次的目的（**防止义务静默消失**）正好相反。⇒ ⇒ 判据改为**集合比对**：
`after - before = {chlib-stream-ready-ownership}`、`before - after = ∅`。
（其余"内容面"判据仍全部为机械可判定：C4 的 12 条 + `verify_openspec_delta.sh` 4 条
+ K-1…K-21 存活核对 ⇒ ⇒ 不依赖这个**跑在错版本上**的校验器。）
