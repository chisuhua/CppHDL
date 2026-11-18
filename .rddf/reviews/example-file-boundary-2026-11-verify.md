# 验收报告 —— `example-file-boundary-2026-11`

- **验收者**：verifier（`source: user` · session `ses_e123fdf50ffekNVuofg777ESL3`）
- **日期**：2026-11-18
- **HEAD**：`ddb0bbb`
- **已用轮次**：**1 / 3**
- **判据来源**：`openspec/changes/example-file-boundary-2026-11/proposal.md` 的 `## 验收标准`（26 条 + AC27 / AC28 = **28 条**）；architecture 蓝图 `docs/architecture/2026-11-17-rtl-verification-single-source.md` §4.3 / §7 为 AC22 / AC23 / AC26 的载体
- **结论**：**ac-fail** —— 23 条通过 · **1 条不通过**（AC5）· **2 条不成立/取证不全**（AC17 部分不成立 · AC25 基线取证不全）· 1 条 N/A（AC26）· 3 条附注（AC10 / AC18 / AC22 / AC27 的口径说明）

---

## 一、逐条判定 + 证据

### 3.1 批次（AC1–AC16 · AC20 · AC21）

| AC | 判定 | 取证（命令 ⇒ 输出摘要） |
|---|---|---|
| **AC1** | ✅ | `ls include/axi4/*example_top*` ⇒ 无；`ls examples/axi4/*example_top*` ⇒ `axi4_lite_example_top.h` / `axi_soc_example_top.h` |
| **AC2** | ✅ | `grep -rn 'AxiLiteTop\|AxiSocTop\|AxiLiteTestMaster' include/` ⇒ **0** |
| **AC3** | ✅ | `grep -rn '"axi4/axi4_lite_example_top\.h"\|"axi4/axi_soc_example_top\.h"' examples tests` ⇒ **0** |
| **AC4** | ✅ | `bash scripts/verify_no_duplicate_rv32i_headers.sh` ⇒ PASS（C3 / C2 / C7 全绿）；`--self-test` ⇒ **全部通过**，含新增两条：`✅ 注释里的 examples/ MUST NOT 误报（exit=0）` · `✅ 指向 examples/axi4 的反向 -I MUST 被抓（exit=1）` |
| **AC5** | ❌ | `git diff 55f3f5a..HEAD -- tests/test_axi_lite_verilator_e2e.cpp`：**TEST_CASE 名被改**（`AXI4-Lite VerilatorBackend E2E AxiLiteTopPortAccessSnapshot` → `…AxiLiteSlavePortAccessSnapshot`），而 AC 明写「路径 / **TEST_CASE 名** / SECTION / 断言**零变化**」，`tasks.md` B1 同样写「路径 / 用例名 / SECTION / 断言*」。其余变更在 AC 允许集内（删顶层头 include · `using` 改库组件 · DUT 类型 · 注释订正）；断言数 14 == 14，无 SECTION 增删 |
| **AC6** | ✅ | `ctest -R test_axi_lite_verilator_e2e` ⇒ `Test #26 … Passed 0.66 sec`（本机 verilator 在 `build/verilator-install/bin`） |
| **AC7** | ✅ | `grep -rn 'axi4_lite_example_top\.h\|axi_soc_example_top\.h' tests/` ⇒ **0** |
| **AC8** | ✅ | TEST_CASE 名集合：拆分前 `tests/test_verif_single_source.cpp` = **13** 个；拆分后（`tests/` + `examples/axi4/tests/test_verif_axi4_lite.cpp`）= **13** 个；**双向差集 = ∅**。共享 helper 全在 `examples/axi4/tests/`（`verif_path` ×9 · `LateAxiLiteTop` ×3 · `read_file` ×5，`tests/` 侧 0）。`axi4_catch_test()` 宏带 `LABELS "base" TIMEOUT 300`（`examples/axi4/CMakeLists.txt:56`），`ctest -N -L base` 含两条 |
| **AC9** | ✅ | `ctest -N` 含 `Test #1 test_axi_lite_test_master` · `Test #199 test_verif_single_source` · `Test #2 test_verif_axi4_lite` |
| **AC10** | ✅（附注） | 实测：`ctest -N` **269** · `-LE perf` **258** · `-L verilator_e2e` **77** · `-L base` **233** · `tests/test_*.cpp` **159** · `EXAMPLES_TEST_SOURCES` **20**；`verify_test_counts.sh` PASS（锚点 == 实测）。⚠️ AC 字面的 268 / 257 / 231 / 158 **未计入同 change 的 F2 新增** `tests/test_product_check.cpp`（+1）；AC 自身写明「上面的算术是**推导**，不是实测值 ⇒ **MUST 以实测回填**」⇒ 以实测为准成立，但**字面数字需 planner 更新**（否则后续 verifier 会误判） |
| **AC11** | ✅（附注） | `.rddf/checks/port-naming-exempt.tsv` 在 `55f3f5a..08298a0` 的 numstat = **23 增 / 54 删** ⇒ 23 行只改第一列路径（23+/23−）+ 31 行删除（A7）= 54 删；算术 1415 − 31 = **1384** == `EXPECTED_EXEMPT_PORTS=1384`；`verify_port_naming.sh` PASS。⚠️ 两段在同一次提交内落地（"3.1 先、A7 后"只在 numstat 上可复原，不能重演两段独立运行） |
| **AC12** | ✅ | `EXPECTED_PROBES=51` · `EXPECTED_REMOVAL_ROWS=30`；`.rddf/checks/public-api-removals.tsv` 新增 5 行：`AxiLiteTop` / `AxiSocTop` / `AxiLiteTestMaster`（`removed` · `scope=include`）+ 两个头的 `nofile` 行；`verify_public_api_visibility.sh` PASS（含 `--self-test`） |
| **AC13** | ✅ | 登记表 3 行保留且判别性用例已改指新落点（`:44-46`）；`scripts/lib/input_port_registry.py:140` `for base in (ROOT/"include", ROOT/"examples")`；`verify_input_port_required_registry.sh` PASS；`verify_rst_n_wiring.sh` PASS（per-TEST-CASE 覆盖零缩面） |
| **AC14** | ✅ | `CH4_ANCHOR` 新增 2 行（`examples/axi4/tests/test_axi_lite_test_master.cpp|…|10` · `test_verif_axi4_lite.cpp|…|41`，实测值）；`verify_example_assertion_quality.sh` PASS + `verify_assertion_quality_fixtures.sh` PASS |
| **AC15** | ✅ | 作用域内旧路径 `include/axi4/(axi4_lite_example_top\|axi_soc_example_top)\.h` 全零命中：三份 `AGENTS.md`（根 / `include/axi4` / `examples/axi4`）· `docs/usage_guide/**` · `.rddf/checks/**` |
| **AC16** | ✅ | `.rddf/checks/product-equivalence-header-move-2026-11-18.md` 记录 4 件 sha256；**我复算**：Lite `top.v` = `20e0f88e…`、`manifest.json` = `cf7b40f6…`（与记录逐字相同）⇒ 与基线一致 |
| **AC20** | ✅ | 根 `AGENTS.md:551-556` 有「`CPPHDL_BUILD_EXAMPLES=OFF` 下的显式接受项」条，点名 `EXAMPLES_TEST_SOURCES` / main() 计数 / `CTEST_TOTAL` 的 `spinalhdl_*` 条目 |
| **AC21** | ✅ | `scripts/verify_example_only_headers.sh` PASS（三态 + 登记表双向 + `EXPECTED_EXEMPT_HEADERS=38`）；`--self-test` **6 条投毒各自成立**：①新头只被 example 消费 ⇒ 红 ②子路径引用被认出 ⇒ 绿 ③登记路径不存在 ⇒ 红 ④登记头新增库侧消费者 ⇒ 红 ⑤孤儿头 ⇒ 不红 ⑥末段同名歧义 ⇒ 不红 |

