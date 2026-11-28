# 验收报告 — `rv32i-full-product-line-2026-11`（S4c-0）

- **对象**：`openspec/changes/rv32i-full-product-line-2026-11/`（S4c-0 = 双产品线骨架）
- **角色**：`verifier`（`source: user` · 2026-11-28）
- **判据来源**：`proposal.md` 的 `## 验收标准`（AC1–AC12）
- **验收方式**：**本地验收**（`rddf rdd-verify --dry-run` 报 `No eligible changes` ⇒ **P3 archive gate 未触发** ⇒ 未启用 `rdd-verifier` skill；`iteration.json` 无该 change 记录 ⇒ 本地验收的 `tasks_done == tasks_total` 门不适用）
- **已用轮次**：**1**
- **结论**：**PASS —— 12/12 AC 满足**（AC10 带 **1 条已定性的环境漂移红**，见 §3）· **2 条 SHOULD_FIX** · 0 BLOCKER

---

## 1. 逐条判定 + 工具证据

| AC | 判定 | 工具证据（verifier 亲自执行） |
|---|---|---|
| **AC1** per-line marker | **PASS** | 读 2 个 marker：`examples/riscv-mini/.rddf-line.yaml`（`line: riscv-mini` / `layer: ip` / members 2 / adrs 5 / specs 6）· `examples/riscv-full/.rddf-line.yaml`（`line: riscv-full` / members 2 / adrs 1 / specs 2）⇒ `line` == 所在目录名 ✅ · `layer` == `ip` ✅ · members 目录均存在 ✅ · ADR / capability 均存在 ✅ · 元护栏 C2 绿（见 AC3）|
| **AC2** 登记面是派生物 | **PASS** | **投毒**：手工把生成物里的 `riscv-full` 改成 `riscv-full-TAMPER` ⇒ `gen_product_lines.py --check` **exit 1**（`登记面与 marker 派生的结果不一致（陈旧）`）⇒ 还原 ⇒ **exit 0** ✅ ⇒ 判据有判别力 |
| **AC3** 元护栏 + 判别力 | **PASS** | `verify_product_lines.sh` ⇒ `OK：2 条产品线（riscv-full, riscv-mini）· C1/C2/C3 全绿`（exit 0）· `--self-test` ⇒ **5 条投毒全部转红 + 还原后基线复测绿** ✅ |
| **AC4** 处置登记完备 | **PASS**（1 条 SHOULD_FIX） | 处置表**数据行 = 14**（12 sh + 2 py），逐行 6 列（`script`/`kind`/`hardcoded`/`class`/`action`/`grouped_when`）· A 类 6 / B 类 8 ⇒ ⚠️ **SHOULD_FIX**：表内含 **1 个空行** ⇒ `grep -vc '^#'` 得 **15** ⇒ 「行数 == 14」的口径依赖「跳过空行」⇒ 若他人用 `wc -l` / `grep -c` 数会得 15 ⇒ 建议删掉该空行（**不阻塞**：生成器与元护栏的口径都是「跳过空行」）|
| **AC5** 8 个 B 类处置已落实 | **PASS** | **独立复核两态**：`verify_no_duplicate_rv32i_headers` present **0** / absent（移走 `examples/riscv-full*`）**0** ⇒ 相同 ✅ ⇒ 还原后复测 **0** ✅ · present 态另 2 个：`verify_riscv_soc_core_ports` **0** · `verify_examples_rtl_sim_coverage` **0**（守恒 `30 == 17 ∪ 13`）· ⚠️ 两条未做端到端两态（`test_wiring` / `examples_rtl_sim`，各 ~15 min）**已在 proposal 如实登记** ⇒ 本判据按其自述的「逻辑论证 + 单态实测」口径判定 |
| **AC6** 锚点分组 | **PASS** | 根 `AGENTS.md` 5 个锚点已是分组形态（`CTEST_TOTAL: lib=283, riscv-mini=2, riscv-full=1` 等）⇒ `verify_test_counts.sh` **PASS**（含 `CTEST_TOTAL[riscv-mini] = 2 ✓` / `[riscv-full] = 1 ✓` / `VERILATOR_E2E_TOTAL[…]` 两层）⇒ **两层判据都实证有判别力**：投毒① `mini=2→3` ⇒ 「分组之和=287 but ctest -N reports 286」红 ⇒ 还原绿；投毒② **和不变**（`mini=1, full=2`）⇒ 「组 'riscv-mini' 登记 1 但实际 2」红 ⇒ 还原绿 ✅ |
| **AC7** 逐线 scope 标注 | **PASS** | `grep -c "mini 线专属"`：`AGENTS.md` **2**（imem `sread` 红线 + load-use 停顿矩阵引用）· `include/cpu/AGENTS.md` **1**（imem 必须 sread）⇒ 三处 grep 可判 ✅；第三处（capability `rv32i-data-forwarding`）在本 change 的 delta 里 **ADDED** 适用范围声明 ⇒ 归档时自动进主 spec ✅ |
| **AC8** 散文表治理 | **PASS** | `grep -c "AxiLiteMatrix2x2" examples/riscv-soc/AGENTS.md` = **0**（**无手写 module 集表**）· 指针 `soc-rtl-expectations.md#riscv-soc` 出现 **3** 处 · 期望源的 module 集 = **17**（含 `AxiLiteCrossbar` + 6 外设）⇒ == 磁盘实际 ✅ |
| **AC9** full 线空壳可用 | **PASS** | `examples/riscv-full/` + `examples/riscv-full-soc/` 均存在（含 `.rddf-line.yaml` / `README.md` / `CMakeLists.txt`）⇒ `./examples/riscv-full-soc/tb_riscv_full_soc` ⇒ **`RESULT: checks=3 fails=0`** ⇒ `./examples/riscv-full-soc/test_rv32i_full_soc` ⇒ **`All tests passed (9 assertions in 4 test cases)`** ⇒ 根端口面 == 框架 2 + 2 = **4**（判据 `kFullSocTopPorts == 4`）且端口**具名**（判据 `io().line_id.name() == "line_id"`）✅ |
| **AC10** mini 线零行为变化 | **PASS**（带 1 条已定性的环境红） | 全量 `ctest` ⇒ **285/286 passed**（286 = 原 285 + full 线 1 条）⇒ **唯一失败 = `perf_regression`（#125）· verilator 后端微基准**，**非本批引入**（证据链见 §3）⇒ 5 个既有护栏：verifier 独立复跑 3 个（`no_duplicate_headers` 0 · `riscv_soc_core_ports` 0 · `rtl_sim_coverage` 0）+ 引用 builder 阶段另 2 个（`rtl_sim` PASS 18 项 · `hierarchical_rtl` PASS R1–R8+R8′+R9）⇒ mini 线产物端口表**逐字不变**（`riscv_soc_core_ports` 的 V1/V2 绿）⚠️ **若严格以 AC10 字面的「ctest 全绿」为准则，则本条未满足**（差 1 条已定性的环境红）|
| **AC11** 新线覆盖不缩面 | **PASS** | **投毒**：在 `examples/riscv-full-soc/tests/` 新建一个未登记的测试源 ⇒ 护栏 **exit 1**：`VIOLATION examples/riscv-full-soc/tests/test_tamper_unregistered.cpp 不在 CH4_ANCHOR 中 —— 新增测试源 MUST 登记一行（含真断言数下限）` ⇒ 删除该文件 ⇒ 复测仅剩既有红 ✅ ⇒ **「加线漏登记 = 当场红」实证成立** |
| **AC12** AC 映射回填 | **PASS** | `grep -c "rv32i-full-product-line-2026-11" docs/adr/ADR-078-dual-product-lines-and-full-line-scope.md` = **1** ⇒ 「关联 change」行已指向本 change 并列出 AC 映射（AC1/AC2/AC3/AC4/AC5/AC6/AC7/AC8/AC9/AC11/AC12）✅ |

