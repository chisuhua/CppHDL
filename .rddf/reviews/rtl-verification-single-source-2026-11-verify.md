# 验收报告 —— `rtl-verification-single-source-2026-11`

- **验收者**：verifier（`source: user` · session `ses_e123fdf50ffekNVuofg777ESL3`）
- **日期**：2026-11-19 · **HEAD**：`85ef6f7`（工作区干净）
- **已用轮次**：**1 / 3**（本 change 的第 1 轮验收）
- **判据来源**：`openspec/changes/rtl-verification-single-source-2026-11/proposal.md` 的 `## 验收标准`（**23 条 AC**）
- **验证方式**：**本地验收**（未走 `rdd-verifier` skill —— 分流判据不满足：`.rddf/state/iteration.json` 不存在 ⇒ P3 archive gate 未触发；`rddf rdd-verify --dry-run` ⇒ `No eligible changes to verify (empty queue)`）
- **结论**：**20 ✅ / 2 ⚠️ / 1 ❌** ⇒ **不通过**（AC18 未闭）⇒ **不可归档**

---

## 一、逐条判定

| AC | 判定 | 证据（本轮实跑） |
|---|---|---|
| **AC1** | ✅ | `grep -rn "toVerilogHierarchical" --include='*.cpp' examples/`（除白名单 `examples/riscv-mini/tests/test_rv32i_hier_rtl.cpp`）⇒ **零命中**；白名单行号 `:161` / `:181` 实测仍是那两个调用（`auto r1 = ch::toVerilogHierarchical(p1, …)` / `auto r2 = …(p2, …)`）。⚠️ 附注：change 3 把机制下沉后，库层 `src/rtl_emit/cli.cpp:209` 成为唯一调用点 —— 它是**发射器实现**，不属「发射入口 `.cpp`」语义 |
| **AC2** | ✅ | 护栏生产轮：`✅ ⑧⁗ 顶层端口身份 1:1（具名 · cpp_member 唯一 · 与 port_table 集合相等）`（载体 `scripts/lib/verif_tb_shape_check.py:58-97`，判定域 = `role ∉ {clock,reset}` ∧ `is_top_io`） |
| **AC3** | ✅ | 护栏生产轮：`✅ ⑥'' 期望源锚点 == 验证源 scenario（7 个）`（期望源 = `.rddf/checks/examples-rtl-expectations.md#axi4-lite-example`） |
| **AC4** | ✅ | 生成的 TB（CLI 重渲染）：`grep -c 'top_unnamed_'` = **0**；护栏 `✅ ⑧ 四件套：manifest port_table[] == 产物端口表（逐字逐序）` ⇒ 连线数 == `port_table[]` 且逐条名 ∈ 集合 |
| **AC5** | ✅ | 生成 TB 的 `grep -c '// TODO:'` = **0**；`grep -c scenario` = **7**（≥1） |
| **AC6** | ✅ | CLI 重渲染 TB → `cmp` 与仓内 `examples/axi4/tb_axi4_lite_example.sv` ⇒ **逐字节相同**；护栏 `✅ ⑥' 生成物新鲜度：逐字节重现 ✓` |
| **AC7** | ✅ | **三方 = 113**：静态展开（CLI `--emit-tb-verif` 打印 `VERIF-CHECKS: 113`）== `.md` 人登记（`.rddf/checks/examples-rtl-expectations.md:527` `VERIF-CHECKS: 113`）== 运行期（护栏 ⑥：`RESULT: checks=113 fails=0`） |
| **AC8** | ✅ | 载体 = `examples/axi4/tests/test_verif_axi4_lite.cpp`：`TEST_CASE("verif: 两后端的 check_id 集合相等（解释器 vs 渲染式 TB）")`（:90，断言 `interp_ids == tb_ids`）+ `TEST_CASE("verif: 刻意失败注入 ⇒ 解释器与渲染式 TB 报同一条 check_id")`（:130）⇒ `ctest -R verif` **2/2 Passed**。⚠️ 附注：`.sv` 侧**未**实现 `RESULT: check <id> <pass\|fail>` 协议（渲染器零命中），走的是 **AC8 明文允许的降级路径**（集合相等 + 同一次失败注入）；`VerifRunResult::check_lines`（`include/rtl_emit/verif.h:96`）是解释器侧的集合载体 |
| **AC9** | ✅ | **3 条时序变异是常驻用例**（不是手工记录）：`test_verif_axi4_lite.cpp:292` M1（删 AW 后一拍 ⇒ `wready` 转红）· `:303` M2（复位后少等一拍 ⇒ `bvalid` 转红）· `:316` M3（DUT 侧 `rdata` 延迟 1 拍 ⇒ 读回值转红）⇒ ctest Passed；**护栏负向对照 ≥5** ⇒ `--self-test` **19 条投毒各自转红 + 2 条必绿仍绿**（含「必绿：中间插列」） |
| **AC10** | ✅ | `tests/test_verif_single_source.cpp` 的 3 个 fail-closed 用例（集合外原语 / 嵌套 REPEAT / 文法外 token）⇒ ctest Passed |
| **AC11** | ✅ | self-test 的「**必绿：中间插列（值非空）**」仍绿（列号由 `#COLUMNS` 推导 ⇒ 插列不破） |
| **AC12** | ✅ | 护栏生产轮：`✅ ⑥⁗ 生成覆盖度三方一致（generated=yes）` 与 `（generated=no）` |
| **AC13** | ✅ | `bash scripts/verify_examples_rtl_sim.sh` ⇒ **PASS（12 项）**；`--self-test` ⇒ **PASS（19 投毒 + 2 必绿 · 还原复绿）** |
| **AC14** | ✅ | `ctest -LE perf` ⇒ **258/258 通过**（base 233 · verilator_e2e 77 · chlib 1 · spinalhdl-ported 24），`ctest -L verilator_e2e` = **77** 条全绿 |
| **AC15** | ✅ | `bash scripts/verify_test_counts.sh` ⇒ PASS；`CTEST_TOTAL` = **269**（`266 → 269` 落在 `5164188`，实测 `git log -S"CTEST_TOTAL: 266"` 命中该 commit）；`VERILATOR_E2E_TOTAL` = **77**，自 `790f8a5` 起**未变** ⇒ 该子判定已完成 |
| **AC16** | ✅ | 「禁 `EXPECT-SRC` → `.cpp`」判据块在 **4 个 ref 间逐字相同**：基线 `5164188^`（上游 change 实施前，块在行 **209**）→ `5164188` → `55f3f5a` → `0395cfc` → `ac48bef` → `HEAD`（块在行 **294**）⇒ **内容未改**（行号漂移属正常） |
| **AC17** | ⚠️ | 护栏 `bash scripts/verify_port_naming.sh` ⇒ **PASS**；但**数字已被后续 change 超越**：豁免表实测 `08298a0~1` = **1415**（= AC 写的终值 ✓ 交付时正确）→ `08298a0`（change 3 的 A7 退役 `axi4_full_example`）= **1384**（−31 行，护栏锚点同步为 1384）⇒ **AC 需 dated addendum**（不是实现缺口） |
| **AC18** | ❌ | **精确目标集内仍有 1 处命中**：`examples/axi4/AGENTS.md:27`（「顶层端口**已具名** ⇒ 产物端口名 = `top_<C++ 成员名>`（**不再是** `top_unnamed_input_N`）」⇒ 语义合规但字面 grep 命中）；**AC 明确要求的动作未做**：`docs/usage_guide/05-component-design.md`（白名单教学段）**未加**「产品面已具名」一句（`grep -c "产品面已具名"` = **0**）⇒ 且该段**主体事实已过时**（:1018 起仍写「端口名是 `top_unnamed_input_N`」+ 12 行旧对照表，而 change 3 的 B2 已把 32 个顶层端口具名）；白名单行号漂移（`:1017` 现为空行，真实命中行 = 78 / 522 / 533 / 698 / 702 / 1018-1031） |
| **AC19** | ✅ | `git show 08298a0^:include/axi4/axi_soc_example_top.h` 与现 `examples/axi4/axi_soc_example_top.h` 的 `AxiLiteTestMaster` 段 ⇒ 端口声明**逐字相同**（diff 空）；端口数 = **14**（`ch_in/ch_out` 声明），基线同 ⚠️（AC 写「23 条」，口径与实测不同 —— **判据「未变」成立**） |
| **AC20** | ✅ | 时序锚点构例 = M1 的 needle `"EXPECT awready 1\nTICK 1\nSET awvalid 0"`（`test_verif_axi4_lite.cpp:294`）⇒ 改序即转红 ⇒ 「TICK(1) 后 EXPECT 寄存器输出」的顺序**真的参与判据**（非恒绿装饰） |
| **AC21** | ✅ | `test_verif_axi4_lite.cpp:325`「verif: AC21 —— 端口名拼错 MUST fail-closed 且错误文本可区分」⇒ 断言 `res.error.find("未命中") != npos`（:342）⇒ ctest Passed；实现侧 `src/rtl_emit/verif_run.cpp:160`（「端口 `x` 未命中（未具名 / 拼写错误 / 不是顶层端口）」）+ `src/simulator.cpp:268`（未命中返 `false`/`nullopt`，**MUST NOT** 静默返 0） |
| **AC22** | ✅ | tsv = **13 列**（`#COLUMNS` 含 `emitter_args`），12 个数据行字段齐；`tb_axi4_lite_example` / `tb_axi_soc_example` 两行 `emitter_args = --emit-rtl`，其余 10 行 = `-`；护栏 `✅ ③' argv 双证据（argv.txt N token · argv.norm 无孤立 '-'）` |
| **AC23** | ⚠️ | ① **成立**：期望值在 `.verif`（唯一语义来源）+ 推导在 `.md`（`@expect-src` 锚点）⇒ 改一个期望值只动 2 处；③ **成立**：`examples/axi4/` 5 个文件的头部都能看出角色（`axi4_lite_example_top.h` = 顶层组件 · `axi_soc_example_top.h` = 顶层组件与测试主设备 · `tb_axi4_lite_example.cpp` = 由 `.verif` 验证源驱动 · `tb_axi_soc_example.cpp` = 激励驱动 + 发射入口 · `phase3a_complete.cpp` = Phase 3A）；② **字面不成立**：`.verif` 引用的是 **C++ 成员名**（`awvalid` 在 `.verif` 出现 **18** 次、`bvalid` 21 次）⇒ 改成员名要动 **C++ + `.verif`**（2 处），不是「只动 C++ 一处」⇒ **口径需明确**（若指「产物端口名」则成立，因其由 C++ 成员名派生） |

