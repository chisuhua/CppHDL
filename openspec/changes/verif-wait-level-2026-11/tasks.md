# tasks — `verif-wait-level-2026-11`

> ⚠️ **决策面不在本文件**：D2′/D3′ = `docs/adr/ADR-074-rtl-verification-single-source.md` **§7**
> （oracle 第 1 轮 APPROVE + critic 第 1 轮 REJECT(3B) → 全闭 → 复审 APPROVE）⇒ ⇒ 本 change
> 只**实施**，**不改任何决策**。
> ⚠️ **顺序**：批次 A（原语 + 三处同步）**先于**批次 B（运行期比对装置 + helper）⇒ ⇒ 理由 =
> 「先有原语，才有可被比对的东西」。
> ⚠️ **无 spec 变更** ⇒ `.openspec.yaml` 已声明 `skip_specs: true` ⇒ 归档用 `--skip-specs`。

## 批次 0 — 实施前的**架构补注门**（architect 域 · AC1 的前置）

| # | 任务 | 落点 | 判据 | 状态 |
|---|---|---|---|---|
| A0 | ⚠️ **ADR-074 §7.2 的 dated 补注（architect 域 · MUST 由 architect 落 + 走 oracle）** —— 两处澄清：① **`high` ≡ 缺省 ≡「端口值 ≠ 0」**（**不是**「N 位全 1」）⇒ 理由：解释器的 `Resolver` **不返回 width** ⇒ 「全 1」判定**缺输入** ⇒ 且二者等价 ⇒ 无歧义；② 「**位宽比较**」的准确含义 = 「按端口自身位宽取值比较，不做符号/宽度扩展」（**不是**「与全 1 比较」）⇒ ⇒ 后果：**N>1 端口**上 `WAIT p high max` **不是**「等全 1」（现役消费者 `uart_tx` 是 1-bit ⇒ 当前无行为差异 ⇒ 但 API 语义以此为准） | `docs/adr/ADR-074-…md` §7.2 | 补注已落 + architect 的 oracle 判定行 | ☑（`cf473fd` · oracle `判定: APPROVE` · 1 must-fix 已闭） |

## 批次 A — 原语与三处同步（AC1 · AC2 · AC3 · AC13 · AC14 · AC15）

| # | 任务 | 落点 | 判据 | 状态 |
|---|---|---|---|---|
| A1 | **AST 加 `level` 字段**（`Wait` 的电平目标；**缺省 = 非零** ⇒ 旧构造零影响）⇒ ⇒ ⚠️ **不得复用** `RawOp::level` / `VerifOp::value`（`Reset` 的 0/1）⇒ ⇒ 否则「缺省 0」会被读成 **low** ⇒ **语义翻转** | `include/rtl_emit/verif.h` | AC1 | ☑ |
| A2 | **扫描器**：`WAIT` 接受 **2 或 3 个实参**（2 ⇒ 缺省 level = 非零）⇒ ⇒ `level` MUST 是 `high`/`low` **关键字** ⇒ ⇒ 数字 / `HIGH` / `High` ⇒ **非零退出**且错误消息**点名 `level`**（不是「非法数字」）⇒ ⇒ ⚠️ `WAIT` 与 `EXPECT_STABLE` **不得共用分支**（否则会顺手放宽后者） | `src/rtl_emit/verif_scan.cpp` | AC2 · AC12 | ☑ |
| A3 | **解释器**：按 `level` 比较（**非零**判定 ⇒ **不需要端口宽度**）⇒ ⇒ 命中后 **≥1 拍推进**（D3′）⇒ ⇒ 失败文案**点名端口与 `max`**（⇒ 与渲染侧的文案差异**只允许**参数差异，不允许语义差异） | `src/rtl_emit/verif_run.cpp` | AC1 · AC9 · AC11 | ☑ |
| A4 | **渲染器**：与解释器**逐字同语义** ⇒ ⇒ ⚠️ **默认路径的渲染文本 MUST NOT 改**（= **禁止重构渲染器**，不是「结果等价」）⇒ ⇒ 双证：⑥' 新鲜度 + `top.v` 的 sha256 与改动前逐字相同 ⇒ ⇒ ⚠️ 解释器遇未知 kind MUST `default: error`（fail-closed） | `src/rtl_emit/verif_render.cpp` | AC3 · AC15 | ☑ |
| A5 | ⚠️ **X 态差异登记**（AC14）：渲染侧 `!==` 对 X 恒真 ⇒ `high` 把 X 当高（fail-open）· `low` 永不命中（fail-closed）⇒ ⇒ **两端对 X 的处理不对称** ⇒ ⇒ **登记**（与既有 `EXPECT` 的 X 差异**同型**）⇒ ⇒ 本 change **不修** | `docs/usage_guide/04-testbench-guide.md` 的「`WAIT` 的已知边界」段（X 态不对称 · >64 位低 64 位口径 · `max=0` 恒 FAIL） | AC14 | ☑ |

## 批次 B — 判别力装置 + helper 下沉（AC4 · AC5 · AC6 · AC7 · AC10）

