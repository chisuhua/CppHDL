# 提案：把**已过时**的「未收口」陈述从 capability 主 spec 里收口（规范文本订正）

- **规模 / 风险**：**小**（零 `src/**` / `include/**` / `tests/**` 改动 ⇒ 零回归风险；
  另含 **1 处护栏脚本头注释** + **2 处台账 / ADR 状态订正** ⇒ 均为**文档面**）
- **规格影响**：`chlib-stream-ready-ownership` **MODIFIED ×2** +
  `rv32i-isa-semantics` **MODIFIED ×1** ⇒ 合计 **MODIFIED ×3**
  ⇒ 归档**禁** `--skip-specs`
- **关联台账**：`.rddf/improvements/stream-forward-ready-closure-2026-10.md`
  （P1-b / P1-c 已收口）· `.rddf/improvements/stream-builder-chain-silent-word-loss-2026-11.md`
- **触发**：2026-11-04 的遗留清扫（扫「已落地但文档仍写未落地」的漂移）

## Why

**主 spec 是「当前态」的真相源** ⇒ ⇒ 而它内部现在**自相矛盾**：

| 位置 | 陈述 | 事实 |
|---|---|---|
| `chlib-stream-ready-ownership/spec.md` Requirement「组合子 MUST NOT **无条件**忽略消费端的 ready」(:86) 的 Scenario「转发类 MUST NOT 被读作「已合规」」 | `demux` / `fork` 「**仍是**两端都断」「**仍**未收口」「**仍属违规**」 | **2026-11-02 已收口**（ADR-056 / ADR-057 + 归档 change `2026-11-02-stream-demux-fork-ready-semantics-2026-11`） |
| 同 capability 的 Requirement「分流 / 分叉类组合子的 ready 语义 MUST 是「消费端 ⇒ 上游」」(:460) | 用 **5 个 Scenario** 规定 `demux` / `fork` 的**确切**行为 | 同上 |

⇒ ⇒ **同一份 spec 里，一条 Requirement 说它们「仍属违规」、另一条 Requirement 说
它们「必须这样实现」** ⇒ ⇒ 实现者按前者会认为 `demux` / `fork` 违规并去"修"，
按后者又必须保留现状 ⇒ ⇒ **规范文本本身成了 bug 源**。

⚠️ 这不是文字洁癖：本仓已多次因「文档落后于实现」被咬（`AGENTS.md` 登记的
「名字 over-promise」三次、`fwft_fifo` / `stream_arbiter_sequence` / ADR-052/053
的 Scenario 订正）⇒ ⇒ **纸面契约与实现漂移 = 本仓的头号复发病症**。

⚠️ **同一 capability 内还有一处「自相矛盾」（2026-11-04 全文件扫描抓到 · metis 审查未覆盖）**：
Requirement「转发类组合子的 ready 所有权 MUST 是**默认值 + 可覆盖**」里
Scenario「转发类现状是死路」要求「MUST 承认其上游 ready **恒为 0**」⇒ ⇒ 而
**同一 Requirement 内**的 Scenario「未覆盖时 MUST 保持『恒就绪』」要求
「上游 ready MUST 为 **1**」⇒ ⇒ **同一份 spec 的两条 Scenario 对同一个情形
给出相反值** ⇒ ⇒ 且「`out_ready = true` 这类局部常量充当 ready 源」的禁令
按字面会把**已落地的形态 E**（默认驱动就在输出侧）判为违规 ⇒ ⇒ ⇒ 本 change
把它一并订正（**MODIFIED ×2**），否则「修了主 spec 的矛盾却留着同一文件里的
矛盾」＝ 半截。

⚠️ **同类漂移还有两处**（一并订正，否则**本次订正会留下新的漂移**）：

1. `rv32i-isa-semantics` 的 Requirement「`DataTCM.strbe` MUST 被其实例化点显式驱动」
   的**范围限定 NOTE** 仍写「其核验需要一个『必需端口注册表』⇒ **独立立项**」
   ⇒ ⇒ 而该登记表**已于 2026-11-03 落地**；
2. **护栏自己的头注释**（`scripts/verify_stream_ready_ownership.sh`）仍写
   「⚠️ **仍未收口** ⇒ ⇒ `stream_fork`（P1-c）· `stream_demux`（P1-b）⇒ ⇒
   **显式豁免 + `::notice` 点名**」⇒ ⇒ 而 in-force spec 即将说它们**已收口**
   ⇒ ⇒ 归档后就是**一处新的漂移**（spec 与它引用的护栏互相矛盾）。

