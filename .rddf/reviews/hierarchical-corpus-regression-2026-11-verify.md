# 验收报告：`hierarchical-corpus-regression-2026-11`（残留 1b · 层次化路径的语料级回归）

- **验收日期**：2026-11-07
- **验收角色**：verifier（**builder 的角色内建完成门** · 非 subagent）
- **结论**：✅ **通过**（AC 逐条达成 · 零回归全绿 · **critic 2 轮：第 1 轮 `REJECT`（1 BLOCKER）⇒ 返工 ⇒ 第 2 轮 `APPROVE_WITH_CONDITIONS`（0 BLOCKER）**）

## 1. 判据来源（按优先级）

| 优先级 | 来源 | 内容 |
|---|---|---|
| 1 | **用户本次显式拍板** | 口径 = **X1**（T0 产物级 + T1 行为级 + 显式登记口径差）⇒ ⇒ **MUST NOT** 单方面改台账 |
| 2 | `proposal.md` §Acceptance | 6 条摘要判据 |
| 3 | 本次变更自己声明的行为 | `tasks.md` **32 项**（T1–T5） |
| 4 | capability delta | `hierarchical-corpus-regression`（**ADDED ×3**） |

⚠️ **本地验收的额外门**：`.rddf/state/iteration.json` **不存在** ⇒ 该门不适用 ⇒ 以 tasks.md 勾选状态为准（**32/32 全勾**）。

## 2. 逐条判定与证据

### 2.1 delta 的 3 条 Requirement

| Requirement | 判定 | 证据 |
|---|---|---|
| **R1 带子组件 DUT MUST 被覆盖或显式豁免** | ✅ | 护栏分析器扫 `tests/`+`examples/`+**`include/`** ⇒ 输出「登记 **10** · 复合 DUT **10** · 用例 **10**」⇒ ⇒ 与清单**一致**；**三向**双向核对（① 登记→用例 ② 用例→登记 ③ ★ 语料→登记 ③' 豁免项 MUST 确实在语料里）⇒ ⇒ **5 条投毒全红**（含**唯一**能抓「新增未登记的复合 DUT」的 ③） |
| **R2 覆盖 MUST 登记判别力上限** | ✅ | `tests/hier_corpus_registry.md` 逐 DUT 记录：恒相关信号（`CsrBankDut` 的 `interrupt_req`）· 顶层只有 1 个 output · 纯组合（子组件 0 reg）· 已知 1 拍错位（`ICachePrefetch`）· **不可**层次化（`ImmConsistencyDut`）· 122 叶子的**结构性零判别力** |
| **R3 覆盖 MUST 由机器核验且 MUST 自证有判别力** | ✅ | 护栏**已接进 CI**（纯静态 ⇒ 与分片无关 ⇒ 每分片都跑）+ `--self-test` **5 条投毒**（① 删一行 · ② 加不存在的 DUT · ③ 新增未登记的复合 DUT · ④ 豁免路径可用 ⇒ **绿** · ⑤ 豁免不存在的 DUT）⇒ ⇒ 全部符合期望 + 还原复绿 |

### 2.2 用户拍板项（口径 X1）

| 拍板 | 判定 | 证据 |
|---|---|---|
| **X1**（双层 + 显式登记口径差） | ✅ | T0 **9** 个 + T0 拒绝边界 **1** 个 + T1 **3** 个结构类 ⇒ ⇒ **10 个复合 DUT 全部被登记**；口径差**已登记**（tasks.md §B + §C-bis）⇒ ⇒ ★ 台账措辞归 architect ⇒ ⇒ **本 change 未改台账** ⇒ ⇒ 归 architect backlog |

### 2.3 判别力

**（a）护栏 `--self-test` 的 5 条投毒 ⇒ 全部符合期望 + 还原复绿**

| # | 投毒 | 方向 | 实测 |
|---|---|---|---|
| ① | 登记表**删一行** | ① + ② | ✅ 红 |
| ② | 登记表**加一个不存在的 DUT** | ① | ✅ 红 |
| ③ | ★ **新增一个未登记的复合 DUT**（合成语料 · **不**污染仓库） | **只有 ③** | ✅ 红 |
| ④ | ★ **豁免路径可用**（未登记 + 无用例 + 已豁免） | 验证 delta 的第二条出路 | ✅ **绿** |
| ⑤ | **豁免一个不存在的 DUT** | ③' | ✅ 红 |

**（b）实现的变异 ⇒ 1 条系统级转红 + 5 条检测器级如实登记为无效**

| 变异 | 层级 | 实测 | 判定 |
|---|---|---|---|
| **S1** | ★ **系统级**：破坏 `Granularity::ForceLayer` 的语义（`src/component.cpp` 的 `is_layer()`）⇒ ⇒ 层次化产物不再分层 | ✅ **转红**（`121 assertions / 3 failed`） | ⇒ ⇒ **证明「发射器 → fixture → 断言」整条链有判别力** |
| M1–M4 | 检测器级：把 fixture 的自报恒置为「好」 | ❌ 恒绿 | ⇒ ⇒ **无效变异**（★ 测试断言的就是 fixture 的**自报** ⇒ ⇒ 「谁来测测试」的**自指限制**）⇒ ⇒ **如实登记** |
| M5 | 护栏级：去掉第 ③ 个方向 | ❌ 恒绿 | ⇒ ⇒ **无效变异**（★ 它是投毒 ③ 的**反向对照** ⇒ ⇒ ⇒ 证明 ③ 是抓住该形态的**唯一**机制） |

