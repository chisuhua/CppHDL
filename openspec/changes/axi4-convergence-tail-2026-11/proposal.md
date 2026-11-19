# Proposal: examples/axi4 的收尾 —— phase3a 并入三角色 + C++ 侧断言纳入判据守护（`axi4-convergence-tail`）

- **状态**: **立项（2026-11-19 · planner）** · 未开工
- **前置**: `example-file-boundary-2026-11`（**已归档 2026-11-18** · 它做了三角色划分的前两步：设计定义归位 `examples/axi4/` + 发射并入 tb + `emit_rtl*.cpp` 全删）
- **相关**: ADR-074（验证单一真相源）· 蓝图 `docs/architecture/2026-11-17-rtl-verification-single-source.md` §4.3.3（三角色职责分离）· capability `examples-rtl-sim-coverage`

---

## Why

`example-file-boundary-2026-11` 归档后，`examples/axi4/` 的三角色形态已落地 2/3，剩两个**同类残留**：
一个**结构未收敛**，一个**判据未覆盖**。二者都属「看得见但没人守」的形态 ⇒ ⇒ 是本 change 的动机。

### 1. `phase3a_complete.cpp` 是该目录**最后一个**「设计定义 + `main` 一体」的 example

`examples/axi4/phase3a_complete.cpp`（**463 行**）里 `Phase3ATop`（`:48`，设计定义：7 个顶层端口
+ 3 个 `ch_module`：`AxiInterconnect4x4` · `AxiToAxiLiteConverter` · `AxiLiteSlave`）与
`main()`（`:311`，激励 + 断言）**同文件** ⇒ ⇒ 与 §4.3.3 的三角色形态不一致
⇒ ⇒ **该目录的其余两个 example 已是三角色**（`tb_axi4_lite_example` · `tb_axi_soc_example`）。

⚠️ **更实质的问题**：它的结论是**散文**而非协议行 ——

```
Phase 3A AXI-Lite 链路仿真通过（写 + 读回）
```

⇒ ⇒ 护栏**无法对账断言条数** ⇒ ⇒ 「断言被删光」只会表现为「exit 0」⇒ ⇒ 与已退役的
`axi4_full_example`（main 只打印不判定）是**同一族缺陷**（该 example 已因「只打印不判定」退役 · AC27）。

### 2. SoC 的 **C++ 侧**断言**无任何判据守护** ⇒ ⇒ 断言全被删也绿

`tb_axi_soc_example` 的默认模式跑 **2 条**手写断言（C++ 侧）⇒ 但：

| 判据 | 读什么 | 对 SoC 行的覆盖 |
|---|---|---|
| ⑥（`verify_examples_rtl_sim.sh:800`） | `run.log` = **tb 列指向的 `.sv`** 的 `RESULT:` 行 ⇒ checks=**14** | ❌ 与 C++ 侧无关 |
| ⑨（`:838`） | 作用域 = **note 列含 `interp-entry`** 的行 ⇒ 断言 `checks == min_checks` | ❌ SoC 行**没有**该标记 ⇒ **跳过** |

⇒ ⇒ **反证**：清单 SoC 行 `min_checks=14`，而 C++ 侧实际 `checks=2` ⇒ 两侧不等
⇒ ⇒ 若 ⑨ 覆盖该行 ⇒ ⑨ 必红 ⇒ ⇒ **但护栏 PASS** ⇒ ⇒ **⑨ 确实未覆盖该行**。

⇒ ⇒ **实测**（2026-11-19）：把 SoC 的 C++ 侧换成「无参输出 `RESULT: checks=0 fails=0`、有参转发真实
二进制」的 stub ⇒ ⇒ **exit 0** ⇒ ⇒ `run_all_ported_tests.sh`（只判 exit 0）**绿**
⇒ ⇒ **SoC 的 2 条 C++ 侧断言全被删掉，判据面无任何反应**。

