# Phase 6 / B2 中期复盘 #2（architect 域 · 2026-11-12）

> **目的**：用户口令「切 architect 对完成和后续任务进行梳理……然后切 planner 对后续还有哪些任务内容都梳理一遍，
> 再给出后续任务内容和实施路径」。
> **本文件的定位**：**当前态快照 + 目标链 + 后续任务清单**（架构主题文档 · **非决策文档**）⇒ ⇒ **决策内容零变更**；
> 本轮**不新建 ADR**、**不反转任何已签批决策**。
> **窗口唯一真相** = `.rddf/roadmap/phases/phase-6.md`（本文件 §5–§6 是 **2026-11-12 的实测快照**，
> **会过期** ⇒ ⇒ 不作为状态真相使用）。
> **上游** = 7 份 RTL 轨 improvement 草案（§1 的病根）+ `phase-6-rtl-productization.md`（批次编排 · 12 条依据可 grep）。
> **审稿记录** = `.rddf/reviews/phase-6-b2-midterm-review-2026-11.md`（oracle `PASS-WITH-NOTES` 9 条建议 **全落实**
> + critic `APPROVE_WITH_CONDITIONS` **0 BLOCKER** · 1 SHOULD_FIX + 3 NITS **全修完** + critic 无权限的 git 考古项已代跑回填）。

---

## 1. 目标链（**做什么 → 为什么**）

### 1.1 顶层目标（为什么要有「RTL 生成能力产品化」这条线）

`CppHDL` 的定位是 **C++23 的硬件描述库** ⇒ ⇒ 它的**交付物**不只是「能仿真的 C++ 代码」，还必须包括
**能被下游 FPGA/ASIC 工具链吃掉的产物**（Verilog + 端口清单 + 行为等价性）。
⇒ ⇒ 而 2026-11-10 用户拍板的 7 项诉求，**每一条都指向同一个病根**：

· **生成器要能当项目提供的能力用**（`rtl-emit-product-tooling`）⇒「**能生成**」与「**是产品**」之间有一整段距离 ·
  **端口名要能用**（`port-naming-ratchet`）⇒ 现状端口名不可用 · **要能装载真程序**（`firmware-load-chain`）⇒ 差一个装载链路
· **存储器要能上板**（`memory-timing-contract`）⇒「仿真正确」与「能上板」是两件事 · **RV32I 重复实现要收口**
  （`rv32i-dead-component-consolidation`）⇒ 现状给的是**虚假信心** · **ISA 缺口要显式化**（`rv32i-isa-gap-closure`）
  ⇒「静默 NOP」是最难查的一类缺陷 · **examples 的生成能力要有测试**（`examples-rtl-sim-coverage`）⇒ 零测试 = 退化无人知

⇒ ⇒ **共同点**：每一条都是「**声称成立但无人可验证**」⇒ ⇒ 所以这条线的统一解法不是加功能，而是
**给每个声称配一条可复算的机械判据**（护栏 / 产物级断言 / 变异自证）。

### 1.2 为什么分两批（B0–B4）

`phase-6-rtl-productization.md` 的批次编排不是「按工作量排」，而是**按「交付风险」排**：
· **B0** = 零交付风险（除 F-A 外）⇒ 先把**零消费者**的组件修对/退役 ⇒ ⇒ 后面批次的红**不是**它们造成的 ⇒ ⇒ 基线干净
· **B1** = 地基（manifest / CLI / 产物护栏）⇒ 下游全部依赖它 ⇒ ⇒ 晚做 ⇒ ⇒ 后面每一批都要重新适配端口面
· **B2** = 消费侧（examples 覆盖）⇒ 只**消费** B1 的产物 ⇒ ⇒ 不改产品语义 ⇒ ⇒ 可与 B1 末段并行
· **B3** = 时序契约（D-TCM 寄存读）⇒ 动流水线时序 ⇒ ⇒ golden 会变 ⇒ ⇒ 必须**单独一批**（否则金标算两遍）
· **B4** = 核心重构（组件接入）⇒ 改 ID/EX 级 ⇒ ⇒ **必须在 B3 之后**（同依据：时序金标算两遍）

