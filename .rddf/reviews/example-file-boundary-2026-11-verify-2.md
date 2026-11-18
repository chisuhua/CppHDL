# 第二轮复核 —— `example-file-boundary-2026-11`

- **验收者**：verifier（`source: user` · session `ses_e123fdf50ffekNVuofg777ESL3`）
- **日期**：2026-11-18 · **HEAD**：`67c5b24`
- **已用轮次**：**2 / 3**
- **复核范围**：第 1 轮判 ❌ 的 AC5、判「部分不成立」的 AC17、判「取证不全」的 AC25，以及**因措辞订正而基准变化**的 AC10 / AC18 / AC24；另判新增 diff 是否引入新 BLOCKER。
- **结论**：**AC5 ✅ · AC17 ✅ · AC25 ✅ · AC10 ✅ · AC18 ✅ · AC24 ✅ ⇒ 28 条 AC 全部成立（AC26 = N/A）**

## 一、逐条判定 + 证据

| AC | 第 1 轮 | 本轮 | 证据（本轮实跑） |
|---|---|---|---|
| **AC5** | ❌（TEST_CASE 名被改名） | ✅ | 订正后口径 = 允许「**TEST_CASE 名含 DUT 名 ⇒ 随 DUT 改名**」且**须同批登记**。① 改名事实：`git diff 55f3f5a..HEAD -- tests/test_axi_lite_verilator_e2e.cpp` ⇒ 唯一 TEST_CASE 改动 = `AxiLiteTopPortAccessSnapshot` → `AxiLiteSlavePortAccessSnapshot`；② 登记：`grep -c AxiLiteSlavePortAccessSnapshot docs/CHANGELOG.md` = **1**（`563fbc7`）；路径 / SECTION / 断言零变化（14 == 14，无 SECTION）⇒ 三项全部可机械核 |
| **AC17** | ❌（「example 侧对每个调用 == 1」不成立） | ✅ | 订正后口径 = 库层 TU 内 `product_modules`/`tamper_hier` 各 1 处、`check_products` **0** 处（入口）+ example 侧 `check_products` 1 处。实跑 `grep -cE '(^\|[^A-Za-z0-9_])<sym>\('`：`product_modules` src=**2**（定义+调用）· `tamper_hier` src=**2** · `check_products` src=**1**（仅定义）；example 侧 **0 / 0 / 1** ⇒ 与口径逐项一致 |
| **AC25** | ⚠️ 取证不全（默认模式基线未单独落盘） | ✅ | 订正后口径 = **等价证据**：`eq/flat/top.v` 为扁平参照 + `manifest.json` 逐字节相同。实跑 `--check-products`：顶层产物 module = **5** · `eq/flat/top.v` = **1** · 4 件 sha256 与记录**逐字相同**（`dc476556…` / `b0019abd…` / `286ecd93…` / `ede8e169…`）⇒ 「1 → 5」+ 端口面不变（manifest 含 `port_table`，逐字节相同）均成立 |
| **AC10** | ✅（附注：字面数字过时） | ✅ | 订正后口径 = 实测回填。实跑：`ctest -N` **269** · `-LE perf` **258** · `-L base` **233** · `tests/test_*.cpp` **159** · `EXAMPLES_TEST_SOURCES` **20**；`verify_test_counts.sh` PASS（锚点 == 实测） |
| **AC18** | ✅（附注：CHECK 3 措辞） | ✅ | 订正后口径 = CHECK 3 **NEVER 静默跳过**。实跑：`./build/tests/test_product_check` ⇒ `All tests passed (27 assertions in 10 test cases)`（① module 集 / ② 端口数 / ③ linter 缺失或报脏 / ④ 观测面空集，各一条注入）；生产护栏 `④″` 执行 **1** 次且通过；退出码只有 0/1 |
| **AC24** | ✅ | ✅ | 订正后口径**新增** ⑧' 覆盖 `kind=soc` + soc 负向对照 + self-test soc fixture（`REGISTERED_SOC` 0 → 1）。实跑：`grep -c "kind=hier.*soc"` = **1**（⑧' 条件已含 soc）；soc self-test 清单的 `# REGISTERED_SOC: 1`（`:985`）；`--self-test` ⇒ **19 投毒 + 2 必绿**整轮 PASS，其中**投毒18** = soc 产物压成单 module ⇒ 转红（`⑧' 层次化产物只有`）、**soc 未投毒** ⇒ 仍绿；生产轮 soc 行打印 `✅ ⑧' 层次化形态成立（module 数 5 · 根 = top）` |

> **其余 22 条**（AC1–AC4 · AC6–AC9 · AC11–AC16 · AC19–AC23 · AC27 · AC28）第 1 轮已 ✅；本轮 diff 对它们的影响面 = 护栏脚本（③' 与 ⑧'）、`disabled_examples.txt`、CHANGELOG、任务板 ⇒ 已复跑相关护栏（见 §三）。**AC26 = N/A**（切分表非空：蓝图 §4.3.1 符号行 = 9）。

## 二、新增 diff 是否引入新 BLOCKER