---

## 二、失败分类与路由

| AC | 形态 | 分类 | 路由 | 修复面 |
|---|---|---|---|---|
| **AC18** | 「保留 + 加一句『产品面已具名』」未执行 + 教学段主体事实已过时（仍写 `top_unnamed_input_N` + 旧对照表） | **`implementation_gap`** | **builder** | ① `docs/usage_guide/05-component-design.md` 的 SoC 端口段：加 dated 注记 + 更新对照表（产物端口列改 `top_<C++ 成员名>`）；② `examples/axi4/AGENTS.md:27` 改措辞（或加入 AC18 白名单） |
| **AC17** | 终值 `1415` 被后续 change（`08298a0`）改为 `1384` | **`proposal_drift`** | **planner** | 给 AC17 加 dated addendum：「交付时终值 = 1415（`08298a0~1` 实测）；`08298a0` 退役 `axi4_full_example` 后为 1384」 |
| **AC23 ②** | 「改一个端口名 ⇒ 只动 C++ 一处」与 `.verif` 引用成员名的事实不符 | **`proposal_drift`** | **planner** | 明确「端口名」口径：`.verif` 引用的 **C++ 成员名**（改它 = C++ + `.verif` 2 处）vs **产物端口名**（改它 = C++ 1 处） |

> **判定不通过的原因**：AC18 是**明确动作未执行**（不是口径歧义）⇒ 按契约「失败结论只需单条带证据的不通过」⇒ 本 change **不通过** ⇒ **不可归档**。
> 其余 22 条成立（20 ✅ + 2 需措辞订正），修复面小且不触碰实现代码。

