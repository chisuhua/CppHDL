# Proposal: RTL 验证的单一真相源与生成式 TB（`rtl-verification-single-source`）

- **状态**: **立项（2026-11-17 · planner）** · 未开工 · **metis 第 1 轮已处置（P0×10 / P1×8 / P2×6）**
- **架构依据（决策面）**: `docs/adr/ADR-074-rtl-verification-single-source.md`（**Proposed**）· `docs/adr/ADR-075-port-naming-prerequisite.md`（**Proposed**）
- **方案细则**: `docs/architecture/2026-11-17-rtl-verification-single-source.md`（当前态唯一权威）
- **improvement**: `.rddf/improvements/rtl-verification-single-source-2026-11.md`
- **被修订的上位决策**: ADR-072 §2.4 D4 第 3 行被**部分 supersede**（「撰写=人」不变 /「渲染=生成器」新增）；ADR-072 D2 · ADR-073 D3/D6 **零变更**

---

## Why

用户 2026-11-17 追问「为什么需要 `emit_rtl.cpp` + `emit_rtl_soc.cpp` + `*.sv`」，并明确「希望项目代码不要过于碎片化」。

**「碎片化」= 同一语义的多个物理载体**：

| 组 | 载体 1 | 载体 2 | 载体 3 | 实测症状 |
|---|---|---|---|---|
| **G-期望**（**产品交付链路**） | `.rddf/checks/*.md` | `tb_*.sv` 的 `expect_*` | — | 同一批期望值写 2 遍（⚠️ `tests/*` 是 **(A) 组件级**判据，**不在本项**） |
| **G-端口** | C++ 成员名 | 产物 `top_unnamed_input_N` | **手写映射表** | 🔴 **当前已漂**：注释 `awprot=input_1` vs 实例化 `.top_unnamed_input_1(arprot)`（`tb_axi4_lite_example.sv:18-25` vs `:84-93`） |
| **G-发射** | `examples/axi4/emit_rtl.cpp`（32） | `examples/axi4/emit_rtl_soc.cpp`（**388** · 壳层 39 行**已**走库层 CLI） | `examples/riscv-mini/emit_rtl_hier.cpp`（48） | ⚠️ **2026-11-18 订正**：原「旧行数里大部分重复实现库层能力」的陈述**已失真**（壳层早已走库层 CLI）⇒ ⇒ 现状 = **checker 机制**仍自实现 ⇒ ⇒ 收敛口径见蓝图 §4.3.1 的**逐符号切分表** ⇒ ⇒ ⚠️ **2026-11-18 二次订正**：`examples/axi4/emit_rtl.cpp` / `examples/axi4/emit_rtl_soc.cpp` **均已删除**（change `emit-cli-into-example-2026-11` / change `example-file-boundary-2026-11` §4.3.3） ⇒ ⇒ checker 机制**已下沉**到 `include/rtl_emit/product_check.h`（库层公开头） ⇒ ⇒ **本行的「现状载体」已被取代**（SoC 发射入口 = `tb_axi_soc_example --emit-rtl`） |

### 为什么现在做

- 库层 `write_tb_stub` 已能生成**端口连线 + clk/rst + VCD**（`include/rtl_emit/tb_stub.h:59`）⇒ 缺口只剩「激励/断言/期望值」。
- 库层 `run_emit_cli` 已支持 `--hierarchical` ⇒ `emit_rtl_soc.cpp` 的前提（「只发扁平」）已失效。 ⇒ ⇒ ⚠️ **2026-11-18 订正**：`emit_rtl_soc.cpp` **已整文件删除**（change `example-file-boundary-2026-11` §4.3.3） ⇒ ⇒ 且 SoC 发射入口**恒发层次化**（蓝图 §4.3.2 · 分支 A）⇒ ⇒ 本条已被 §4.3.2 取代。
- N2 棘轮已把「端口名可解析到 C++ 成员名」立为 MUST（只对新增）⇒ 提到本 change 登记的 32 个是**回到规范**。

### 净成本（**如实披露** · 供用户判断值不值）

| 项 | 量 |
|---|---|
| 净新增机制 | **≈ +1300 行**（扫描器 / IR / IR 解释器 / 渲染器 / 判据 / 3 个测试文件） |
| 净删除 | ⚠️ **2026-11-18 订正**：原「≈ −530 行」估算**作废**（实测 388 行 ⇒ 原行数目标算术不成立）⇒ ⇒ 目标改为**结构判据**（蓝图 §4.3.1）⇒ ⇒ 净删除量 MUST 按实测重算 |
| 换掉的重复 | 2 个手写 TB 的 **51 条断言**（37 + 14）的端口映射与期望值双写 |

