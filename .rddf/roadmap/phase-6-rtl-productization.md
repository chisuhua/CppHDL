# Phase 6 · RTL 生成能力产品化 —— 实施批次表（planner 域 · 2026-11-10）

> **触发**：用户 2026-11-10 拍板「生成器可以作为项目提供的能力」等 7 项 RTL 侧诉求
> ⇒ ⇒ 拆成 7 份 improvement 草案（commit `1dc1d98`）⇒ ⇒ 三道审查（metis 1 轮 → oracle →
> critic 2 轮）⇒ ⇒ 用户裁决 B1–B4 / G3 / ⑨。
> **上游**：
> - 7 份草案：`.rddf/improvements/{rtl-emit-product-tooling, port-naming-ratchet,
>   firmware-load-chain, memory-timing-contract, rv32i-dead-component-consolidation,
>   rv32i-isa-gap-closure, examples-rtl-sim-coverage}-2026-11.md`
> - 索引：`improvement-suggestions.md`
> - 任务状态唯一真相仍 = `.rddf/roadmap/phases/phase-6.md`（**本文件不复制状态格**，
>   只承载「**跨 7 份草案的批次编排**」）
> **本文件不新增任何架构义务** —— 判据全部映射各草案已有的 work item 与 AC。
> ⚠️ **不是 feature fragment** ⇒ ⇒ **不进** 根 `AGENTS.md` 的 AUTO 块 ⇒ ⇒
> `scripts/verify_roadmap_fragments.sh` 的 C1 **不受影响**（护栏实测绿，见 §5）。

---

## 0. ⚠️ 本表是**重建**（如实登记）

原「B0–B4 批次表」**只存在于评审对话**，**未落进任何工件**（critic 第 1/2 轮已指出：
`grep B0` 只命中 critic 段落 ⇒ ⇒ 表本身**不可 grep** ⇒ ⇒ **不可评审、不可复现**）。
⇒ ⇒ **本表按磁盘上可验证的约束重建**，**不是**原表的逐字还原。

**重建依据**（每条都可 grep / 可复现）：

| # | 依据 | 出处 |
|---|---|---|
| ① | 「若先在 B0 改语义、后面才改实现 ⇒ 中间态出现**声称成立但不成立**」⇒ ⇒ **T1 与 T3 必须同批**；且 **⑤ `data_ready` 语义重定义从 B0 移进该批** | `memory-timing-contract-2026-11.md:60`（T3 行）+ `:267`（critic SHOULD_FIX ⑤） |
| ② | 「C5 MUST 与 C6 同批」⇒ ⇒ `hazard_detection_unit.h:19` include `rv32i_forwarding.h` ⇒ ⇒ 单删**必红** | `rv32i-dead-component-consolidation-2026-11.md:71`（C5 行 · B3 修复） |
| ③ | 「批内顺序 MUST 为 **③ 尾部填 NOP → G1 + G4（同批）→ 其余**」⇒ ⇒ 反序会让 golden **改两次** | `rv32i-isa-gap-closure-2026-11.md:60`（G4 行） |
| ④ | 「本项在 `memory-timing-contract` **之后**做（否则时序金标算两遍）」 | `rv32i-dead-component-consolidation-2026-11.md:80` |
| ⑤ | 「若 ISA 项改了 ID 级 ⇒ ⇒ 建议 ISA 项在本项**之后**（避免 ID 级改两遍）」 | `rv32i-dead-component-consolidation-2026-11.md:246` |
| ⑥ | 「本项依赖本项的 manifest ⇒ ⇒ **本项先做**」 | `rtl-emit-product-tooling-2026-11.md:243`（依赖 manifest）/ `port-naming-ratchet-2026-11.md:227` |
| ⑦ | 「依赖本项的 CLI + manifest ⇒ ⇒ **本项先做**」 | `rtl-emit-product-tooling-2026-11.md:244` |
| ⑧ | 「I5 会改 `Rv32iDecoder` / `Rv32iAlu` / `HazardUnit` 的 `__io` ⇒ ⇒ 触发命名棘轮 P1 ⇒ ⇒ **必须带名** ⇒ ⇒ **建议 I5 与本项首批合并**」 | `port-naming-ratchet-2026-11.md:230` |
| ⑨ | 「与 `memory-timing-contract` 的时序耦合 ⇒ 本项**先**做 ⇒ ⇒ 时序项 MUST **在**本项之后改 ⇒ ⇒ 或两者同批但**顺序执行**」 | `firmware-load-chain-2026-11.md:208` |
| ⑩ | 「`firmware-load-chain` ⇒ ⇒ **会改 manifest 的 `semantic`** ⇒ ⇒ 需同步 manifest 快照」 | `rtl-emit-product-tooling-2026-11.md:246` |
| ⑪ | 「C6 改**共享发射器** ⇒ ⇒ 层次化产物**字节必变** ⇒ ⇒ MUST 加 `emit_hygiene` 开关（**默认 off**）」 | `rtl-emit-product-tooling-2026-11.md:66`（C6 行） |
| ⑫ | 「`mem_is_load` 的修正**原本 7 份草案无任何一条承接** ⇒ ⇒ 落点补入 **C8**」 | `rv32i-dead-component-consolidation-2026-11.md`（C8 行 · planner 复核补入 2026-11-10） |


