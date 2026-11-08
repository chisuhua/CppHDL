# 审稿记录：层次化 RTL 输出缺口登记（架构台账 §7 · 新增 id 15 / 16 / 17）

- 日期：**2026-11-08**
- 角色：**architect**（只落笔 + 审查记录；**零实现改动**）
- 触发：用户口令「选择 3」⇒ 用户问「层次化 RTL 输出是否已完整支持」，审计得到三个缺口 ⇒ 用户裁定
  **只做缺口登记**（不做实现 / 不做修复）⇒ 并要求 **oracle + critic** 审查
- 范围：登记表 `.rddf/checks/arch-residual-registry.md`（**唯一真相**）新增 3 行 + 架构台账
  `docs/architecture/2026-11-02-hierarchical-rtl-output.md` §7 新增 3 行（**派生视图**）+ §7 的 2026-11-08 注记
- 配套工件（**本次不改**）：护栏 `scripts/verify_arch_ledger_residuals.sh` · 分析器
  `scripts/lib/ledger_residual.py`（**builder 域**）

> ★ **本记录的角色**：`scripts/verify_arch_ledger_residuals.sh --check-normalization --review <本文件>`
  的**登记载体** ⇒ ⇒ ⇒ **本次被删散文段 = 0 个**（护栏核对：`被删散文段 0 个`）⇒ ⇒ ⇒ 故本文件无「逐条登记旧散文」之义务。

---

## 1. 意图（为什么做这次登记）

层次化 RTL 输出的只读审计 + 实测探针得到**三个缺口**。用户裁定**只登记、不实现** ⇒ ⇒ ⇒
目标是让这三个缺口**在架构台账里可见且可被 CI 核对** ⇒ ⇒ ⇒ 而本仓的残留状态机制是
「**登记表 = 唯一真相** · **台账 §7 = 派生**」⇒ ⇒ ⇒ 缺口 MUST 落进登记表（否则护栏的双向 1:1
核对会判漂移）⇒ ⇒ ⇒ 台账 §7 的状态格 MUST 以登记表**确定性生成**的规范句开头。

⇒ ⇒ **本次改动 = 2 个 markdown 文件 · 8 行新增 · 零代码 / 零测试 / 零脚本改动。**

---

## 2. 登记的三行（逐行）

| id | 状态 · 归属 | 缺口 | 复核条件 |
|---|---|---|---|
| 15 | `已知` · builder | **`PortPolicy` 三档在可合成形态上语义未分化** —— `port_policy` 全仓唯一读取点在 `src/hierarchy_verilog.cpp:784`（fail-closed 分支）⇒ 成功合成路径从不读策略 ⇒ 三档行为完全一致 ⇒ **三档各自对应的 spec Scenario 全部未兑现** | 三档差异化处置 + 默认档告警义务**分别**落地后复核 |
| 16 | `未决` · architect | **父层运算直接消费子 `ch_out` 被拒，且诊断不可行动** —— 矩阵「子 → 父」行以「父侧 `assign` 取实例输出」为前提 ⇒ 父侧多一层运算即**出矩阵** ⇒ 发射器 fail-closed 拒绝 ⇒ 但**分析层 `pending_implicit_edges=0` · 诊断层 `verdict=kClean`** ⇒ **三层判断不一致** | 父侧落点语义裁定后复核 |
| 17 | `未决` · builder | **语料登记表的拒绝原因沿用已过时的归因，并被断言固化** —— `tests/hier_corpus_registry.md:31` 的「需要隐式端口合成 ⇒ 发射器不支持」已过时（B2b 五批 2026-11-05 全部落地）⇒ 真实原因是行 16 的形态 ⇒ `tests/test_rv32i_stage_dual.cpp:440` 的断言把过时归因**固化** | 行 16 的语义裁定落地后一并修订 |

★ **状态语义**：行 15 = **契约已定 · 实现未达** ⇒ `已知`（非终态的「已知限制」而是「已知的实现缺口」，
复核条件已登记重新判定时机）；行 16 / 17 = **开放问题**（父侧落点语义尚无裁决）⇒ `未决`。

---

## 3. 实测证据（★ 每条事实都有工具证据）

