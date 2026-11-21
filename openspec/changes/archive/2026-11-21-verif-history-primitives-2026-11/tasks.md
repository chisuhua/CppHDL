# tasks — `verif-history-primitives-2026-11`

> ⚠️ **决策面在 ADR**：`docs/adr/ADR-076-verif-history-primitives.md`（新独立 ADR）⇒ ⇒ 本 change 只**实施**，**不改 ADR-074 的任何决策**。
> ⚠️ **顺序**：批次 0（ADR + 双审）**先于**批次 A（原语 + 四处同步）⇒ ⇒ 理由 = 「先有决策，才有可被实施的东西」。
> ⚠️ **无 spec 变更** ⇒ `.openspec.yaml` 已声明 `skip_specs: true` ⇒ 归档用 `--skip-specs`。

## 批次 0 — 决策面（ADR-076 · oracle → critic）

| # | 任务 | 落点 | 判据 | 状态 |
|---|---|---|---|---|
| A0 | **ADR-076 新建**（独立 ADR · **不改** ADR-074 正文）⇒ ⇒ 四要素齐（元数据 / 决策 / 选项对比表 ≥3 行 / 后果代价章 / Verification 章）⇒ ⇒ oracle 第 1 轮 → critic 第 1 轮（顺序不可颠倒）⇒ ⇒ BLOCKER 全闭 ⇒ ⇒ ⚠️ **双审结论若与 `design.md` §2 / §4 冲突 ⇒ MUST 回流修订 `design.md`**（不得让双审结论滞留在 ADR 层）⇒ ⇒ ⚠️ 选项对比表 MUST 含被拒方案：「加 `EXPECT_SEQ_EQ`」·「表达式化参数（§7.4 C/D 已否决路线）」·「宽端口登记差异而非拒绝」 | `docs/adr/ADR-076-verif-history-primitives.md` + `.rddf/reviews/ADR-076-verif-history-primitives.md` | 双审闭合 | ☑ |
| A1 | **不改 ADR-074** ⇒ ⇒ `git diff docs/adr/ADR-074-*.md` **为空** | — | 逐字未改 | ☑ |

## 批次 A — 原语 + 四处同步（AC1 · AC2 · AC3 · AC7 · AC9 · AC11 · AC13 · AC14）

