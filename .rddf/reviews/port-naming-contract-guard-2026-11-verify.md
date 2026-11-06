# 验收报告：`port-naming-contract-guard-2026-11`（残留 5 · 端口命名契约护栏）

- **验收日期**：2026-11-06
- **验收角色**：verifier（**builder 的角色内建完成门** · 非 subagent）
- **结论**：✅ **通过**（AC 逐条达成 · 零回归全绿 · 判别力 6/6 具备）

## 1. 判据来源（按优先级）

| 优先级 | 来源 | 内容 |
|---|---|---|
| 1 | **用户本次显式拍板**（本会话） | ① 残留 5 的 N2b 取 **B1**（补 `__x_`/`__up_` **真实产物** fixture ⇒ 夹具所有权收敛为「生成器独立子命令」）；② 「MUST NOT 自研文本解析器」；③ 「MUST NOT 往共享产物目录加第 4 个 `.v`」 |
| 2 | `openspec/changes/port-naming-contract-guard-2026-11/proposal.md` §Acceptance | 9 条摘要判据 |
| 3 | **本次变更自己声明的行为**（commit + tasks.md） | 24 条逐条判据（tasks.md §T1–§T6） |
| 4 | capability delta `hierarchical-port-naming-guard` | **ADDED ×4**（4 条 Requirement，各带 Scenario） |

⚠️ **本地验收的额外门**：`.rddf/state/iteration.json` **不存在**（本仓无该文件）
⇒ ⇒ 该门**不适用** ⇒ ⇒ 改以 tasks.md 的勾选状态为准（**T1–T6 共 24 条，
已完成 21 条 · 3 条标注「待归档后」**）。

## 2. 逐条判定与证据

> 每条判据至少 1 条工具证据（`shell` 实跑输出 / 文件读取）。
> ⚠️ 0 工具调用即判 pass = 假阳性 —— 本报告**无**此类条目。

### 2.1 capability delta（4 条 Requirement）

| Requirement | 判定 | 证据 |
|---|---|---|
| **R1 端口命名契约 MUST 有静态护栏核验** | ✅ | `scripts/verify_hierarchical_port_naming.sh`（新文件，296 行）· 主路径 **PASS** · `--self-test` **PASS** · CI 挂载两处（`.github/workflows/ci.yml:1405-1421`） |
| **R2 护栏 MUST 同时核验「禁止」与「确有被判对象」** | ✅ | N1（根端口正则）+ N2a（存在量词）+ N2b（覆盖矩阵）三条**独立**判据 ⇒ 实测：语料含 **4** 个非根合成端口；`{__if_,__x_,__up_}` 三前缀**全覆盖** ⇒ ⇒ **缺任一面都会红**（投毒 A/B/C 分别打掉 N1 / N2 的前提） |
| **R3 护栏 MUST 由结构判定根 module** | ✅ | `declared − instantiated`（`comm -23`，`declared` 取自新增的 `--modules`）⇒ 实测 5 个产物根**全判对**；投毒 D（根改名 `my_dut`）⇒ **仍绿**（不依赖名字）；投毒 E（改名 **且** 插合成端口）⇒ **转红**（跟着改名后的根走）⇒ ⇒ 排掉「找不到根 ⇒ 恒绿」的假绿 |
| **R4 护栏 MUST fail-closed 且 MUST 只判定端口声明** | ✅ | N0 七类各有可区分前缀（`N0-MISSING-*` / `N0-GENERATOR-FAILED` / `N0-PRODUCT-COUNT` / `N0-NO-TREE-JSON`）· 退出码 **2**；实测：缺提取器 ⇒ 退出码 **2** + 点名；「只判端口声明」由 `--module-ports`（一级 `stmtsp` + `direction ∈ {INPUT,OUTPUT}`）保证 ⇒ ⇒ 实测：根体内 `__up_0` wire 与根实例映射 `.__x_0(...)` **均未**被判红 |

### 2.2 proposal §Acceptance（9 条）