> ✅ **2026-11-11 · critic 第 3 轮 SHOULD_FIX-1 修复**：本表的 12 条重建依据里 **6 条 file:line 错位**
> ⇒ ⇒ 已**逐条实测**（读该行的实际内容）后改正：④ `:79`→`:80` · ⑤ `:227`→`:246` · ⑥ tooling `:242`→`:243`
> · ⑥ ratchet `:224`→`:227` · ⑦ `:243`→`:244` · ⑧ `:227`→`:230` · ⑩ `:245`→`:246`
> ⇒ ⇒ ⚠️ **2026-11-11 · critic 第 4 轮 SHOULD_FIX-2 修复**：原说明写「⑥ ratchet `:224`→`:225`」⇒ ⇒ **与表内实际值不符**（表内已是 `:227` ⇒ ⇒ `:225` 是该文件的表头行「\| 项 \| 关系 \|」）⇒ ⇒ ⇒ 已改正
> ⇒ ⇒ ⚠️ **教训**：`file:line` 指针**会随文件编辑漂移** ⇒ ⇒ ⇒ 本表每次编辑后 MUST **重跑全量实测**（⇒ ⇒ 建议长期改为「章节名 + 锚点」，见 critic 第 4 轮 scope_exceeded ④）
> ⇒ ⇒ **修正后的每一条都已实测其内容确为所引的那句话**（不是「行号看起来对」）⇒ ⇒ ⇒ 表格现在**可复现**。

---

## 1. 批次表（B0 → B4）

> **选取准则**（按批次递进）：
> **B0** = **零交付风险**（只动「无流水线消费者的组件」或「纯文档 / 纯登记」）
> ⇒ ⇒ ⚠️ **2026-11-11 · critic 第 3 轮 SHOULD_FIX-6①**：**F-A（`node-name-prefix-consistency` P1）是本准则的唯一例外** ⇒ ⇒ ⇒ 它改 `include/component.h:274` 的 `Component::name()` **语义** ⇒ ⇒ **仓内零影响 + 下游可见行为变更** ⇒ ⇒ ⇒ ⇒ ⇒ 故 MUST **走公开面登记**（`docs/CHANGELOG.md` + `.rddf/checks/public-api-surface.md` 的「行为变更（非符号移除）」节）⇒ ⇒ ⇒ ⇒ ⇒ 该登记**不因「一行改动」而豁免** ⇒ ⇒ ⇒ 「零交付风险」的准确表述 = 「**除 F-A 外**，B0 的其余 work item 零交付风险」
> ⇒ **B1** = **地基**（manifest / CLI / 护栏 —— 下游全部依赖它）
> ⇒ **B2** = **消费侧**（示例覆盖）
> ⇒ **B3** = **时序契约**（动流水线时序 ⇒ ⇒ golden 可能变 ⇒ ⇒ 排在 B0 之后）
> ⇒ **B4** = **核心重构**（组件接入 —— 必须在 B3 之后，否则时序金标算两遍）

### B0 —— 零交付风险批