⇒ ⇒ **硬约束（本轨最容易被违反的一条）**：**B3 → B4 不得乱序**。其余依据（12 条）逐条在
`phase-6-rtl-productization.md` §0，**每条可 grep**。

### 1.3 B2 为什么值得单列一批（本轮的**实质产出**）

B2 的名义交付是「10 个 example 的独立 RTL 仿真覆盖」。但 2026-11-12 这一轮的实际产出是
**三个被仿真抓出来的真实实现缺陷** —— 它们**在 C++ 侧的 demo 里全部是绿的**：

· **D5** 手写 FIFO demo 的 `full` 门限错（31 在途 ⇒ 第 17 次写入起地址混叠）⇒ C++ 侧 demo 只写 2 个元素 ⇒ **永远够不到门限**
· **D6** `uart_rx` 的采样点**硬编码** `div == 4`（不随 `prescale` 缩放）⇒ demo 的 `prescale == 9` ⇒ 硬编码**恰好**等于 `prescale/2`
· **D7** `uart_rx` 解出的字节**位反转**（线上 `0x10` ⇒ 解出 `0x08`）⇒ demo 的测试值 **0x5A 是位回文** ⇒ **恰好掩盖**

⇒ ⇒ **这才是 B2 的目的**：不是「覆盖率 +N」，而是「**换一个观察者**（独立 Verilog TB 按端口名驱动）
⇒ ⇒ 抓出同源 C++ demo 结构上看不见的缺陷」。⇒ ⇒ **D7 教训可外推**：**测试值必须避开对称值**
（位回文 / 字节回文），否则位序缺陷被恰好掩盖 ⇒ ⇒ 与仓内「期望值 MUST 手推」是同族纪律：
**手推的期望值本身也要能判别**。

---

## 2. 已完成哪些（**实测快照 · 2026-11-12**）

| 批 | 状态 | 证据 |
|---|---|---|
| **B0** | ✅ **6/6** | `67926cf` + `bd21b45` · `.rddf/reviews/b0-batch-verify-2026-11.md`（ctest 257/257 · examples 28/28 · 判别力 4/4） |
| **B1** | ✅ **11/11** | C4/C1/C3/C5/C6/N1/N2/N3/N4/N5(a)/N6/P1 · `.rddf/reviews/b1-batch-verify-2026-11.md`（**复审 APPROVE**）· ⚠️ 记号 12 个而项数 11（`N5(a)` 计入 `N5`）= 沿用源文件口径 |
| **B2** | ✅ **E1–E8 全部落地** | E3 = **10/10 verified** · E6 = CI step · 护栏**严格模式 PASS**（不再需 `--allow-pending`） |

### 2.1 B2 的 10 个 TB（**实测 1038 条断言 · 登记下界 782**）

**按断言数降序**（括号内 = spec-derived）：`counter_simple` **768**（512）· `fifo_stream` **62**（46）·
`counter` **54**（14）· `axi4_lite` **45**（33）· `uart_tx` **34**（16）· `riscv` **24**（22）· `uart_rx` **22**（16）·
`pwm` **13**（13）· `gpio` **9**（**0** ← D1：移植简化 ⇒ 只判 impl-contract）· `quadrature` **7**（4）

⇒ ⇒ **四个口径必须分清**（⚠️ **混用 = 假账** ⇒ 本轮初稿就是在这里算错了）：**运行时实测断言 = 1038**
（10 项 `RESULT: checks=<N>` 之和）· **登记下界 = 782**（TSV `min_checks` 之和 ⇒ ⚠️ `counter_simple`
登记 512 / 实测 768 ⇒ 下界**低于**实测）· **语义场景 = 54** · **spec-derived = 676**（后两者 = TSV 之和）。

⇒ ⇒ **每项都做了变异自证**（5 条/项，逐条转红 ⇒ ⇒ 判据有判别力 ⇒ ⇒ 不是恒绿护栏）。