| # | 判据 | 判定 | 证据 |
|---|---|---|---|
| 1 | N1 精确正则 + 只判端口声明 | ✅ | 正则 `^__(if\|x\|up)_[0-9]+$`（**非**「开头」）· 实测投毒 A/B **转红** |
| 2 | N2a / N2b 两面性 | ✅ | 实测：4 个合成端口 · 三前缀全覆盖 |
| 3 | N3 结构判根，MUST NOT 硬编码 | ✅ | 投毒 D/E（见 R3） |
| 4 | N0 fail-closed 每类可区分 | ✅ | 实测：缺提取器 ⇒ rc=2 + `N0-MISSING-EXTRACTOR` |
| 5 | **不误报**（体内 wire / 实例映射 / 无合成端口的产物） | ✅ | 主路径 PASS（含 `three_layers.v` / `bubble.v` 两个**零合成端口**的产物）· 而两个 fixture 产物**确实**含根体内 `__up_0` wire 与根实例映射 ⇒ ⇒ 仍绿 |
| 6 | `--self-test` 逐条投毒必红 ⇒ 还原复绿 | ✅ | **5 条投毒 + 1 条 N0** ⇒ 全部符合期望 · 还原后复绿 · **无**无效变异（每条投毒后都断言了 `--json-only` rc==0 且 `.tree.json` 存在） |
| 7 | 零回归（既有护栏 / ctest / samples / examples / 计数锚点） | ✅ | `verify_hierarchical_rtl.sh` 主路径 PASS（R1–R8）+ `--self-test` PASS · `ctest -LE perf` **240/240**（455.67 s）· samples **39/39** · examples **28/28** · `verify_test_counts.sh` PASS（251/76 未变）· parity **66 / 289** 未变 |
| 8 | CI 挂载两处 | ✅ | `.github/workflows/ci.yml:1405-1421` · YAML 语法经 `yaml.safe_load` 校验 |
| 9 | 登记面（AGENTS.md / roadmap / 归档后 Purpose 骨架） | ⚠️ **部分** | `AGENTS.md` COMMANDS 已增行（体积 51104 B ≤ 72 KiB）· **roadmap 打勾**与**归档后 `## Purpose` 替换**按流程属**归档后**动作 ⇒ ⇒ 见 §3 |

### 2.3 tasks.md 逐组

| 组 | 条数 | 判定 | 关键证据 |
|---|---|---|---|
| T1 提取器扩展 | 4 | ✅ | `--modules` + `--module-ports` 落地 · 5 产物实测 · **既有 `--ports` 逐字未改**（⇒ R8 不受影响） |
| T2 fixture + 默认路径不变 | 6 | ✅ | 2 个 fixture 落地 · 合成端口**实测**在产物里 · **默认路径 3 产物逐字节相同**（`DIFF_COUNT = 0` · sha256 校验 + 重建 + 复验） |
| T3 护栏脚本 | 11 | ✅ | N0–N3 全部落地 · 退出码 0/1/2 可区分 · 判据路径**零** Verilog 文本解析 |
| T4 self-test | 9 | ✅ | 5 条投毒 + N0 + 还原复绿 |
| T5 零回归 | 7 | ✅ | 见 §2.2 #7 + 8 个护栏全 PASS |
| T6 登记面 | 5 | ⚠️ **3 条待归档后** | CI ✓ · AGENTS.md ✓ · roadmap 打勾 / Purpose 替换 / 主 spec strict 校验 ⇒ 归档时执行 |

## 3. 失败项与路由

**无失败项**（AC 逐条达成 ⇒ ⇒ 无需回 builder 修实现）。

⚠️ **3 条「待归档后」**（**流程性**，非实现缺陷）⇒ 路由 = **本 change 的 P3 归档步骤**：

| 待办 | 归属 | 风险 |
|---|---|---|
| 6.3 roadmap 残留 5 行打勾 + 归档名 | P3 | 低 |
| 6.4 **归档后手工替换 `## Purpose` 骨架**（openspec 生成的骨架**含 change 名** ⇒ 不替换即违反 delta 规则 1，见 `docs/developer_guide/openspec-specs-guide.md:76-87`） | P3 | ⚠️ **中**（漏做 ⇒ 主 spec 留 `TBD - created by archiving change <name>`） |
| 6.5 `openspec validate hierarchical-port-naming-guard --type spec --strict` 通过 | P3 | 低 |