### 3.2 批次（AC17–AC19 · AC22–AC26）

| AC | 判定 | 取证 |
|---|---|---|
| **AC17** | ❌ **部分不成立** | **成立的部分**：① 库层 3 个公开函数齐备——`product_modules`（`include/rtl_emit/product_check.h:51`）· `tamper_hier`（`:61`）· `check_products`（`:107`）② 值侧 5 项留 example：`kExpectedModules` / `kExpectedTopPorts` 在 `tb_axi_soc_example.cpp`（各 2 处），`include/rtl_emit/**` + `src/rtl_emit/**` **剥注释后**对这些符号 / `0x5A` / `0xA5` / `top.rst_n` / `top.gpio` 的命中 = **0** ③ 库层无阈值（`>= 2` 剥注释后 = 0），根名参数化（`root->name()` 派生；`"top"` 仅出现在 `tb_stub.h:30` / `verif.h:111` 的 **`dut_module` 默认值**——属既有 TB 生成器，不是 checker 的 owner 基准）。**不成立的部分**：「**example 侧对每个的调用 == 1**」实测 `product_modules` = **0**、`tamper_hier` = **0**、`check_products` = **1** ⇒ 与同一 AC 的「**编排整体下沉**」（蓝图 §4.3.1 钉死）**互斥**：编排下沉后前两者只能由库层内部调用 |
| **AC18** | ✅（附注） | `tests/test_product_check.cpp`：10 用例 / 27 断言全绿，且**逐判据**覆盖——`① module 集注入错期望 ⇒ CHECK 1 必红` · `② 端口数注入错期望 ⇒ CHECK 2 必红` · `③ linter 缺失/报脏 ⇒ CHECK 3 必红` · `④ 观测面注入空集 ⇒ CHECK 4 必红`；`④″` 在生产护栏里**仍执行**（`✅ ④″ SoC 产物级判据通过`）；退出码只有 0/1（用法错误按失败计）。⚠️ **CHECK 3 由 `SKIP`（不计失败）改判 `FAIL`（计失败）**——有意偏离 AC 的「已知边界」描述；AC 原文把 SKIP 写成合法语义，需 planner 订正 |
| **AC19** | ✅ | `include/rtl_emit/product_check.h` 已进 `.rddf/checks/example-only-headers.tsv`（`:50`，理由行写明「公开 API（src 实现 TU + tests 用法）⇒ R2：实现 ≠ 使用」）；`docs/CHANGELOG.md:9,32` 有专条。`public-api-surface.md` 未登记——与 AC 的「子目录头不在扫描面 ⇒ 登记到那里零判别力」一致 |
| **AC22** | ✅（附注） | 蓝图 §4.3 段（199 行）内 9 个符号各 ≥1 行；`待定` = **0**；`规则 vs 值` 出现；`tamper_hier` 的「语义不同」标注 3 处；**按 AC 指定的复算命令逐符号取证成功**（12 条命中，总体 exit 0）：值侧 5 项 → `examples/axi4/tb_axi_soc_example.cpp`；规则侧 3 项 → `include/rtl_emit/product_check.h` + `src/rtl_emit/product_check.cpp`；`find_verilator` → example。⚠️ 表内仍保留裸行号列（`行` 列 `:43`/`:50`…）—— 对象文件已删除 ⇒ 不会漂移，但字面上与「禁以裸行号 pin」不一致 |
| **AC23** | ✅ | 蓝图全文中 `483` / `~60` / `415-483` 仅 **1 处**命中：`:450`（§5.1 **N10** 行，估 `examples/axi4/verif_run.cpp` 行数 ⇒ AC 明示豁免）；`.rddf/improvements/rtl-verification-single-source-2026-11.md` 零命中 |
| **AC24** | ✅ | 默认模式（**不加** `--hierarchical`）⇒ `exit=0`，`grep -c '^module ' top.v` = **5**（`AxiLiteTestMaster` / `AxiLiteMatrix2x2_32_32_` / `AxiLiteGpio_8_` / `AxiLiteUart_16_` / `top`）；落盘物 = `{manifest.json, top.v}`；tsv SoC 行 `kind=soc` 且 note 已登记 5 module / 顶层端口 14；护栏 ③ + ⑧ 绿（`verify_examples_rtl_sim.sh` PASS） |
| **AC25** | ⚠️ **取证不全** | **已取证**：① `--check-products` 模式与 `eq/` **零差异**——4 件 sha256 与记录逐字相同（`top.v dc4765566e34ad4f` · `manifest b0019abd22f1bfd8` · `eq/flat 286ecd939707f0a4` · `eq/hier ede8e1698cbd8127`）② `manifest.json` 与基线**逐字节相同**（同一 sha）⇒ 其中的 `port_table` 逐字逐序相同 ③ 默认模式 **5 module** vs `eq/flat/top.v` **1 module** ④ 默认模式落盘物集合 == `{top.v, manifest.json}`。**未取证**：AC 要求的「**默认模式**基线**单独一套**」在 `.rddf/checks/` 内**不存在**——`product-equivalence-header-move-2026-11-18.md` 记录的 SoC 基线用的是 `emit_rtl_axi_soc --hierarchical`（层次化），不是批次 F 之前的**扁平默认**产物 |
| **AC26** | N/A | 该条只在「F1 裁定**无符号可下沉**」时生效；实际切分表**非空**（9 符号、下沉集 = 完整 2 + 拆分 1）⇒ 不适用 |