⚠️ **这是「一次性机制成本换长期单一真相」的取舍** ⇒ 若不接受，替代 = 只做 C2/C3（发射收敛 + 具名），跳过 C4–C8。

---

## What Changes

| # | 变更 | 依据 |
|---|---|---|
| **C1** | **抽取顶层组件**到 `include/axi4/{axi4_lite,axi_soc}_example_top.h`；删 `tests/test_axi_lite_verilator_e2e.cpp:74-142` 的 `AxiLiteTop` 副本（**C3/C7 不放宽** ⇒ 抽上去） | 蓝图 §4.5 |
| **C2** | **发射面收敛**：~~`emit_rtl_soc.cpp` 的**壳层**退化为「组装参数 + 调用」~~ ⇒ ⇒ ⚠️ **2026-11-18 订正**：该形态**已被取代** —— `emit_rtl_soc.cpp` **整文件删除**（蓝图 §4.3.3） ⇒ ⇒ 实际形态 = **发射并入** `tb_axi_soc_example --emit-rtl`（恒层次化）；删 `examples/axi4/emit_rtl.cpp`（并入 example 的 `--emit-rtl` 模式）；`--check-products` 的**机制**下沉 `include/rtl_emit/`（公开头）+ 实现 `src/rtl_emit/` | 蓝图 §4.3（2026-11-18 订正 · 含 §4.3.1 切分表 + §4.3.2 形态裁决） |
| **C3** | **端口具名（本 change 登记的 32 个）**：`AxiLiteTop` **20** + `AxiSocTop` **12**；⚠️ `AxiLiteTestMaster` 的 **23** 条**不改**；豁免表 `1447 → 1415` | **ADR-075 E1–E6** |
| **C4** | **`.verif` DSL → 一份 IR → 两个执行后端**；封闭原语 + 表达式文法 + **时序锚点** + **fail-closed**（含 `SET→EXPECT` 序列、名字未命中/多义） | **ADR-074 D1/D2/D3** |
| **C5** | **TB 改生成物（checked-in）+ 新鲜度（逐字节）+ 条数三方等式** | **ADR-074 D4** |
| **C6** | **期望值机器层（`.verif`）/ 推导层（`.md`）分离**（⚠️ 1:1 **只判键集合**，数值一致性 = process/review） | **ADR-074 D5** |
| **C7** | **`Simulator` 新增 node_id 级非类型化访问器**（读/写两侧）+ `kRejectDuplicates` 或等价唯一性校验 | **ADR-074 D2** |
| **C8** | **护栏改造**：⑥ 改等式 + 新增 ⑥'/⑥''/⑥‴/⑧'；⚠️ **列号 MUST 从 `#COLUMNS` 推导**（现状位置绑定，见 T5） | 蓝图 §4.4 |
| **C9** | **文档同步**（usage/developer guide + CHANGELOG + AGENTS 锚点 + tsv 增列 + `tb_stub.h` 头注释） | 蓝图 Step 6 |

### 任务映射（6 步 · **顺序硬约束**）

| task | 内容 | 依赖 | 交付物 |
|---|---|---|---|
| **T0** | **spec delta 定稿**（D1–D4，见下） | — | `specs/**` |
| **T1** | 抽取顶层组件 | — | N1/N2 + M11 |
| **T2** | 发射面收敛 | T1 | M3/M4/M5 + **M8 的 emitter 调用契约同步**（见 AC22） |
| **T3** | 端口具名 32 个 | T1,T2 | M1/M2/M9/M10 + TB/期望源同步 |
| **T4** | DSL + IR + 解释器 | T1,T3 | N3–N5/N7/N10/N8/N9 + **N11（测试）** |
| **T5** | 生成 TB + 护栏 | T4 | N6 + M6–M8 + M18–M22 + **N12/N13** |
| **T6** | 文档同步 | T1–T5 | M16/M17 + M23–M25 |

⚠️ **T3 必须早于 T4**（`name → node_id` 依赖具名）· **T1 必须早于 T3/T4**。
⚠️ **T2 必须与 M8/tsv 同 commit**：`emit_rtl_axi4_lite` 删除后护栏 ③ 立刻按 `--out/--emit-hygiene` 调用 `emitter` 列 ⇒ 不同批则 T2 单步即红。
⚠️ **T3 的中间态代价**（如实）：32 个端口改名后两个**手写** TB 的连线全失效 ⇒ 必须手改一次（~51 条断言的端口名），T5 又要用生成器重写 ⇒ **一次废工**。AC23 保护该手改与后续 `.verif` 的 `check_id` **同集合**。