| 检查 | 结果 |
|---|---|
| ⑧' 扩到 soc 会不会**误红**正常 SoC 产物 | 否：生产轮 soc 行 module 数 = 5 ⇒ 绿；self-test 的 soc 绿用例亦绿 |
| ⑧' 扩到 soc 会不会**恒绿** | 否：投毒18（soc 压成单 module）转红，原因文本为 `⑧' 层次化产物只有` |
| ③' 是否影响「落盘物集合」类判据（AC25） | 否：③' 写的是**护栏的 work 目录**，不是发射器 `--out` 目录 |
| `disabled_examples.txt` 的登记是否**掩盖**真问题 | 否：被登记目标的行为覆盖在 `scripts/verify_rv32i_fw_init_rtl.sh`（本轮 PASS：F0–F5）；登记行带理由与归位条件 |
| guard-reachability 的双向 1:1 | `b34dba5` 补齐后 PASS（51 引用脚本）——该缺口属上一批漏登记，**本轮已闭** |

## 三、本轮复跑的护栏（影响面覆盖）

```
ctest -LE perf                        258/258
verify_examples_rtl_sim.sh            PASS（12 项）· --self-test PASS（19 投毒 + 2 必绿）
verify_examples_rtl_sim_coverage.sh   PASS（27 == 10 ∪ 17）
verify_example_test_wiring.sh         PASS
verify_example_only_headers.sh        PASS · --self-test PASS（6 投毒）
verify_no_duplicate_rv32i_headers.sh  PASS · --self-test PASS
verify_public_api_visibility.sh       PASS · --self-test PASS
verify_port_naming.sh                 PASS · verify_test_counts.sh PASS
verify_input_port_required_registry.sh PASS · verify_rst_n_wiring.sh PASS
verify_example_assertion_quality.sh   PASS · verify_assertion_quality_fixtures.sh PASS
verify_guard_reachability.sh          PASS（51 脚本 · 双向 1:1）
verify_flat_rtl_emit.sh PASS · verify_dual_backend_parity.sh PASS · verify_arch_ledger_residuals.sh PASS
verify_hierarchical_rtl.sh            PASS · --self-test PASS（12/12）
verify_roadmap_fragments.sh PASS · verify_agents_size.sh PASS · verify_openspec_delta.sh PASS
run_all_ported_tests.sh               exit 0（PASS 27 / FAIL 0 / STALE-ARTIFACT 0）
```

## 四、遗留清扫（本 change 范围内）

| 项 | 结果 |
|---|---|
| `tasks.md` 账实 | 除 **F4 = follow-up**（真实第二载体已写明）与 **F9 = N/A**（切分表非空）外全 ✅，并附「证据索引」（每批一条复算命令） |
| 临时文件 / dummy | `/tmp/opencode/vfy/**`（未跟踪）；`build-nojit/` 为验收期构建目录（未跟踪） |
| 调试日志 / 注释代码 | `product_check.cpp` 零 `TEMP DEBUG`；本批新增脚本零 `TODO` |
| 被 skip 的测试 | 无新增 |

## 五、归档前提与残留（**不属本 change 的 AC**）

| 项 | 状态 |
|---|---|
| **D6-1**：本批**只归档本 change** | 前置已满足：`validate --strict` exit 0 · delta 自检 4 绿 · 其 delta = 2 `MODIFIED`（capability 已存在）+ 1 `ADDED`（新建 `example-component-boundary`）⇒ **零 forward reference** |
| 归档后 MUST 补 `## Purpose`（新建 capability） | 未做（归档动作的一部分，由阶段 4 执行） |
| 主 specs 的 **2 处悬空引用**（`verilator-backend-e2e/spec.md:174` 的 `::AxiLiteTop`；`verilator-e2e-coverage/spec.md:95` 的 `axi4_full_example`） | **未闭** ⇒ 需二选一：做成本 change 的 `MODIFIED Requirements`，或登记为**接受项**（AC15 作用域显式排除 `openspec/**` ⇒ 必须显式裁决，不得静默） |
| 蓝图 §7 **R12** 的「落实 ⚠️ 未闭」 | 判据面已闭（⑧' 覆盖 soc，`0395cfc`），但**状态行**属 architect 写集 ⇒ 待翻 |

## 六、复现命令

```bash
git diff 55f3f5a..HEAD -- tests/test_axi_lite_verilator_e2e.cpp | grep -E '^[-+].*TEST_CASE\('
grep -c AxiLiteSlavePortAccessSnapshot docs/CHANGELOG.md            # AC5 ⇒ 1
for s in product_modules tamper_hier check_products; do
  printf '%s src=%s example=%s\n' "$s" \
    "$(grep -cE "(^|[^A-Za-z0-9_])$s\(" src/rtl_emit/product_check.cpp)" \
    "$(grep -cE "(^|[^A-Za-z0-9_])$s\(" examples/axi4/tb_axi_soc_example.cpp)"; done   # AC17 ⇒ 2/2/1 · 0/0/1
D=$(mktemp -d); ./build/examples/axi4/tb_axi_soc_example --check-products --out "$D" --emit-hygiene
grep -c '^module ' "$D/top.v"; grep -c '^module ' "$D/eq/flat/top.v"; sha256sum "$D/manifest.json"   # AC25
bash scripts/verify_examples_rtl_sim.sh --self-test                 # AC24 ⇒ 19 投毒 + 2 必绿（含投毒18）
./build/tests/test_product_check                                    # AC18 ⇒ 27 断言 / 10 用例
```
