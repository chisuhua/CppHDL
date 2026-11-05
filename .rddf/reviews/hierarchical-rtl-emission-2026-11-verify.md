# 验收报告：`hierarchical-rtl-emission-2026-11`（T3-④ 层次发射）

- 日期：2026-11-05
- 验收范围：change 全量（13 个批次 B1 / B2a / B2b-1..6 / T0h / B3a / B3b / B3c / B4）
- 已用轮次：**1 / 3**（`RDDF_VERIFIER_MAX_LOOPS` 对齐）
- 结论：**PASS**（无失败项）；残留边界如实登记于 §4

## 1. 判据来源（按优先级）

1. 用户本次约束：每批固定验证门（全量重建 · `ctest -LE perf` · 扁平外部基线 `cmp` ·
   层次化产物 `verilator --lint-only` · 6 护栏 + samples + examples · 计数锚点不变 ·
   `openspec validate --strict`）；每条判据 MUST 有变异对照；提交按路径显式 add；
   归档**禁** `--skip-specs`。
2. `proposal.md` §Acceptance（归档后的副本：
   `openspec/changes/archive/2026-11-05-hierarchical-rtl-emission-2026-11/proposal.md`）。
3. capability delta 11 条 ADDED Requirement（已合并进
   `openspec/specs/rtl-hierarchical-output/spec.md`，3 → **14** Requirements）。
4. ADR-058 §5（结构判据）· ADR-060 §2.5（lint 门的可执行定义）/ §5（G0–G3）·
   ADR-062 §5（接口白名单）· ADR-061 v1.6.3（粒度作用域）。

## 2. 逐条判定与证据

| # | 判据 | 结果 | 证据（工具 / 命令） |
|---|---|---|---|
| 1 | 全量重建无错误 | PASS | `cmake --build build -j$(nproc)` → `BUILD_DONE`，`grep -E "error:"` 0 行 |
| 2 | `ctest -LE perf` 全绿 | PASS | `ctest --test-dir build -LE perf` → **238/238** |
| 3 | 层次化单测 | PASS | `./build/tests/test_hierarchy_verilog` → **28 用例 / 495 断言**（B4 前 27/378） |
| 4 | 分析层 golden 逐字节不变 | PASS | `tests/test_hierarchy_analysis`（4 份 golden，含其中 1 份手推）随 ctest 全绿 |
| 5 | 扁平外部基线 `cmp` 全 0 | PASS | 用当前 `libcpphdl.a` 重编 `/tmp/opencode/vbase/gen.cpp` → `out_mem.v` / `out_regchain.v` / `out_rv32i.v` 与 `final8/` **cmp 全 0** |
| 6 | 产物级护栏 R1–R7 | PASS | `bash scripts/verify_hierarchical_rtl.sh` → R1..R7 全绿 |
| 7 | 护栏判别力（投毒） | PASS | `--self-test` → **9/9 条对照**（8 fixture + 1 配对断言） |
| 8 | 判据不重复（R3 ≠ R1） | PASS | 投毒端口宽度 8→9：**R1 仍绿**（`lint_off` 生效）、**R3 转红**（剥掉 `lint_off`） |
| 9 | 结构判据不被 lint 覆盖 | PASS | 删一条实例化语句：**R1 仍绿**、**R6 转红**（树少一条边） |
| 10 | 探针矩阵有上界且每格有断言 | PASS | `[matrix]`：36 格 / 16 Ok / 15 Reject / 5 Na；证据 tag 在本文件内机械校验 |
| 11 | 计数锚点不变 | PASS | `scripts/verify_test_counts.sh` → PASS（`CTEST_TOTAL 249` / `tests/test_*.cpp 141`；新增生成器**不是** ctest 用例） |
| 12 | 既有护栏零回归 | PASS | `verify_component_naming` · `verify_header_odr` · `verify_dual_backend_parity` · `verify_roadmap_fragments` · `verify_agents_size` · `verify_hierarchical_tree_extract` 全 PASS |
| 13 | samples / examples | PASS | `run_all_samples.sh` 全绿 · `run_all_ported_tests.sh` **28 PASS / 0 FAIL** |
| 14 | OpenSpec 合法 + 归档 | PASS | `openspec validate rtl-hierarchical-output --type spec --strict` → valid；`openspec archive`（**无 `--skip-specs`**）→ `+11 / ~0 / -0` |
| 15 | 文档无陈旧计数 | PASS | design/tasks/proposal/feature/AGENTS 订正（批次表 13/13 · ADDED ×11 · 错误载体口径 · CI 落点 `pr-verilator-fast`） |

## 3. 失败项与路由

无。

## 4. 遗留清扫（本次范围内）

| 项 | 结果 |
|---|---|
| TODO / FIXME / HACK / DEBUG | 新增的 3 个文件（`scripts/verify_hierarchical_rtl.sh` · `tests/hier_rtl_products.cpp` · 单测增量）**零命中** |
| 注释掉的代码 | 无 |
| 临时调试日志 | 生成器的 `products-ok` / `reject-ok` 是**护栏消费的输出**（不是调试残留）；INFO 日志来自框架 logger，既有行为 |
| 被 skip 的测试 | 无新增（未引入 `SKIP()`） |
| 未替换的 mock | 无仓内 mock；`--self-test` 的假生成器建在 `mktemp` 目录里，随 `trap` 清理 |
| 临时 fallback / 半截实现 | 无 |

### 4.1 如实登记的判别力边界（不是遗留，是**已知且已记录**的边界）

1. **R1–R7 全绿 ≠ 行为正确**：本 change **不做**逐拍等价性 ⇒ 归 **T3-⑤**
   （`hierarchical-rtl-equivalence-2026-11`）。
2. **R7 只解析字面量路径实参**：`path` 变量实参静态不可判 ⇒ 由「层次化产物走独立输出目录」
   的约定承担；适用面 = 仓内调用点（MUST NOT 扩成「扫描所有 `.v`」）。
3. **R2 / R3 / R6 的判别力依赖 verilator**（这是**有意**选择：MUST NOT 自研 Verilog 解析器）。
4. **接口白名单守卫的接入点不可达**：四类违规当前没有公开 API 路径 ⇒ 「删掉调用」这条变异
   **无判据**（B3c 已登记；覆盖 = 决策表单测 + 与 G2 同段的代码位置审查）。
5. **`UndrivenPolicy::Reject` / 冒泡的值语义**：值判据归 T3-⑤（B3a 只做连通性 +
   零父层引用的结构判据）。

## 5. 复现命令

```bash
cd /workspace/project/CppHDL
cmake --build build -j"$(nproc)"
ctest --test-dir build -LE perf --output-on-failure
bash scripts/verify_hierarchical_rtl.sh
bash scripts/verify_hierarchical_rtl.sh --self-test
openspec validate rtl-hierarchical-output --type spec --strict
bash scripts/verify_test_counts.sh
bash scripts/run_all_samples.sh && ./run_all_ported_tests.sh
```