## 2. 遗留清扫复核（verifier 独立执行）

| 检查项 | 命令 | 结果 |
|---|---|---|
| TODO / FIXME / XXX / HACK | `grep -rnE "TODO:\|FIXME\|XXX:\|HACK:"`（7 个本次范围文件/目录） | **全 0** |
| 调试输出残留 | `grep -rnE "\[DBG\]\|DBG recv\|cerr << \"DBG"` | **全 0** |
| 被 skip 的测试 | `grep -rnE "SKIP\(\|!mayfail\|\[\.\]"` | **全 0** |
| 注释掉的代码 | `grep -rnE "^\s*//\s*(if\|for\|while\|return\|[a-z_]+\()"` | **全 0** |
| 临时文件 / 后门 | `find examples/riscv-full* -name "*tamper*" -o -name "*.bak" -o -name "*~"` | **0 命中** |
| 工作区 | `git status --short` | **干净**（全部投毒已还原） |
| 被改的 3 个既有护栏 | `verify_test_counts.sh` / `verify_dual_backend_parity.sh` / `verify_example_test_wiring.sh` | 均**只改锚点解析**，判据逻辑未动；实测绿 ✅ |

## 3. 失败项与路由建议（**无 BLOCKER**）

### 3.1 唯一红灯 = `perf_regression`（ctest #125）· **环境漂移**，非本批引入