### 2.2 phase-6 轨（P1–P15）

✅ 已闭：**P1 / P2 / P2b / P3 / P4 / P5 / P9 / P10 / P12 / P13 / P14 / P15** ·
⬜ 未闭：**P6**（RV32I ISA 缺口侦察+立项）· **P7**（②⑤ + ④ 闭口，见 §3.2）· **P8**（①②④）·
🔒 **P11**（ADR-036 物理移除 · 日历锁最早 **2027-04-17**）⇒ ⇒ **P7 的**部分**已落地但状态格未更新**（见 §3.2）；
**P8 的 stub 仍在**（实测：`include/simulator.h:95-96,655` + `src/simulator.cpp:297-300,332,529-530` 五处）。

### 2.3 残差台账（`arch-residual-registry.md` · 22 行）

**10 已收口 / 4 已裁决 / 4 不做 / 2 已知 / 2 未决** ⇒ ⇒ 护栏 `verify_arch_ledger_residuals.sh` **PASS**（双向 1:1 守恒）
⇒ ⇒ 2 条「已知」= 台账行 15（三档 PortPolicy 未分化 · 归属 builder）+ 行 9（Verilator 等价性排除项）；
2 条「未决」= 行 21 / 21b（route (c) 字面同名 · **触发条件 = 用户在看到 manifest 产物后仍要求人眼可读**）。

---

## 3. ★ 本轮**实测发现**（4 条 · 每条都有可复跑证据）

> 这 4 条是本轮 architect 复盘的**实质产出** —— 不是复述状态，是**实测发现的漂移与缺口**。


### 3.1 发现 1 🔴：**`verify_ledger_evidence.sh` 自 2026-11-10 起一直红 ⇒ CI 长期红**

**实测轨迹**：护栏本体 = `4293fc6`（2026-11-01，**先于** RTL 轨草案）⇒ 在 **`1dc1d98`（2026-11-10 · 立项
7 份 RTL 轨草案）** 上实测 ⇒ **红 7 项** ⇒ **那次提交引入的** ⇒ 今天**红 10 项**（9 份 RTL 轨 improvement +
1 份 `decision-request-port-naming-parity`）⇒ CI 挂载在 `.github/workflows/ci.yml:894`（`build-and-test` job ·
**每个矩阵分片都跑**）⇒ ⇒ **CI 是红的**。

⇒ ⇒ **根因**：这 10 份台账的 `evidence:` 是**散文尾巴**，不是可解析路径 ——
如 `evidence: probe:scripts/verify_hierarchical_rtl.sh（既有产物级护栏样板）+ 本轮实测 verilator 编译 ≈8s/设计`
⇒ 护栏要求 `probe:<path>` 的 `<path>` **必须是仓内存在的文件**（`scripts/lib/ledger_evidence.py:136-137`）
⇒ 「散文 + 括号注释」整体被当成路径 ⇒ **不存在** ⇒ FAIL。⇒ ⇒ ⚠️ **这不是 B2 引入的**（B2 之前就红）
⇒ 但它**卡住 CI** ⇒ 必须修。

⇒ ⇒ **修法 = 解析器口径的四条禁令 + 6 份升级**（对照表见附录 A）：**禁散文尾巴**（解析器把整串当路径）·
**禁 `+` 连接多路径**（只留主路径）· **禁 `:行号` 后缀**（⚠️ `ledger_evidence.py:142` **整串**查存在性
⇒ `:112-133` 同样不存在 ⇒ ⇒ **行号移进正文**，或改用 **`#L<行>` 锚**
⇒ ⇒ ⚠️ 解析器**已支持** `#anchor`（`:126` / `:137` 均 `split("#", 1)` ⇒ 锚被剥离后再查存在性）
⇒ ⇒ ⇒ **`#L123` 既可机器判存在又保留行锚**）· **禁 `/tmp/opencode/...`**（会话即蒸发）。
⇒ 6 份升级到**更硬的证据**（全部实测存在；`test:` 形态还要求 **CMake 注册** ⇒ 两个 test 文件已注册）；
另 4 份（`firmware-load-chain` / `memory-timing-contract` / `rv32i-dead-component-consolidation` /
`rv32i-isa-gap-closure`）用**裸的已提交路径**（`probe:` 形态）。

