# 审稿记录：`port-naming-contract-guard-2026-11`（残留 5 · 端口命名契约护栏）

- **日期**：2026-11-06
- **审查者**：critic（profile `build`）
- **轮次**：第 1 轮 / 2（预算上限）
- **结论**：✅ **APPROVE** · `mergeable: yes` · `scope_exceeded: false`
  ⇒ **0 BLOCKER** · **0 SHOULD_FIX** · **3 NIT**（**全部已采纳并修**）

## 1. focus 三条 · 逐条证据（critic 原文要点）

### focus 1 · 判据的「判别力基础」是否成立 ⇒ **成立**

- **判根失败时 N1 不静默跳过**：`scripts/verify_hierarchical_port_naming.sh:151-157`
  ⇒ 根为空 ⇒ 显式 `fail "N1 无法判定…"` + `fail_n1=1`。
- **多根情形** N1 只查第一个根（`head -1`）⇒ 但该产物**已被 N3 判红** ⇒ 不存在
  「跳过即绿」路径。
- **N2a 在根失败时**跳过该产物的贡献 ⇒ 全部产物判根失败 ⇒ `n_synth=0` ⇒ **N2a 自己转红**；
  部分失败 ⇒ N3/N1 已红 ⇒ ⇒ **不存在「判根失败 ⇒ 三条全绿」的组合路径**。
- **「同一处失效三绿耦合」的穷举**：提取整体返空 ⇒ 声明集空 ⇒ **N3 红**；格式漂移致
  方向字段失配 ⇒ N1/N2a 同时看不到端口 ⇒ **N2a 红**。唯一理论上的全绿耦合是
  `--modules` 与 `--module-ports` 的 **module 名解析分叉** ⇒ 但两者对同一 `modulesp`
  条目使用**逐字相同**的 `name or verilogName` 解析（`verilog_tree_extract.py:120`
  vs `:78`）⇒ **同一份代码**而非两份拷贝 ⇒ 今天**结构性地不可能分叉**。

### focus 2 · 判据面是否精确 ⇒ **精确**

- 提取作用于 **verilator 解析后的 JSON 树**，不是 Verilog 源码文本 ⇒
  **ANSI vs 非 ANSI 在到达判据前已被 verilator 归一化** ⇒ 对本判据**不构成两种形态**。
- `output reg` / `output wire` 只影响 `varType`（REG/WIRE），**`direction` 仍是 OUTPUT**。
- 体内 wire 恒为 `NONE` ⇒ 被正确排除。`INOUT` 不在 CppHDL 发射器的发射文法内。
⇒ 判据面与发射器的实际产物文法一致，**未遗漏**「根端口带合成前缀」的可发射形态。

### focus 3 · 共享工具的向后兼容 ⇒ **成立**

- `extract_ports()` 与默认三元组模式本体**逐字未改**；新增两个分支以**新 flag 门控**
  插在 `main` 前部 ⇒ 旧调用路径（flag 缺省）**不可能**落入新代码。
- `verify_hierarchical_rtl.sh` / `verify_hierarchical_tree_extract.sh` 消费的面**未动**。
- 新模式的输出契约（TSV 列序 `module\tport\tdirection\twidth` · 声明序 ·
  `--modules` 去重 · 两种 JSON 形态）在 docstring 里**成文**；护栏自身对 `--modules`
  输出再 `sort -u` ⇒ **不依赖声明序** ⇒ 稳定到可被未来护栏依赖。

## 2. NIT 三条 · 处置（**全部已修**）

| # | NIT | 处置 |
|---|---|---|
| 1 | 脚本头注释「`top_module_name` 可配」⇒ 读者会误以为脚本里有可配置项（实际根全靠结构判定；该名字是**发射器**的旋钮） | ✅ **已修**：改为「根名**无需配置**（结构判定使『配置根名』不必要 ⇒ 且**不硬编码** `top` / 任何文件名 —— 发射器的根名旋钮是 `top_module_name`，本脚本**不依赖**它）」 |
| 2 | `collect_data` 内三条 `python3 …` 管道**未检查退出码** ⇒ 运行时异常会退化成「空输入」⇒ 失败分类从 N0（退出码 **2**）漂到「判据失败」（退出码 **1**） | ✅ **已修**：三条提取**各自检查退出码** ⇒ 失败即 `fail "N0 提取失败：…"` + 返回非 0 ⇒ 失败分类恢复精确（仍 fail-closed：非零退出、不误绿） |
| 3 | delta Requirement 3 的「根名可配置」措辞与实现的关系是「结构判定使配置不必要」而非「提供了配置」 | ✅ **已修**：delta 正文改为「结构判定使『配置根名』**不必要** ⇒ ⇒ 本契约 **MUST NOT** 要求提供根名配置项」；Scenario 的判据（改名后仍判对）**不变**（已由 `rename_root` 投毒验证） |

⇒ ⇒ 修后**复跑**：护栏主路径 **PASS** · `--self-test` **PASS**（6 条投毒仍全符合期望）·
`openspec validate --type change --strict` **valid**（4 条 Requirement 正文
223 / 238 / 247 / 210 字符 ⇒ 全 ≤ 500）。

## 3. critic 的「验证建议」· 处置

| 项 | 处置 |
|---|---|
| 复核 focus 2 的推断（`output reg` 仍 `direction=OUTPUT`）：可选地手工把一条投毒改成 `output reg [7:0] __if_9` 跑 `--self-test`，确认 N1 仍转红 | ⚠️ **未做**（critic 标注为「推断、未验证」）⇒ ⇒ 登记为**已知未验证项**（见下） |
| 归档时按 verifier 报告 §3 的 3 条「待归档后」动作逐条执行并留证 | ✅ 见归档步骤 |

## 4. ⚠️ 登记：critic 指出的**未验证推断**（不掩盖）

**focus 2 的「`output reg` / `output wire` 的 `direction` 仍是 OUTPUT」是 critic 基于
verilator tree-JSON 语义的推断，本轮**未实测**。**

⚠️ **为什么本仓当前不需要它**：CppHDL 的发射器**只发 `input` / `output`**（无
`output reg` / `output wire` 形态）⇒ ⇒ 该形态在**本仓产物里不可达** ⇒ ⇒ 不是
判别力缺口。⇒ ⇒ 但若将来发射器支持 `output reg`，**必须**补一条投毒用例。

⇒ ⇒ 记为 **backlog**（不是本 change 的缺口）。

## 5. 未纳入本轮审稿的相邻漂移（只登记不修）

- 主 spec `rtl-hierarchical-output` 的 Purpose 仍写「R1–R7」而脚本已是 R1–R8
- `scripts/verify_hierarchical_rtl.sh` 的 R8 硬编码 `$1=="top"`

⇒ ⇒ 与本 change 无关 ⇒ ⇒ 按「改动最小化」**不在本 change 修**。