---

## 三、遗留清扫（本次范围内）

| 项 | 结果 |
|---|---|
| `TODO` / `FIXME` / 临时调试日志 | 新增文件 `include/rtl_emit/{verif.h,cli.h}` · `src/rtl_emit/{verif_scan,verif_run,verif_render,tb_stub}.cpp` · `examples/axi4/tests/test_verif_axi4_lite.cpp` · `tests/test_verif_single_source.cpp` ⇒ 命中 4 处 **全是字面文本**（`cli.h` 的 2 处是「骨架里留 `// TODO:`」的**说明**；`tb_stub.cpp` 的 2 处是**故意生成**骨架的 TODO 行）⇒ **无未完成实现** |
| 被 skip 的测试 | `SKIP(` 命中全部是**环境性跳过**（`verilator not on PATH` / `verilator dlopen did not populate port_access_`）⇒ 既有，非本次引入 |
| 未替换的 mock / 临时 fallback | `src/rtl_emit/**` + `include/rtl_emit/**` 零命中 |
| 临时文件 | `/tmp/opencode/vfy/**`（未跟踪）；无仓内残留 |
| **未同步的用户文档** | ⛔ `docs/usage_guide/05-component-design.md` 的 SoC 端口段仍陈述旧形态 ⇒ **即 AC18 的失败项** |