### 3.2 发现 2 🟡：**P7 的状态格落后于实现**（phase-6.md 与 next-steps.md 不一致）

| 来源 | 说的是什么 |
|---|---|
| `phase-6.md` P7 行 | `⬜（① ✅ / ②③④⑤ ⬜）` |
| `phase-6-next-steps.md`「T3 的实施进展」 | ✅ **三档已分化**（实测元组）· 提交 **`1d9683b`**（2026-11-10） |
| 源码实测 | `src/hierarchy_verilog.cpp:1610-1630` **三档分支都在**（`SynthesizeFatal` / `Reject` / 默认 `SynthesizeWarn`）+ `:1264` 成功路径读 policy + `:987` Warn 档逐条 CHWARN |

⇒ ⇒ **真相 = 「实现已落地」但「判据 / 立项 / 闭口」未做** ⇒ ⇒ **状态格必须写成这样**：
· **①** architect 步（ADR-060 v1.18）⇒ ✅ 已闭
· **②** 三档**两两可区分**（产物级元组断言）⇒ ⬜ **未做**（⚠️ 实测 `tests/test_hierarchy_verilog.cpp:1461-1505`
  仍是**旧失败路径**用例：样本 = `TwoOpTop`（**不可合成**）⇒ 三档恒 `(false, ·, ·)` ⇒ 判不了分化）
· **③** 行为点（成功路径 + 失败路径）⇒ ✅ 已做（`src/hierarchy_verilog.cpp:1610-1630` 三档分支 + `:1264`
  成功路径读 policy + `:987` Warn 档逐条 CHWARN）
· **④** 文档/注释义务 + **台账行 15 闭口** ⇒ 🟡 注释已做（`// CppHDL-synth:` 两处发射点）· **闭口未做**
  （登记表行 15 仍是 `已知` / 归属 `builder`）
· **⑤** 计数**分别**判（`implicit port` / `net` 分别计数）⇒ ⬜ **未做**

⇒ ⇒ ⚠️ **④ 的依赖链 = 「change 归档」**，**不是**「依赖 ⑤」
（登记表行 15 的 `闭口证据` = **archive 路径** ⇒ ⇒ MUST 先有归档件）⇒ ⇒
**正确顺序 = 修 Reject 粒度 → 补判据用例（②⑤）→ 立项并归档 → 闭口行 15（④）**。

### 3.3 发现 3 🟡：**phase-6.md 的 B2 行自相矛盾**（同一格里两套状态）

同一行内既有「**E1–E8 全部落地 ✅**」又有「⇒ ⇒ **E6 ⬜ 未做**」+「**E3 余项 + E6 + 墙钟登记 + exclusions
文档 + 两个 examples 护栏声明** = 本批剩余全部」⇒ ⇒ **两个时点的快照被并排粘在同一格** ⇒ ⇒ 读的人会以为
B2 还有 E6 没做 ⇒ ⇒ **必须订正**（planner 域）。

### 3.4 发现 4 🟡：**B3 的🚪 入口门有两道未过**（实测）

· ① ADR-068 已 Accepted（T2′ = (F) 字节道拆分）⇒ ✅
· ② **T2（MEM 数据对齐 A/B）已裁决** ⇒ ⛔ **未裁决**（ADR-068 §2.1 第 3 点明写「**本 ADR 不裁决 T2**」；
  `memory-timing-contract-2026-11.md` 的 T2 行仍是 A/B 两选项）
· ③ **公开面前置门**（V9 登记已落）⇒ ⛔ **未做**（`public-api-surface.md` + `CHANGELOG.md` 对
  `D-TCM` / `prog_addr` / `data_ready` **零命中**）⇒ ⇒ **B3 不得开工**
