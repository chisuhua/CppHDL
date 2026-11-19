# 后续实施路径 —— 三份 change 的验收收口（2026-11-18）

> **前提**：用户已批准 verifier 的全部推荐（D1–D5 · P1–P3 · B1–B3），见 `decisions-required-2026-11-18.md`。
> **本文件是**路径（谁做 / 改哪 / 怎么验 / 何时算完）**—— 不是实施**。
> **角色边界**：verifier 只读 + 写 `.rddf/reviews/**` ⇒ ⇒ 阶段 1 需 **planner**，阶段 2 需 **builder**，阶段 3 回到 **verifier**。

## 〇、总览（5 阶段 · 4 次角色交接）

| 阶段 | 执行者 | 产出 | 完成判据 | 角色交接 |
|---|---|---|---|---|
| **0** 裁决固化 | verifier（本回合） | 本文件 + 4 份报告 + 裁决记录 | 报告落盘且 `git status` 可查 | → planner |
| **1** AC 措辞批改 | **planner** | 4 份 proposal 的 AC 行 | `verify_openspec_delta.sh` 绿 + strict 无**新增**告警 | → builder |
| **2** 实现/记录修 | **builder** | B1–B3 + D3 + D5 回填 | ctest 全绿 + 全部护栏（含新自检）+ `run_all_ported_tests` exit 0 | → verifier |
| **3** 第二轮复核 | **verifier** | 三份 `-verify-2.md` | 上轮 10 条不通过项全部闭合 + 无新增 BLOCKER | → builder/planner（若有残留） |
| **4** 归档 + push | builder（或 general） | 三份 change 归档 + push | `openspec archive` 成功 + CI 绿 | → 用户（push 确认） |

⚠️ **阶段 2 的 D3 会改变 `run_all_ported_tests.sh` 的退出码（1 → 0）** ⇒ ⇒ 这是**行为变更**，须单独成 commit 并在 commit message 里写明「C1 由红转绿」。CI 不受影响（CI 只构建 `all-examples` ⇒ 该 emitter 不会被构建 ⇒ C1 看不见它）；受影响的是**做全量构建的开发者**（根 `AGENTS.md` 的 COMMANDS 段正是这么教的）。

---

## 一、阶段 0 · 裁决固化（verifier · 本回合）

| # | 动作 | 落点 | 状态 |
|---|---|---|---|
| 0.1 | 写裁决请求 + 路径 | `.rddf/reviews/decisions-required-2026-11-18.md` · 本文件 | ✅ |
| 0.2 | 写三份验收报告 | `.rddf/reviews/{axi4-verif-driven-cpp,emit-cli-into-example,example-file-boundary}-2026-11-verify.md` | ✅ |
| 0.3 | **补「已批准」状态** | 在 `decisions-required-2026-11-18.md` 顶部追加一段「2026-11-18 用户批准 D1–D5 / P1–P3 / B1–B3」⇒ ⇒ 决策**可审计** | ☐ 本回合补 |
| 0.4 | 提交 0.1–0.3 | `git commit`（4 个新文件，路径显式 `git add`）⇒ ⇒ **需 builder/general 角色**（verifier 不做实施类 commit；报告 commit 是否算实施类由工具层判定 ⇒ 保守起见交给 builder） | ☐ |

**阶段 0 完成判据**：`git status --short` 只剩 4 个 `?? .rddf/reviews/...`；其余仓内文件零改动。

---

## 二、阶段 1 · planner 批改 AC 措辞（4 份 proposal · 11 处）

> **只改 AC 行的措辞 ⇒ 不新增/删除 AC 条目 ⇒ 不构成「重写 ≥ 1/3」⇒ 不触发 metis**（角色矩阵的 planner 审查触发条件）。
> ⚠️ **不得**借机改 AC 的**语义**（如 AC17 的下沉集、AC18 的四项判据）—— 只改**口径与数字**。

### 1.1 `openspec/changes/example-file-boundary-2026-11/proposal.md`

| # | AC | 改什么 | 措辞草案 |
|---|---|---|---|
| P1 | **AC17** | 调用点计数口径 | 「**库层实现 TU 内**每个公开函数恰 1 处调用 + **example 侧**对 `check_products` 恰 1 处（`product_modules` / `tamper_hier` 由编排内部调用 ⇒ example 侧为 0）」——并删掉「禁写 == 2」这句自相矛盾的话 |
| P2 | **AC18** | CHECK 3 语义 | 「CHECK 3 **NEVER 静默跳过**：linter 缺失 ⇒ **按失败计**（fail-closed）；⚠️ 实现已由 `SKIP` 改判 `FAIL`，差异登记于 `docs/CHANGELOG.md`（2026-11-18）」 |
| P3 | **AC22** | 行号列 | 「表内 `行` 列仅作**历史标注**（对象文件已删除 ⇒ 不会漂移）；**可复算定位** MUST 用新落点的 grep 命令」 |
| D4-b | **AC10** | 计数数字 | 「CTEST_TOTAL **269** · non-perf **258** · `-L base` **233** · `EXAMPLES_TEST_SOURCES` **20** · `tests/test_*.cpp` **159**（⚠️ 推导值未计入本 change 的 F2 新增 `test_product_check` ⇒ **以实测回填**）」 |
| D1-b | **AC5** | 允许集 | 加一句「**TEST_CASE 名含 DUT 名 ⇒ 允许随 DUT 改名**（须同批登记进 `docs/CHANGELOG.md`）」⇒ ⇒ 与本次改名对齐 |
| D2-b | **AC25** | 基线口径 | 「默认模式的『1 → 5』以 `eq/flat/top.v`（扁平参照，1 module）+ `manifest.json` **逐字节相同**（⇒ `port_table` 逐字逐序相同）作**等价证据**；⚠️ 批次 F 前的扁平基线**已无法补采** ⇒ ⇒『分模式两套基线』降为**今后的登记要求**」 |