---

## Spec delta 计划（**T0 的唯一权威输入**）

> ⚠️ **归属纠偏（metis P0-1/2）**：`hierarchical-port-naming-guard/spec.md:8-9,18-19` **明文禁止**该 capability 复述命名规则 ⇒ ⇒ **规则归 `rtl-hierarchical-output`**，护栏只收**机制义务**。`rtl-emit-portability` 的 Purpose 明写「只管清单的形状」⇒ ⇒ 仿真护栏的判据**不属它**；且 `examples-rtl-sim-coverage` 这个名字**已在用**（tsv 表头 / `run_all_ported_tests.sh:283`）却**从未建 spec**（`openspec/specs/` 0 命中）⇒ **本次补建**。

| # | capability | 种类 | Requirement | 改法要点 |
|---|---|---|---|---|
| **D1a** | `rtl-hierarchical-output`（**命名规则所有者**） | **ADDED** | 「**本 change 登记的 32 个产品顶层端口 MUST 具名**」 | ⚠️ 范围 = **32 条清单内联**（`AxiLiteTop` 20 + `AxiSocTop` 12）⇒ **不是**「产品面顶层全量」（豁免表 1415 条里大量是其它 example 的顶层 ⇒ 全量版一落盘即全仓恒红） |
| **D1b** | `hierarchical-port-naming-guard`（**机制义务**） | **ADDED** | 「端口名可解析性 MUST 有静态护栏核验」 | 只写**机制**：① `cpp_member != "unnamed_input"` ② `port_table[].name ↔ ports[].cpp_member` **1:1 唯一**（判定域见 AC2）③ 缺失/不唯一 ⇒ fail-closed。**不复述命名规则** |
| **D2a** | `rtl-soc-output` | **MODIFIED**（标题不变） | 「SoC 层 MUST 有独立可跑的 Verilog testbench」 | 加：该 TB **由 `.verif` 单一真相源渲染** + **checked-in** + 重新生成逐字节相同 |
| **D2b** | `rtl-soc-output` | **MODIFIED**（标题不变） | 「SoC 层 TB 的期望源 MUST 是仓内可审计的登记」 | 加：**机器层在 `.verif`**、推导层仍在 `.md`、TB 头 `EXPECT-SRC` 仍指 `.md` |
| **D2c** | `rtl-soc-output` | **ADDED** | 「验证判据 MUST 有单一语义来源（IR）」 | 新义务：两个执行后端（IR 解释器 / Verilog 渲染器）**不得各解释一遍** |
| **D3** | **`examples-rtl-sim-coverage`（新建 capability）** | **ADDED** | 6 条：⑥ 条数**三方等式** · ⑥' 新鲜度 · ⑥'' 双向键集合 · ⑥‴ TODO=0 · ⑧' 生成性（无 `top_unnamed_*`）· 生成覆盖度**登记列** | ⚠️ 该 capability 名字已在 tsv 表头与 `run_all_ported_tests.sh:283` 使用 ⇒ 补建 spec 即「补登记」，**不是新造概念** |
| **D4** | `example-runtime-coverage` · `example-assertion-quality` | **无需 delta**（**metis P1-14 已核**） | — | 前者管 `run_all_ported_tests.sh` / CTest 注册；后者的扫描面是 `examples/**` 的 Catch2 测试源，而 `verif_run.cpp` 是 `main`（不是测试） |
| **D5** | `rtl-emit-portability` | **不改**（撤销 v1 的 MODIFIED） | — | 「清单形状」未变（**不加 `cpp_member` 字段**，用 join） |

⚠️ **一律避免标题变更**（MODIFIED 改不了标题 ⇒ 需 REMOVED+ADDED ⇒ archive 合并会残留旧 requirement）。
⚠️ **delta 未落盘前 ⇒ 不得进入 T1**（契约先行）。

---

## 验收标准

**23 条**（编号与 ADR-074 §5 / ADR-075 §5 / 蓝图 §8 的**双向对照**见 improvement §3）