## What Changes

| 文件 | 改动 |
|---|---|
| `openspec/specs/chlib-stream-ready-ownership/spec.md`（**MODIFIED ×1**）| Requirement「组合子 MUST NOT **无条件**忽略消费端的 ready」的**正文与 Scenario 内容**定点订正：把 `demux` / `fork` 的「未收口」陈述改成「**2026-11-02 已收口**」+ 指向 ADR-056/057 + 归档 change + 护栏 **R13**；并订正**ADR 归属**（转发类收口的依据 = **ADR-054 §3.2 第 2 条**，**不是** ADR-055 ⇒ ⇒ ADR-055 = builder 链内中间级，全文 0 处提及转发类）；把「未收口者集合」明确为**空**；**保留全部**面向实现者的规范性义务（「默认恒就绪的豁免 MUST NOT 扩展到转发类」「收口落地时 MUST 经 delta 加回 + 补行为判据与护栏」「硬编码常量 ready 仍属违规」）⇒ ⇒ **Scenario 标题 MUST NOT 改**（CLI 按标题匹配）⇒ ⇒ **不新增**前瞻性 MUST（那属另一个 change）|
| `openspec/specs/chlib-stream-ready-ownership/spec.md`（**MODIFIED ×2** · 其一）| Requirement「转发类组合子的 ready 所有权 MUST 是**默认值 + 可覆盖**」：**正文 + 2 个 Scenario** 的当前态陈述订正 —— ①「转发类现状 = 上游恒不 ready = 死路」⇒ ⇒ 改为「**收口前**的现状」（收口后未覆盖时上游 ready = **1** ⇒ ⇒ 与**同一 Requirement 内**的 Scenario「未覆盖时 MUST 保持『恒就绪』」**直接矛盾**）② Scenario「转发类现状是死路」的「MUST 承认其上游 ready **恒为 0**」⇒ ⇒ 改为「**收口前**」+ 指向现行值 ③ ④ 两处「`out_ready = true` 这类局部常量充当 ready 源」⇒ ⇒ 限定为**上游侧**（输入侧）⇒ ⇒ 否则照字面读 ⇒ ⇒ **规范禁止的正是已落地的形态 E**（形态 E 的默认驱动就在输出侧：stream 头文件内 7 处 ⇒ ⇒ 见 **ADR-054 §3.2**；⚠️ 此处**不引行号** —— 行号会漂移，正是本 change 要治的病）⇒ ⇒ 判据与护栏 **R8** 的「输出侧 / 输入侧」角色分类**同口径**；⇒ ⇒ **Scenario 标题 MUST NOT 改** |
| `openspec/specs/rv32i-isa-semantics/spec.md`（**MODIFIED ×1**）| Requirement「`DataTCM.strbe` MUST 被其实例化点显式驱动」的**范围限定 NOTE** 状态订正：注册表现已落地（给出两个产物路径）；**并如实保留边界**（登记表**不是全仓普查** ⇒ 未登记类别仍无护栏 ⇒ 义务仍归人工 + code review）；**并修正同段的实例化点计数（3 → 4）**|
| `openspec/specs/chlib-stream-ready-ownership/spec.md` 的 **`## Purpose`**（**归档后手改**）| ⚠️ archive **不合并** `## Purpose` ⇒ ⇒ 必须手写订正**四处替换 + 一处追加**（精确文本见 `design.md` §4.1–§4.5）：① `❌ 仍未收口：demux / fork` ⇒ ⇒ `✅ 已收口`（ADR-056/057）②「builder 上游反压从未建立」这条**独立缺陷** ⇒ ⇒ 已由 **ADR-055** 修复（**并如实保留其判据缺口**）③ 结尾「转发类 MUST NOT 被声称『全部已兑现』」⇒ ⇒ 现在**确实全部已兑现** ⇒ ⇒ 改写为「全部已兑现 ⇒ 但『硬编码常量 ready 仍属违规』对**所有**转发类成立」④ 「✅ 已收口（形态 E · 单阶段）」清单里**含已死组件** `stream_arbiter_sequence` ⇒ ⇒ 订正（否则**本次订正当场制造一处新矛盾**）⑤ 「**为什么需要**」段整段是**收口前**的现状（含「在当前 API 下无法演示」「现状**违反**已签批 ADR」）⇒ ⇒ 原文逐字保留 + 首尾各加一条 dated 订正 ⇒ ⇒ **不改任何历史叙述**，只标明它记录的是**收口前**|
| `scripts/verify_stream_ready_ownership.sh` | **头注释**的状态订正（「仍未收口 / 显式豁免」⇒ ⇒ 「2026-11-02 已收口 ⇒ ⇒ 在 R13 覆盖范围内」）⇒ ⇒ **零逻辑改动** |
| `.rddf/improvements/stream-forward-ready-closure-2026-10.md` | 「穷举全部入口」验收项的**状态订正**（清单含已死的 `sequence` + 「`demux`/`fork` 显式豁免」的过时陈述）⇒ ⇒ **不**改其达成判定 |
| `docs/adr/ADR-054-stream-ready-ownership-form.md` | §3.2 的「本条对 `demux` / `fork` 尚未生效」补一条**状态订正**（只订正状态，**不改** ADR 的决策内容）⇒ ⇒ 消除与 §4（已于 2026-11-04 订正）的**内部矛盾** |