### 1.2 `openspec/changes/axi4-verif-driven-cpp-2026-11/proposal.md`

| # | AC | 改什么 |
|---|---|---|
| D4-a | **AC10** | 「PASS 计数仍 28」⇒ 改「**27**（= 28 − change 3 AC27 的 `axi4_full_example` 退役）」；并补「脚本绿的前提 = 无 `UNTRACKED`（既有缺口，见 D3）」 |

### 1.3 `openspec/changes/emit-cli-into-example-2026-11/proposal.md`

| # | AC | 改什么 |
|---|---|---|
| D4-c | **AC9** | 同 1.2 的 AC10（`28` → `27`，同一来源） |

### 1.4 上游活跃 change（**若 F0 ② 未完成则必须完成**）

`openspec/changes/rtl-verification-single-source-2026-11/` 的同款陈旧目标（`proposal.md:21/34/46` + `tasks.md:29`）——**未归档 ⇒ 不是历史记录 ⇒ 豁免不成立**。上一批记录称「已同批订正」⇒ 阶段 1 先**核对**，若已改则只在报告里记「已闭」。

### 阶段 1 验证命令

```bash
# ① delta 自检（archive 前的实际门）
bash scripts/verify_openspec_delta.sh

# ② 严格校验（⚠️ CLI 1.14.x 有两条**既有** warning：正文 >500 字符 / Purpose 占位符
#    ⇒ ⇒ 只判「有无**新增** warning」）
openspec validate example-file-boundary-2026-11 --strict
openspec validate axi4-verif-driven-cpp-2026-11 --strict
openspec validate emit-cli-into-example-2026-11 --strict

# ③ 措辞改动不得动语义（人工对照本文件 1.1–1.3 的措辞草案）
git diff --stat openspec/changes/
```

**阶段 1 完成判据**：三份 proposal 的 diff **只**落在上表列出的 AC 行；`verify_openspec_delta.sh` 绿；strict 校验无新增告警。

---

## 三、阶段 2 · builder 修实现与记录（5 笔 · 按风险从低到高）

> **纪律**：**每笔单独 commit**，每笔之后**立刻**跑该笔的验证命令 ⇒ 不积累未验证改动。

### 2.1 B1（零风险 · 一行注释）— change 1 AC16

| 项 | 内容 |
|---|---|
| 改动 | `examples/axi4/axi4_lite_example_top.h:3`：`examples/axi4/axi4_lite_example.cpp` → `examples/axi4/tb_axi4_lite_example.cpp` |
| 验证 | `grep -rnE 'axi4_lite_example\.cpp\|(^|[^A-Za-z0-9_])axi4_lite_example([^A-Za-z0-9_]\|$)' examples scripts tests docs/*.md AGENTS.md \| grep -v tb_axi4_lite_example` ⇒ **零命中**（`docs/CHANGELOG.md` 按白名单排除） |
| commit | `fix(axi4): 顶层头注释的旧文件名订正（AC16 旧名归零）` |

### 2.2 B3（零风险 · 一行 CHANGELOG）— change 3 AC27

| 项 | 内容 |
|---|---|
| 改动 | `docs/CHANGELOG.md` 补一条：「`axi4_full_example` 退役（2026-11-18 · `example-file-boundary-2026-11` AC27）⇒ 行为覆盖迁至 `tests/test_axi4_full_burst.cpp`（dual-backend）⇒ **显式缩面登记**」 |
| 验证 | `grep -c "axi4_full_example" docs/CHANGELOG.md` ⇒ ≥1；`bash scripts/verify_agents_size.sh`（CHANGELOG 不在根 AGENTS.md 内，无尺寸门） |
| commit | `docs(changelog): 登记 axi4_full_example 退役（显式缩面）` |

### 2.3 D3（一行登记 · **有行为变更**）— 让 `run_all_ported_tests.sh` 恢复 exit 0

| 项 | 内容 |
|---|---|
| 改动 | `examples/disabled_examples.txt` 追加一行：`emit_rtl_rv32i_soc_fw_init<TAB>发射器：由 scripts/verify_rv32i_fw_init_rtl.sh 专属消费，不属 RTL 仿真产品清单（tsv 的 emitter 列）⇒ 故不进 CH_TRACKED_EXAMPLE_TARGETS<TAB>归位条件：若它成为 RTL 仿真产品清单的一行 ⇒ 移到 tsv 的 emitter 列并移除本行` |
| ⚠️ 副作用 | `run_all_ported_tests.sh` 从 **exit 1 → exit 0** ⇒ 修掉**全量构建者**的红；CI 不受影响（CI 只构建 `all-examples`） |
| 验证 | `./run_all_ported_tests.sh; echo "exit=$?"` ⇒ **exit=0**、`PASS: 27 FAIL: 0`、`UNTRACKED` 行消失；`bash scripts/verify_examples_rtl_sim_coverage.sh` 仍 PASS |
| commit | `fix(examples): 登记 fw_init 发射器的归属 ⇒ run_all_ported_tests 恢复 exit 0` |