| # | 任务 | 落点 | 判据 | 状态 |
|---|---|---|---|---|
| B1 | **运行期同结论比对夹具**：同一份 `.verif` ⇒ IR 解释器 + **渲染 TB 实跑** ⇒ 逐条 `(check_id, PASS/FAIL)` **对**比对相等（⚠️ **不是**静态 id 集合比对 ⇒ 那是现状且对时序敏感原语**结构性失明**）⇒ ⇒ **载体 = ctest**（新测试文件 · 打 `verilator_e2e` 标签）⇒ ⇒ ⚠️ 比对粒度 = `(check_id, 结论)` 对（**不是**原始文本 ⇒ 两端的 FAIL 文案本就不同） | `tests/test_verif_wait_level.cpp`（新） | AC4 | ☐ |
| B2 | **4 条时序判别性构例**：① 首拍即成立 ⇒ 期望「**命中后仍推进 ≥1 拍**」（⚠️ **不是**「立即返回」⇒ 「入口即成立 ⇒ MUST 用 `TICK(n)`」是**规范义务**，不是 `WAIT` 的行为）② 目标在窗口**最后一拍**才成立 ⇒ PASS ③ 命中**恰在第 `max` 拍** ⇒ PASS · 第 `max+1` 拍 ⇒ **FAIL** 且点名 `check_id` ④ 窗口内**从未**成立 ⇒ FAIL 且点名 | 同上 | AC5 · AC9 · AC11 · AC13 | ☐ |
| B3 | **N>1 端口的 high/low 构例** ⇒ ⇒ 钉死「`high` = 非零」在多位端口上的行为（**不是**全 1） | 同上 | AC10 | ☐ |
| B4 | **变异自证（自动化）**：变异**投在夹具的 `.verif` 输入上**（改 `WAIT` 的 `max` ⇒ ⇒ 边界构例必红）⇒ ⇒ **不动实现代码** ⇒ ⇒ 可重跑 ⇒ ⇒ 优于「改解释器一行」（与「还原源码 ≠ 复原」同型 ⇒ 需重建 ⇒ 成本高且易留污染） | 同上 | AC5 | ☐ |
| B5 | **库层 `run_verif` helper 下沉**：把 Lite 侧内联的「默认模式 `run_verif` 七步序列」下沉为库层 helper ⇒ ⇒ Lite 改调它 ⇒ ⇒ ⚠️ helper **MUST NOT 返回 `RESULT:` 结论行**（结论行由各 example 自己打印 ⇒ 否则双权威）⇒ ⇒ ⚠️ 落点若为**新公开头** ⇒ MUST 登记 `.rddf/checks/public-api-surface.md` | `include/rtl_emit/verif.h`（或新头）· `examples/axi4/tb_axi4_lite_example.cpp` | AC7 | ☐ |

## 批次 C — 验收与归档（AC6 · AC8 · AC15）

| # | 任务 | 落点 | 判据 | 状态 |
|---|---|---|---|---|
| C1 | **锚点回填**：实测后回填 `CTEST_TOTAL` · `VERILATOR_E2E_TOTAL`（**会变**）· `tests/test_*.cpp`（**会变**）⇒ ⇒ ⚠️ `EXAMPLES_TEST_SOURCES` **不变**（夹具在 `tests/**`） | 根 `AGENTS.md` | AC8 | ☐ |
| C2 | **遗留清扫**：本 change 触及的文件 MUST NOT 有 TODO / 半截实现 / 注释掉的代码 / 临时日志 | 全部触及文件 | AC8 | ☐ |
| C3 | **文档同步**：`include/rtl_emit/` 附近的 AGENTS.md（若有）+ `docs/CHANGELOG.md` + 本 change 的 `## 用户可见变更` 段 | — | AC8 | ☐ |
| C4 | **归档**：`openspec archive verif-wait-level-2026-11 --yes --skip-specs` ⇒ ⇒ 归档后核对主 spec **零变更** | — | AC8 | ☐ |

---

## AC → 任务映射

| AC | 任务 |
|---|---|
| AC1 | A0 · A1 · A3 |
| AC2 | A2 |
| AC3 | A4 |
| AC4 | B1 |
| AC5 | B2 · B4 |
| AC6 | C1 · B5 |
| AC7 | B5 |
| AC8 | C1 · C2 · C3 |
| AC9 | B2 · A3 |
| AC10 | B3 |
| AC11 | B2 · A3 |
| AC12 | A2 |
| AC13 | B2 |
| AC14 | A5 |
| AC15 | A4 · C1 |

---

## 验收取证表

| 任务 | 命令 | 期望 |
|---|---|---|
| A1 | `grep -c "level" include/rtl_emit/verif.h` | ≥ 1（新增字段） |
| A2 | 负向：`WAIT p 1 10` / `WAIT p HIGH 10` ⇒ 扫描器退出码 | **非 0** + 消息含 `level` |
| A3–A4 | 运行期比对夹具 | 逐条 `(check_id, PASS/FAIL)` 相等 |
| B2 | 4 条构例 | 各自的期望成立（含 max 拍 PASS / max+1 拍 FAIL） |
| B4 | 变异（改夹具 `.verif` 的 `max`） | 边界构例**必红** |
| B5 | `grep -c "run_verif" examples/axi4/tb_axi4_lite_example.cpp` | ≥ 1（改调 helper）· helper 内 **无** `RESULT:` |
| C1 | `bash scripts/verify_test_counts.sh` | 锚点与实测一致 |
| 全量 | `ctest --test-dir build --output-on-failure` | **全绿** |
| 全量 | `bash scripts/verify_examples_rtl_sim.sh` · `--self-test` | 全 PASS |