（ADR-068 §5.2 **V10** 明写这是**开工前置门，不是事后补记**）。

---

## 4. 为什么要这么排（**后续的顺序依赖**）

### 4.1 轨内：B3 → B4（**金标只算一遍**）

**B3**（D-TCM 改寄存读 ⇒ 拍数变 ⇒ golden 重算）→ **B4**（组件接入改 ID/EX 级 ⇒ golden 再变）
⇒ ⇒ **反序 ⇒ 时序金标算两遍** ⇒ ⇒ 中间态的「178 条退休 / 6 次 load-use 停顿」会**两次作废**
⇒ ⇒ ⇒ **B4 MUST 在 B3 之后**（`phase-6-rtl-productization.md` 依据 ④⑤）。

### 4.2 轨间：**B3 与 phase-6 余量（P6 / P7 / P8）可并行**（写点不重叠）

B3 动 `include/cpu/**` + `include/chlib/**`；P6/P7/P8 分别动 `include/cpu/pipeline/**`（ID/EX 级）·
`src/hierarchy_verilog.cpp` + `src/codegen_verilog.cpp` · `include/simulator.h` + `src/simulator.cpp`
⇒ ⇒ **函数级不重叠** ⇒ ⇒ 可并行。⚠️ **但共享 3 个单点资源**（`phase-6-next-steps.md` §1）：
① `rtl-hierarchical-output/spec.md`（**仅 P7 动** —— B3 的 spec 面在 rv32i / dtcm / manifest `semantic`）
② 根 `AGENTS.md` 的 4 个计数锚点（各 **2 处** ⇒ 文本必冲突）③ `.rddf/checks/public-api-*`
⇒ ⇒ **★ 锚点编辑权 = 最后合入者** ⇒ ⇒ 合入前 MUST **全量重建并重算**（禁写死「257」⇒ ⇒ 写成
「基线 + 新增 − 删除 == `ctest -N`」）⇒ ⇒ **B2 收尾不碰这 3 个资源** ⇒ ⇒ 与 B3 **无冲突**。

### 4.3 为什么 D8（`ext_clk` 死端口）**不阻塞**任何一批

`ext_clk` 是 **SoC 顶层的一个 input 端口**，但产物里**所有**流水线寄存器都锁在框架 `default_clock`
⇒ ⇒ ⇒ 它对行为**零影响** ⇒ ⇒ 删它 / 注释它都**不改任何时序金标** ⇒ ⇒ 与 B3/B4 无耦合 ⇒ ⇒ **可独立立项**。

---

## 5. 目前的任务是什么（**按可执行性排序**）