⇒ ⇒ **这不是缺陷而是过渡态的必然**：蓝图 §7 **R13** 已登记「SoC 的激励是手写过渡态」⇒ ⇒
**C++ 侧条数 ≠ `.sv` 侧 14 是预期** ⇒ ⇒ 但「R13 允许条数不等」**不等于**「条数可以无人守护」
⇒ ⇒ **缺的是「C++ 侧自己的期望条数」这个登记值 + 一条守护它的判据**。

### 3. 为什么这两件事**必须一起做**

⇒ ⇒ 两者都指向**同一个缺口**：「`examples/axi4/` 的每个产品单元，C++ 侧的断言**由谁认定它有多少条**
**又由谁保证它没被删**」⇒ ⇒ ① 提供第三个产品单元（phase3a）⇒ ⇒ ② 提供守护机制
⇒ ⇒ **先做 ② 再做 ①** ⇒ ⇒ ① 产出的新 tb **一落地就被守护**（否则又是一个「没人守的单元」）。

---

## What Changes

### ① 清单新增 `interp_checks` 列 + ⑨ 改造 + **反向判据**

| 项 | 现状 | 改为 |
|---|---|---|
| 清单列 | 13 列（`target` … `note`） | **14 列**（`emitter_args` 之后插入 `interp_checks`）⇒ ⇒ 「清单列号 MUST 从机器可读表头推导」已覆盖插列 ⇒ 无风险 |
| `interp_checks` 语义 | — | **该行「C++ 侧入口」的期望断言条数**（等式）；**无 C++ 侧入口 ⇒ 写 `-` 哨兵** |
| ⑨ 作用域 | note 列含 `interp-entry`（**自由文本约定**） | **`interp_checks` 列非 `-`** ⇒ ⇒ **退役 note 约定**（自由文本承载判据 = 脆弱） |
| ⑨ 等式 | `checks == min_checks`（`.sv` 侧登记值） | `checks == interp_checks`（**C++ 侧登记值**）⇒ ⇒ Lite 行两者都 = 113（不变）· SoC 行 = 2（`min_checks` 仍是 `.sv` 侧的 14） |
| **反向判据（新增）** | — | **`emitter == target`（自发行）⇒ MUST 有 `interp_checks` 且 > 0** ⇒ ⇒ 未来任何「自己发自己」的产品**自动**纳入守护（当前恰好 2 行：Lite + SoC） |

⇒ ⇒ **为什么反向判据可判定**：`emitter == target` ⇔ 该行的发射器**就是**它自己 ⇒ ⇒ 它必然有一个
C++ 侧 `main` ⇒ ⇒ MUST 有一个可被登记的 C++ 侧条数 ⇒ ⇒ 反之（`emitter != target`）⇒ 发射器是**别的**
target ⇒ ⇒ 该行**没有**自己的 C++ 侧入口 ⇒ ⇒ `interp_checks = -` 合法。

⇒ ⇒ **note 约定的退役**：现有 `interp-entry` 只出现在 Lite 行（`:53`）⇒ ⇒ 改列后删该标记
⇒ ⇒ ⚠️ 护栏自检的 fixture 同步（`make_list` 的 3 行也要加列）。

### ② `phase3a_complete` 拆三角色

| 文件 | 处置 | 内容 |
|---|---|---|
| `examples/axi4/phase3a_top.h`（**新建**） | 设计定义 | `Phase3ATop`（7 端口 + 3 个 `ch_module`）⇒ ⇒ **无 `main`** |
| `examples/axi4/tb_phase3a_example.cpp`（**新建**） | 激励驱动 | `main`：写 0xAB → 读回 ⇒ ⇒ **2 条断言** + **`RESULT: checks=2 fails=0`** 协议行 |
| `examples/axi4/phase3a_complete.cpp` | **删除** | 内容已拆走（`git mv` 保留历史 ⇒ ⇒ 拆成两个新文件 + 删原文件） |

