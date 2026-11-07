# 验收报告：`p8-hierarchical-equivalence-public-helper-2026-11`（P8 · 等价性 helper 公共化）

- **验收日期**：2026-11-07
- **验收角色**：verifier（**builder 的角色内建完成门** · 非 subagent）
- **结论**：✅ **通过**（AC 逐条达成 · 零回归全绿 · **critic 1 轮 `APPROVE_WITH_CONDITIONS`（0 BLOCKER）**）

## 1. 判据来源（按优先级）

| 优先级 | 来源 | 内容 |
|---|---|---|
| 1 | **用户本次显式拍板** | Q1 = **X1**（口径差显式登记）· Q2 = **P8 先做** · Q3 = `include/hierarchy_equivalence.h` + 命名空间 `ch::hier_eq` |
| 2 | `proposal.md` §Acceptance | 8 条摘要判据 |
| 3 | 本次变更自己声明的行为 | `tasks.md` **62 项**（T1–T8） |
| 4 | capability delta | `hierarchical-equivalence-public-helper`（**ADDED ×4**）+ `dual-backend-test-coverage`（**MODIFIED ×1**：登记新单后端条目） |

⚠️ **本地验收的额外门**：`.rddf/state/iteration.json` **不存在** ⇒ 该门不适用 ⇒ 以 tasks.md 勾选状态为准（**62/62 全勾**）。

## 2. 逐条判定与证据

### 2.1 delta 的 4 条 Requirement

| Requirement | 判定 | 证据 |
|---|---|---|
| **R1 公共面 MUST 是闭合集合** | ✅ | 公共头声明 **7 个符号**（`DrivenInput` · `Observation` · `ProductSet` · `Result` · `emit` · `compare` · `make_observation`）⇒ ⇒ 护栏 **R2''** 用 python 枚举并与白名单**双向**比对 ⇒ ⇒ 实测 `✅ R2'' 公共头的声明符号集 ≡ 闭合集合（7 项 · 双向核对）` |
| **R2 fail-closed + 错误可行动** | ✅ | 10 个稳定前缀（`empty-observations` / `empty-stimulus` / `no-varying-signal` / `emit-failed` / `port-set-empty` / `port-width-64-exceeded` / `no-top-level-output` / `backend-init-failed` / `verilator-unavailable` / `observation-not-visible`）⇒ ⇒ `tests/test_public_equivalence_api.cpp` 的 5 用例**逐条**断言前缀；★ `verilator-unavailable` 的用例**真跑**（`setenv("PATH", ...)` 掏空 PATH）⇒ ⇒ 断言「含 `cmake --build build --target verilator`」+「**不含** `apt install verilator`」 |
| **R3 MUST 声明判别力边界** | ✅ | 公共头 + 文档 §5.4-ter 的**互补对照表**（本 helper vs 双后端 parity）+ 观测盲区 + 观测面 MUST 手写 + 单拍豁免的边界 |
| **R4 MUST NOT 暴露内部实现** | ✅ | 护栏 **R2**（6 个 deny 符号 · **双向核对**「仍存在于代码库」）+ **R2'**（声明 `namespace ch::hier_eq` 的文件**恰好一个**）+ **R1'**（公共头的 `#include` 图里**无**内部实现头） |

### 2.2 用户拍板项

| 拍板 | 判定 | 证据 |
|---|---|---|
| **Q1 = X1**（口径差显式登记） | ✅ | `design.md` §1.3 的 T0/T1 双层 + `proposal.md` §3.6 的三选项（X1/X2/X3）⇒ ⇒ **如实登记**「台账 §7 残留 1b 的『行为覆盖』原文 vs 本 change 的双层口径」⇒ ⇒ 台账措辞归 architect（**未**单方面改） |
| **Q2 = P8 先做** | ✅ | 本次只做 P8；残留 1b 的草稿 v2 已就位（`.rddf/improvements/hierarchical-corpus-regression-2026-11.md`） |
| **Q3 = 头名/命名空间** | ✅ | `include/hierarchy_equivalence.h` + `ch::hier_eq` ⇒ ⇒ **头名 ≠ 命名空间**已在公共头注释 + 文档 + AGENTS.md 三处写明 |