> ⚠️ **本节是快照** ⇒ 状态真相仍以 `phase-6.md` 为准。
| # | 任务 | 归属 | 前置 | 机械判据 |
|---|---|---|---|---|
| **T0** | 🔴 修 `verify_ledger_evidence.sh` 的 10 项红（10 份台账的 `evidence:` ⇒ 裸路径 / 升级形态 ⇒ §3.1 四条禁令 + 附表 A）⇒ ⇒ **解 CI 红** | planner | — | 护栏 **PASS**（含 `--self-test`） |
| **T1** | 🟡 订正 phase-6.md 的 **B2 行**（删「E6 ⬜ 未做」那段陈旧快照 ⇒ 保留「E1–E8 全部落地 ✅」） | planner | — | `grep -c "E6 ⬜ 未做"` == **0** |
| **T2** | 🟡 订正 phase-6.md 的 **P7 行状态格**（①③ ✅ · ②⑤ ⬜ · ④ 🟡 半闭）⇒ 与 next-steps 的 T3 遗留段一致 | planner | — | 两文件说法一致 + **不得**把「实现已落地」写成「子步已完成」 |
| **T3** | 🟡 **P7 的 ②⑤ + Reject 粒度 → change 立项/归档 → ④ 台账行 15 闭口**（④ 的依赖 = **change 归档**，不是依赖 ⑤） | builder → architect | T2 | 行 15 = `已收口` · `归属`=`—` · 闭口证据 = **archive 路径** |
| **T4** | 🟡 **P8 builder 步**（删 `set_ab_verification` stub + golden 下限派生 + 两份文案订正） | builder | — | 公开面护栏绿 + 源码面 **0** 命中 |
| **T5** | 🔴 **B3 的 T2 裁决**（MEM 数据对齐 A/B）⇒ **MUST 与 ADR-068 的 D4 一并读**（若选 A1 ⇒ MUST 追加同址处置）⇒ ⚠️ **载体 = 新立 ADR**（建议 **ADR-069** · Parent = ADR-068）⇒ **禁**改写刚 Accepted 的「本 ADR 不裁决 T2」句 | architect | — | 新 ADR + **oracle → critic 全审**（**决策内容变更**，非轻量判据替换） |
| **T6** | 🔴 **B3 的公开面前置门**（V9：CHANGELOG + `public-api-surface.md` 各一行）⇒ **登记未做 ⇒ 不得开工** | planner | — | 两文件各 1 行 + 公开面护栏绿 |
| **T7** | 🟡 **P6-0 侦察**（RV32I ISA 缺口：复用面清单 + `FENCE` 裁决候选）⇒ 可全程并行 | planner → architect | — | 复用面清单落进 `rv32i-isa-gap-closure-2026-11.md` |
| **T8** | 🟢 **D8**（`ext_clk` 死端口）⇒ 建议 **A2**；**A1 另独立立项**（⚠️ A1 触 ADR-008 单时钟域）⇒ ⚠️ **A2 爆炸半径**：SoC 顶层输入 **5 → 4** ⇒ `tb_rv32i_soc.sv` 的 `.top_unnamed_input(ext_clk)` + `test_rv32i_soc_dual_parity.cpp` 的驱动点 MUST 同步删（**行为零变化**成立 —— B2 已实测） | architect → 用户 | — | 决策请求单结案 |
| **T9** | 🔒 **P11**（ADR-036 物理移除） | builder | 日历锁 **2027-04-17** | 日历锁护栏守 |
| **T10** | 🟡 **第三处状态漂移**：`mem-port-arg-cross-layer-2026-11.md` 的 frontmatter 仍是 `状态: draft` + `evidence: unmeasured` ⇒ 而 **P5 已归档**、台账行 20 **已收口**（登记表 `:55`）⇒ 与 T1/T2 同族 ⇒ **T2 扫一遍时带上** | planner | — | `状态:`=`implemented` + `evidence:` 指真实路径 |
| **T11** | 🟡 **`FENCE` 语义裁决**（architect · **planner 无权拍板**）⇒ 须在 **T7 侦察之后** ⇒ 可与 B3 并行 | architect | T7 | 新 capability spec 落 `FENCE` 条款 |

⇒ ⇒ **T0 是唯一「现在就能做且解 CI 红」的项** ⇒ ⇒ T1/T2 是**状态真相订正**（便宜 ⇒ ⇒ 先做）
⇒ ⇒ T5/T6 是 **B3 的🚪 门**（⇒ ⇒ B3 在这两项之后才能开工）。

---

## 6. 后续任务内容 + 实施路径（**planner 域承接**）

⇒ ⇒ **具体实施路径由 planner 落进 `.rddf/roadmap/phase-6-next-steps.md`**（planner 域 · 那是「任务内容 +
实施路径」的权威载体）⇒ ⇒ architect **不抢** planner 的产出。本节只给**架构层的边界**：

**批次层面的后续内容**（**详细清单见 planner 文件**）：
· **B2 收尾** = ① 验收报告（**尚未写**）② T0（ledger 修）③ T1/T2/T10（状态订正）⇒ 出口 = ctest 263/263
  + 12 护栏 + examples 28/28 + samples 39/39 + 报告落盘
· **B3** = T1+T3 同批（D-TCM `aread`→`sread` + `data_ready` 语义重定义）· T6（store→load 同址）· T2/T4/T5 · L1–L6
  ⇒ 开工门 = **T5** + **T6** ⇒ 出口 = ADR-068 的 V1–V10 全绿 + 时序 golden **手推重算**