### 2.4 B2（**唯一有行为风险** · 护栏新增落盘 + 新增自检）— change 2 AC4

| 项 | 内容 |
|---|---|
| 改动 1 | `scripts/verify_examples_rtl_sim.sh:465` 发射调用点前后落两份证据：`<work>/argv.txt`（原始 argv，逐行）+ `<work>/argv.norm`（剥 `argv[0]` + 模式标记后的 **flag token 序列**） |
| 改动 2 | `--self-test` 新增**一条**投毒：「`argv.norm` 含由 `emitter_args` 透传的 `-` ⇒ 必红」⇒ **没有这条，落盘就无判别力**（AC4 明写「两文件 MUST NOT 含由 `emitter_args` 透传的 `-`」） |
| 改动 3 | 断言「原始 argv MUST 含 `--emit-rtl`」（AC4 原文要求） |
| 验证 | `bash scripts/verify_examples_rtl_sim.sh` PASS；`bash scripts/verify_examples_rtl_sim.sh --self-test` ⇒ **18 投毒**（原 17 +1）全红 + 必绿仍绿 + 还原复绿；实跑后 `find $W -name 'argv*'` ⇒ **2 个文件**且内容符合口径 |
| ⚠️ 回滚 | 若新投毒误伤既有路径 ⇒ **单独回退本 commit**（不影响 B1/B3/D3） |
| commit | `feat(guards): 发射 argv 双证据落盘 + 归一化序列的反向自检（AC4）` |

### 2.5 D5（账实回填 · **只改勾选状态，不改内容**）— 归档门

| change | 动作 | 判据 |
|---|---|---|
| change 1 | 逐条核对后回填 `tasks.md` 的 29 行 ✅ | **不得**批量勾 ⇒ 每行须对应本轮 AC 判定 |
| change 2 | 同上（30 行） | 同上 |
| change 3 | `F9` ⇒ 标 **N/A**（切分表非空）；`F4` ⇒ 标 **follow-up** 并记入 backlog | `F4` 的第二消费者（`tests/test_hierarchy_verilog.cpp` 内存字符串计数 + 护栏 ⑧' 产物文件）与 `product_modules(path)` **非 drop-in** ⇒ 归一化**另立项** |
| 附加 | `.rddf/state/iteration.json` 不存在 ⇒ 二选一：补该文件，或在 change 记录里注明「本仓以 `tasks.md` 为准」 | 本地验收门「`tasks_done == tasks_total > 0`」当前**不可评估** |

### 阶段 2 全量验证（每笔之后至少跑本笔的；全部完成后跑一遍）

```bash
cmake --build build -j$(nproc)                 # 0 error
ctest --test-dir build -LE perf --output-on-failure   # 258/258
bash scripts/verify_examples_rtl_sim.sh && bash scripts/verify_examples_rtl_sim.sh --self-test
bash scripts/verify_example_only_headers.sh && bash scripts/verify_example_only_headers.sh --self-test
bash scripts/verify_no_duplicate_rv32i_headers.sh && bash scripts/verify_no_duplicate_rv32i_headers.sh --self-test
bash scripts/verify_public_api_visibility.sh && bash scripts/verify_public_api_visibility.sh --self-test
bash scripts/verify_input_port_required_registry.sh
bash scripts/verify_rst_n_wiring.sh
bash scripts/verify_example_assertion_quality.sh && bash scripts/verify_assertion_quality_fixtures.sh
bash scripts/verify_port_naming.sh
bash scripts/verify_test_counts.sh && bash scripts/verify_agents_size.sh
bash scripts/verify_roadmap_fragments.sh && bash scripts/verify_roadmap_fragments.sh --self-test
bash scripts/verify_openspec_delta.sh
./run_all_ported_tests.sh; echo "exit=$?"        # ⇒ D3 之后应为 0
```

**阶段 2 完成判据**：以上全绿 + `git status --short` 只剩预期的未跟踪项（`build-nojit/` 可删）。

---

## 四、阶段 3 · verifier 第二轮复核（预算 3 轮 · 已用 1 轮）

**只复核「上轮不通过项 + 本轮新增 diff」**（禁止全量重审）：