| AC | 判据（摘要） | 载体 |
|---|---|---|
| AC1 | 发射入口 `.cpp` 无直接 `toVerilogHierarchical`（白名单 = `examples/riscv-mini/tests/test_rv32i_hier_rtl.cpp:161,181` + 注释） | shell |
| **AC2** | **32 条清单内**每个端口 `cpp_member != "unnamed_input"` 且 `port_table[].name ↔ ports[].cpp_member` **1:1 唯一**；⚠️ **判定域 = `role ∉ {clock, reset}` 且 `is_top_io == true`**（A 类 clock/reset 的 `cpp_member` **MUST NOT** 被要求 —— 见 `rtl-hierarchical-output/spec.md:645-656`） | probe |
| **AC3** | `.md` 锚点 ↔ `.verif` **按 `scenario` 级 1:1**（**推荐粒度**；⚠️ **只判键集合**，**不判数值** —— 数值一致性 = process/review，见 R9） | shell |
| **AC4** | 生成的 `.sv` 内**无** `top_unnamed_*`，**且** DUT 实例连线数 == `port_table[]` 条目数、逐条名 ∈ `port_table[]`（**存在面** ⇒ 防「什么都没生成」全绿） | grep + probe |
| **AC5** | 生成的 `.sv` 内 `// TODO:` == 0，**且** `scenarios ≥ 1`（同上存在面） | grep |
| AC6 | 重新生成 ⇒ **逐字节相同**（执行者 = CI step / `--self-test`，见 AC20） | shell |
| **AC7** | **三方等式**：`.md` 手推登记的期望条数（**人**）**==** `.verif` 静态展开数 **==** 运行期 `checks`；⚠️ **`min_checks` MUST NOT 由生成器输出回填** | shell |
| **AC8** | `check_id` **集合相等** + 各自结论相同；⚠️ **实现形态 = `.sv` 增 `RESULT: check <id> <pass\|fail>` 协议**（与 `verify_examples_rtl_sim.sh:475` 的 `RESULT: checks=` 正则**不冲突**）；否则降级为「计数相等 + 一次刻意失败注入下两后端同一失败」（二者择一，T0 定） | 双跑 |
| **AC9** | ≥3 条时序变异 + 5 条护栏判据负向对照，**逐条注入点 + 期望错误文本**；⚠️ **无效变异（恒绿但不改变可观测行为）MUST 如实登记且不计入 3 条**；⚠️ 变异③（端口方向互换）在 `.sv` 侧可能是**编译错误** ⇒ **编译失败算红** | 变异脚本 |
| AC10 | 扫描器 fail-closed（集合外构造 ⇒ 非零退出） | shell |
| **AC11** | **列号 MUST 从 `#COLUMNS` 推导**（禁位置绑定）；负向对照 = **在中间插一列仍绿**（现状 `read -r ... note` 位置绑定会静默错列 —— 已实测 `:159,:504`） | shell |
| **AC12** | 生成覆盖度**三方一致**：tsv `generated` 列值 == TB 头部生成器标记存在 == 对应 `.verif` 存在 | shell |
| AC13 | `verify_examples_rtl_sim.sh` 绿 + `--self-test` 绿 | shell |
| AC14 | `ctest -LE perf` 绿 + `ctest -L verilator_e2e` 绿 | ctest |
| **AC15** | `bash scripts/verify_test_counts.sh` 绿，**且** `CTEST_TOTAL` 266 → **269**（含 `VERILATOR_E2E_TOTAL` 是否变化的判定） | shell |
| AC16 | 护栏 `:214-217`（禁 `EXPECT-SRC` → `.cpp`）**逐字未改** | diff |
| AC17 | 豁免表 `1447 → 1415` + `verify_port_naming.sh` 绿 | shell |
| **AC18** | **活动面** `grep "只发扁平\|top_unnamed_input"` 归零；⚠️ **白名单（禁改）** = `docs/adr/**` · `docs/architecture/**` · `docs/CHANGELOG.md`（历史条目）· `.rddf/{reviews,roadmap,improvements}/**` · `openspec/changes/archive/**` · `docs/usage_guide/05-component-design.md:78,522,533,698,702,1017`（**教学段**：讲通用未具名机制，1415 条仍在 ⇒ 保留 + 加一句「产品面已具名」）；**精确目标集** = `examples/axi4/**` · `.rddf/checks/{examples,soc}-rtl-expectations.md` 的端口映射段 | grep |
| AC19 | 子 module 端口名（`AxiLiteTestMaster` 23 条）**未变** | diff |
| AC20 | 时序锚点构例（`TICK(1)` 后 `EXPECT` 寄存器输出）两后端同结论 | 双跑 |
| **AC21** | **名字未命中 / 多义 ⇒ fail-closed + 点名**；负向对照：`.verif` 把 `gpio_out` 写成 `gpio_outt` ⇒ 必红且错误文本可区分（⚠️ 现状 `data_map_` 未命中是 `CHWARN` + **返回 0** ⇒ 拼错会静默读 0 ⇒ 必须显式拒绝） | probe |
| **AC22** | 发射器调用契约：tsv `emitter` 列指向的二进制 MUST 接受 `--out/--emit-hygiene`（或 tsv 增 `emitter_args` 列）；现状 = `verify_examples_rtl_sim.sh:265` 的调用形态 | shell |
| **AC23** | **碎片化量化**（隐藏意图）：① 改一个 AXI 期望值 ⇒ 只动 `.verif` 一处 + `.md` 锚点一处 ② 改一个端口名 ⇒ 只动 C++ 一处 ③ `examples/axi4/` 每个文件能从**文件名 + 头部 5 行**看出角色（读者视角） | 人工 + review |

