# 验收报告：`stream-arbiter-rtl-product-2026-11`（stream_arbiter 进 RTL 产品面）

- **验收日期**：2026-11-20
- **验收角色**：verifier（**builder 的角色内建完成门** · 非 subagent）
- **结论**：✅ **通过**（AC1–AC13 逐条达成 · 变异自证 3/3 转红 · 附一处**会话前既有红**的登记）

## 1. 判据来源（按优先级）

| 优先级 | 来源 | 内容 |
|---|---|---|
| 1 | **用户本次显式拍板** | 「参考 examples/axi4 的改进，继续改进 examples/stream 下的示例产品化」+「继续 demux / arbiter / fork」 |
| 2 | `openspec/changes/stream-arbiter-rtl-product-2026-11/proposal.md` §验收标准 | AC1–AC13 |
| 3 | tasks.md | A1–A5 · B1–B5 · C1–C3 · D1–D6 |
| 4 | 既有 capability | `examples-rtl-sim-coverage`（四件套）· `chlib-stream-arbitration`（轮转 / 无饿死 / **相位不入契约**）· `chlib-stream-ready-ownership`（ready 归属） |

⚠️ **`rdd-verifier` skill 分流三条件**：① change 存在 ✓；② `proposal.md` 有 `## 验收标准` ✓；
③ P3 archive gate **未触发**（未归档、`iteration.json` 无该 change）⇒ ⇒ **走本地验收**（不走 skill）。

## 2. 逐条判定与证据

| AC | 判据 | 判定 | 证据 |
|---|---|---|---|
| AC1 | 16 根端口全部具名 · 无 `top_unnamed_*` | ✅ | 护栏 ⑧⁗ `顶层端口身份 1:1（具名 · cpp_member 唯一 · 与 port_table 集合相等）`；manifest `ports[].cpp_member` 以 `unnamed` 开头者 = **0** |
| AC2 | `--emit-rtl --out <dir>` ⇒ exit 0 + `top.v` + `manifest.json` | ✅ | 实跑 `exit=0`；目录含 `top.v` / `manifest.json` / `tb_stream_arbiter_demo.sv`；`--emit-rtl` 缺 `--out` ⇒ **exit 2** |
| AC3 | module 集 `{top}`（1 个）· 顶层端口 **18** | ✅ | `grep -c '^module' top.v` = **1**；`manifest.json` 的 `port_table` = **18**（框架 2 + 根自有 16）；护栏 ⑧ 逐字逐序 PASS |
| AC4 | 独立 TB 编译通过 · `RESULT: checks=38 fails=0` | ✅ | 护栏 ④⑤⑥ PASS（Verilator 编译 + 运行）；`tb_stream_arbiter_demo：RESULT: checks=38 fails=0` |
| AC5 | 清单 +1 行 · `REGISTERED_TARGETS == 15` · 守恒 `27 == 15 ∪ 12` | ✅ | `verify_examples_rtl_sim_coverage.sh` → `① 守恒：被跟踪 27 == 已覆盖 15 ∪ 已登记落选 12`；`awk` 数 `kind ∈ {target,dut}` = **15** |
| AC6 | 期望源段手推三项 + `VERIF-CHECKS: 38` | ✅ | `.rddf/checks/examples-rtl-expectations.md#stream-arbiter-demo`（module 集 / 端口 18 / 38 条 / 5 个 scenario 锚点 / **相位口径段**）；护栏 ⑥'' `期望源锚点 == 验证源 scenario（5 个）` |
| AC7 | `interp_checks == min_checks == 38` | ✅ | 护栏 ⑨ `解释器条数相等（checks=38 fails=0）`；tsv 行 `38 / 38` |
| AC8 | 默认模式只跑 `.verif`；未知参数 exit 2 | ✅ | `tb_stream_arbiter_demo` → `RESULT: checks=38 fails=0`（exit 0）；`--bogus` → **exit 2** |
| AC9 | 多道护栏 + 全量回归 | ✅ | `verify_examples_rtl_sim.sh` PASS（17 项）· `verify_examples_rtl_sim_coverage.sh` PASS · `verify_port_naming.sh` PASS（1522 端口 / 1377 合规豁免，新增 16 端口全合规）· `run_all_ported_tests.sh` PASS（27/27）· `verify_test_counts.sh` PASS（271/78 未变）· `verify_docs_links.sh` / `verify_agents_size.sh` / `verify_openspec_delta.sh` / `verify_example_test_wiring.sh` PASS |
| AC10 | `openspec validate --strict` valid | ✅ | `Change 'stream-arbiter-rtl-product-2026-11' is valid`（skip_specs 申报） |
| AC11 | 变异自证 ≥ 3 条全部转红 | ✅ | 见 §2.2（M1/M2/M3，**均先确认 `compile_rc=0`**） |
| AC12 | **相位不入契约** | ✅ | `.verif` 里 `selected` 的字面量断言**只有两处**：`single_valid_channel_held` 的 `EXPECT selected 1`（**唯一**有效通道 ⇒ 扫描起点无论落在哪都落到它 ⇒ **与相位无关**）与 `no_valid_no_grant` 的 `EXPECT selected 0`（无有效 ⇒ `grant=0` ⇒ **与相位无关**）⇒ ⇒ **无**任何依赖轮转相位的字面量断言 ⇒ ⇒ 多路场景一律用 `WAIT in*_ready <紧界>` 表达「效果属性」；期望源「⚠️⚠️ 相位口径」段逐字记录了「**不锁相位**」的理由与 `WAIT` 紧界的推导 |
| AC13 | **codegen 修复**：零 `WIDTHTRUNC` + 其余产物文本不变 | ✅ | 见 §2.3 |