| 项 | 判据 | 复现命令 |
|---|---|---|
| change 1 AC16 | 旧名归零（作用域内零命中） | 见 2.1 |
| change 1 AC10 | `run_all_ported_tests.sh` exit 0 + PASS 计数 27（按订正后口径） | `./run_all_ported_tests.sh` |
| change 2 AC4 | `argv.txt` / `argv.norm` 存在 + 新投毒转红 | `RTL_SIM_WORK=$(mktemp -d) bash scripts/verify_examples_rtl_sim.sh` + `--self-test` |
| change 2 AC9 | 同 change 1 AC10 | 同上 |
| change 3 AC5 | 新 AC 措辞允许「随 DUT 改名」⇒ 判定按新措辞；且 CHANGELOG 有登记 | `git diff 55f3f5a..HEAD -- tests/test_axi_lite_verilator_e2e.cpp` |
| change 3 AC10/AC17/AC18/AC22/AC25 | 按**订正后**的措辞判定（关键：判定口径必须与新 AC 一致 ⇒ ⇒ **不能沿用旧口径**） | 见各报告 §五 |
| change 3 AC27 | CHANGELOG 有缩面登记 | `grep -c axi4_full_example docs/CHANGELOG.md` |
| D5 账实 | 三份 tasks.md 的 ✅/☐ 与实测一致 | 逐条核对 |
| 新增 diff | 阶段 1–2 的全部 diff 无新 BLOCKER（尤其 B2 的护栏改动） | `git diff <基线>..HEAD` |

**产物**：`.rddf/reviews/<change>-verify-2.md`（或同文件追加「第二轮」段）。
⚠️ 若某条仍取证不可用 ⇒ 按「取证不可用 ⇒ 交回」写受阻记录，**不得**判 pass。

---

## 五、阶段 4 · 归档 + push

| # | 动作 | 判据 |
|---|---|---|
| 4.1 | `openspec archive example-file-boundary-2026-11` | archive 前实门 = `verify_openspec_delta.sh` 绿；**禁** `--skip-specs`（本 change 有 spec 增量） |
| 4.2 | `openspec archive axi4-verif-driven-cpp-2026-11` | 同上 |
| 4.3 | `openspec archive emit-cli-into-example-2026-11` | 同上 |
| 4.4 | 归档后复核 | `openspec validate --specs`（非 strict，作为 ADR 判据面）+ `bash scripts/verify_test_counts.sh`（CTEST_TOTAL 不变 ⇒ 归档不增删测试）+ `bash scripts/verify_roadmap_fragments.sh`（AUTO 块与 roadmap 一致） |
| 4.5 | `git push` | **必须用户确认**（`git push` 不由工具层自动放行） |

⚠️ **上游 `rtl-verification-single-source-2026-11` 不在本批归档范围**（它自己的 AC 尚未验收）⇒ ⇒ 它的「同款陈旧目标」必须已订正（1.4）。

---

## 六、阶段 5 · 遗留独立立项（本批**不做**，只登记）

| 项 | 来源 | 建议落点 |
|---|---|---|
| `.verif` 原语集扩展（UART 8N1 帧解码等） | ADR-074 D2 | 独立 change（SoC 的激励仍是手写过渡态） |
| `emit_rtl_rv32i_soc_fw_init` 的**归位**（真正进产品清单或退役） | D3 的「归位条件」 | 独立立项（D3 只把归属**登记**清楚） |
| F4 第二消费者归一化（`tests/test_hierarchy_verilog.cpp` 的内存字符串计数 + 护栏 ⑧' 的产物文件 ⇒ 与 `product_modules(path)` 非 drop-in） | change 3 F4 | 独立立项 |
| 蓝图 §4.3.2「SoC 恒层次化」⇒ 一般化为「形态由 `ch_layer` 决定」 | architect 写集 | **architect**（builder 未代改） |
| `docs/CHANGELOG.md` 未单列 `axi4_full_example` 缩面 | 本轮 B3 补 | 阶段 2 完成 |

---

## 七、风险清单（按可能性 × 影响排序）

| 风险 | 触发点 | 缓解 |
|---|---|---|
| **B2 的新投毒误伤**（护栏自检对既有 fixture 敏感） | 阶段 2.4 | 单独 commit ⇒ 可单独回退；自检必须「还原后复绿」 |
| **D3 改变退出码 ⇒ 有人依赖旧行为** | 阶段 2.3 | 只把 1 → 0（红 → 绿）⇒ 无消费者依赖「红」；commit message 明写 |
| **阶段 1 的措辞改动被误读为语义放宽** | 阶段 1 | 措辞草案逐条列在 1.1–1.3；planner 只改该行，不动判据集合 |
| **阶段 3 沿用旧口径判 AC**（AC 已改 ⇒ 判定基准也变） | 阶段 3 | 复核清单里显式写「按订正后措辞判定」 |
| 归档后 capability 合并冲突 | 阶段 4 | `verify_openspec_delta.sh` 是实门；禁 `--skip-specs` |

---

## 八、角色交接清单（需要你下口令）

| 时点 | 口令 | 我会做什么 |
|---|---|---|
| 进入阶段 1 | `切换到 planner` | 只改 4 份 proposal 的 AC 措辞（1.1–1.4）+ 跑阶段 1 验证 |
| 进入阶段 2 | `切换到 builder` | 按 2.1→2.5 顺序、**每笔单独 commit**、每笔跑验证 |
| 进入阶段 3 | `切换到 verifier` | 第二轮复核（只看上轮不通过项 + 新增 diff） |
| 阶段 4 | `git push` 需单独确认 | 归档 + 复核；**push 前等你确认** |

> **阶段 0.3 / 0.4**（补「已批准」状态 + 提交报告）需要 builder/general 角色；可与阶段 1 合并在同一次 `切换到 builder` 里做（报告 commit 与 planner 的 AC 改动互不依赖，但**建议报告先落**，因为阶段 1 的措辞以报告为依据）。

---

