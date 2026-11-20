# Proposal: phase3a 进 RTL 产品面 —— `--emit-rtl` + 冻结手推期望（`phase3a-rtl-product`）

- **状态**: **立项（2026-11-20 · planner）** · 未开工
- **用户诉求**（2026-11-20）: 「最后做 phase3a 加 `--emit-rtl` ⇒ 进产品清单 + **冻结手推的 module 集 / 端口数期望**（新增产品面）」
- **前置**: `axi4-convergence-tail-2026-11`（**已归档 2026-11-20** · 把 `phase3a_complete.cpp` 拆成 `phase3a_top.h` + `tb_phase3a_example.cpp` 三角色）· `emit-cli-into-example-2026-11`（发射入口并入 tb 的形态）
- **相关**: ADR-072（产品端口面 · 四件套）· ADR-075（端口具名 E1）· ADR-074（验证单一真相源）· capability `examples-rtl-sim-coverage`

---

## Why

`examples/axi4/` 的三个 example 里，**两个已是 RTL 产品面**（`tb_axi4_lite_example` · `tb_axi_soc_example`），只有 **phase3a** 不是 ⇒ ⇒ 它是**该目录最后一个未进产品面的单元** ⇒ ⇒ 后果有二：

1. **它的 RTL 产物从未被独立验证过** —— C++ 侧 2 条断言（`tb_phase3a_example.cpp` · 本会话刚补的协议行）只覆盖**解释器路径**；⇒ ⇒ 「同一份设计在 Verilog 产物上的行为」**无任何证据**。
2. **它的端口面是「未具名」的** ⇒ ⇒ 若直接发射，产物端口名会是 `top_unnamed_input_N` 形态 ⇒ ⇒ 独立 TB **无法按名连线** ⇒ ⇒ 进产品面**必须先具名**（ADR-075 E1）。

⇒ ⇒ 本 change = **新增产品面**（不是收敛）⇒ ⇒ 成本 = 一份**手推**的 module 集 / 端口数 / 断言条数期望 + 一份**手写**独立 TB。

---

## What Changes

### ① 端口具名（**硬前置** · ADR-075 E1）

`examples/axi4/phase3a_top.h` 的 7 个顶层端口（`start` · `is_write` · `addr` · `wdata` · `rdata` · `done` · `rst_n`）**改为具名构造**（`ch_in<ch_bool> start{"start"};`）⇒ ⇒ 产物端口名 = `top_<C++ 成员名>`。

⇒ ⇒ **连带** `.rddf/checks/port-naming-exempt.tsv` 的 **7 行**（phase3a 的未具名端口）**删除** ⇒ ⇒ 棘轮 `EXPECTED_EXEMPT_PORTS` **1384 → 1377**。

### ② `--emit-rtl` 模式（照 Lite / SoC 的**同型**形态）

`tb_phase3a_example.cpp` 加**发射模式**（`strip_mode_flags` + `require_out_dir` + `run_emit_cli` —— **复用本会话已下沉到库层**的共用实现）⇒ ⇒ 默认模式（无参）行为**逐字不变**（仍是 2 条断言 + `RESULT: checks=2 fails=0`）。

⚠️ **不进 `--check-products`**（phase3a 无 `ch_layer` ⇒ 层次化 = 1 module ⇒ 层次化自检无判别力）⇒ ⇒ 只做发射。

### ③ 独立 TB（**手写** · `examples/axi4/tb_phase3a_example.sv`）

驱动 5 个输入 + 观测 2 个输出（**9 根连线 = 框架 2 + 根自有 7** —— 框架 2 漏连 ⇒ 严格 flags 下 `%Warning-PINMISSING` **致命**）⇒ ⇒ 断言 = **AXI-Lite 寄存器写后读回**（与 C++ 侧同语义，但**在 Verilog 产物上**）⇒ ⇒ 断言条数 **N 由实施期手推冻结**（登记进清单的 `min_checks`）。