· **B4** = C1 差分（先做）· C2 ALU 编码归一 · C3/C4 组件接入 · G3→G1+G4（同批）→ 其余 ⇒ 开工门 = **B3 已闭**
  ⇒ 出口 = 组件接入后 20 项 golden + 计数锚点重算

**两条轨的交叉点**（实测）：phase-6 的 **P6**（RV32I ISA 缺口）与 B4 的 **G1–G5** 是**同一片区域的两个视角**
⇒ ⇒ **实施前 MUST 确认 P6-0 侦察结论已并入** `rv32i-isa-gap-closure-2026-11.md`
⇒ ⇒ **不得两条轨各改一次 ID 级**；P6 的**判据同步修改已获批准**（用户 2026-11-11）
⇒ ⇒ 改判据时 MUST 同时记「原判据被什么取代、为什么」。

**四条不得越界的边界**（复盘结论）：

| 边界 | 内容 | 依据 |
|---|---|---|
| **不得改已签批 ADR 的决策内容** | 改 ADR 决策 ⇒ MUST 切 architect ⇒ builder 不得单方面改 | `phase-6.md` P4 行 |
| **不得 B3/B4 乱序** | 时序金标只算一遍 | `phase-6-rtl-productization.md` 依据 ④⑤ |
| **不得用「覆盖率 +N」替代判别力** | 每个 TB 的变异 MUST 逐条转红 ⇒ ⇒ 恒绿护栏 = 没有判据 | 本轮 D5/D6/D7 的成因分析 |
| **不得放宽 `ledger_evidence` 的解析器来修发现 1** | 容忍散文尾 / `+` 多路径 / `:行号` 后缀 = **拿 fail-closed 语义交易便利** ⇒ ⇒ 散文尾巴正是该护栏要抓的「结论先于测量」形态 ⇒ ⇒ 放宽 = 复辟它要治的病 | `scripts/lib/ledger_evidence.py:136-143` |

---

## 7. 判据自检（**可复跑**）

```bash
bash scripts/verify_examples_rtl_sim.sh && bash scripts/verify_arch_ledger_residuals.sh   # ①② ⇒ PASS
bash scripts/verify_ledger_evidence.sh      # ③ 发现 1（待 T0）⇒ 现 FAIL(10) ⇒ 修后 PASS
grep -c "PortPolicy::SynthesizeFatal" src/hierarchy_verilog.cpp   # ④ 发现 2（实测 2）
grep -c "PortPolicy::Reject"            src/hierarchy_verilog.cpp   # ④ 发现 2（实测 3）
grep -c "E6 ⬜ 未做" .rddf/roadmap/phases/phase-6.md               # ⑤ 发现 3（待 T1）⇒ 现 1 ⇒ 修后 0
grep -c "D-TCM" .rddf/checks/public-api-surface.md                 # ⑥ 发现 4（待 T6）⇒ 现 0 ⇒ 修后 ≥1
grep -m1 "CTEST_TOTAL" AGENTS.md                                  # ⑦ 锚点 ⇒ 263
```

---

## 附录 A · 发现 1 的「6 份升级对照表」（**推导** ⇒ 供 planner 直接执行）

· `examples-rtl-sim-coverage` ⇒ `probe:scripts/verify_examples_rtl_sim.sh`（✓）· `rtl-emit-product-tooling`
  ⇒ `probe:scripts/verify_flat_rtl_emit.sh`（✓）
· `port-naming-ratchet` ⇒ `test:tests/test_port_name_named_ctor.cpp`（✓ + **CMake 已注册** ✓）
· `node-name-prefix-consistency` / `port-name-literal-parity` / `decision-request-port-naming-parity`
  ⇒ `test:tests/test_port_name_parity.cpp`（✓ + **CMake 已注册** ✓）
⇒ ⇒ 判据出处：`scripts/lib/ledger_evidence.py:125-143`（`test:` 额外要求 `_cmake_registered`）。