| # | 任务 | 落点 | 判据 | 状态 |
|---|---|---|---|---|
| A2 | **IR 扩展**：4 个 kind + 5 个新字段（`port2` / `seq_n` / `seq_lag` / `seq_value` / `seq_count`）⇒ ⇒ ⚠️ **不复用** `value`/`count` | `include/rtl_emit/verif.h` | AC1 | ☑ |
| A3 | **扫描器**：新原语解析（参数个数 / 取值域 / 关键字）⇒ ⇒ **静态良构性**：每端口 `RECORD` 累计计数（深度上限 1024）· 序列断言的**历史充足性**（到该语句为止的累计次数 ≥ 所需 · `EXPECT_SEQ_STEP` **两个端口各判**）· **32 位上溢**（复用既有形态）· **端口 width > 64 ⇒ fail-closed**（从 **manifest** 查 width ⇒ 两端同源）· **sanitize 撞名 ⇒ fail-closed**（扫描期计算 + 与渲染器**共用**同一函数 ⇒ 落 `src/rtl_emit/verif_internal.h`）⇒ ⇒ 不符 ⇒ **非零退出** + 消息点名 ⇒ ⇒ `check_id` 分配 + `verif_check_count` 计入（`EXPECT_SEQ_*` 各 1 · `RECORD` 0）⇒ ⇒ `enforce_settle_after_drive` 扩到 `RECORD`（**不**扩到 `EXPECT_SEQ_*`） | `src/rtl_emit/verif_scan.cpp`（+ 内部头） | AC1 · AC2 · AC11 · AC13 · AC14 · AC15 · AC16 · AC18 | ☑ |
| A4 | **解释器**：历史缓冲（key = 端口标识符）· `RECORD` 追加 · 3 条序列断言按 `design.md` §2 的公式计算（mod 2^64）⇒ ⇒ 失败消息**点名首个不符项的索引与两端值** ⇒ ⇒ ⚠️ 遇未知 kind ⇒ `default: error` | `src/rtl_emit/verif_run.cpp` | AC3 · AC9 | ☑ |
| A4b | ⚠️ **实施期发现（2026-11-21 · 由判别力夹具抓出）**：`EXPECT_SEQ_STEP` 的公式**差一错位** —— V 侧写成 `hV[LV-n+i+1]`（⇒ **越界**读 `hV[LV]` ⇒ UB）· E 侧写成 `hE[LE-n+i]`（⇒ **晚一拍**的 E 值）⇒ ⇒ **两端一致地错**（⇒ 「两端一致不能当正确性证据」铁律的活标本）⇒ ⇒ 已按 `design.md` §2.3 的「对齐的精确定义」修正 ⇒ ⇒ **解释器 / 渲染器 / 扫描期充足性（两端口各需 ≥ n+1）** 三处同批改 ⇒ ⇒ **design.md §2.3 与 ADR-076 D5 已同步** | 同上 + `design.md` + ADR-076 | AC3 · AC17 | ☑ |
| A5 | **渲染器**：为每个被 `RECORD` 的端口声明 `logic [63:0] hist_<id>[0:1023]` + `integer hist_idx_<id>` ⇒ ⇒ `RECORD` 写入 ⇒ ⇒ 3 条序列断言渲染（变量索引 · 64 位减法回绕 · `!==` ⇒ X 判不等 ⇒ FAIL）⇒ ⇒ ⚠️ **width > 64 的端口 ⇒ fail-closed**（渲染期已知 width ⇒ 不渲染）⇒ ⇒ ⚠️ **既有原语的渲染文本 MUST NOT 改** | `src/rtl_emit/verif_render.cpp` | AC3 · AC7 · AC10 · AC15 | ☑ |
| A6 | **文档登记**：① **X 态差异**（`EXPECT_SEQ_SELF`/`STEP` 的「X ⇒ FAIL」+ `EXPECT_SEQ_COUNT` 的「X 项不计数 ⇒ 计数偏少 ⇒ FAIL」）② **复位尾缝**（场景首条 `RECORD` 继承 wait-level 已登记的 1 拍接缝 ⇒ ⇒ 夹具 MUST 沿用 `SET`+`TICK` 前缀范式）③ **「一个物理端口只用一个标识符 `RECORD`」**（否则两条独立历史）④ **「记录与比较 MUST 分到不同拍次」**（`REPEAT` 自举形态被过拒 ⇒ 已知限制）⑤ **序列类断言的推导层 MUST 含 `lag`/`count`/`value` 的由来**（D5 义务扩展 ⇒ 迁移 change MUST 落地）⑥ **比对形态升级**（逐对 `(check_id, PASS/FAIL)` 严于 D8 原文的「集合相等」⇒ ⇒ ADR-076 MUST 明记） | `docs/usage_guide/04-testbench-guide.md`（或 `.verif` 的载体文档）+ ADR-076 | AC10 · AC12 · AC17 · AC19 · AC20 · AC21 | ☑ |

### 批次 A′ — 迁移实施期发现的两条框架缺陷（**已修** · 判据载体在迁移 change）

| # | 任务 | 落点 | 判据 | 状态 |
|---|---|---|---|---|
| A7 | **渲染侧窄端口 `WIDTHEXPAND`**：`hold_val` / 历史数组为 `logic [63:0]`，窄端口直接赋 ⇒ 护栏 ④（禁 `-Wno-fatal`）下 **TB 编译失败** ⇒ ⇒ `u64_rhs()` 零扩展 | `src/rtl_emit/verif_render.cpp` | AC3 · AC7 | ☑ |
| A8 | **解释器框架复位不载入寄存器 INIT**：`Simulator::reset()` 清 0 ⇒ `INIT != 0` 的寄存器与 RTL **分叉** ⇒ ⇒ 复位后按 `regimpl::get_init_val()` 回填（⚠️ 不是驱动 reset 节点：`rst_val` 为 nullptr ⇒ 解释器复位期间只 return） | `src/rtl_emit/verif_run.cpp` | AC3 | ☑ |

## 批次 B — 判别力装置（AC4 · AC5 · AC6）