### 2.3 判别力（**两套自证**）

**(a) 护栏 `--self-test` 的 7 条投毒 ⇒ 全部转红 + 还原复绿**

| # | 投毒 | 抓它的判据 | 实测 |
|---|---|---|---|
| ① | 内部实现符号泄漏 | R2 | ✅ 红 |
| ② | 公共头**改名** | R2'（内容）+ R3（存在） | ✅ 红 |
| ③ | 公共头**缺失** | R3 | ✅ 红 |
| ④ | **多出第 2 个公共头**（名字**不**命中旧模式） | R4②（活账） | ✅ 红 |
| ⑤ | 公共头 `#include` 内部实现头 | R1'（include 图） | ✅ 红 |
| ⑥ | 公共面**多**一个符号 | R2'' | ✅ 红 |
| ⑦ | 活账登记了**不存在**的头 | R4②（反向） | ✅ 红 |
| — | 还原 | — | ✅ 复绿 |

⚠️ **实施期发现的一个真缺口（已修）**：自检最初让**每条投毒只跑单条检查** ⇒ ⇒ 投毒② **恒绿**（R1 的 ∀ 否定在「找不到匹配文件」时**真空通过**）⇒ ⇒ 改成聚合 `judge()` 才转红 ⇒ ⇒ 已落成脚本结构 + 注释记录。

**(b) 实现的 5 条变异 ⇒ 全部转红 + 逐字节还原 + 重建 + 复绿**

| # | 变异 | 判据归属 | 实测 |
|---|---|---|---|
| M1 | `emit` 端口集合改**手写** | 「端口集合必须是**派生**的」 | ✅ 红（`10 / 5 failed`） |
| M2 | **用户侧**输入**未被驱动** | 「观测恒定 ⇒ `no-varying-signal`」 | ✅ 红（`33 / 1 failed`） |
| M3 | `compare` 恒 `equal=true` | 「篡改产物 MUST 转红」 | ✅ 红（`411 / 1 failed`） |
| M4 | 去掉 `no-varying-signal` | 同上 | ✅ 红（`41 / 1 failed`） |
| M5 | 去掉 `observation-not-visible` | 同上 | ✅ 红（`42 / 1 failed`） |

⇒ ⇒ 还原后 **4 个源文件 sha256 逐字节相同** + 重建成功 + 两套用例复绿。

### 2.4 零回归

| 项 | 结果 |
|---|---|
| `ctest -LE perf` | **242/242**（`100% tests passed, 0 tests failed out of 242`） |
| samples | **39/39** |
| examples | **28/28** |
| 既有 `test_hierarchy_equivalence` | **9 用例 / 414 断言**（断言集合与期望值**不变** ⇒ 仅 7 处 `trace`→`flat_trace` 机械改名） |
| 既有 3 个层次化产物 | ✅ **逐字节不变**（`bubble.v` / `row1_implicit.v` / `three_layers.v` ⇒ 对照 HEAD 的 `cmp` = **0**） |
| 计数锚点 | `CTEST_TOTAL` 252→**253** · non-perf 241→**242** · `VERILATOR_E2E_TOTAL` 76→**77** · parity **66/289 不变** |
| 护栏 | p8 / header_self_contained / header_odr / test_counts / agents_size / openspec_delta / roadmap_fragments / hierarchical_rtl / component_naming **全 PASS** |

### 2.5 文档

| 项 | 判定 | 证据 |
|---|---|---|
| §5.4-ter 用户面小节 | ✅ | 显式 include + 头名≠命名空间 + 端口口径表 + 10 个错误前缀 + 互补对照表 + 边界（A 侧固定 / ctx 绑定 / Linux-only / 无超时 / 不可重入） |
| 示例**自包含可编译** | ✅ | `g++ -std=c++23 -fsyntax-only` **0 错误**（含 `::mkdir(work_dir)`） |
| 示例**由真实测试源文件承载** | ✅ | `tests/test_public_equivalence_api.cpp`（仓外视角 · 真编译+链接+运行） |