## 九、审查后调整（oracle + metis · 2026-11-18）—— **本节优先于 §一–§八**

> 三方审查（oracle 咨询 · metis gap 分析）共提出 30+ 条，逐条自查后**采纳 11 条**（其余为描述性或不适用）。
> **阶段 1 已按 A1–A4 二次订正**（`git diff` 现有 12 处 AC/任务行改写）；**阶段 2 按 A5–A9 执行**。

| # | 调整 | 依据（已自查） | 落点 |
|---|---|---|---|
| **A1** | **阶段 1 二次订正：AC17 的计数口径** —— 原写「库层实现 TU 内**每个**公开函数恰 1 处调用」对 `check_products` **恒假**（它是入口，实测 `src/rtl_emit/product_check.cpp` 命中 2/2/1 = 定义+调用）⇒ 改为「`product_modules`·`tamper_hier` 各 1 处、`check_products` 0 处」并写死可执行计数 | metis | ✅ 已改 `proposal.md` AC17 · `tasks.md` F3 同步 |
| **A2** | **阶段 1 二次订正：可判化** —— ① change2 **AC4** 的「两文件 MUST NOT 含由 `emitter_args` 透传的 `-`」**不可判/不可满足**（argv 必然含以 `-` 开头的 flag）⇒ 改为「`argv.norm` MUST NOT 含**恰好等于 `-` 的 token**」② change3 **AC25/F8** 的「基线扁平产物」绑定到 **`eq/flat/top.v`** ③ change1 **AC8** / change2 **AC10** 的 `28 == 10 ∪ 18` → **`27 == 10 ∪ 17`** ④ change3 **AC10** 删掉「净 0」推导与「AGENTS.md 现写 145」两处拼接残留 | metis | ✅ 已改（5 处） |
| **A3** | **阶段 1 二次订正：AC24 补 ⑧'** —— 蓝图 §4.3.2 的后置条件「护栏 ⑧' MUST 扩到 `kind=soc`」+ §7 **R12 的闭口判据**，原 AC24 **未覆盖**（只写 ⑧）⇒ 已补进 AC24；`tasks.md` **F7 由 ✅ 降为 ☐**（实测：`scripts/verify_examples_rtl_sim.sh:615` 仍 `kind=hier` · self-test 无 soc 行 · `REGISTERED_SOC` 自测锚点 0） | oracle | ✅ 已改 |
| **A4** | **阶段 2 增一笔 `B0`**：`docs/CHANGELOG.md` 补「`tests/test_axi_lite_verilator_e2e.cpp` 的 TEST_CASE 名随 DUT 改名（`…AxiLiteTopPortAccessSnapshot` → `…AxiLiteSlavePortAccessSnapshot`）」—— 这是 **D1 批准项**，原 §三 的 5 笔**漏收**，而新 AC5 现在**要求**它 ⇒ 不补则阶段 3 的 AC5 必红 | oracle | 阶段 2（新 commit） |
| **A5** | **阶段 2 增一笔 `F7 补漏`**：⑧' 条件 `kind=hier` → `kind ∈ {hier, soc}` + self-test 增 **soc fixture 行** + `REGISTERED_SOC` **0 → 1** + **正交负向对照**（`RTL_SIM_TAMPER=flatten_soc` ⇒ 必红）⇒ 判据：self-test 投毒数 **18 → 20**、新投毒各自转红、生产轮绿。**单独 commit**（回滚预案同 B2）⇒ 落地后请 **architect** 翻蓝图 §7 R12 的「落实」状态（architect 写集，builder 不代改） | oracle | 阶段 2（新 commit） |
| **A6** | **B2 的投毒改正交形态**：不能沿用「`emitter_args` 置 `-`」（既有**投毒17** 会先让 ③ 发射失败 ⇒ 新断言到不了 ⇒ 该 case 以**错误原因**变红）⇒ 改为 `RTL_SIM_TAMPER=argv_norm_dash`（发射**成功后**向 `argv.norm` 追加一行 `-`）⇒ ③ 仍绿、只有新断言红。判据：`argv.txt`/`argv.norm` **各 ≥1 行** + `argv.txt` 含 `--emit-rtl` + `grep -qx -- '-' <work>/argv.norm` 期望非零 + 在 dump 前 `rm -f` 旧文件 | metis | 阶段 2 的 B2 |
| **A7** | **B1 / B3 / D3 的判据补严**：B1 的 grep 作用域加 `include/` 与 `docs/**/*.md`（且禁 `grep -v tb_` 一刀切）；B3 改为「`退役`或`缩面` + change 名 + `tests/test_axi4_full_burst.cpp` **同行同现**」；D3 加 3 条前置（`test -x` 该二进制 / `disabled_examples.txt` 的 `cut -f1` 能解析到 / 输出 `STALE-ARTIFACT` 计数 == 0） | metis | 阶段 2 |
| **A8** | **阶段 2 全量验证换成 CI guard 全集**（原清单漏 6 个）：补 `verify_examples_rtl_sim_coverage.sh`（**AC8/AC10 点名的那一个**）· `verify_example_test_wiring.sh` · `verify_flat_rtl_emit.sh` · `verify_arch_ledger_residuals.sh` · `verify_dual_backend_parity.sh` · `verify_guard_reachability.sh` · `verify_rv32i_fw_init_rtl.sh`（D3 登记后证明「登记 ≠ 失去覆盖」） | metis + oracle | 阶段 2 全量验证 |
| **A9** | **阶段 3 复核清单加两项**：① AC24/F7 的 ⑧' 落地核验 ② AC5 的**两条** CHANGELOG 登记（D1 的 e2e 改名行 + B3 的退役行）。另：`§四` 的「10 条」改为派生表述（change1 2 · change2 2 · change3 3 = **7 条 ❌/不成立** + 4 条附注），每条写死判定命令 | oracle + metis | 阶段 3 |
| **A10** | **阶段 4 增两项**：① 归档会**新建** capability `examples-rtl-sim-coverage`（`openspec/specs/` 现无）⇒ 归档后 MUST 补 `## Purpose`（对照 `openspec/specs/rtl-soc-output/spec.md:3` 先例）再跑 `openspec validate --specs` ② 主 specs 有 **2 处悬空引用**（`verilator-backend-e2e/spec.md:174` 的 `::AxiLiteTop`、`verilator-e2e-coverage/spec.md:95` 的 `axi4_full_example`）⇒ 二选一：做成 change 3 的 `MODIFIED Requirements`，或登记为**接受项**（AC15 作用域显式排除 `openspec/**` ⇒ 需显式裁决，不得静默）。另：归档顺序**先做 CLI 探针**（change1/2 的 delta 都是对「仅存在于未归档上游 change」的既有 requirement 的 ADDED + 两 change 的 G-1/G-3 都写「前置 change 先归档」） | oracle + metis | 阶段 4 |
| **A11** | **阶段 5 backlog 补 3 条**：① D2 的「今后两套基线」登记要求（原路径**无执行者** ⇒ 会蒸发）② F4 第二消费者归一化（真实载体 = `tests/test_hierarchy_verilog.cpp` 的内存字符串计数 + 护栏 ⑧'' 产物文件，**不是** `test_rv32i_hier_rtl.cpp`——该文件**存在**（29496 B）且已是「文件 → 字符串」形态（`read_file` → `module_count`）⇒ 其 module 计数**可** drop-in 换成 `product_modules(path).size()`，但同文件的 `module_header` 端口表比对**无对应 API** ⇒ 收益 1 行 vs 丢掉端口表判据 ⇒ 记 follow-up；⚠️ **2026-11-18 两次事实订正**：原文「该文件不存在」= 错、改写「内存字符串形态」= 也错）③ `emit_rtl_rv32i_soc_fw_init` 的**归位**（D3 只登记归属，真正的「并入 example 或退役」另立项） | oracle + metis | 阶段 5 |
| **A12** | **计数订正**：§0 的「4 个新文件」→ **5 个**（`git status` 实测）；`iteration.json` 只保留「以 `tasks.md` 为准」一个选项（**不要**新建该文件——本仓 5 份既有 verify 报告都按 tasks.md 处置） | metis | 本节 |