| work item | 来源 | 内容 | 交付风险 |
|---|---|---|---|
| **⑥ 关键字表补全** | `port-naming-ratchet` N3/N1 | `sanitize_verilog_name` 的关键字表补全 ⇒ ⇒ **判据 MUST 用单元判据**（`sanitize_verilog_name("bit") == "bit_r"` 等 4 条）⇒ ⇒ **不能靠产物 diff**（现网产物节点名恒带 owner 路径前缀 ⇒ `top_time` ≠ 裸关键字） | **零**（纯函数） |
| **⑧ SLTU 修缺陷** | `rv32i-dead-component-consolidation` AC① | `rv32i_alu.h:93` `lt_unsigned_sig = bits<31,31>(sub_result)` ⇒ ⇒ `0 < 0xFFFFFFFF` 得 0（期望 1）⇒ ⇒ MUST 加 ctest `execute_alu(0, 0xFFFFFFFF, SLTU) == 1` | **零**（`Rv32iAlu` **零流水线消费者** ⇒ 交付路径不经过它） |
| **C5 + C6 同批退役** | `rv32i-dead-component-consolidation` C5/C6 | 删 `rv32i_forwarding.h` + `hazard_detection_unit.h` + `rv32i_hazard_complete.h` ⇒ ⇒ ⚠️ **必须同批**（`hazard_detection_unit.h:19` include forwarding ⇒ 单删必红）⇒ ⇒ 走 `public-api-remove-visibility-2026-11` 的移除登记 | **零**（三者**零流水线消费者** ⇒ 仅测试消费） |
| **C7 交接登记** | `rv32i-dead-component-consolidation` C7 | `include/cpu/AGENTS.md`「WHERE TO LOOK」对每个**保留但不接入**的组件明写「⚠️ 非流水线路径」；分支预测器明写「未接入 ⇒ 每个 taken 分支付 3 拍冲刷」 | **零**（纯文档） |
| **C8 `mem_is_load` tie-off** | `rv32i-dead-component-consolidation` C8（**planner 复核补入**） | 装配点 `hazard.io().mem_is_load <<= ch_bool(false);` ⇒ ⇒ 顶层 input **8 → 7** ⇒ ⇒ ⚠️ **契约相容**（契约说的是「**无 driver 的** input」⇒ ⇒ 补 driver = **修缺陷**，不是改契约）⇒ ⇒ 行为零变化（两后端实测均读 0） | **低**（动流水线装配，但 golden 零变化 ⇒ 有 20 项 golden + 双后端 parity 双重看守） |
| **F-A 前缀口径对齐** | `node-name-prefix-consistency` P1 | `include/component.h:274` 删掉 `\|\| parent_path == "unnamed"` ⇒ ⇒ 与 `src/core/lnodeimpl.cpp:30-35` **同口径** ⇒ ⇒ **一处一行**<br>⚠️ **⚠️ metis 订正了它的定级**：原稿按 oracle 的「B0 零 churn」归入 B0 ⇒ ⇒ **实测后修正** = 「**仓内零影响 + 下游可见行为变更**」（`Component` 的公开默认实参 `name = ""` · `include/component.h:61` ⇒ ⇒ 下游 `MyDut dut;` = 无名根 ⇒ ⇒ 其子组件 `name()` 从 `leaf` 变 `unnamed.leaf`）⇒ ⇒ ⇒ **必须**走 CHANGELOG + `public-api-surface.md`「行为变更（非符号移除）」节登记 ⇒ ⇒ ⇒ 故**仍属 B0**（一批内最小）但**不是零风险** | **低-中**（仓内零影响 · 下游可见）⇒ ⇒ 需 P4 登记 |

> ⚠️ **B0 内不得出现**：`data_ready` 语义重定义（⑤）⇒ ⇒ **移进 B3**（依据 ①）。

### B1 —— 地基批（manifest / CLI / 产物护栏 / 命名地基）