| 事实 | 证据 | 复现命令 |
|---|---|---|
| `port_policy` 唯一读取点 = `:784`（fail-closed 分支） | `grep -rn "port_policy\|PortPolicy" src/ include/` | 见左 |
| 三档行为完全一致（`ok=1` · `warnings=0` · stderr 零 CHWARN · 产物零合成注释 · 合成端口 `__x_0` / `__x_1`） | 探针 `/tmp/opencode/probe_policy2.cpp` | `g++-14 -std=c++23 -O0 -w -I include -I include/core -I include/ast -I include/utils -I external/inipp /tmp/opencode/probe_policy2.cpp -o /tmp/opencode/probe_policy2 -L build -lcpphdl && /tmp/opencode/probe_policy2` |
| **B2b 隐式端口合成已于 2026-11-05 全部落地**（⇒ 行 15 的根因**不是**「合成未实现」） | `openspec/changes/archive/2026-11-05-hierarchical-rtl-emission-2026-11/design.md` §7.1（`:116` 「B2b 隐式端口合成 ✅ **五批全部落地**」） | `grep -n "B2b" openspec/changes/archive/2026-11-05-hierarchical-rtl-emission-2026-11/design.md` |
| 直通基线（子 output 端口直接落父 output 端口）`ok=1` 成功 | 探针 `/tmp/opencode/probe_matrix2.cpp`（形态 A） | 同上编译方式 |
| 父层运算消费子 `ch_out` 被拒 + 诊断原文 | 探针 `probe_matrix2`（形态 B）：`child module 'top.l1' drives 'top_eq_eq' in module 'top', which is not an output port of that module` | 同上 |
| 三层判断不一致（分析层 `modules=3` · `edges=5` · `pending_implicit_edges=0` · 诊断层 `verdict=kClean` · 发射层 `ok=0`） | 探针 `/tmp/opencode/probe_layers.cpp` | 同上编译方式 |
| 仓内实例 = `examples/riscv-mini/src/rv32i_pipeline_top.h:112` 的 `bits<kTcmAddrWidth + 1, 2>(pipeline.io().instr_addr)`；`instr_addr` 是 `ch_out` | `include/cpu/pipeline/rv32i_pipeline.h:52` | `grep -n "instr_addr" include/cpu/pipeline/rv32i_pipeline.h` |
| 语料登记行 31 的「内部线网」不准（`dec.io().imm` / `ids.io().imm` 是子 output 端口） | `include/cpu/pipeline/rv32i_decoder.h:64` | `grep -n "ch_out<ch_uint<32>> imm" include/cpu/pipeline/rv32i_decoder.h` |
| 生产代码零 `CH_HIERARCHY_LAYER`（宏在 `include/component.h:36`）⇒ 当前不影响已发布产物 | `grep -rn "CH_HIERARCHY_LAYER" include/ src/ examples/ samples/`（命中仅宏定义 + `tests/`） | 见左 |
| 默认粒度 = `RespectDeclarations` | `src/component.cpp:160` | 见左 |

---

## 4. **oracle 审稿（第 1 轮 · 2026-11-08）**

- **范围**：登记的**结构与枚举选型 + 每条事实断言**（arch 档 · 只读）
- **结论**：结构与选型成立（护栏双向 1:1 PASS 已复核）⇒ ⇒ **但 3 处事实错误 + 2 处证据错位** ⇒ ⇒
  ⇒ ⇒ **全部返工**（★ 事实断言是本次登记的承重项）

| # | oracle finding | 处置 | 落点 |
|---|---|---|---|
| 1 | 行 15「隐式端口合成（B2b）尚未落地」**错误** —— B2b 五批已于 2026-11-05 全部落地（归档 design §7.1）；引用 B2a 的中间态注记属**断章取义** | ✅ **改** —— 行 15 现文改为「`port_policy` 全仓唯一读取点在 `src/hierarchy_verilog.cpp:784`（fail-closed 分支）⇒ 成功合成路径从不读策略」，并显式标注「⚠️ **不是**『隐式端口合成未落地』——B2b 五批已于 2026-11-05 全部落地」 | 登记表 id 15 + 台账行 15 |
| 2 | 行 15「三档当前一律 fail-closed」与同句实测（`ok=1` · 产物生成）**自相矛盾** | ✅ **改** —— 改为「在**可合成形态**上三档完全一致（放行且无告警）」，不再声称一律 fail-closed；并补「三档各自对应的 Scenario 全部未兑现」 | 台账行 15 |
| 3 | 行 16「诊断误归因」是**稻草人** —— 诊断原文本身成立（`top_eq_eq` 确实不是父 module 的 output 端口） | ✅ **改** —— 改为「**诊断不可行动**（不点名『父侧带运算』· 不给改法）」+「**归因易被误读**（错误串称『需要隐式端口合成 · 发射器不支持』，而 B2b 已落地）」 | 登记表 id 16 后缀 + 台账行 16 |
| 4 | 行 17「登记表原因已过时」判断错位 | ✅ **改** —— 改为「**沿用已过时的归因**」：B2b 已落地 ⇒ 「发射器不支持隐式端口合成」过时；并点名「内部线网」描述不准 | 登记表 id 17 + 台账行 17 |
| 5 | 行 16 缺「仅 `ForceLayer` 触发」限定（优先级被高估） | ✅ **改** —— 补「触发面：仅 `ForceLayer`（默认粒度 = `RespectDeclarations` · 仓内零成层声明 ⇒ 全 inline ⇒ 不触发）」 | 台账行 16 |