⚠️ **`generated=no`**（phase3a 的激励**不是** `.verif` 驱动 ⇒ ⇒ 与 SoC 行同型）⇒ ⇒ 护栏 ⑥'/⑥''/⑥‴/⑧''/⑥⁗ 对该行**不适用**（D7 的手写逃生口）。

### ④ 清单登记 + 锚点更新

| 项 | 前 | 后 |
|---|---|---|
| 清单行 | **无**（但**落选表**里有它 ⇒ 见下） | 新增 `tb_phase3a_example`（`kind=target` · `emitter==target`（自发行）· `interp_checks=2`（C++ 侧）· `min_checks=N`（TB 侧）· `generated=no` · `scenarios` / `spec_checks` 取值口径见下） |
| **落选表** | `.rddf/checks/examples-rtl-sim-exclusions.tsv` 有该行（理由 =「阶段汇总回归 · 非独立产品单元」⇒ ⇒ **与本 change 的前提冲突** ⇒ ⇒ 本 change 即是把它变成独立产品单元） | **删该行**（⇒ ⇒ 落选 18 → 17）⇒ ⇒ 原落选理由由本 proposal 取代（**留痕在此**） |
| 守恒式的**陈旧值** | 两处写的是 `28 == 10 ∪ 18`（**已旧** —— tracked 已是 **27**，`axi4_full_example` 退役未同步） | 同批订正为 **`27 == 11 ∪ 16`**（`examples-rtl-sim-exclusions.tsv` 头注释 + `.rddf/checks/examples-rtl-sim-exclusions.md`） |

⚠️ **桶判据（`kind=target` 的口径 · 免得被读成「规避 ⑧'」）**：`soc` 桶 = **有显式 `ch_layer` 成层声明**的装配（capability `rtl-soc-output`）⇒ ⇒ phase3a 的 `describe()` 里**无 `ch_layer`**（三个 `ch_module` 都是**内联**组件）⇒ ⇒ 故它是 `target` 桶 ⇒ ⇒ 顺带：护栏 ⑧'（「≥2 module」）对 `target` 桶**不适用**（而 phase3a 的层次化 = 扁平 = 1 module ⇒ 若归 `soc` 桶反而会被 ⑧' 判红 ⇒ ⇒ **桶判据与产物形态互为印证**）。
| `# REGISTERED_TARGETS` | **10** | **11** |
| coverage 守恒 | 27 == 10 ∪ 17 | 27 == **11 ∪ 16** |
| `EXPECTED_EXEMPT_PORTS` | **1384** | **1377** |

⇒ ⇒ ⚠️ **两侧条数各自登记**（`interp_checks=2` ≠ `min_checks=N`）⇒ ⇒ 与 SoC 行同型（ADR-074 §7.6 的 AC-R13′-2 口径：各自登记，**MUST NOT** 用一侧代用另一侧）。

### ⑤ 期望源登记（**手推** · 禁抄仿真输出）

`.rddf/checks/examples-rtl-expectations.md` 新增 `## tb_phase3a_example {#phase3a-example}` 段 ⇒ ⇒ 记 **module 集**（= **{top}**，1 个 —— phase3a 无 `ch_layer` ⇒ 层次化与扁平**逐字节相同**）· **顶层端口表 = 9 个**（**框架 2**（`default_clock` · `default_reset`，框架注入）+ **根自有 7**（`start` · `is_write` · `addr` · `wdata` · `rdata` · `done` · `rst_n`）—— ⚠️ **口径 = SoC 的同款**（`soc-rtl-expectations.md` §1「顶层端口表（**14** 个 = 框架 2 + 根自有 12）」）⇒ ⇒ **不是 7**）· **TB 断言条数 N** 及其**逐条推导**。

⚠️ **归属判据**：期望源载体 = **`examples-rtl-expectations.md`**（本仓的通用期望登记）⇒ ⇒ **不是** `soc-rtl-expectations.md`（后者是 **SoC 层专用**（ADR-073）⇒ ⇒ phase3a 不属 SoC 层）⇒ ⇒ 归属判据 = 「**该单元是不是 SoC 层产物**」。