### 3.2 附加条（AC27 / AC28）

| AC | 判定 | 取证 |
|---|---|---|
| **AC27** | ✅（附注） | `examples/axi4/axi4_full_example.cpp` 不存在；`axi4_full_example` 在 `examples/**`/`tests/CMakeLists.txt` 的 **target 声明**零命中（仅 `examples/axi4/CMakeLists.txt:19` 的退役说明注释）；`ctest -N` 无 `spinalhdl_axi4_full`；`CH_TRACKED_EXAMPLE_TARGETS` = **27**；`.rddf/checks/examples-rtl-sim-exclusions.tsv` 无该行；`port-naming-exempt.tsv` 无 `examples/axi4/axi4_full_example.cpp` 的行；覆盖证据 `tests/test_axi4_full_burst.cpp` ⇒ `Test #161 … Passed 15.26 sec`。⚠️ 「显式登记为缩面」的载体 = `examples/axi4/AGENTS.md:7` + `tasks.md` A7 + 等价记录；**`docs/CHANGELOG.md` 未单列该缩面**（AC 未指定载体，故不判红，但建议补一行） |
| **AC28** | ✅ | ① 默认 `exit=0`、`RESULT: checks=2 fails=0` ② `--emit-rtl --out <d> --emit-hygiene` ⇒ `exit=0`，5 module ③ `--check-products --out <d> --emit-hygiene` ⇒ `exit=0`，CHECK 1–4 全 PASS + `PRODUCTS-CHECK: PASS` ④ `--bogus` ⇒ `exit=2` |