| work item | 来源 | 内容 | 依赖 |
|---|---|---|---|
| **C4 manifest schema** | `rtl-emit-product-tooling` C4 | `name` / `dir` / `width` / `role` / `driven`（节点级）/ `effective`（功能级）/ `owner` / `is_top_io` / `cpp_member` / `semantic`（可选）⇒ ⇒ `rtl-emit-manifest-v1` | — | ✅ **2026-11-11 已完成**（纯规格 change `rtl-emit-manifest-schema-2026-11` **已归档** ⇒ ⇒ 新 capability `rtl-emit-portability` 已进主 spec ⇒ ⇒ `openspec validate --specs` = **45 passed / 0 failed**）|
| **C3 悬空输入诊断** | `rtl-emit-product-tooling` C3 | 复用 `diagnose_drive()` ⇒ ⇒ `owner ≠ 根` 且无 driver ⇒ ⇒ 记入 `diagnostics[]` ⇒ ⇒ **只诊断 · 不动端口面** ⇒ ⇒ 与 **C8** 互补（一个修本设计、一个兜底其它设计） | C4 |
| **C1 CLI** | `rtl-emit-product-tooling` C1 | `tools/cpphdl-emit-rtl`：**库 + 薄 CLI**（用户在自己的可执行文件里登记）⇒ ⇒ 本仓二进制 **MUST NOT** 链 `examples/**` ⇒ ⇒ 退出码 `0` / `1` / `2`（fail-closed） | C4 |
| **C5 产物护栏** | `rtl-emit-product-tooling` C5 | `scripts/verify_flat_rtl_emit.sh`：F1（剥 blanket `lint_off` 后重 lint · 两段式基线）· F2（逐字节确定性）· F3（三方比对：manifest ∥ verilog 独立提取 ∥ 手推期望集）· F4（悬空顶层输入**可被发现**，非「必须消失」）· **F5 两段（零引用信号数 == 0 · proxy 恒等直通数 == 0）** · F6（`ctest -L verilator_e2e` 全绿）· F7（`--self-test` 逐条投毒） | C1 · C4 |
| **C6 codegen 卫生** | `rtl-emit-product-tooling` C6 | `` `default_nettype none `` + 生成元数据头（时间只进 manifest）· 剪零引用信号 + proxy 恒等直通 · 复位写法规范化 · `lint_off` 改按节点白名单 ⇒ ⇒ ⚠️ **MUST 加 `emit_hygiene` 开关（默认 off）** ⇒ ⇒ 层次化产物**字节不变**这条 AC 才成立 | C5 |
| **N1 具名用法约定 + 探针** | `port-naming-ratchet` N1 | ① 用法约定（`ch_in<ch_bool> prog_we{"prog_we"};`）② **最小可运行探针（MUST · 原为「建议」· 理由：具名构造的调用点实测 = 0 ⇒ 这条路径从未被端到端验证过）** ③ 一条 ctest ④ 文档 | C4 |
| **N5 第 1 步：manifest 双字段** | `port-naming-ratchet` N5(a) | manifest 加 `name`（RTL 全名）+ `cpp_member`（C++ 成员路径）⇒ ⇒ **零产物变更 · 零 ADR 变更 · 零快照漂移** ⇒ ⇒ 工具链**立刻可用** | C4 |
| **N3 名字稳定性** | `port-naming-ratchet` N3 | 显式名路径**绕开** eval-list 序计数器 ⇒ ⇒ 零序号 ⇒ ⇒ 存量走兼容路径（否则 3103 处端口名**全体变**） | N1 |
| **N2 基线棘轮** | `port-naming-ratchet` N2 | 登记表 `.rddf/checks/port-naming-registry.md` + `scripts/verify_port_naming.sh` ⇒ ⇒ **必须先于 N4**：R1-a 的「C 类（product 侧）MUST 被登记在棘轮豁免表」依赖该表**已存在** ⇒ ⇒ ⇒ **否则规范 Requirement 会先于表落地 ⇒ ⇒ 条款当场不可满足** ⇒ ⇒ ⇒ ⚠️ **2026-11-11 · critic 第 5 轮 SHOULD_FIX-1 修复**：本行原把「先做」写进**标题**且依赖列写成 `—` ⇒ ⇒ **已改回**：标题去掉「先做」· 依赖列恢复 **`N1`**（棘轮护栏的判据「标识符 == 显式名字符串」以 N1 的用法约定为前提）⇒ ⇒ ⇒ 「先做」的限定语**仅指「先于 N4」**（写在内容列）⇒ ⇒ ⇒ | N1 |
| **N4 规范落点** | `port-naming-ratchet` N4 | 命名规则写进 `rtl-hierarchical-output`（规范所有者）⇒ ⇒ `hierarchical-port-naming-guard` **MUST NOT** 复述 ⇒ ⇒ ⚠️ **2026-11-11 · critic 第 4 轮 SHOULD_FIX-6 修复**：**Requirement 的唯一承载 = 本行（N4）** ⇒ ⇒ sister 的 P2（`port-name-literal-parity` P2）**只是本项的契约化提案** ⇒ ⇒ ⇒ ⇒ **MUST NOT 另立一个 change 承载同一条 Requirement** ⇒ ⇒ ⇒ 否则归档后主 spec 会出现**两条同名 Requirement** ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⚠️ sister 的 R1-b（`component-ownership-and-naming` 的追加）**由姊妹草案 `node-name-prefix-consistency` P3 承载** ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **一份 Requirement · 一处落点** | N2（**先做** · 见上） |
| **N6 公开面登记** | `port-naming-ratchet` N6 | 第 1 步零公开 API 变更 ⇒ 无需登记；第 2 步若成立 ⇒ MUST 走登记 + CHANGELOG + ADR 修订 | — |
| **P1 探针固化**（**并行 · 零生产面**） | `port-name-literal-parity-2026-11` P1 | 把三探针**逐字搬进** `tests/test_port_name_parity.cpp`（5 条用例 + 退化用例）⇒ ⇒ **零生产面改动** ⇒ ⇒ 产物逐字节不变 ⇒ ⇒ **不阻塞** B1 其余项 ⇒ ⇒ ⚠️ **它是那份草案的「最低交付」**（未做 ⇒ 那份草案不算完成）⇒ ⇒ ⚠️ 探针源码目前在 `/tmp/opencode/`（scratch）⇒ ⇒ **结论随会话蒸发** ⇒ ⇒ 优先做 | 无（纯测试） |

> ⚠️ **N5 第 2 步（根实例名清空验 `parent==nullptr` 分支）= 独立立项** ⇒ ⇒ **不在 B1** ⇒ ⇒
> 详见 §4 的「独立立项」清单。

### B2 —— 消费侧批（examples 的 RTL 仿真覆盖）

| work item | 来源 | 内容 | 依赖 |
|---|---|---|---|
| **E1–E8** | `examples-rtl-sim-coverage` | `--emit-tb-stub`（只发骨架）· **首批 10 个 target**（确切 target 名以 `examples/CMakeLists.txt:34-76` 为准）· **E4′ 已验证 = 1 / 待补期望源 = 9**（每项 MUST 列「缺什么」）· E5 运行护栏 `verify_examples_rtl_sim.sh`（缺失 ⇒ **失败**不跳过；TB 末尾 MUST 打印 `RESULT: checks=<N> fails=0`）· **E6 CI = 挂 `pr-verilator-fast` 的一个 step**（**不**新建 job · **不**注册 CTest · **不**加 `rtl_sim` label）· 缓存 key 含 TB 名（独立目录） | B1 的 CLI + manifest |

> ⚠️ **E6 的 CI 形态**已由用户裁决 ⇒ ⇒ 本批**不得**改成新 job（会引入第二次完整 build）。

### B3 —— 时序契约批（D-TCM 寄存读）

| work item | 来源 | 内容 | 依赖 |
|---|---|---|---|
| **T1 + T3 同批** | `memory-timing-contract` T1/T3 | D-TCM `aread` → `sread`（1 拍）⇒ ⇒ **T1 与 T3 的 `data_ready` 语义重定义同批**（依据 ① ⇒ ⇒ ⑤ 从 B0 移进本批） | — |
| **T6 store→load 同址** | `memory-timing-contract` T6（**critic 第 1 轮 BLOCKER 新增**） | 显式定义 read-during-write 语义 ⇒ ⇒ **只能选 `read-new`**（`read-old` **违反 ISA**）⇒ ⇒ 判别性用例：**紧邻** `SW x1,0(x2)` / `LW x3,0(x2)`（同址 · 零间隔）⇒ ⇒ ⚠️ 现固件 `SW`@word16 / `LW`@word20 **隔 4 字 ⇒ 零判别力** ⇒ ⇒ MUST 补紧邻形态 | T1 |
| **T2 / T4 / T5** | `memory-timing-contract` | T2（load 数据对齐）· T4（字节写相位 + 跨地址探针）· T5（`verify_dtcm_read_timing.sh`：**结构判据**进 CI；**yosys 判据降为本地/opt-in** ⇒ ⇒ 「真实 BRAM 可推断性未在 CI 验证」MUST 登记进 `.rddf/checks/arch-residual-registry.md`） | T1 |
| **L1–L6 加载链路** | `firmware-load-chain` | 编码器库化 · 地址映射（`dtcm = 0x8001_0000` · size **由 TCM 容量派生** = `1 << (kTcmAddrWidth+2)`）· 统一加载口 · ELF · **截断等价的最小判据**<br>⚠️ **2026-11-11 · critic 第 3 轮 SHOULD_FIX-6③ · 本批的公开面前置门**：B3 含**公开面变更项** ⇒ ⇒ ⇒ ① `firmware-load-chain` L1–L6（**统一加载口** ⇒ 若新增/改名组件端口 ⇒ ⇒ 走 `public-api-remove-visibility-2026-11` 机制）② `memory-timing-contract` T1/T3（`data_ready` **语义重定义** ⇒ ⇒ 下游按旧语义写的 TB 会断 ⇒ ⇒ 须登记「**行为变更（非符号移除）**」）⇒ ⇒ ⇒ ⇒ **本批开工前 MUST 先登记**（`.rddf/checks/public-api-surface.md` + `docs/CHANGELOG.md`）⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **登记未做 ⇒ 本批不得开工**（前置门 · 不是事后补记）（16 KB 对齐 ⇒ `0x80010000 % 0x4000 == 0` ✓ · `0x80005000 % 0x4000 == 4096` ✗） | ⚠️ 依据 ⑨：**本项先做** ⇒ ⇒ 或与 B3 同批但**顺序执行**（D-TCM 改 `sread` 后加载写回晚一拍） |

### B4 —— 核心重构批（RV32I 组件收口 + ISA 缺口闭合）

| work item | 来源 | 内容 | 依赖 |
|---|---|---|---|
| **C1 差分（先做）** | `rv32i-dead-component-consolidation` C1 | 复用既有 `kExCases`(**28**) / `kDecCases`(**17**) / `ImmConsistencyDut` ⇒ ⇒ **MUST NOT** 重造第二套 ⇒ ⇒ 「等价物」MUST **逐行照抄** `ex_stage.h:136-208` ⇒ ⇒ 差异清单 MUST 覆盖**修复前**的差异全集（**基准 = B0 ⑧ 之前**）且**逐项标注处置**（已修 / 待修 / 不修）—— 已知项含 **SLTU 借位**（**已在 B0 ⑧ 修** ⇒ 标「已修」）· **ADD 33 位宽度** · **`less_than_u` 与 `result` 内部矛盾** · 死输出 · `funct7` 双方都不消费 ⚠️ **2026-11-11 · critic 第 4 轮 SHOULD_FIX-1**（原措辞「MUST 至少含 SLTU」**不可满足** ⇒ ⇒ B0 已修 ⇒ ⇒ 不可再作门槛）| B3（依据 ④） |
| **C2 ALU 编码归一** | `rv32i-dead-component-consolidation` C2 | 以 `ex_stage.h::ALU_OP_*` 为唯一权威 ⇒ ⇒ `rv32i_alu.h` 改值（`ALU_SUB` 10→1 · `ALU_SRA` 11→7）⇒ ⇒ **破坏**既有手写常量 ⇒ ⇒ MUST 同步更新 + 登记 ⇒ ⇒ ⚠️ `test_pipeline_exec.cpp:124` 的**字面量 `SLTU = 0x3`** MUST 改符号常量（**今天 0x3 == `ALU_SLTU`，归一后变 XOR**） | C1 |
| **C3 / C4 组件接入** | `rv32i-dead-component-consolidation` C3/C4 | `Rv32iAlu` 接入 EX 级（算术核心）· `Rv32iDecoder` 接入 ID 级（字段 + 立即数）⇒ ⇒ ⚠️ C4 的裁决：**标志继续由 opcode 比较**（`instr_type` 对 `OP_SYSTEM` 默认 `INSTR_R` ⇒ 若用它派生 flags ⇒ 未知指令会**写 rd** ⇒ 行为变更）⇒ ⇒ **不得**让两者并存 | C2 |
| **G3 → G1 + G4（同批）→ 其余** | `rv32i-isa-gap-closure` | 批内顺序 MUST 为 **③ 尾部填 NOP → G1 + G4 → 其余**（依据 ③）⇒ ⇒ G3 采纳「装载器把尾部填成 NOP `0x00000013`」⇒ ⇒ 退休数 **178 不变** · `non_retired == 21` 不变 ⇒ ⇒ G1 未知 opcode 检测（**判据 MUST 含 `io().valid`**）· G2 冻结策略 · G5 `ExceptionUnit` `mcause` 修复（首选 B：跨两拍 · `CsrBank` 零改动） | C3/C4（依据 ⑤ ⇒ ISA 项在组件收口**之后**） |

---

## 2. 跨批次的**硬约束**（合并前的必查项）

| # | 约束 | 依据 |
|---|---|---|
| ★1 | **锚点编辑权 = 最后合入者** ⇒ ⇒ 根 `AGENTS.md` 的 4 个计数锚点（`CTEST_TOTAL` / `VERILATOR_E2E_TOTAL` / `DUAL_BACKEND_PARITY_TOTAL` / `DUAL_BACKEND_PARITY_PAIRS`）各 2 处 ⇒ ⇒ 文本必冲突 ⇒ ⇒ 合入前 MUST **全量重建并重算**（否则两条轨各自 +K ⇒ ⇒ 错的最终值） | 沿用 `.rddf/roadmap/phase-6-next-steps.md` §1 的同款纪律 |
| ★2 | **护栏清单** MUST 含 `scripts/verify_dual_backend_parity.sh` | 同上 |
| ★3 | **层次化产物逐字节不变**这条 AC **只在 `emit_hygiene` 默认 off 时成立** ⇒ ⇒ B1 的 C6 落地时 MUST 显式确认该开关默认 off | 依据 ⑪ |
| ★4 | **B0 的 C8 会改顶层端口面**（input 8 → 7）⇒ ⇒ 任何依赖「顶层 115 端口」的快照 MUST 同步 ⇒ ⇒ 且 C5 的 F3「手推期望集」MUST 重算 | 本表 §1 B0 |
| ★5 | **退役 ⇒ CTest 条目会减少** ⇒ ⇒ 「全绿」判据 MUST 写成「`CTEST_TOTAL` + 新增 − 删除 == `ctest -N`」⇒ ⇒ **禁**写死「257」⇒ ⇒ **缩面须 reviewer 显式签字** | `rv32i-dead-component-consolidation` AC「不回归」 |

---

## 3. 与 phase-6 的关系

- 本表**不复制** phase-6 的状态格 ⇒ ⇒ 状态仍以 `.rddf/roadmap/phases/phase-6.md` 为准。
- 本表覆盖的是 **phase-6 之外的**「RTL 生成能力产品化」7 份 improvement ⇒ ⇒
  **不占用** phase-6 的 T1–T8 编号 ⇒ ⇒ 两表可并存。
- ⚠️ **交叉点**：phase-6 的 **T5**（P6 侦察 + 立项：RV32I ISA 缺口）与本表 **B4**（G1–G5）
  **是同一片区域的两个视角** ⇒ ⇒ 实施前 MUST 先确认 phase-6 T5 的侦察结论是否已并入
  `rv32i-isa-gap-closure-2026-11.md` ⇒ ⇒ **不得**两条轨各改一次 ID 级。

---

## 4. 不在本批次表内的**独立立项**（须另起草案）

| 项 | 出处 | 为什么独立 |
|---|---|---|
| **N5 第 2 步**：把 `ch_device` 的根实例名清空（`include/device.h:28` 的 `"top"` → 空）⇒ ⇒ ⚠️ **2026-11-11 · critic 第 3 轮 SHOULD_FIX-2 修复**：原回退条件（「若产出的是 `.prog_addr` → `_prog_addr` ⇒ ⇒ (c) 也不成立」）**已被探针实测证伪** ⇒ ⇒ **实测 = `unnamed_prog_addr`**（**第三形态**：既非裸 `prog_addr` 也非 `_prog_addr`）⇒ ⇒ ⇒ ⇒ ⇒ **route (c) 在当前命名机制下不可实现**（清空实例名**不足以**产出裸名 ⇒ ⇒ 根因是 `"unnamed"` 哨兵被拼进节点名）⇒ ⇒ ⇒ ⇒ **独立立项的前置条件 = 引入新的命名机制**（不是清空实例名）⇒ ⇒ ⇒ ⚠️ 该项**当前不做** ⇒ ⇒ ⇒ 触发条件 = 用户在看到 manifest 产物后仍要求人眼可读（与台账行 21 一致）⇒ ⇒ ⇒ 详见 `port-name-literal-parity` A.1①② | `port-naming-ratchet` N5(c) | ⇒ ⇒ 触及 `Component::name()` 语义 ⇒ ⇒ **公开 API 变更** ⇒ ⇒ MUST 走 `public-api-remove-visibility-2026-11` + ADR 修订 |
| **分支预测器接入** | `rv32i-dead-component-consolidation`「不在本项」 | 功能增强 ≠ 重复实现 |
| **特权级接线**（CSR 指令 / `ECALL` / `EBREAK` / `MRET` / trap 入口 / 中断） | `rv32i-isa-gap-closure`「不在本项」 | 周期长、面大 ⇒ ⇒ 与 phase-6 P6「MUST 独立立项」的裁决一致 |
| **AXI cache 接入流水线**（MEM 级 stall 反压通路） | 本轮 planner 实测 | 流水线 MEM 级**无反压通路** ⇒ ⇒ 架构级改动 |
| **「BRAM 可推断」的真实验证** | `memory-timing-contract` ④ | CI **无 yosys** ⇒ ⇒ 只能本地/opt-in ⇒ ⇒ 已登记进 `.rddf/checks/arch-residual-registry.md` |

---

## 5. 护栏实测（2026-11-10）

```bash
bash scripts/verify_roadmap_fragments.sh          # ⇒ 绿（本文件不是 feature fragment ⇒ AUTO 块不受影响）
bash scripts/verify_roadmap_fragments.sh --self-test
```

---

## 6. ✅ 裁决记录（2026-11-10 · oracle 建议 1/2/3 · **用户已点头**）

> ✅ 三项待裁决**全部已裁**（用户 2026-11-10「按这个顺序推进」）⇒ ⇒ 裁决结果已回填进各草案文本。

| 原待裁决 | 裁决 | 落点（已回填） |
|---|---|---|
| **④ 「顶层端口」= T1 还是 T2** | **MUST 覆盖 T1**（顶层 module 端口表全体）· **字面同名 SHOULD 限定 T2**（`is_top_io == true`）· 判别字段 = `is_top_io`（不新造）⇒ ⇒ **T1/T2 不再阻塞 P2** | `port-name-literal-parity` §A.0 + P2 + §Impact 风险表（已删「裁决前不得定稿」行） |
| **⑤ 与 ratchet N4/AC:159 的正面冲突** | **方案 A + 三条加固**：① `cpp_member` 字段语义归 `rtl-emit-portability`（只引用）+ **ratchet 护栏 P1 MUST 增判「标识符 == 名字串」**（框架无反射 ⇒ 否则是**不可验证的 MUST**）② 成本估算按「唯一性论证 = 对既有不变式的文档化」**下调**（E2 已证伪「碰撞概率猛增」）③ 「字面同名」**不作 MUST** ⇒ ⇒ 改为**带前提的 SHOULD** + 「不可能条件 MUST NOT」⇒ ⇒ **不选方案 B**（会把 B1 整批阻塞在纯命名问题上）⇒ ⇒ route (c) 记为「**已评估 · 当前不做 · 触发条件 = 用户看到 manifest 产物后仍要求人眼可读**」 | `port-naming-ratchet` N4 / N2 / AC / What Changes / Capabilities（**已回填**）· `port-name-literal-parity` P2 / P3 / §裁决记录（**已回填**） |
| **⑥ P1 的用例范围** | **①②③④⑥ 一次搬入** · **⑤ 不作 SKIP ⇒ 转残差登记**（残差登记有 fail-closed 护栏 ⇒ ⇒ **会主动催办**；SKIP 不会）⇒ ⇒ 补两条 AC 硬要求：①④ MUST 断言**发射出的 module 文本** · 变异自证 MUST 含**重建**步骤 | `port-name-literal-parity` P1 + Acceptance（**已回填**） |

### ⚠️ 仍待处理（**不是**裁决项 · 是执行顺序）

| # | 事项 | 状态 |
|---|---|---|
| **⑦** | **A7 的落点**（残差登记）✅ **已落（2026-11-10 · architect 角色 · commit `ba99793`）**：登记表 `.rddf/checks/arch-residual-registry.md` 与台账 `docs/architecture/2026-11-02-hierarchical-rtl-output.md` §7 是**双向 1:1** 耦合（护栏 `scripts/verify_arch_ledger_residuals.sh` · 当前 **PASS / 22 行**）⇒ ⇒ **单改一侧 ⇒ 护栏红** ⇒ ⇒ ⇒ `docs/architecture` 在 `.rddf/config/arch-docs.json` 的 `arch_paths` 内 ⇒ ⇒ **ledger 侧属 architect 域** ⇒ ⇒ ⇒ **两侧已同批落地**（台账 §7 **行 21 / 21b** + 登记表末尾 **2 行**）⇒ ⇒ ⇒ 残差 1 = route (c) 含方向 (i) · 残差 2 = ⑤ 目标钉 ⇒ ⇒ ⚠️ **`node-name-prefix-consistency` MUST NOT 新增行**（它承载方向 (ii)，方向 (i) 由残差 1 覆盖 ⇒ ⇒ 两草案共用一条） | ✅ **已落（2026-11-10 · architect 角色 · commit `ba99793`）** |
| **⑧** | **critic 第 3 轮** ✅ **已跑完（REJECT · 上轮 7/7 闭合 + 新引入 5 条 BLOCKER）· 全部已闭合** ⇒ ⇒ **第 4 轮**（用户 2026-11-11 批准 · scope 极窄）⇒ ⇒ 结论 = REJECT（1B-4 判未闭合 + 新引入 2 条 BLOCKER）⇒ ⇒ ⇒ ⇒ **修复中**（R1-a 重写为三类互斥划分 + 3 处条款正文真正删除）⇒ ⇒ ⇒ ⇒ ⇒ ⇒ 审查记录：第 3 轮 = `.rddf/reviews/port-naming-parity-critic-round3-2026-11.md` |

---

## 7. 本表的**自检**记录（2026-11-10）

| 项 | 结果 |
|---|---|
| `scripts/verify_roadmap_fragments.sh` | ✅ PASS（AUTO 块守恒 + feature 内部一致性）⇒ ⇒ 本文件**不是** feature fragment ⇒ ⇒ 不影响根 `AGENTS.md` 的 AUTO 块 |
| `scripts/verify_roadmap_fragments.sh --self-test` | ✅ 全部通过 |
| 8 份 improvement 草案的 `pre_create_brainstorm_check.sh` | ✅ **9/9 PASS**（2026-11-10 · 新增 `node-name-prefix-consistency-2026-11` 后） |
| 本表**不含**任何未落盘的「批次」 | ✅ B0–B4 的每个 work item 都指到**具体的 improvement 草案 + work item ID** ⇒ ⇒ 可 grep、可追 |