⚠️ **N 的构成（MUST ≥ 5 条 · 冻结在期望源里）**：① 写事务在有界拍数内完成（`done` 拉高）② 读回值 == 写入值 ③ `default_reset` 生效（高有效）④ `rst_n` 生效（低有效）⑤ 未完成事务的有界超时 ⇒ ⇒ **具体条数**在期望源段里**逐条推导并冻结**（⇒ ⇒ 清单的 `min_checks` == 该条数 ⇒ ⇒ 护栏 ⑥ 的**等式**成立）。⚠️ **期望值 MUST 手推**（根 `AGENTS.md`：「两后端一致」不能当作正确性证据 ⇒ ⇒ 禁抄 C++ 侧输出 / 禁抄被测实现）⇒ ⇒ 外部规范 = **AXI4-Lite 协议**（写地址 + 写数据 ⇒ 读回同址同值）⇒ ⇒ 该部分 `spec-derived`；复位与拍数余量 = **impl-contract**。

---

## Capabilities

**无 spec 变更** ⇒ `.openspec.yaml` 声明 `skip_specs: true` ⇒ 归档用 `openspec archive … --skip-specs`。

⇒ ⇒ 既有 `examples-rtl-sim-coverage` 的 requirement 已覆盖「登记 + 期望源 + 三方等式 + 端口表四件套」⇒ ⇒ **本 change 不新增也不修改任何 Requirement** ⇒ ⇒ 故 **不写 delta**。

---

## Impact

| 面 | 变化 |
|---|---|
| 代码 | `examples/axi4/phase3a_top.h`（7 端口具名）· `examples/axi4/tb_phase3a_example.cpp`（+ `--emit-rtl` 模式）· **+1 文件** `examples/axi4/tb_phase3a_example.sv` |
| 清单 | `.rddf/checks/examples-rtl-sim-targets.tsv` **+1 行** · `REGISTERED_TARGETS` 10 → 11 |
| 期望源 | `.rddf/checks/examples-rtl-expectations.md` **+1 段**（手推） |
| 棘轮 | `.rddf/checks/port-naming-exempt.tsv` **−7 行** · `scripts/verify_port_naming.sh` 的 `EXPECTED_EXEMPT_PORTS` 1384 → 1377 |
| 护栏 | `verify_examples_rtl_sim.sh` 的 ⓪ 锚点随清单（**不改脚本**）· `verify_examples_rtl_sim_coverage.sh` 的守恒式随锚点（**不改脚本**） |
| 锚点 | `CTEST_TOTAL` / non-perf / `tests/test_*.cpp` / `EXAMPLES_TEST_SOURCES` **全部不变**（无新增测试源 · 无新增 CTest 条目） |
| 文档 | `examples/axi4/AGENTS.md`（FILES + PATTERNS）· `docs/CHANGELOG.md` · ⚠️ **`tb_phase3a_example.cpp` 的头注释**（本 change 前的「不进产品清单 · 无发射模式」陈述**过时** ⇒ MUST 同步） |
| **公开面登记** | **不需要** ⇒ ⇒ phase3a 的端口名此前**无产物**（`top_unnamed_input_N` 形态从未被冻结过）⇒ ⇒ 本 change 是**首次定义**而非**变更** ⇒ ⇒ `.rddf/checks/public-api-surface.md` 的「已移除 / 已改名的公开符号」章**不适用** |

---

## Out of Scope

| 项 | 理由 |
|---|---|
| **`--check-products`（产品级自检）** | phase3a 无 `ch_layer` ⇒ 层次化 = 扁平 ⇒ **层次化自检无判别力**（同 Lite 的 `AxiLiteTop`）⇒ ⇒ 属**新增自检面**，非本 change |
| **把 phase3a 迁成 `.verif` 驱动** | 那是 ADR-074 §7（D2′/D3′）的**实施侧**范围（SoC 迁移的同族）⇒ ⇒ 独立立项 |
| **16 对路由的验收** | `tests/test_axi_interconnect_4x4.cpp` 承担 ⇒ ⇒ example **不是** 16 对路由的验收手段（既有纪律） |
| **`examples/riscv-mini` 侧同类** | 另一批 example ⇒ 独立立项 |