## 3. ⚠️ 一处**归档后才转绿**的护栏（如实登记）

`scripts/verify_dual_backend_parity.sh` 在**归档前**读**主 spec**（`openspec/specs/dual-backend-test-coverage/spec.md`）
⇒ ⇒ 本 change 的登记在 **delta** 里（`MODIFIED ×1`）⇒ ⇒ **归档时 CLI 合并** ⇒ ⇒
⇒ **归档前该护栏红（`unclassified-labeled-entry: test_public_equivalence_api`）⇒ 归档后绿**。

⚠️ **这不是新引入的漏洞** ⇒ ⇒ 它是「**spec 变更 MUST 走 change 流程**」的**必然代价**
（★ 同一形态在 T3-005 登记 `test_hierarchy_equivalence.cpp` 时也发生过）
⇒ ⇒ **已实测**：归档后该护栏 **PASS**（见 §5）。

## 4. 遗留清扫（本次范围内）

| 项 | 结论 |
|---|---|
| TODO / FIXME / XXX / HACK / 待补 | **0** |
| 临时调试输出 | **0**（`std::printf` 只出现在**文档示例**与**测试的错误分支**） |
| 变异残留标记 | **0**（`grep "M1:"`/`"M2:"` 等在 `include/` `src/` **零命中** —— 变异脚本已逐条还原并 sha256 校验） |
| 半截实现 / 注释掉的代码 | **0** |
| 被 skip 的测试 / 未替换的 mock | **0**（`verilator_e2e` 用例在 verilator 缺失时走**明确失败**而非 skip ⇒ 见 D5 的判据） |
| 临时文件落在仓内 | **0** |
| **范围外历史遗留** | ⚠️ **1 项已修 + 1 项只记录**：<br>• **已修**：`tests/test_hierarchy_verilog.cpp` 的 `kOutDir` 硬编码且**从不创建** ⇒ 干净树 **26 条断言全红**（`git stash` 到 HEAD **同样复现** ⇒ 既有缺陷）⇒ 修它是因为**它让零回归门不成立**（改动面只限建目录）<br>• **只记录**：`include/` 下公共头的**聚合器归属不统一**（部分不在 `ch.hpp` 里）⇒ **既有状态** ⇒ 本 change **只**保证新头与惯例一致（不进聚合器）|

## 5. 复现命令

```bash
cd /workspace/project/CppHDL
set -euo pipefail
cmake --build build -j"$(nproc)"
./build/tests/test_public_equivalence_api          # 5 用例 / 43 断言
./build/tests/test_hierarchy_equivalence           # 9 用例 / 414 断言
( cd build && ctest -LE perf --output-on-failure )  # 242/242
bash scripts/run_all_samples.sh                     # 39/39
./run_all_ported_tests.sh                           # 28/28
bash scripts/verify_hierarchical_p8.sh              # R1–R4（含 R1'/R2''）
bash scripts/verify_hierarchical_p8.sh --self-test  # 7 条投毒
bash scripts/verify_hierarchical_rtl.sh
bash scripts/verify_header_self_contained.sh
bash scripts/verify_header_odr.sh
bash scripts/verify_test_counts.sh
bash scripts/verify_dual_backend_parity.sh          # ⚠️ 归档后才是 PASS
openspec validate p8-hierarchical-equivalence-public-helper-2026-11 --type change --strict
bash scripts/verify_openspec_delta.sh
```

## 6. 已用轮次

- **验收轮次**：1 / 3
- **critic 轮次**：**1 / 2**（`APPROVE_WITH_CONDITIONS` · **0 BLOCKER** ⇒ ⇒ 不触发返工与复审；2 SHOULD_FIX + 4 NIT **已全部处置并重跑验证门**）
- **失败回环**：0 次 `implementation_gap` / `proposal_drift`