### 调整后的阶段 2 执行序（7 笔 · 每笔单独 commit）

| 序 | 笔 | 判据（可机械判定） |
|---|---|---|
| 1 | **B1** 一行注释（change1 AC16） | 旧名在 `include/ examples scripts tests docs/**(递归) AGENTS.md` **零命中**（`docs/CHANGELOG.md` 白名单） |
| 2 | **B0** CHANGELOG：e2e TEST_CASE 改名行（D1） | `AxiLiteSlavePortAccessSnapshot` 在 CHANGELOG **≥1 行**且同期出现「改名/DUT」语义词 |
| 3 | **B3** CHANGELOG：`axi4_full_example` 退役行（AC27） | 「退役」+ `example-file-boundary-2026-11` + `tests/test_axi4_full_burst.cpp` **同行同现** |
| 4 | **D3** 登记 `emit_rtl_rv32i_soc_fw_init`（AC10/AC9 的「绿」前提） | `test -x` 该二进制 ∧ `cut -f1` 能解析到 ∧ `run_all_ported_tests.sh` **exit=0** ∧ `STALE-ARTIFACT` = 0 ∧ `verify_rv32i_fw_init_rtl.sh` 仍绿 |
| 5 | **F7 补漏** ⑧' → soc + soc fixture + 锚点 0→1 + 投毒（AC24 / 蓝图 R12 闭口） | `verify_examples_rtl_sim.sh --self-test` 投毒 18→**20** 且新投毒转红；生产轮绿；`REGISTERED_SOC` 自测锚点 = 1 |
| 6 | **B2** argv 双证据 + 正交自检（change2 AC4） | 两文件各 ≥1 行 · `argv.txt` 含 `--emit-rtl` · `argv.norm` 无**孤立 `-` token** · 新投毒 19→**20**（与投毒17 正交） |
| 7 | **D5** tasks.md 逐条回填（3 份） | 每行 ✅ 附一条命令/路径证据 · `F9` ⇒ N/A（蓝图 §4.3.1 符号行 == 9）· `F4` ⇒ follow-up（写真实载体） |

---

## 十、**D6-1 裁决**（用户 2026-11-18）—— 覆盖 §五（阶段 4）

> **裁决原文**：用户选 **D6-1**（critic 的 plan 档 BLOCKER 的处置）。
> **理由（critic 已排除另两项）**：D6-2 = 修订已签批 gate（留债）；D6-3 = 归档未验收的上游 change（违反「验收后才归档」）。