| # | 任务 | 落点 | 判据 | 状态 |
|---|---|---|---|---|
| B1 | **运行期同结论夹具**：同一份 `.verif` ⇒ IR 解释器 + 渲染 TB 真 verilator 编译运行 ⇒ ⇒ 逐条 `(check_id, PASS/FAIL)` **对**比对相等 ⇒ ⇒ 复用 `tests/test_verif_wait_level.cpp` 的装置形态 | `tests/test_verif_history_primitives.cpp`（新） | AC4 | ☑ |
| B2 | **每条新原语 ≥2 条构例**（PASS + FAIL）+ **手推期望**（禁抄仿真输出 ⇒ 两端一致**不是**正确性证据）⇒ ⇒ 含：自滞后恒等（PASS / FAIL）· 相邻差（PASS / FAIL）· 窗口计数（PASS / FAIL）· `RECORD` 的采样窗 · 跨 scenario 不清空 · `EXPECT_SEQ_*` 不受 `SETTLE` 纪律 | 同上 | AC5 · AC12 | ☑ |
| B3 | **变异自证**：变异投在**夹具 `.verif` 输入**上（不改实现代码）⇒ ⇒ ≥3 条变异各自必红（两端同时）⇒ ⇒ **无效变异如实登记** | 同上 | AC6 | ☑ |

## 批次 C — 验收与归档（AC7 · AC8）

| # | 任务 | 落点 | 判据 | 状态 |
|---|---|---|---|---|
| C1 | **既有产物逐字节不变**：Lite 的 `tb_axi4_lite_example.sv` sha256 与改动前相同 ⇒ ⇒ 护栏 ⑥' 新鲜度 + sha256 双证 | — | AC7 | ☑ |
| C2 | **锚点回填**：`CTEST_TOTAL` · `VERILATOR_E2E_TOTAL` · `tests/test_*.cpp` 计数 ⇒ ⇒ ⚠️ `EXAMPLES_TEST_SOURCES` **不变**（夹具在 `tests/**`） | 根 `AGENTS.md` | AC8 | ☑ |
| C3 | **遗留清扫**：本 change 触及的文件 MUST NOT 有 TODO / 半截实现 / 注释掉的代码 / 临时日志 | 全部触及文件 | AC8 | ☑ |
| C4 | ☑ **已完成（2026-11-21）**· **文档同步**：`docs/CHANGELOG.md` + `include/rtl_emit/` 附近的说明 + 本 change 的「用户可见变更」段 ⇒ ⇒ ⚠️ **公开面登记**：本 change 给公开头 `include/rtl_emit/verif.h` 加 **4 个 kind + 5 个字段** ⇒ ⇒ 登记 `.rddf/checks/public-api-surface.md`（ADR-074 §6「新增公开 API ⇒ 登记 public-api-surface + CHANGELOG」；wait-level 先例 `tasks B5` 同位置携带该义务）⇒ ⇒ 若裁决「既有公开头的枚举/字段扩增**不**触发登记」⇒ ⇒ **MUST 在本行写明该裁决与理由**，不得让义务悬空 | — | AC8 | ☑ |
| C5 | **归档**：`openspec archive verif-history-primitives-2026-11 --yes --skip-specs` ⇒ ⇒ 归档后核对主 spec **零变更** | — | AC8 | ☐ |

---

## AC → 任务映射

| AC | 任务 |
|---|---|
| AC1 | A2 · A3 |
| AC2 | A3 |
| AC3 | A4 · A5 |
| AC4 | B1 |
| AC5 | B2 |
| AC6 | B3 |
| AC7 | A5 · C1 |
| AC8 | C2 · C3 · C4 · C5 |
| AC9 | A4 |
| AC10 | A5 · A6 |
| AC11 | A3 · B2 |
| AC12 | B2 |
| AC13 | A3 · B2 |
| AC14 | A3 |
| AC15 | A3 · A5 |
| AC16 | A3 |
| AC17 | A6 |
| AC18 | A3 |
| AC19 | A6 |
| AC20 | A6 |
| AC21 | A6 |

---

## 验收取证表

| 任务 | 命令 | 期望 |
|---|---|---|
| A1 | `git diff --stat docs/adr/ADR-074-rtl-verification-single-source.md` | **空** |
| A3 | 负向构例（参数个数 / `n=0` / `lag=0` / `count>n` / 历史不足 / 深度超限） | 扫描器**非零退出** + 消息点名 |
| A4–A5 | 运行期比对夹具 | 逐条 `(check_id, PASS/FAIL)` 相等 |
| B2 | 构例表 | 各自的**手推**期望成立 |
| B3 | 变异（改夹具 `.verif`） | 对应变例**必红**（两端同时） |
| C1 | `sha256sum examples/axi4/tb_axi4_lite_example.sv` | 与改动前**逐字相同** |
| C2 | `bash scripts/verify_test_counts.sh` | 锚点与实测一致 |
| 全量 | `ctest --test-dir build --output-on-failure` | **全绿** |
| 全量 | `bash scripts/verify_examples_rtl_sim.sh` · `--self-test` | 全 PASS |
