# 验收报告：`p6-hierarchy-friendliness-diagnosis-2026-11`（P6 · 层次友好度诊断 API）

- **验收日期**：2026-11-06
- **验收角色**：verifier（**builder 的角色内建完成门** · 非 subagent）
- **结论**：✅ **通过**（AC 逐条达成 · 零回归全绿 · **critic 2 轮后 APPROVE**）

## 1. 判据来源（按优先级）

| 优先级 | 来源 | 内容 |
|---|---|---|
| 1 | **用户本次显式拍板** | ① 项 4 走 **A1**（`std::optional` + 独立预扫描）② 3 档名 `kClean`/`kNeedsWork`/`kNotAnalyzable` ③ 5 项报告面**不收窄** |
| 2 | `proposal.md` §Acceptance | 11 条摘要判据 |
| 3 | 本次变更自己声明的行为 | `tasks.md` 41 条（T1–T6） |
| 4 | capability delta `hierarchy-friendliness-diagnosis` | **ADDED ×3**（3 条 Requirement，各带 Scenario） |

⚠️ **本地验收的额外门**：`.rddf/state/iteration.json` **不存在** ⇒ 该门不适用 ⇒ 以 tasks.md 勾选状态为准（**41/41 全勾**）。

## 2. 逐条判定与证据

### 2.1 delta 的 3 条 Requirement

| Requirement | 判定 | 证据 |
|---|---|---|
| **R1 诊断入口 MUST 存在且只读** | ✅ | `ch::diagnose_hierarchy(Component&)`（`include/hierarchy_analysis.h`）· 实测「只读」判据 = **全节点 `(num_srcs, src(i)…)` 快照前后相同** + 节点数相同（用例 `[readonly]`）⇒ ⇒ 比「只比节点数」强（能抓「改写 src」） |
| **R2 报告 MUST 给出可判定的 5 项事实** | ✅ | 5 项**全部** `std::optional` ⇒ 「未测得」与「实测为 0」**结构上可区分**（`to_text()` 输出 `<unmeasured>`）· 实测：`implicit_edge_count` / `synthesized_port_count` / `dangling_input_count` / `unconsumed_child_output_count` / `cross_module_mem_count` 各有 DUT 取到**非零值** |
| **R3 诊断 MUST 与发射器同源 + 确定性快照** | ✅ | 3 个原语（`synth_port_key` / `implicit_input_synthesizable` / `synth_width_ok`）在 `src/hierarchy_synth_count.h` 由**两侧共用**· 实测「诊断的端口数 == 发射产物里 `origin==Synthesized` 且端口名 `^__(x\|if)_` 的条数」（含第三轴/上穿 DUT ⇒ 两侧**都** 0）· `to_text()` 两次调用**逐字节相同** |

### 2.2 用户拍板项

| 拍板 | 判定 | 证据 |
|---|---|---|
| **A1**：项 4 = `std::optional` + 独立预扫描 | ✅ | `[not-analyzable][mem]` 用例：不可分析时 `cross_module_mem_count` **有值且 ≥ 1**（**非** `nullopt`）· 其余 4 项 `nullopt` |
| **3 档**：`kClean`/`kNeedsWork`/`kNotAnalyzable` | ✅ | 枚举定义 + 3 种情形均有用例覆盖 |
| **5 项不收窄** | ✅ | 5 项 + 各层规模（3 个数分开给）全部在报告里 |

### 2.3 判别力（变异对照 · 6 条）

| 变异 | 期望 | 实测 |
|---|---|---|
| M1 `synthesized_port_count` 恒 0 | 红 | ✅ 转红（46/2） |
| M2 项 3 不并 `pending_implicit_edges` | 红 | ⚠️ **仍绿 ⇒ 无效变异**（如实登记：该分支在当前 DUT 集下**不可达** ⇒ **不是**判据缺口） |
| M3 `verdict` 恒 `kClean` | 红 | ✅ 转红（44/1） |
| M4 `Record` → `Reject` | 红 | ✅ 转红（33/3） |
| M5 `details` 只取局部名 | 红 | ✅ 转红（46/2） |
| **M6 去掉路由门**（BLOCKER 1 的缺陷形态） | 红 | ✅ 转红（52/1）⇒ **证明 BLOCKER 1 的修复有判别力** |
| 还原 | 绿 | ✅ sha256 相同 + 重建 + 复绿（58 断言 / 13 用例） |