★ **oracle 明确排除的更差方案**（记录以免后人重走）：行 15 改登 `未决`（义务已被 ADR + spec 双重裁定，
登 `未决` 会把它伪装成开放问题）· 行 16/17 合并为一行（归属不同 · 闭口证据不同 · 会让 builder 的簿记
耦合 architect 的裁决节奏）· 不入登记表改走 `.rddf/improvements/`（登记表机制的设计用途就是「已知残留唯一真相」）·
趁行 16 未裁定先删 `tests/test_rv32i_stage_dual.cpp:440` 的钉住断言（那是**故意设计的 tripwire 对**，
删掉等于销毁探测器）· 只在本记录里写更正而不动台账（台账是长期权威且每次会话可能被注入上下文）。

---

## 5. **critic 审稿（第 1 轮 · 2026-11-08）**

- **范围**：登记的事实断言 / 状态语义 / 归属与复核条件 / 可操作性（`review_profile = arch`）
- **结论**：**REJECT** · `mergeable: no` · `scope_exceeded: false` ⇒ ⇒ **1 条 BLOCKER + 2 条 SHOULD_FIX + 3 条 NIT**

| # | 级别 | critic finding | 处置 |
|---|---|---|---|
| 1 | **BLOCKER** | 台账行 16 把「两个子输出比较」的最小形态与「真实设计 `rv32i_pipeline_top.h:112`」并列 ⇒ 按引用找不到所声称的形态，且「真实设计」夸大影响面 | ✅ **改** —— 明确区分「**最小形态**（探针构造）」与「**仓内实例** = `:112` 的 `bits<kTcmAddrWidth + 1, 2>(pipeline.io().instr_addr)`」，并给出 `instr_addr` 是 `ch_out` 的出处（`rv32i_pipeline.h:52`）；删除「真实设计」措辞 ⇒ ⇒ **行 17 的 `ImmConsistencyDut` 才是仓内唯一「两个子输出比较」实例**（行 17 已正确引用） |
| 2 | SHOULD_FIX | 行 16 未引 capability spec 的「父层消费子输出 MUST 有连接网」Scenario（`rtl-hierarchical-output/spec.md:241-246`）⇒ 裁定者可能漏掉 spec 侧既有约束 | ✅ **改** —— 行 16 末尾补该 Scenario 引用 + 明写「裁定的既有约束」 |
| 3 | SHOULD_FIX | 行 15 的复核条件把「默认档告警义务」与「三档分化」捆成一条复核线（spec `:349-353` 的「未发生合成时零注释」是独立判据） | ✅ **改** —— 复核条件改为「三档差异化处置（…）**且** 默认档 `SynthesizeWarn` 的 CHWARN + 产物注释义务**分别**落地后复核」 |
| 4 | NIT | 「矩阵行 2」标签不准（该行定语是「子侧端点**不是**子端口」；对照形态是**直通基线**） | ✅ **改** —— 改为「同一父层的**直通基线**（…· **无合成义务**）`ok=1` 成功 ⇒ 本形态 = 矩阵「子 → 父」行**未覆盖**的形态（该行以『父侧 `assign` 取实例输出』为前提）」 |
| 5 | NIT | 「默认粒度全 inline」措辞易误读为「默认 == ForceInline」（默认实为 `RespectDeclarations`，`src/component.cpp:160`） | ✅ **改** —— 改为「默认粒度 = `RespectDeclarations`（`src/component.cpp:160`）· 仓内零成层声明 ⇒ 全 inline ⇒ 不触发」 |
| 6 | NIT | §7 注记引用的审稿记录文件不存在（悬空引用） | ✅ **补** —— 本文件即该载体（§7 注记指向本文件） |