⇒ ⇒ **归档时 MUST 逐条执行并留证**。

## 4. 遗留清扫（本次范围内）

| 项 | 结论 |
|---|---|
| TODO / FIXME / XXX / HACK / 待补 | **0**（grep 覆盖 6 个改动文件） |
| 临时调试输出 | **0**（`tests/hier_rtl_products.cpp` 的 3 处 `std::cout` 是**正常成功输出**：`reject-ok` / `prefix-fixtures-ok` / `products-ok`） |
| 变异残留标记（MUTANT / M-x） | **0** |
| 半截实现 / 注释掉的代码 | **0** |
| 被 skip 的测试 / 未替换的 mock | **0**（本 change **不新增** ctest 条目 ⇒ 计数锚点全部不变 ⇒ 已在 tasks.md T5.5 记录） |
| 临时文件落在仓内 | **0**（全部临时目录在 `mktemp -d` + `trap rm -rf` ⇒ 且**不共享** `verify_hierarchical_rtl.sh` 的 `$WORK`） |
| 范围外历史遗留 | **只记录不清理**：主 spec Purpose 仍写「R1–R7」而脚本已 R1–R8；R8 硬编码 `$1=="top"` ⇒ 已登记在 improvement §5 与 tasks（**本 change 不修** ⇒ 改动最小化） |

⚠️ **另记 4 条实施期发现**（tasks.md「实施期发现」段，**已全部处置**）：

| # | 发现 | 处置 |
|---|---|---|
| ① | **「只判 `isPrimaryIO` 的端口声明」是事实错误**（`isPrimaryIO` 只标顶层 ⇒ N2a/N2b 会恒绿 = 假绿） | ✅ 新增 `--module-ports`（一级 `stmtsp` + `direction ∈ {INPUT,OUTPUT}`）· 既有 `--ports` 逐字未改 |
| ② | 投毒打到「第一个 module」而非根 ⇒ N1 恒绿（产物里根不一定在前） | ✅ 投毒按**结构判出的根名**定位 |
| ③ | 投毒污染 `ROOT_OF` ⇒ 后续投毒打错模块 | ✅ 每条投毒前恢复基线数据 |
| ④ | 生成器缺目录 ⇒ 错误信息指向**路径**而非根因 | ✅ 生成器自建输出目录（幂等） |

⚠️ **① 的连带**：`proposal.md` 附录 A.5 / `design.md` §2.2 / §3 已**就地更正**；
但 **improvement 草稿**（`.rddf/improvements/port-naming-contract-guard-2026-11.md`
§2.2「只判 `isPrimaryIO` 的端口声明」）仍是旧措辞 ⇒ ⇒ **planner 域文件，builder 不改**
⇒ ⇒ 记为 `noted` ⇒ **建议下一轮 planner 更正**（否则该措辞会误导后续读者）。

## 5. 复现命令

```bash
cd /workspace/project/CppHDL
set -euo pipefail
cmake --build build --target hier_rtl_products -j"$(nproc)"
bash scripts/verify_hierarchical_port_naming.sh              # 主路径 ⇒ PASS
bash scripts/verify_hierarchical_port_naming.sh --self-test   # 6 条投毒 ⇒ 必红/必绿
bash scripts/verify_hierarchical_rtl.sh && bash scripts/verify_hierarchical_rtl.sh --self-test
( cd build && ctest -LE perf --output-on-failure )            # 240/240
bash scripts/run_all_samples.sh && ./run_all_ported_tests.sh
bash scripts/verify_test_counts.sh
openspec validate port-naming-contract-guard-2026-11 --type change --strict
bash scripts/verify_openspec_delta.sh
```

## 6. 已用轮次

- **验收轮次**：1 / 3（`RDDF_VERIFIER_MAX_LOOPS=3`）
- **失败回环**：0 次（AC 一次达成 ⇒ 无 `implementation_gap` / `proposal_drift`）