⇒ ⇒ **target 改名**：`phase3a_complete` → `tb_phase3a_example`
⇒ ⇒ **CTest 条目名 `spinalhdl_axi4_phase3a` 不变**（`add_spinalhdl_ported_test(axi4_phase3a tb_phase3a_example)`
⇒ ⇒ **改名不是缩面** ⇒ ⇒ CTest 总数**不变**。
⇒ ⇒ `CH_TRACKED_EXAMPLE_TARGETS`（**27 项不变**）+ `install(TARGETS …)` 同名项改名。
⇒ ⇒ `examples/axi4/AGENTS.md` 的 FILES 行 + PATTERNS 段同步。

⇒ ⇒ ⚠️ **本 change 不给 phase3a 加 `--emit-rtl`** ⇒ ⇒ 它**不进**清单 ⇒ ⇒ **不冻结** module 集期望
⇒ ⇒ 理由：phase3a 是 **interconnect 演示**（4×4 矩阵 + 转换器），把它变成「产品面」需要
**新增一份手推的 module 集 / 端口数期望** + verilator lint 门 ⇒ ⇒ 那是**新增产品面**，
不是**收敛** ⇒ ⇒ 登记为 follow-up（见「Out of Scope」）。

---

## Capabilities

| capability | 动作 |
|---|---|
| `examples-rtl-sim-coverage` | **ADDED** 1 条 requirement：「C++ 侧入口的断言条数 MUST 登记且被守护」（含反向判据） |
| `axi-interconnect-routing` | **MODIFIED** 1 条 requirement：「AXI 互连层 MUST 提供低有效复位」—— 仅订正**证据引用**（`phase3a_complete.cpp` 已拆 / `axi_soc_example.cpp` 已改名 ⇒ 行号悬空）⇒ ⇒ 语义条款**一字未改** |

⇒ ⇒ 既有 requirement **不修改**：「C++ 侧 MUST 与生成式 TB 共享同一验证源」**只约束 `.verif` 驱动的产品**
⇒ ⇒ SoC 是**手写激励**（R13 过渡态）⇒ ⇒ **不适用**那条 ⇒ ⇒ 本条**补的是它没覆盖的另一半**
（条数登记 + 守护）⇒ ⇒ 与之**不冲突**。

---

## Impact

| 面 | 变化 |
|---|---|
| 清单 | `.rddf/checks/examples-rtl-sim-targets.tsv` **13 → 14 列**（+`interp_checks`）⇒ Lite = 113 · SoC = 2 · 其余 = `-` |
| 护栏 | `scripts/verify_examples_rtl_sim.sh`：⑨ 作用域改造 + **新增反向判据** + `--self-test` **+2 条投毒** |
| 代码 | `examples/axi4/` **+2 文件**（`phase3a_top.h` · `tb_phase3a_example.cpp`）· **−1 文件**（`phase3a_complete.cpp`） |
| 构建 | target 改名（**条目数不变**）· tracked 名单 / install 同名项改名 |
| 文档 | `examples/axi4/AGENTS.md` · `docs/CHANGELOG.md` |
| 锚点 | **全部不变**（CTest 条目名不变 ⇒ `CTEST_TOTAL` 不变 · 无新增测试源 ⇒ `EXAMPLES_TEST_SOURCES` / `tests/test_*.cpp` 不变） |

---

## 用户可见变更