### 2.1 零回归

| 面 | 证据 |
|---|---|
| ctest 全量 | `100% tests passed, 0 tests failed out of 271` |
| examples 运行护栏 | `PASS: 27 FAIL: 0 MISSING: 0 STALE-ARTIFACT: 0`（旧 `stream_arbiter_demo` 二进制已清理） |
| 计数锚点 | `CTEST_TOTAL 271` ✓ · `VERILATOR_E2E_TOTAL 78` ✓ |
| RTL 产物面 | `verify_hierarchical_rtl.sh` PASS（R1–R8）· `verify_flat_rtl_emit.sh` PASS（F1–F8 · 含「剥掉 lint_off 后仍干净」） |

### 2.2 变异自证（判别力 · 打在临时产物上 · 仓内产物零改动）

| # | 变异 | 实测 | 抓的是 |
|---|---|---|---|
| M1 | `assign top_out_valid = 1'b0;` | ✅ `fails=3`（`single_valid_channel_held#1/#8` · `backpressure_stalls_upstream#4`） | 输出 valid |
| M2 | `assign top_selected = 2'h0;` | ✅ `fails=3`（`single_valid_channel_held#0/#7/#11`） | 授予索引 |
| M3 | `top_rr_last_grant <= 4'h0;`（**轮转反馈恒 0** ⇒ 退化为固定优先级） | ✅ `fails=9`（**5 条 `WAIT` 超时** + 4 条 payload 判据） | **轮转 / 无饿死**（capability 记载的**历史缺陷形态**） |

⇒ ⇒ 还原后复跑 ⇒ `RESULT: checks=38 fails=0`（记录在期望源段）。
⚠️ **M3 是本行的核心判别力**：它证明「**相位无关**的判据照样抓得住轮转失效」
⇒ ⇒ 「不锁相位」**不等于**「没有判别力」。

### 2.3 codegen 修复（AC13）

| 项 | 内容 |
|---|---|
| 根因 | hygiene 模式只把**操作数**宽度写成显式 cast（`print_binary_op`）⇒ ⇒ **结果宽度**未对齐 ⇒ ⇒ `mod` 节点声明宽（框架按「模字面量 ⇒ `bit_width(N-1)`」定宽 = 2 bit）< 运算自然宽（`max(两侧)` = 3 bit）⇒ ⇒ 赋值处**隐式截断** ⇒ ⇒ Verilator `WIDTHTRUNC` × 5 ⇒ ⇒ **卫生产物编译失败** |
| 修法 | `print_binary_op` 补「**结果宽度对齐**」：仅当「运算自然宽 > 节点声明宽」时把截断写成显式 cast ⇒ ⇒ 产物 `assign x = a % 3'h4;` → `assign x = 2'(a % 3'h4);` ⇒ ⇒ **语义逐位不变**（显式 ≡ 隐式截断） |
| 判别力 | 修复前：arbiter 产物 `verilator --binary` **compile_rc=1**（5 warning）；修复后：**compile_rc=0** 且 `RESULT: checks=38 fails=0` |
| 零副作用 | 「运算自然宽 > 声明宽」只可能出现在 `mod` / `div` 的字面量特例 ⇒ ⇒ 比较族（自然宽 1 == 声明宽 1）· 加减（`w+1` == 声明宽）· 位运算（`w` == 声明宽）**一律不触发** ⇒ ⇒ 其余算子产物文本**一字不变** ⇒ ⇒ 实测：既有 16 个产品的护栏（`verify_examples_rtl_sim.sh` / `verify_flat_rtl_emit.sh` / `verify_hierarchical_rtl.sh`）**全绿** |

## 3. 失败项与路由建议

**无**（本 change 范围内）。

⚠️ **附：一处【会话前既有】的红（只登记，不修）**

`scripts/verify_hierarchical_p8.sh` 的 R4② 报「活账登记的头**不存在**：
`include/axi4/axi4_lite_example_top.h` / `include/axi4/axi_soc_example_top.h`」。

- **与本 change 无关**的证据：`git ls-tree c5fb777 include/axi4/` ⇒ 这两个头在**本次会话前**
  就**已不存在**（commit `08298a0` · `example-file-boundary-2026-11` 把设计定义头
  归位到 `examples/axi4/`）⇒ ⇒ 该 guard 的活账与「已移除/已改名」登记
  （`.rddf/checks/public-api-removals.tsv`）**语义相冲** ⇒ ⇒ 本次会话**未触碰**
  `include/axi4/` 与该账 ⇒ ⇒ 该红**在本次会话前即存在**。
- **处置**：按「范围外的历史遗留只记录、不趁机清理」⇒ ⇒ **本 change 不修**
  ⇒ ⇒ 归为**独立立项**（护栏的活账口径 vs 移除登记的语义相冲）。

## 4. 遗留清扫（本次范围内）

| 项 | 结果 |
|---|---|
| TODO / FIXME / XXX / 注释掉的代码 / 临时日志 | `grep` 零命中（`stream_arbiter_top.h` · `tb_stream_arbiter_demo.cpp` · `tb_stream_arbiter_demo.verif` · `AGENTS.md` · `src/codegen_verilog.cpp` 的新增段） |
| 半截实现 / 临时 fallback | 无（`natural_binary_width` 的每个分支都有注释口径） |
| 被 skip 的测试 | 无（未新增 Catch2 用例） |
| 未替换的 mock | 无 |
| 旧 target 残留 | `build/examples/stream/stream_arbiter_demo` 已删除；`git grep stream_arbiter_demo` 仅剩**有意**的转移说明 |

## 5. 已用轮次

- **第 1 轮**（本次）：AC 全通过 ⇒ 无需打回。