---

## 四、归档门状态（**不通过 ⇒ 不可归档**）

| 门 | 状态 |
|---|---|
| AC 全部成立 | ⛔ **AC18 未闭**（另 AC17 / AC23 需措辞订正） |
| `openspec validate <change> --strict` | exit 1 —— **仅** 2 条 `>500 字符` requirement 的 strict-only 规则（`hierarchical-port-naming-guard` / `rtl-hierarchical-output`），仓级既有红，**与本 change 无关**（AGENTS.md 已登记该漂移） |
| `openspec validate <change>`（非 strict） | **valid**（exit 0） |
| `scripts/verify_openspec_delta.sh` | **4 条全绿** |
| delta 与主 spec 冲突 | **零**：3 个 ADDED 的 requirement 标题均不与主 spec 撞名；2 个 MODIFIED 标题逐字匹配；`examples-rtl-sim-coverage` 为新建 capability（7 reqs） |
| 归档后的连带效果 | 新建 `examples-rtl-sim-coverage` ⇒ **这正是 change 1 的 G-1 前置** ⇒ 上游归档后 change 1 才具备归档条件 |

---

## 五、复现命令

```bash
# AC1 / AC16 / AC19（diff 类）
grep -rn "toVerilogHierarchical" --include='*.cpp' examples/ | grep -v test_rv32i_hier_rtl.cpp   # ⇒ 空
git show 08298a0^:include/axi4/axi_soc_example_top.h | diff - <(cat examples/axi4/axi_soc_example_top.h)  # 端口声明

# AC2–AC6 / AC12 / AC13（护栏类）
bash scripts/verify_examples_rtl_sim.sh                       # ⇒ PASS（12 项）
bash scripts/verify_examples_rtl_sim.sh --self-test            # ⇒ PASS（19 投毒 + 2 必绿）

# AC7 / AC8（静态展开 vs 登记 vs 运行期）
D=$(mktemp -d); ./build/examples/axi4/tb_axi4_lite_example --emit-rtl --out "$D" \
    --emit-tb-verif examples/axi4/tb_axi4_lite_example.verif --tb-verif-out "$D/tb.sv" | grep VERIF-CHECKS   # ⇒ 113
grep -m1 "VERIF-CHECKS" .rddf/checks/examples-rtl-expectations.md                                  # ⇒ 113
cmp "$D/tb.sv" examples/axi4/tb_axi4_lite_example.sv                                                 # ⇒ 逐字节相同

# AC9 / AC10 / AC20 / AC21（测试类）
ctest --test-dir build -R verif --output-on-failure    # ⇒ 2/2 Passed

# AC14 / AC15
ctest --test-dir build -LE perf --output-on-failure    # ⇒ 258/258
bash scripts/verify_test_counts.sh                     # ⇒ PASS

# AC18（失败项）
grep -rn "只发扁平\|top_unnamed_input" examples/axi4/ .rddf/checks/examples-rtl-expectations.md \
  .rddf/checks/soc-rtl-expectations.md                  # ⇒ 1 处命中（examples/axi4/AGENTS.md:27）
grep -c "产品面已具名" docs/usage_guide/05-component-design.md   # ⇒ 0（AC 要求的一句未加）
```