| 项 | 前 | 后 |
|---|---|---|
| `examples/axi4/phase3a_complete.cpp` | 存在（463 行 · 设计定义 + `main` 一体） | 拆为 `phase3a_top.h`（设计定义）+ `tb_phase3a_example.cpp`（激励驱动） |
| example 二进制名 | `phase3a_complete` | `tb_phase3a_example` |
| CTest 条目 `spinalhdl_axi4_phase3a` | 存在 | **存在（名字不变 ⇒ 非缩面）** |
| `phase3a` 的结论输出 | 散文「…链路仿真通过」 | `RESULT: checks=2 fails=0`（协议行） |
| `phase3a` 收到未知参数 | exit 0（静默忽略） | **exit 2**（默认模式不接受参数） |
| SoC 的 C++ 侧 2 条断言 | **无守护**（全删也绿） | 由 ⑨ 按 `interp_checks=2` 对账 |
| 清单 `.rddf/checks/examples-rtl-sim-targets.tsv` | 13 列 | **14 列**（+`interp_checks`） |
| `--self-test` 投毒数 | 19 | **21** |

---

## Out of Scope

| 项 | 理由 |
|---|---|
| **phase3a 的 RTL 产物覆盖**（`--emit-rtl` + 进清单 + 冻结 module 集 / 端口数期望） | 那是**新增产品面**（新增一份手推期望）而非收敛 ⇒ ⇒ **follow-up**，需独立立项 |
| **`.verif` 原语集扩展**（让 SoC 手写 TB 变成生成物） | ADR-074 **D2 的签批决策变更** ⇒ ⇒ 独立立项（蓝图 §7 R13 的闭口判据依赖它） |
| **SoC 手写激励改 `.verif` 驱动** | 同上（R13 过渡态） |
| **note 列的 `interp-entry` 之外的自由文本判据** | 自由文本承载判据 = 脆弱 ⇒ ⇒ 本 change 把判据搬进**列** |
| **`examples/riscv-mini` 侧的同类收敛** | 另一批 example ⇒ 独立立项 |

---

## 验收标准

| # | 判据 | 方式 |
|---|---|---|
| AC1 | `examples/axi4/phase3a_top.h` 存在且**无 `main`**（`grep -c 'int main'` == 0） | grep |
| AC2 | `examples/axi4/tb_phase3a_example.cpp` 存在 ⇒ 无参运行 ⇒ stdout 含 **`RESULT: checks=2 fails=0`** 且 **exit 0** | shell |
| AC3 | `examples/axi4/phase3a_complete.cpp` **不存在**；target `phase3a_complete` 不存在；`tb_phase3a_example` 存在 | 文件 + cmake |
| AC4 | `ctest -N` 里 **`spinalhdl_axi4_phase3a` 仍在**（条目名不变 ⇒ 非缩面） | `ctest -N` |
| AC5 | `CH_TRACKED_EXAMPLE_TARGETS` == **27** 项（改名 ⇒ 不增不减）；`install(TARGETS …)` 含 `tb_phase3a_example` | grep |
| AC6 | 清单 == **14 列**（`#COLUMNS:` 含 `interp_checks`）；Lite = **113** · SoC = **2** · 其余行 = `-`；**无行**字段数 ≠ 14 | awk |
| AC7 | ⑨ **覆盖 SoC 行**：把 SoC 的 C++ 侧条数改成 ≠ 2 ⇒ ⇒ ⑨ **必红**（**实测**：改前护栏绿 ⇒ 判别力自证） | 投毒 |
| AC8 | **反向判据**：把任一**自发行**（`emitter == target`）的 `interp_checks` 置 `-` ⇒ ⇒ **必红** | 投毒 |
| AC9 | note 列的 **`interp-entry` 标记零残留**（该约定已退役） | grep |
| AC10 | `--self-test` **投毒数 +2**（AC7 + AC8 各一条）且**全部转红** ⇒ ⇒ 还原后复绿 | self-test |
| AC11 | `scripts/verify_examples_rtl_sim.sh` **PASS** · `verify_examples_rtl_sim_coverage.sh` **PASS** · `verify_example_test_wiring.sh` **PASS** · `ctest` **全绿** · `run_all_ported_tests.sh` **PASS 27** | shell |
| AC12 | spec delta = `examples-rtl-sim-coverage` 的 **ADDED 1 条**（`openspec validate --strict` 通过） | openspec |
