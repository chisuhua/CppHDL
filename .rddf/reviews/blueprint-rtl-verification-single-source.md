# 审稿记录 · 蓝图 `docs/architecture/2026-11-17-rtl-verification-single-source.md`

## review_profile
`arch`（项目内架构主题文档 · 非治理面）

## 关联
- **ADR-074**（验证单一真相源 + 生成式 TB）· **ADR-075**（端口具名化）
- **change**：`example-file-boundary-2026-11`（本蓝图 §4.3 的裁决由其 3.2 批次落实）· `emit-cli-into-example-2026-11` · `axi4-verif-driven-cpp-2026-11`（已实施）

## 轮次总览

| 轮 | 范围 | 结论 | BLOCKER |
|---|---|---|---|
| oracle 第 1 轮 | §4.3 订正 + §4.3.1 切分表 + §4.3.2 形态裁决 | REJECT | 3（已全闭） |
| oracle 复审 | 同上 · 只验闭合 | REJECT | 1 新 B + B3 收尾（已全闭） |
| critic 第 1 轮 | 同上（arch + plan 双档） | APPROVE_WITH_CONDITIONS · `mergeable: yes` | 0（SHOULD_FIX 10 条已处置） |
| oracle 第 2 轮 | §4.3.3 职责分离裁决 | REJECT | 2（B1 命运未定 / B2 未给同步清单） |
| oracle 复审 2 | 同上 · 只验闭合 | REJECT | B1 闭 / B2 形式闭但引出 NB1·NB2（已全闭） |
| critic 第 1 轮（§4.3.3） | §4.3.3 | REJECT | 1（change 3 的 AC11 / D1 与「删 31 行」冲突） |
| critic 复审（§4.3.3） | 只验闭合 + 新引入 | **APPROVE · `mergeable: yes`** | 0 |

## §4.3.3 的 BLOCKER 处置（最终轮）

| # | BLOCKER | 处置 |
|---|---|---|
| B1 | `axi_soc_example.cpp` 命运未定；「迁移」事实不成立（现 main 只有 2 笔 GPIO 事务 vs tsv 登记 4 场景含 UART；无 `RESULT:` 行） | §4.3.3 增「被取代/删除文件处置表」（5 行）+ 改「**重写，不是迁移**」+ C++ 侧激励裁定 = **最小可用集**（不要求与 `.sv` 的 14 对齐）+ tsv SoC 行 **MUST NOT** 加 `interp-entry` + 否决「C++ 侧重写 UART 解码」并给理由 |
| B2 | 裁决与 change 3 现文档冲突，但未输出 planner 同步清单 | §4.3.3 增「planner 同步清单」（**13 行**）+ 明确 `openspec/changes/**` = planner 写集 ⇒ architect 不代改 |
| NB1 | 处置表漏 3 个登记面（`examples-rtl-sim-exclusions.tsv` ghost / `port-naming-exempt.tsv` 31 行 + 锚点 / `input-port-required-registry.md:45`）+ tsv 四列 + row 1 假 install 条目 | 处置表逐行补齐；锚点算术加 `EXPECTED_EXEMPT_PORTS` **1415 → 1384** |
| NB2 | 同步清单漏 change 3 的 `:298/:307/:318/:321-322/:26` / AC22 命令 / ④″ 契约 / 编号空间 | 同步清单扩到 11 行（后补 row 12/13） |
| **critic B** | **change 3 的 AC11 / D1 明写「23 行只改路径、1415 不变」，与本裁决「删 31 行 → 1384」直接冲突** | 同步清单 **row 12**：AC11/D1 MUST **拆成两段** —— 3.1（头归位）= 23 行改路径（锚点不变）· A7（退役）= 删 31 行 + 锚点 1415 → **1384** |

## SHOULD_FIX / NITS 处置

| # | 项 | 处置 |
|---|---|---|
| S1 | `check_products` 漏「设计 factory」参数 + 编排归属未钉死 | §4.3.1 补「下沉 API MUST 收 factory」+「**编排归属（钉死）**：4 项顺序 / failures 累加 / `eq/` 产出 / 结论行 / 退出码 / CHECK 3 SKIP ⇒ 随规则侧整体下沉」 |
| S2 | SoC 根名约束未从 change 2 继承 | §4.3.3 补根名纪律（两模式根名同为 `"top"` · `device.h:28` 事实订正） |
| S3 | R13 无检查落点 / 无漂移判据 / 未照 R12 自登记 | R13 重写（检查时机 + 漂移判据 = tsv SoC 行 MUST NOT 加 `interp-entry` + 声明不被 ledger 护栏覆盖） |
| S4 | `examples/axi4/AGENTS.md` 与 §4.5 目标列未同步 | §4.5 加 dated 覆盖注；§5.2 M2 改「删除」 |
| — | M9 跨 change 时序未钉死（1447→1415 与 1415→1384 同一锚点） | M9 加 dated 顺序约束（rtl-verification Step 3 **先于** change 3 的 A7） |
| — | ④″ 的 `--emit-hygiene` 透传未钉死 | row 10 补「该模式下其余 argv MUST 照常透传」+「Lite 侧无需 `--check-products`」 |
| — | 处置表漏 `include/axi4/AGENTS.md` / `docs/usage_guide` 的命令示例 | 处置表 row 3 补；M16 扩 grep 词 |
| NIT | `openspec/specs/axi-interconnect-routing/spec.md:242` 行引用悬空 | 同步清单 row 13（登记为已知漂移 · spec 非护栏面 ⇒ 不红） |
| NIT | row 10「无需改代码」与「调用点同步改指」字面相冲 | 改为「**逻辑不改 · 路径必改**」，路径改指归 row 9 / §4.3.2 |

## 未闭合 / 已知边界（如实登记）

| # | 项 | 处置 |
|---|---|---|
| 1 | **`openspec/changes/**` 的 13 项同步属 planner 写集** | 本记录写的是**交接凭据**；change 3 文件本体仍是旧文（预期），由 planner 落实 |
| 2 | **R12 / R13 无机械判据** | 本蓝图 §7 不被 `verify_arch_ledger_residuals.sh` 覆盖（台账硬编码另一本蓝图）⇒ 闭口判据只能来自**实现侧护栏**（⑧' 扩到 `kind=soc`）与 change 归档时的 verifier 核对 |
| 3 | **`.verif` 原语集扩展 = ADR-074 D2 的签批决策变更** | **独立立项**（不在本蓝图实施面）⇒ R13 的闭口判据依赖它 |
| 4 | **`AXI4 Full` 退役的 RTL 产物面** | 退役后 `Axi4Slave` 不再有 example 级 RTL 产物；行为覆盖仍在 `tests/test_axi4_full_burst.cpp`（dual-backend）⇒ 如实登记，不当作「无损」 |