---

## 用户可见变更

| 项 | 前 | 后 |
|---|---|---|
| `phase3a` 的 RTL 产物 | **不在产品面**（无登记、无独立 TB ⇒ 只有 C++ 侧 2 条断言） | 登记为 RTL 产品面 ⇒ 产物被**独立 TB** 验证（`RESULT: checks=6 fails=0`） |
| `tb_phase3a_example` 的命令行 | 只有默认模式（跑 C++ 侧激励） | 新增 `--emit-rtl <dir>`（发射 `top.v` + `manifest.json`）⇒ 默认模式行为**逐字不变** |
| 顶层端口名 | `top_unnamed_input_N`（未具名 ⇒ 无法按名连线） | `top_<C++ 成员名>`（7 个根自有端口全部具名） |
| 端口命名豁免表 | 含 phase3a 的 7 行 ⇒ `EXPECTED_EXEMPT_PORTS` = 1384 | 无 phase3a 行 ⇒ 棘轮收敛到 **1377** |
| RTL 产品清单 | 10 个 target | **11** 个（新增 phase3a 行） |

---

## 验收标准

| # | 判据 | 方式 |
|---|---|---|
| AC1 | `phase3a_top.h` 的 7 个端口**全部具名** ⇒ 产物端口名 = `top_<成员名>`（**实测**） | 发射 + 查产物 |
| AC2 | `EXPECTED_EXEMPT_PORTS` == **1377** · `port-naming-exempt.tsv` 无 phase3a 行 | grep + 护栏 |
| AC3 | `tb_phase3a_example --emit-rtl --out <dir>` ⇒ exit 0 + 产出 `top.v` + `manifest.json` | shell |
| AC4 | 产物 **module 集 == {top}**（**1 个**）· **顶层端口 == 9 根**（框架 2 + 根自有 7 · **手推期望**） | grep + 护栏 ⑧ |
| AC5 | 独立 TB `examples/axi4/tb_phase3a_example.sv` **存在** · 编译通过 · 跑通 ⇒ `RESULT: checks=N fails=0` 且 **N == 清单登记值** | 护栏 ③④⑤⑥ |
| AC6 | 清单新增该行 · `REGISTERED_TARGETS` == **11** · coverage 守恒 `27 == 11 ∪ 16` | awk + 护栏 |
| AC7 | 期望源段**存在**且含 **module 集 / 端口数 / 条数** 三项**手推**登记 | 读文件 |
| AC8 | `interp_checks` == **2**（C++ 侧）且 **≠** `min_checks`（TB 侧）⇒ ⇒ 两侧各自登记 | awk |
| AC9 | `verify_examples_rtl_sim.sh` **PASS**（13 项）· coverage **PASS** · port_naming **PASS** · `ctest` **全绿** | shell |
| AC11 | **变异自证 ≥2 条**（如「`rdata` 恒 0」·「`done` 恒 0」·「复位失效」）⇒ ⇒ TB **必红**（`fails>0`）⇒ ⇒ 变异形态登记在期望源段 | 投毒 |
| AC12 | `scenarios` / `spec_checks` 两列**已登记**且取值有依据；⚠️ 若 `spec_checks < 3` ⇒ ⇒ 护栏 ②'' 的告警**接受理由**写进期望源（phase3a 的外部规范面 = AXI4-Lite 写后读回，**窄于** SoC 的 UART 帧面 ⇒ ⇒ 条数天然更少 ⇒ ⇒ **不是**「断言强度不足」的信号） | awk + 读期望源 |
| AC10 | `openspec validate <change> --strict` **valid**（**无 spec 变更** ⇒ `--skip-specs` 归档） | openspec |