⇒ ⇒ 还原后：`tests/hier_corpus_fixture.h` · `scripts/verify_hier_corpus_coverage.sh` ·
`src/component.cpp` **sha256 逐字节相同** + **重建成功** + **复绿**。

### 2.4 零回归

| 项 | 结果 |
|---|---|
| `ctest -LE perf` | **242/242**（返工后重跑） |
| samples / examples | **39/39** · **28/28** |
| 计数锚点 | **全部不变**：`CTEST_TOTAL` **253** · `VERILATOR_E2E_TOTAL` **77** · parity **66 / 289** ⇒ ⇒ ⇒ 新用例进**既有** target ⇒ ⇒ **不**产生新 ctest 条目 ⇒ ⇒ ⇒ **零**锚点变动 |
| 护栏 | `verify_hier_corpus_coverage` · `verify_hierarchical_p8` · `verify_dual_backend_parity` · `verify_test_counts` · `verify_openspec_delta` · `verify_roadmap_fragments` · `verify_agents_size` **全 PASS**（+ 两个 `--self-test` PASS） |
| 既有 DUT 的**类定义** | **零改动**（只新增用例）⇒ ⇒ 既有 289 处 `DUAL_BACKEND_TEST_PAIR` 调用**零改动** |

### 2.5 ⚠️ **实施期的两处实测订正**（如实登记）

1. **`ImmConsistencyDut` 不可层次化**：planner 普查说它是 T1 的 C1 类代表 ⇒ ⇒ **实测**它的
   `equal` 输出依赖两个子组件的**非端口**内部线网 ⇒ ⇒ 需隐式端口合成 ⇒ ⇒ 发射器不支持
   ⇒ ⇒ ⇒ 改登记为**拒绝边界**样本（用例把**拒绝原因**钉住）⇒ ⇒ 「双子组件各自带寄存器」
   这一结构类**无**可用样本 ⇒ ⇒ **backlog**（★ 不得造 DUT 凑数 ⇒ ⇒ 那会回到 T3-005 的
   「专用 DUT」形态 ⇒ ⇒ 违背 change 前提）。

2. **4 条检测器级变异恒绿** ⇒ ⇒ 那是「谁来测测试」的**自指限制** ⇒ ⇒ **如实登记为无效变异**
   ⇒ ⇒ 判别力改由**系统级**变异（S1）承担。

### 2.6 遗留清扫（本次范围内）

| 项 | 结论 |
|---|---|
| TODO / FIXME / XXX / HACK | **0** |
| 变异残留标记 | **0**（`grep "M1:"`/`"S1:"` 在 `tests/` `src/` **零命中** ⇒ 变异脚本已逐条还原并 sha256 校验） |
| 半截实现 / 注释掉的代码 | **0** |
| 被 skip 的测试 / 未替换的 mock | **0**（T1 在 verilator 不可用时 **WARN + return** ⇒ ★ **不是** skip ⇒ ⇒ 与 `DUAL_BACKEND_TEST_PAIR` 的既有约定**同款** ⇒ ⇒ macOS/Windows 的 `-DBUILD_VERILATOR=OFF` 本地开发**不会**新增硬失败） |
| 临时文件落在仓内 | **0**（合成语料文件只落在 `mktemp -d` ⇒ ⇒ **不**污染仓库） |
| **范围外历史遗留** | ⚠️ **只记录不清理**：① 122 个叶子 DUT 未覆盖（**结构性零判别力**）② `Rv32iPipelineTop`（已被 T3-005 覆盖 + TCM 4 MB 展开）③ `CounterWide<96>`（>64bit ⇒ Verilator 合法拒绝）④ **lint 口径的复制**（R1 与 fixture 两处 ⇒ ⇒ 抽共享 helper 记为后续项）⑤ **③' 的键控不一致**（`::` 限定的复合 DUT ⇒ 合法豁免会被误杀 ⇒ 当前语料零限定名调用）⑥ 「豁免 MUST 附**具体**原因」**一半**无机器核验 |

## 3. 复现命令

```bash
cd /workspace/project/CppHDL
set -euo pipefail
cmake --build build -j"$(nproc)"
( cd build && ctest -LE perf --output-on-failure )            # 242/242
bash scripts/run_all_samples.sh                               # 39/39
./run_all_ported_tests.sh                                     # 28/28
bash scripts/verify_hier_corpus_coverage.sh                   # 三向双向核对
bash scripts/verify_hier_corpus_coverage.sh --self-test       # 5 条投毒
bash scripts/verify_hierarchical_p8.sh
bash scripts/verify_dual_backend_parity.sh
bash scripts/verify_test_counts.sh
openspec validate hierarchical-corpus-regression-2026-11 --type change --strict
bash scripts/verify_openspec_delta.sh
```

## 4. 已用轮次

- **验收轮次**：1 / 3
- **critic 轮次**：**2 / 2**（第 1 轮 `REJECT`（**1 BLOCKER**：豁免机制不可用）⇒ 返工 ⇒
  第 2 轮 `APPROVE_WITH_CONDITIONS`（**0 BLOCKER** ⇒ ⇒ 不触发进一步返工/复审））
- **失败回环**：1 次 `implementation_gap`（BLOCKER = 实现与 delta 契约不一致 ⇒ 已在同一 change 内返工闭合）