### 阶段 4 改为「**只归档 change 3**」

| # | 动作 | 前置判据 |
|---|---|---|
| 4.1 | `openspec archive example-file-boundary-2026-11` | 阶段 2/3 全绿；`verify_openspec_delta.sh` 绿；其 delta = **2 个 `MODIFIED`**（`dual-backend-test-coverage` / `verilator-backend-e2e` 均已存在）+ **1 个 `ADDED`**（`example-component-boundary` 新建）⇒ **零 forward reference**（已核实） |
| 4.2 | 归档后补 `## Purpose`（`example-component-boundary` 是新建 capability） | 对照 `openspec/specs/rtl-soc-output/spec.md:3` 先例；之后跑 `openspec validate --specs`（**非** strict，作 ADR 判据面） |
| 4.3 | 主 specs 的 2 处**悬空引用**改由 change 3 的 delta 覆盖或**显式登记为接受项** | `openspec/specs/verilator-backend-e2e/spec.md:174`（`::AxiLiteTop`）· `verilator-e2e-coverage/spec.md:95`（`axi4_full_example`）⇒ 二选一必须**留痕** |
| 4.4 | `git push` | **等用户确认** |

### change 1 / change 2 **挂起**（不归档）

| 项 | 处置 |
|---|---|
| 阻塞原因 | ① change 1 **G-1** 的可执行前置 `test -f openspec/specs/examples-rtl-sim-coverage/spec.md` **不满足**（该 capability 尚不存在）② change 2 **G-3** 要求「前置 change 先归档」③ 两者 delta 引用的「**既有 requirement**」只存在于**未归档**的上游 change 的 delta（7 条） |
| 解除条件 | 上游 `rtl-verification-single-source-2026-11` **验收通过 + 归档**（独立 commit）之后 ⇒ change 1 → change 2 依次归档 |
| 本轮不做的后果 | change 1/2 的代码与验证已完成并保持在工作区/主线上；只是**不落正典**（`openspec/specs/` 不出现 `examples-rtl-sim-coverage`）。**无返工**，仅延后 |
| 留痕位置 | 本节 + `.rddf/reviews/decisions-required-2026-11-18.md`（待补 D6 行）+ 归档时 change 1/2 的 proposal 需加 dated addendum（届时由 planner 执行） |

---

## 十一、阶段 2 执行实况（builder · 2026-11-18）—— **已闭**