⚠️ **本 change 零行为改动** ⇒ ⇒ 只订正**规范文本的当前态陈述** ⇒ ⇒
**不新增义务、不删义务、不改任何 Scenario 标题**。

⚠️ **「不新增义务」的精确口径**（critic 第 1 轮 SHOULD_FIX #7）：A-1 与 A-4 把
**有到期条件**的义务（「直至它们落地」）改写为**永久**条件式（「将来新增的转发类
MUST 同样受约束」）⇒ ⇒ 严格说这是**适用范围扩大**（**不是**新增一条 MUST，
而是让既有 MUST 不再随该次落地而失效）⇒ ⇒ **义务强度不变、义务条数不变**
⇒ ⇒ 且 A-6 的改写**不新增超出 B-4 的义务**（B-4 本就无条件禁止硬编码常量
ready）⇒ ⇒ 唯一**新增**的是「判别力缺口」的**如实登记**（非规范性）。

## Non-Goals

- **不**改任何组件行为 / 护栏逻辑 / 测试（⇒ 零 `src/**`/`include/**`/`tests/**` 改动）；
- **不**给 capability 加新 Requirement 或新 Scenario；
- **不**新增前瞻性 MUST（例如「转发类 MUST 由自动化护栏覆盖」）⇒ ⇒ 它的覆盖面
  与 R8 扫描面**不等**（R8 只扫 `stream.h` + `stream_arbiter.h` 内带 `ch_stream<`
  形参的函数 ⇒ 放在别处的新组合子它看不见）⇒ ⇒ 无限定形式 = **不可判定**
  ⇒ ⇒ 属**另一个 change**；
- **不**把「硬编码常量 ready 仍属违规」**加强**成「一律违规（无例外集合）」
  ⇒ ⇒ 加强后的义务在现有护栏下**判不了**（R8 只判 `operator=` / 无驱动；
  R13 只判 helper 用 `<<=` ⇒ 把上游驱动换成常量 ⇒ 两护栏全绿 ⇒ ⇒ 唯一能抓
  「值不跟随」的行为判据 R1 **不覆盖** `demux`/`fork`）⇒ ⇒ 加强 = 不可执行的 MUST；
- **不**处理 `.rddf/improvements/stream-builder-chain-silent-word-loss-2026-11.md`
  里「builder 链中 5 个过滤类零行为用例」那条**判据缺口**（⇒ 需先定 `haltWhen`
  期间「上游 ready=1 而 valid=0」是丢弃还是保持 ⇒ 属 planner/architect 面）
  ⇒ ⇒ 但本次**在 Purpose 里如实保留**该缺口的登记（缺陷已修 ≠ 判据已补）；
- **不**处理 `AxiDma.rst_n` 的零行为覆盖（⇒ 独立 change）；
- ⚠️ **有意保留、不处理**（防下一轮清扫重复处理）：
  ① `.rddf/checks/architect-decisions-pending-2026-11.md` 的历史段落
     （该文件头部已声明「保留作决策史」）；②
  `.rddf/improvements/input-port-required-registry-2026-10.md` 的历史引文；
  ③ `.rddf/improvements/chlib-naming-honesty-audit-2026-11.md` 里
  「demux 的 ready **方向**问题已另立 P1-b」的低优先历史表述；