---

## 二、失败项与路由建议

| 项 | 失败形态 | 分类 | 路由 |
|---|---|---|---|
| **AC5** | e2e 测试的 **TEST_CASE 名被改名**（AC 与 B1 都写「用例名零变化」） | `implementation_gap` | **回 builder**：把名字改回 `AXI4-Lite VerilatorBackend E2E AxiLiteTopPortAccessSnapshot`（**或**由 planner 修订 AC5 的允许集，加「TEST_CASE 名含 DUT 名 ⇒ 允许随 DUT 改名」）。⚠️ **先决定走哪条**：改回名字会与新 DUT（`AxiLiteSlave`）语义不符；改 AC 需要 planner 授权 |
| **AC17** | 「example 侧对每个的调用 == 1」不成立（`product_modules`=0 / `tamper_hier`=0） | **`proposal_drift`**（AC 与蓝图 §4.3.1 的「编排整体下沉」互斥，**不可能同时成立**） | **planner** 改 AC17 的调用点口径为「库层实现 TU 内每个公开函数恰 1 处调用 + example 侧对 `check_products` 恰 1 处」——这正是 `tasks.md` 的「需 planner 订正」表里已登记的两条之一 |
| **AC25** | 「默认模式基线」未单独落盘 | **取证不全**（记录缺口，非代码缺陷） | **planner** 二选一：① 把 AC25 的默认模式基线口径改为「以 `eq/flat/top.v` 为扁平参照 + `manifest.json` 逐字节相同 ⇒ 端口面不变」（当前实现已满足）② 要求**今后**同类 change 必须落两套基线。⚠️ 现在已**无法补采**（代码已改） |

> **跨 change 的 AC 冲突（必须由 planner 一次性订正）**：change 3 的 **AC10** 与 change 1 **AC10** / change 2 **AC9** 的「PASS 计数 28 / CTest 268」互相矛盾（差 1 = 本 change AC27 的显式退役、+1 = 本 change F2 新增测试）。三处措辞同批订正，否则每轮验收都会重复命中。

---

## 三、遗留清扫（本次范围内）

| 项 | 结果 |
|---|---|
| 新增的临时产物 | `/tmp/opencode/vfy/{poison,soc*,ac25*,m1,m2,m3,work*,lite*,nojit_run*}` 与 `build-nojit/`（构建目录，未跟踪） |
| 仓内源文件 | 未改动（`git status --short` 空） |
| 被 skip 的测试 | 无新增 SKIP；`test_axi_lite_verilator_e2e` 的 `SKIP("verilator not on PATH")` 为既有环境门，本机未触发 |
| 半截实现 / 注释代码 / 调试日志 | 本次扫描范围内未发现（`product_check.cpp` 的临时 debug 段已在此前批次删除，`grep -c 'TEMP DEBUG'` = 0） |
| 未替换的 mock | 无 |