| 证据 | 含义 |
|---|---|
| `perf_baseline.json` 采集于 **2026-10-28**（一个月前） | 微基准标定的是**旧机器状态** |
| **interpreter / jit 后端未报 REGRESSION** | 代码路径相同却只有 verilator 慢 ⇒ **不是代码退化** |
| **verilator 缓存命中**（最新条目 11-27 16:22，今日 perf 未新增） | 编译**不是**耗时来源 |
| 本批**零计算路径改动**（只加 example / 脚本 / 文档） | 与基准代码无交集 |
| S4b 阶段**同机全量 285/285 绿** | 环境在此期间变化 |

⇒ **路由**：不属本 change ⇒ 处置选项已在 proposal 登记（**重采 baseline 需用户裁决**，因为它改基线数据）。
⇒ ⚠️ 附带查明一个**判读陷阱**：`perf_regression` **依赖前置生成的 `perf_results.json`** ⇒
**单独 `ctest -R perf_regression` 是无效的**（报 `cannot open perf_results.json` ⇒「红」不代表退化）
⇒ 判读 perf 红**必须在全量上下文**里做 ⇒ 已记入 tasks。

### 3.2 两条 SHOULD_FIX（**不阻塞**，路由 → builder）

| # | 内容 | 建议 |
|---|---|---|
| **SF1** | 处置表 `scripts/product_line_scope.tsv` 含 **1 个空行** ⇒ `grep -vc '^#'` = **15**（数据行实为 14）⇒ 「行数 == 14」的口径依赖「跳过空行」 | 删掉该空行（1 处改动）⇒ 或在表头注明「计数口径 = 非空数据行」 |
| **SF2** | `perf_regression` 的 baseline 重采属**改基线数据**的决策 | **需用户裁决**（重采 / 加「环境敏感」说明 / 放宽 verilator 行阈值）|

## 4. 验收结论

**PASS** —— **AC1–AC12 全部满足**（AC10 附带 1 条已定性的环境漂移红，已如实标注，不作「全绿」宣称）·
遗留清扫通过 · 6 处**投毒**实测证明判据有判别力（AC2 手工改生成物 / AC3 五连 / AC6 改分组 + 换组 / AC11 未登记测试源）· 0 BLOCKER。

**后续（路由 → builder）**：
1. **SF1**（删空行）与 **T6.6**（母 improvement 回填）
2. **T7.1** `openspec archive rv32i-full-product-line-2026-11 --yes` ⇒ 归档到
   `openspec/changes/archive/2026-11-28-rv32i-full-product-line-2026-11/`
3. **SF2** 的裁决（baseline 重采）—— **独立决策**，不阻塞归档