| 序 | 笔 | commit | 判据结果 |
|---|---|---|---|
| 0 | 报告 + 裁决清单 + 路径（阶段 0） | `34e2225` | 5 份 `??` 已入库 |
| 0' | AC 措辞（阶段 1 · 12 处） | `f59dcde` | 3 份 `validate --strict` exit 0 · delta 自检 4 绿 |
| 1 | **B1** 一行注释（AC16） | `549163f` | 旧名在作用域内零命中（含 `include/` 与 docs 递归） |
| 2 | **B0** CHANGELOG：e2e 用例名改名（D1 漏项） | `563fbc7` | `AxiLiteSlavePortAccessSnapshot` ≥1 行 |
| 3 | **B3** CHANGELOG：`axi4_full_example` 退役（AC27） | `dbc0ab4` | 三 token 同行同现 |
| 4 | **D3** 登记 fw_init 发射器（AC9/AC10 的「绿」前提） | `f389c3c` | `run_all_ported_tests` **exit 0** · PASS 27 / FAIL 0 · 无 UNTRACKED · `verify_rv32i_fw_init_rtl` 仍绿 |
| 5 | **F7 补漏** ⑧' → `kind ∈ {hier, soc}`（蓝图 §7 R12 闭口） | `0395cfc` | 生产轮 soc 行 `⑧' module 数 5`；self-test **18 投毒 + 2 必绿** |
| 6 | **B2** argv 双证据 + 正交自检（AC4） | `ac48bef` | `argv.txt` 5 token 含 `--emit-rtl` · `argv.norm` 3 token 无孤立 `-`；self-test **19 投毒 + 2 必绿** |
| 7 | **D5** tasks.md 账实回填（3 份） | `3f9fc1f` | change1 **30/30** · change2 **30/30** · change3 除 F4（follow-up）/F9（N/A）外全 ✅ |
| 附 | **补登记** guard-reachability（双向 1:1 缺口） | `b34dba5` | 该缺口由 A8「换成 CI guard 全集」抓到（属上一批 `b9da984` 的漏登记） |

### 全量验证（A8 的 CI guard 全集）

```
ctest -LE perf                       258/258 通过（base 233 · verilator_e2e 77）
verify_examples_rtl_sim.sh           PASS（12 项）· --self-test PASS（19 投毒 + 2 必绿）
verify_example_only_headers.sh       PASS · --self-test PASS（6 投毒）
verify_no_duplicate_rv32i_headers    PASS · --self-test PASS
verify_public_api_visibility.sh      PASS · --self-test PASS
verify_roadmap_fragments.sh          PASS · --self-test PASS
verify_example_assertion_quality.sh  PASS · verify_assertion_quality_fixtures.sh PASS
verify_input_port_required_registry  PASS · verify_rst_n_wiring.sh PASS
verify_example_test_wiring.sh        PASS · verify_examples_rtl_sim_coverage.sh PASS（27 == 10 ∪ 17）
verify_guard_reachability.sh         PASS（51 被引用脚本 · 双向 1:1）
verify_flat_rtl_emit.sh              PASS · verify_dual_backend_parity.sh PASS
verify_arch_ledger_residuals.sh      PASS
verify_hierarchical_rtl.sh           PASS · --self-test PASS（12/12 投毒）
verify_port_naming.sh                PASS · verify_test_counts.sh PASS · verify_agents_size.sh PASS
verify_openspec_delta.sh             PASS
run_all_ported_tests.sh              **exit 0**（PASS 27 / FAIL 0 / STALE-ARTIFACT 0）
```

### 遗留（交阶段 3 / 阶段 4）

| 项 | 处置 |
|---|---|
| 蓝图 §7 **R12** 的「落实」状态仍是「⚠️ 未闭」 | **architect 写集**（builder 不代改）⇒ 需把「落实」翻为已闭，闭口证据 = `0395cfc` 的 ⑧' 覆盖 |
| 阶段 3 复核清单（A9） | AC24/F7 的 ⑧' 落地 + AC5 的**两条** CHANGELOG 登记（`563fbc7` / `dbc0ab4`） |
| 阶段 4（D6-1） | **只归档 change 3**；change 1/2 挂起至上游验收 + 归档 |

---

## 十二、归档链路收口（2026-11-19）—— **4 个 change 全部归档**

### 12.1 上游 change 的验收（verifier 两轮）

| 轮次 | 结论 | 3 条 finding 的处置 |
|---|---|---|
| 第 1 轮（`8df27c6`） | **20 ✅ / 2 ⚠️ / 1 ❌** ⇒ 不通过 | **AC18** ❌（`implementation_gap`：目标集内 1 处字面命中 + AC 要求的「加一句『产品面已具名』」未做 + 教学段主体事实已过时）· **AC17** ⚠️（终值 1415 被后续 change 改为 1384）· **AC23 ②** ⚠️（口径歧义） |
| 修复 | builder `73cb612` + planner `efc383e` + 收尾 `3f84e34` | AC18：usage_guide §5.8 段改具名形态 + 对照表按产物 `port_table` 更新（12 行集合相等）+ `examples/axi4/AGENTS.md:27` 改措辞 · AC17：dated addendum · AC23 ②：口径澄清（产物端口名 vs `.verif` 引用的 C++ 成员名） |
| 第 2 轮（`20ed1d2`） | **3 条全闭合 ⇒ 23/23** | 新增 diff = 4 笔文档/proposal 改动（零 `src/**`）⇒ 无新 BLOCKER |

### 12.2 归档顺序（D6-1 的解除链）

| 序 | change | 门 | 合并 | 归档 commit |
|---|---|---|---|---|
| 1 | `rtl-verification-single-source-2026-11`（上游） | — | **新建** `examples-rtl-sim-coverage`（7 reqs）+ 2 capability 各 +1 + `rtl-soc-output` +1/~2 | `3e2df5a` |
| 2 | `example-file-boundary-2026-11` | — | **新建** `example-component-boundary`（3 reqs）+ 2 个 capability 的 MODIFIED | `137fb59` |
| 3 | `axi4-verif-driven-cpp-2026-11` | **G-1** ✓（capability 已存在 + 上游归档独立 commit） | `examples-rtl-sim-coverage` **+2** | `27b1e2d` |
| 4 | `emit-cli-into-example-2026-11` | **G-3** ✓（前置 change 已独立 commit 归档） | `examples-rtl-sim-coverage` **+1** | 本批最后一笔 |

⇒ `openspec/changes/` 下**已无活跃 change**；`examples-rtl-sim-coverage` 现 **10 条** requirement（7 + 2 + 1）· 无重复标题 · `## Purpose` 已写（非 TBD）。

### 12.3 归档后的合并校验（逐条，不是抽查）

- 10 条 ADDED/MODIFIED 在 delta 侧与主 spec 侧**逐字相同**（唯一差异 = CLI 补的尾部空行）⇒ 无内容丢失 / 重复
- 4 个 capability 的主 spec **无重复标题**；`validate --specs` ⇒ **51 passed / 0 failed**；新 capability strict ⇒ valid
- capability 数 49 → **51**（AGENTS.md 同步）

### 12.4 遗留（交用户裁决 / 另立项）

| 项 | 状态 |
|---|---|
| **`verify_examples_rtl_sim.sh --self-test` 挂进 CI** | ✅ **已实施**（`c6b0a0b`，2026-11-19）⇒ 「⑧' 被改回 `kind=hier` ⇒ 自动红」现在有**系统性保证**（CI 常驻 · 本机实测 ≈ 1 分 48 秒）⇒ ⇒ 蓝图 §7 R12 的行内四式锚点降级为「快速复算」而非唯一守护 |
| `git push` | ⛔ 本批 20 笔提交**全在本地 `main`**，未 push ⇒ **等用户确认** |
| 上游 change 的 `## Purpose`（新建 capability 之外的 3 个） | 已存在 TBD（`build-cpp-standard` 等 8 个既有 spec 同样如此）⇒ 仓级既有问题，非本批引入 |
