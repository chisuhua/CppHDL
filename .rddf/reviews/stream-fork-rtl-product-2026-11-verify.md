# 验收报告：`stream-fork-rtl-product-2026-11`（stream_fork 进 RTL 产品面）

- **验收日期**：2026-11-20
- **验收角色**：verifier（**builder 的角色内建完成门** · 非 subagent）
- **结论**：✅ **通过**（AC1–AC12 逐条达成 · 零回归 · 变异自证 3/3 转红）

## 1. 判据来源（按优先级）

| 优先级 | 来源 | 内容 |
|---|---|---|
| 1 | **用户本次显式拍板** | 「参考 examples/axi4 的改进，继续改进 examples/stream 下的示例产品化」+「继续 demux / arbiter / fork」 |
| 2 | `openspec/changes/stream-fork-rtl-product-2026-11/proposal.md` §验收标准 | AC1–AC12 |
| 3 | tasks.md | A1–A6 · B1–B5 · C1–C5 |
| 4 | 既有 capability | `examples-rtl-sim-coverage`（四件套）· `chlib-stream-ready-ownership`（分流 / 分叉类 ready 语义）· ADR-057 |

⚠️ **`rdd-verifier` skill 分流三条件**：① change 存在 ✓；② `proposal.md` 有 `## 验收标准` ✓；
③ P3 archive gate **未触发**（未归档、`iteration.json` 无该 change）⇒ ⇒ **走本地验收**（不走 skill）。

## 2. 逐条判定与证据

| AC | 判据 | 判定 | 证据 |
|---|---|---|---|
| AC1 | 12 根端口全部具名 · 无 `top_unnamed_*` | ✅ | 护栏 ⑧⁗ `顶层端口身份 1:1（具名 · cpp_member 唯一 · 与 port_table 集合相等）`；manifest `ports[].cpp_member` 以 `unnamed` 开头者 = **0** |
| AC2 | `--emit-rtl --out <dir>` ⇒ exit 0 + `top.v` + `manifest.json` | ✅ | 实跑 `exit=0`；目录含 `top.v` / `manifest.json` / `tb_stream_fork_demo.sv` |
| AC3 | module 集 `{top}`（1 个）· 顶层端口 **14** | ✅ | `grep -c '^module' top.v` = **1**；`manifest.json` 的 `port_table` = **14**（框架 2 + 根自有 12）；护栏 ⑧ 逐字逐序 PASS |
| AC4 | 独立 TB 编译通过 · `RESULT: checks=26 fails=0` | ✅ | 护栏 ④⑤⑥ PASS；`tb_stream_fork_demo：RESULT: checks=26 fails=0` |
| AC5 | 清单 +1 行 · `REGISTERED_TARGETS == 14` · 守恒 `27 == 14 ∪ 13` | ✅ | `verify_examples_rtl_sim_coverage.sh` → `① 守恒：被跟踪 27 == 已覆盖 14 ∪ 已登记落选 13`；`awk` 数 `kind ∈ {target,dut}` = **14** |
| AC6 | 期望源段手推三项 + `VERIF-CHECKS: 26` | ✅ | `.rddf/checks/examples-rtl-expectations.md#stream-fork-demo`（module 集 / 端口 14 / 26 条 / 4 个 scenario 锚点）；护栏 ⑥'' `期望源锚点 == 验证源 scenario（4 个）` |
| AC7 | `interp_checks == min_checks == 26` | ✅ | 护栏 ⑨ `解释器条数相等（checks=26 fails=0）`；tsv 行 `26 / 26` |
| AC8 | 默认模式只跑 `.verif`；未知参数 exit 2 | ✅ | `tb_stream_fork_demo` → `RESULT: checks=26 fails=0`（exit 0）；`--bogus` → **exit 2** |
| AC9 | 四道护栏 + 全量回归 | ✅ | `verify_examples_rtl_sim.sh` PASS（16 项）· `verify_examples_rtl_sim_coverage.sh` PASS · `verify_port_naming.sh` PASS（1506 端口 / 1377 合规豁免，新增 12 端口全合规）· `run_all_ported_tests.sh` PASS（27/27）· `verify_test_counts.sh` PASS（271/78 未变）· `verify_docs_links.sh` / `verify_agents_size.sh` / `verify_openspec_delta.sh` PASS |
| AC10 | `openspec validate --strict` valid | ✅ | `Change 'stream-fork-rtl-product-2026-11' is valid`（skip_specs 申报） |
| AC11 | 变异自证 ≥ 3 条全部转红 | ✅ | 见 §2.2（M1/M2/M3，**均先确认 `compile_rc=0`**） |
| AC12 | AND 语义两面都判 | ✅ | `.verif` 场景 2 = `EXPECT in_ready 1`（三路全高）；场景 3 = `out1_ready=0` ⇒ `EXPECT in_ready 0` ⇒ ⇒ 「OR」「只看第一路」的实现会红（M1 实测转红即证） |

### 2.1 零回归

| 面 | 证据 |
|---|---|
| examples 运行护栏 | `PASS: 27 FAIL: 0 MISSING: 0 STALE-ARTIFACT: 0`（旧 `stream_fork_demo` 二进制已清理） |
| 计数锚点 | `CTEST_TOTAL 271` ✓ · `VERILATOR_E2E_TOTAL 78` ✓ |

### 2.2 变异自证（判别力 · 打在临时产物上 · 仓内产物零改动）

| # | 变异 | 实测 | 抓的是 |
|---|---|---|---|
| M1 | `assign top_in_ready = 1'b1;`（AND 断成恒就绪） | ✅ `fails=2`（`reset_defaults#6` · `ready_is_and_of_outputs#0`） | ready 的 AND 语义 |
| M2 | `assign top_out1_valid = 1'b0;` | ✅ `fails=2`（`broadcast_payload_and_valid#1` · `ready_is_and_of_outputs#2`） | valid 广播 |
| M3 | `assign top_out1_payload = 8'd0;` | ✅ `fails=2`（`broadcast_payload_and_valid#4` · `ready_is_and_of_outputs#5`） | payload 广播 |

⇒ ⇒ 还原后复跑 ⇒ `RESULT: checks=26 fails=0`（记录在期望源段）。

## 3. 失败项与路由建议

**无**。

## 4. 遗留清扫（本次范围内）

| 项 | 结果 |
|---|---|
| TODO / FIXME / XXX / 注释掉的代码 / 临时日志 | `grep` 零命中（`stream_fork_top.h` · `tb_stream_fork_demo.cpp` · `tb_stream_fork_demo.verif` · `AGENTS.md`） |
| 误留的标签 / 死代码 | 实施期发现 `stream_fork_top.h` 里误留一个 `demux_like_ready_override:` 标签 ⇒ **已在提交前删除** |
| 被 skip 的测试 | 无（未新增 Catch2 用例） |
| 未替换的 mock / 临时 fallback | 无 |
| 旧 target 残留 | `build/examples/stream/stream_fork_demo` 已删除 |

## 5. 已用轮次

- **第 1 轮**（本次）：AC 全通过 ⇒ 无需打回。