---

## 四、结构性记账缺口

| 缺口 | 证据 |
|---|---|
| tasks.md 尚有 **2 条 ☐** | `F4`（第二消费者归一化 → follow-up）· `F9`（切分表为空终态 ⇒ **不适用**，但状态仍写 ☐ ⇒ 建议改 `N/A`） |
| 缺 `.rddf/state/iteration.json` | 本地验收的 `tasks_done == tasks_total > 0` 门不可评估 |
| 三份 change 均未归档 | `openspec/changes/archive/` 无同名副本 |

---

## 五、复现命令清单

```bash
# 3.1 —— 头归位 / 依赖方向 / 锚点
ls include/axi4/*example_top* 2>/dev/null; ls examples/axi4/*example_top*
grep -rn 'AxiLiteTop\|AxiSocTop\|AxiLiteTestMaster' include/            # ⇒ 空
grep -rn '"axi4/axi4_lite_example_top\.h"\|"axi4/axi_soc_example_top\.h"' examples tests   # ⇒ 空
bash scripts/verify_no_duplicate_rv32i_headers.sh && bash scripts/verify_no_duplicate_rv32i_headers.sh --self-test

# AC5（唯一修改面：名字）
git diff 55f3f5a..HEAD -- tests/test_axi_lite_verilator_e2e.cpp | grep -E '^[-+].*TEST_CASE\('

# AC8（用例名集合守恒）
python3 - <<'PY'
import re,subprocess
old=subprocess.run(['git','show','55f3f5a:tests/test_verif_single_source.cpp'],capture_output=True,text=True).stdout
new=open('tests/test_verif_single_source.cpp').read()+open('examples/axi4/tests/test_verif_axi4_lite.cpp').read()
n=lambda t:set(re.findall(r'TEST_CASE\(\s*"([^"]+)"',t))
print("差集 =", n(old)^n(new), "| 数量 =", len(n(old)), len(n(new)))
PY

# AC11–AC14（四条登记面护栏）
bash scripts/verify_port_naming.sh
bash scripts/verify_public_api_visibility.sh && bash scripts/verify_public_api_visibility.sh --self-test
bash scripts/verify_input_port_required_registry.sh && bash scripts/verify_rst_n_wiring.sh
bash scripts/verify_example_assertion_quality.sh

# AC16 / AC25（产物等价）
D=$(mktemp -d); ./build/examples/axi4/tb_axi_soc_example --check-products --out "$D" --emit-hygiene
sha256sum "$D/top.v" "$D/manifest.json" "$D/eq/flat/top.v" "$D/eq/hier/top.v"

# AC18
./build/tests/test_product_check --list-tests          # ⇒ 4 项判据各有用例
./build/tests/test_product_check                        # ⇒ 27 assertions in 10 test cases

# AC21
bash scripts/verify_example_only_headers.sh && bash scripts/verify_example_only_headers.sh --self-test

# AC22 / AC23（蓝图）
for s in kExpectedModules kExpectedTopPorts make_inputs make_observations make_stimulus; do
  grep -nE "(^|[^A-Za-z0-9_])$s([[:space:]]*\(|[[:space:]]*=)" examples/axi4/tb_axi_soc_example.cpp; done
for s in product_modules tamper_hier check_products; do
  grep -nE "(^|[^A-Za-z0-9_])$s([[:space:]]*\(|[[:space:]]*=)" include/rtl_emit/product_check.h src/rtl_emit/product_check.cpp; done
grep -n "483\|~60\|415-483" docs/architecture/2026-11-17-rtl-verification-single-source.md   # ⇒ 仅 §5.1 N10（豁免）

# AC24 / AC28
D=$(mktemp -d); ./build/examples/axi4/tb_axi_soc_example --emit-rtl --out "$D" --emit-hygiene
grep -c '^module ' "$D/top.v"        # ⇒ 5
./build/examples/axi4/tb_axi_soc_example --bogus; echo $?    # ⇒ 2

# AC27
ctest --test-dir build -N | grep -c spinalhdl_axi4_full      # ⇒ 0
awk '/^set\(CH_TRACKED_EXAMPLE_TARGETS/{inb=1;next} inb&&/^\)/{inb=0} inb&&!/^[[:space:]]*#/&&NF{print $1}' examples/CMakeLists.txt | wc -l   # ⇒ 27
ctest --test-dir build -R test_axi4_full_burst
```