---

## 6. **critic 复审（第 2 轮 · 2026-11-08）**

- **范围**：★ **只验上一轮 BLOCKER 是否闭合 + 本轮新增改动**（禁止全量重审）
- **结论**：**APPROVE_WITH_CONDITIONS** · `mergeable: yes` · `scope_exceeded: false` · **0 BLOCKER**
- critic 逐条复核：BLOCKER-1（引用精度）**已闭合**（并独立复测 `rv32i_pipeline_top.h:112` 含
  `bits<kTcmAddrWidth + 1, 2>(pipeline.io().instr_addr)`、`rv32i_pipeline.h:52` 确为 `ch_out`）·
  SHOULD_FIX-1 **已闭合**（复测 spec `:241-246` 存在且吻合）· SHOULD_FIX-2 **已闭合**（两项义务已拆）·
  NIT-1 **已闭合** · NIT-2 **已闭合**（复测 `src/component.cpp:160`）· NIT-3 由本文件闭合
- 遗留 1 条 SHOULD_FIX（「§7 注记引用本文件」在本文件创建前为悬空引用）⇒ ⇒ **本文件创建即闭合**

---

## 7. 验证（★ 每条判据都有工具证据）

| 判据 | 命令 | 结果 |
|---|---|---|
| 登记表 ↔ 台账 §7 双向 1:1 + 逐字节前缀一致 | `bash scripts/verify_arch_ledger_residuals.sh` | **PASS（18 行）· exit 0** |
| 护栏自身判别力（11 条投毒） | `bash scripts/verify_arch_ledger_residuals.sh --self-test` | **11 投毒 0 失败 · exit 0** |
| 归一化「信息不丢」核对（被删散文段） | `bash scripts/verify_arch_ledger_residuals.sh --check-normalization --review .rddf/reviews/hierarchical-rtl-gap-registration-2026-11.md` | **被删散文段 0 个 · exit 0** |
| 零实现改动 | `git diff --stat` | **2 文件 · 8 行新增**（无 `src/**` · 无 `include/**` · 无 `tests/**` · 无 `scripts/**`） |

---

## 8. 判别力边界（★ 显式声明，勿过度承诺）

- 本次登记**不产生任何行为证据**：三个缺口的事实由**探针实测**支撑（§3），但探针在**仓外**
  `/tmp/opencode/` ⇒ ⇒ **未进仓内测试** ⇒ ⇒ 未来若有人质疑，本记录给出**复现命令**而不是可回归的断言。
- 护栏只核「登记表 ↔ 台账 §7」**一致性与生成句前缀** ⇒ ⇒ **不核散文里的事实真伪** ⇒ ⇒
  本次的事实正确性由 **oracle / critic 审稿 + 探针**保证，**不由 CI 保证**。
- 行 16 / 17 的 `未决` 状态**不表达**「已接受为设计边界」⇒ ⇒ **不得**被引用为「该形态本来就不支持」
  （恰恰相反：其落点语义**尚无裁决**）。

## 9. 范围外遗留（★ 只记录，本次**不**清理）

| 遗留 | 归属 |
|---|---|
| `tests/hier_corpus_registry.md:31` 的过时原因 + `tests/test_rv32i_stage_dual.cpp:423-429/438/440` 的过时归因断言 | **builder** · 阻塞于行 16 的裁定 ⇒ ⇒ 本次**不改**（用户裁定 = 只登记） |
| 行 15 的 spec 违反（`SynthesizeWarn` 的 CHWARN + 产物注释义务）可**独立立项**（AC 可直接抄 spec `:344-361` 的 4 个 Scenario） | **builder** · 待用户决定是否立项 |
| 「父层消费子输出 MUST 有连接网」Scenario（MUST NOT 用 `assign` 接表达式）与「父层运算读子端口」的 C++ 表达方式之间的缺口 | **architect** · 即行 16 的裁定对象 |
| **用户面文档同样承诺了「告警」**：`docs/usage_guide/05-component-design.md:699` 写 `opts.port_policy = ch::PortPolicy::SynthesizeWarn;   // 默认：隐式端口 + 告警` ⇒ ⇒ 而实测**无**告警（行 15）⇒ ⇒ 该注释属**行 15 同族**的用户面失真 | **builder** · `docs/usage_guide/**`（★ **不在** `arch-docs.json` 的 `arch_paths` 内 ⇒ ⇒ **architect 域不可写** ⇒ ⇒ 本次**只登记不改**） |