- **不**追其它 capability 的同类漂移（⇒ 若存在，单独盘点后立项）。

## Capabilities（契约影响）

- **MODIFIED** `chlib-stream-ready-ownership`（**2** 条 Requirement 的当前态陈述
  —— Requirement A「组合子 MUST NOT **无条件**忽略消费端的 ready」+
  Requirement B「转发类组合子的 ready 所有权 MUST 是**默认值 + 可覆盖**」）
- **MODIFIED** `rv32i-isa-semantics`（1 条 Requirement 的范围限定 NOTE）
- ⇒ ⇒ **无新增 capability** ⇒ ⇒ 归档**禁** `--skip-specs**

## Impact（影响面）

- **爆炸半径**：**零**（纯规范文本 + 文档面订正）⇒ ⇒ 计数锚点不变
  （`CTEST_TOTAL` / `VERILATOR_E2E_TOTAL` / `DUAL_BACKEND_PARITY_*` /
  `EXAMPLES_TEST_SOURCES`）
- **风险**（按真实严重度排序）：
  1. 🔴 **delta 漏抄一段 ⇒ 归档后整条 Requirement 被替换 ⇒ 静默丢内容**
     ⇒ ⇒ `verify_openspec_delta.sh` **不查**这一项 ⇒ ⇒ MUST 靠 `design.md` §7 的
     **保留项逐段清单**（K-1 … K-21）人工逐段核；
  2. 🟠 **Requirement 标题不逐字一致** ⇒ CLI **静默不合并** ⇒ ⇒ R3 机械拦住；
  3. 🟠 **Scenario 标题被改** ⇒ CLI **拒绝归档** ⇒ ⇒ 归档前做标题**集合**比对
     （`verify_openspec_delta.sh` **不查**）；
  4. 🟡 ** Purpose 手改漏改 / 只改三处** ⇒ ⇒ 拆成两个 commit（C3a / C3b）+ grep 核对；
  5. 🟢 零回归（无代码改动）
- **回滚路径**：`git revert` 归档 commit

## 验收标准

- [ ] `openspec validate spec-stale-closure-text-correction-2026-11 --type change --strict` = valid
      （⚠️ 用仓内钉住的 **CLI 1.4.1**；本机实测 **1.14.3** ⇒ ⇒ 1.14.x 新增
      「Requirement 正文 > 500 字符」「Purpose 仍是占位符」两条 strict 规则
      ⇒ ⇒ **不得**用 1.14.x 的结论替代 —— 根 `AGENTS.md` 已登记这条）
- [ ] `scripts/verify_openspec_delta.sh spec-stale-closure-text-correction-2026-11` 4 条全绿
- [ ] **保留项逐段核对**：`design.md` §7 的 **K-1 … K-21** 全部在 delta 中原样出现
      （⇒ 合并 = 整条替换 ⇒ 漏一条即静默丢内容 ⇒ ⇒ 这是**头号风险**）
- [ ] **Scenario 标题集合**比对（归档**前**）：两个 capability 的
      `grep '^#### Scenario:'` 主 spec vs delta ⇒ **集合相等**（数量不可增删）
- [ ] 归档后 `awk '/^## Requirements/,0' openspec/specs/chlib-stream-ready-ownership/spec.md | grep -c "直至它们落地\|⚠️ \*\*仍\*\*未收口者\|❌ \*\*仍未收口\*\*"`
      ⇒ **0 命中**（**未决语气**残留 ⇒ ⇒ ⚠️ 面**收窄**到未决语气子集
      ⇒ ⇒ 「两端都断」「未收口者」作为 **dated / 历史语境**引用是**合法的**
      ⇒ ⇒ 全量判 0 会**逼实施方删掉历史说明** ⇒ ⇒ **倒退**
      ⇒ ⇒ ⚠️ **必须限定 Requirements 段**（`## Purpose` 的对应陈述由
      **C3b 手改**处理 ⇒ ⇒ 手改前该处**仍会命中** ⇒ ⇒ 全文件判 0 = **必红**）
- [ ] 归档后 `grep -c "仍属违规" openspec/specs/chlib-stream-ready-ownership/spec.md`
      ⇒ **>= 1**（⚠️ **不是** `== 1`：实测（模拟合并 + 模拟手改 Purpose）命中 =
      **2**（K-4 正文 1 + **Purpose 新文本自带 1**）⇒ ⇒ `== 1` 会**必红**
      ⇒ ⇒ 且会诱使删掉其中一处来凑数
      ⇒ ⇒ **正是本 change 要防的义务静默消失**
      ⇒ ⇒ 「K-4 没被漏抄」的**承重判据**是下面那条 `grep -F`）