---

## Capabilities

- `rtl-hierarchical-output`（**ADDED**：32 端口具名规则）
- `hierarchical-port-naming-guard`（**ADDED**：机制义务）
- `rtl-soc-output`（**MODIFIED** ×2 + **ADDED** ×1）
- **`examples-rtl-sim-coverage`（新建 · ADDED）**
- `rtl-emit-portability` / `example-runtime-coverage` / `example-assertion-quality`（**无需 delta**）

## 用户可见变更

| # | 变更 | 破坏性 |
|---|---|---|
| 1 | **32 个产品顶层端口名**：`top_unnamed_input_N` → `top_<成员名>` | ⚠️ **是** |
| 2 | **2 个新公开头**：`include/axi4/{axi4_lite,axi_soc}_example_top.h`（⇒ MUST 登记 `public-api-surface.md`） | 否（新增） |
| 3 | `Simulator` 新增 node_id 级非类型化访问器（⇒ 同登记） | 否 |
| 4 | `--check-products` 下沉为库层入口（新公开头 + CLI 选项 ⇒ 同登记） | 否 |
| 5 | `examples/axi4/emit_rtl_axi4_lite` target **删除** | ⚠️ 是（仓内消费者 = 护栏 tsv） |
| 6 | `.verif` 新文件类型 | 否 |

## Out of Scope

- ❌ **全仓 1415 条端口债**归零（棘轮只减不增）
- ❌ **子 module 端口名**（`AxiLiteTestMaster` 23 条）具名化（另一类产品可见变更 ⇒ 单独立项）
- ❌ **`tests/*` 的组件级期望值载体收敛**（`(A)` 路径判据，与交付验证不同层 ⇒ 蓝图 R10）
- ❌ **反之亦然**：`tests/*` 不受本 change 影响（`test_axi_lite_slave_conformance.cpp` 的 15 例不动）
- ❌ **其余 10 个 example 的 `.sv` TB 仍手写**（不生成）· **`riscv-mini` 的 `tb_rv32i_soc.sv` 不生成**
- ❌ **取消 (C) 独立 Verilog TB**（ADR-073 D3 分工不变）
- ❌ **不是「不在范围」而是归本 change T6 的一项**：`docs/developer_guide/**` / `docs/usage_guide/**` 的**同步改写**（架构决策不在本 change 变更范围；**用户面文档同步在**）
- ❌ 历史记录（`.rddf/improvements/` 旧条目 · `openspec/changes/archive/**`）不追溯
- ⚠️ **唯一的「`.md` 可生成」例外**：`.rddf/checks/{examples,soc}-rtl-expectations.md` 的**端口映射段**由生成器产出（它不是期望值，是端口身份）⇒ 与 AC3 的「期望值正文禁生成」**分开**

## 🚪 门

| 门 | 条件 | 状态 |
|---|---|---|
| **G-0** | ADR-074/075 双审闭合 + 复审 0 BLOCKER | ✅（`Proposed`） |
| **G-1** | T0 的 spec delta 落盘（D1a–D3 定稿） | 🔴 **未满足** |
| **G-2** | ADR-074/075 → `Accepted` + 回填 AC 映射 | 🔴 待 T0 后 |
| **G-3** | metis 第 1 轮 findings 全处置 | ✅ 本版已处置（P0×10 / P1×8 / P2×6） |