### 2.4 零回归（返工后重跑）

| 项 | 结果 |
|---|---|
| `ctest -LE perf` | **241/241**（`100% tests passed, 0 tests failed out of 241`） |
| samples | **39/39** |
| examples | **28/28** |
| `verify_hierarchical_rtl.sh`（R1–R8） | PASS |
| 「既有 3 产物逐字节不变」 | ✅ `DIFF_COUNT = 0`（对照 HEAD · sha256 + 全量重建 + 护栏 PASS） |
| `verify_hierarchical_p8.sh` / `port_naming` / `header_self_contained` / `test_counts` / `openspec_delta` / `roadmap_fragments` / `dual_backend_parity` / `agents_size` | 全 PASS |
| 计数锚点 | 252 / 76 / 66 / 289（parity 两锚点**不变**） |

### 2.5 文档

| 项 | 判定 | 证据 |
|---|---|---|
| 用法指南 §5.4-bis | ✅ | 显式 include + 5 项含义 + 3 档 + **4 条边界**（粒度相关 / FATAL 副作用 / 静态析构期 / 不覆盖体内无驱动的子输出） |
| 示例可编译 | ✅ | `g++ -std=c++23 -fsyntax-only` **0 错误** |

## 3. 失败项与路由

**无失败项**。

⚠️ **两处如实登记的缺口**（**不影响通过**，但 MUST 让用户知道）：

| # | 缺口 | 判定依据 |
|---|---|---|
| 1 | **BLOCKER 2 的修复（null 守卫）无 DUT 覆盖** —— 构造「组件挂在 root 子树外 + 节点同 ctx」需仓内无先例的手工 build | critic 第 2 轮判定：「守卫对 `nullptr` 路径是**无条件**覆盖（不是概率性修复）⇒ 不影响闭合」⇒ ⇒ 只由代码审查验证 |
| 2 | **变异 M2 是无效变异** | 该分支在当前 DUT 集下不可达（`pending_implicit_edges.from_node` 恒是**子内部节点** ⇒ 不进 ports 列表）⇒ ⇒ **规则保留**（防御性）· 「兄弟边消费」DUT 记 backlog |

## 4. 遗留清扫（本次范围内）

| 项 | 结论 |
|---|---|
| TODO / FIXME / XXX / HACK / 待补 | **0** |
| 临时调试输出 | **0**（`tests/hier_rtl_products.cpp` 的 `std::cout` 是**正常成功输出**，非本次新增） |
| 变异残留标记 | **0**（`grep M5:/M6:/MUTANT` 在 `include/` `src/` 均 0 命中 —— 变异脚本已逐条还原并 sha256 校验） |
| 半截实现 / 注释掉的代码 | **0** |
| 被 skip 的测试 / 未替换的 mock | **0** |
| 临时文件落在仓内 | **0** |
| 范围外历史遗留 | **只记录不清理**（主 spec Purpose 的 R1–R7 措辞 / R8 硬编码 `top` ⇒ 与本 change 无关） |

## 5. 复现命令

```bash
cd /workspace/project/CppHDL
set -euo pipefail
cmake --build build -j"$(nproc)"
./build/tests/test_hierarchy_friendliness_diagnosis          # 13 用例 / 58 断言
( cd build && ctest -LE perf --output-on-failure )            # 241/241
bash scripts/verify_hierarchical_rtl.sh && bash scripts/verify_hierarchical_rtl.sh --self-test
bash scripts/run_all_samples.sh && ./run_all_ported_tests.sh
openspec validate p6-hierarchy-friendliness-diagnosis-2026-11 --type change --strict
```

## 6. 已用轮次

- **验收轮次**：1 / 3
- **critic 轮次**：**2 / 2**（第 1 轮 REJECT ⇒ 2 BLOCKER 返工 ⇒ 第 2 轮 APPROVE，只验闭合）
- **失败回环**：0 次 `implementation_gap` / `proposal_drift`（critic 的 2 个 BLOCKER 属**实现与 delta 不一致** ⇒ 已在同一 change 内返工闭合）