- [ ] 归档后 `grep -F "转发类**（mux / demux / arbiter）" openspec/specs/chlib-stream-ready-ownership/spec.md`
      ⇒ **命中**（K-4 逐字存在 ⇒ ⇒ 义务没被漏抄）
- [ ] 归档后 `awk '/^## Requirements/,0' openspec/specs/chlib-stream-ready-ownership/spec.md | grep -c "stream_arbiter_sequence"`
      ⇒ **== 0**（已死组件不得回写**契约面** ⇒ ⇒ ⚠️ **Purpose 段允许**点名它
      ⇒ ⇒ §4.4 的新文本**故意**写「已物理删除」作 dated 说明）
- [ ] 归档后 `grep -n "3 个实例化点" openspec/specs/rv32i-isa-semantics/spec.md`
      ⇒ **0 命中**
- [ ] 归档后 `grep -rnE "ch_module<DataTCM|ch_device<DataTCM|^using [A-Za-z_]+ = riscv::DataTCM<" include/ src/ examples/ tests/`
      ⇒ **4 处**（⚠️ 原判据的模式（以 `riscv::DataTCM<` 收尾）实测命中 **7** 处
      —— 额外 3 处是 `tests/test_rv32i_timing_dual.cpp` 里的**注释**
      ⇒ ⇒ 裸 `grep "DataTCM<"` 更是命中 **12** 处 ⇒ ⇒ ⇒ 二者**均不可复现**
      ⇒ ⇒ ⇒ 判据必须写**实例化形态**：3 处 `ch_module` / `ch_device` **构造**
      + 1 处 `using` **类型别名**（`tests/test_rv32i_timing_dual.cpp:92`
      ⇒ ⇒ 该别名被当 DUT 构造 ⇒ ⇒ 计入）⇒ ⇒ 与前 3 处合计 **4**）
- [ ] 归档后 `grep -n "demux\|fork" openspec/specs/chlib-stream-ready-ownership/spec.md`
      ⇒ **逐条人工过一遍**（每处 MUST 是「已收口 / 现行契约」语境）
- [ ] 归档后 `grep -n "out_ready = true" openspec/specs/chlib-stream-ready-ownership/spec.md`
      ⇒ **0 命中**（侧歧义已消除 ⇒ ⇒ 否则规范按字面禁止**已落地的形态 E**）
- [ ] 归档后 `grep -n "转发类现状 = 「上游恒不 ready = 死路」" openspec/specs/chlib-stream-ready-ownership/spec.md`
      ⇒ **0 命中**（该陈述已标注为**收口前**）
- [ ] `## Purpose` 的**五处**已手改（`design.md` §4.1–§4.5 精确文本）⇒ ⇒ 与 archive
      **分两个 commit**（C3a / C3b）⇒ ⇒ 避免「漏改则除一条 grep 外全绿」
- [ ] 归档**前**：`design.md` §4.1–§4.5 的五个「旧」块**仍逐字存在**于主 spec
      （`grep -F`）⇒ ⇒ 否则**先重算**替换文本
- [ ] 行号引用漂移已处理：`.rddf/checks/architect-decisions-pending-2026-11.md:26`
      与 `.rddf/checks/next-execution-review-brief-2026-11.md:19` 引用主 spec 的
      **行号** ⇒ ⇒ 归档后**位移行号** ⇒ ⇒ 改为**标题引用**（本仓已有此惯例）
- [ ] 护栏全绿：`scripts/verify_openspec_delta.sh`（全部 active change）·
      `scripts/verify_stream_ready_ownership.sh`（+ `--self-test`）·
      `scripts/verify_chlib_naming_honesty.sh`（+ `--self-test`）·
      `scripts/verify_input_port_required_registry.sh`（+ `--self-test`）
      ⇒ ⇒ 全部 exit 0
- [ ] **零代码改动 ⇒ 无需跑 `ctest`**（计数锚点不变 ⇒ 护栏 + delta 校验即足够）
- [ ] 归档：**未**用 `--skip-specs`（两个 MODIFIED 已合并进主 spec）
