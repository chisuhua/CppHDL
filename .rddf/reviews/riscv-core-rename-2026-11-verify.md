# 验收报告 —— `rv32i-core-rename-2026-11`（RV32I 核改名 R3a / R3b / R3c）

- **验收者**：verifier（角色内建 · builder 会话内执行）
- **日期**：2026-11-23 · **范围**：本 change 的 AC1–AC15 + 遗留清扫
- **change**：`openspec/changes/riscv-core-rename-2026-11/`（proposal / design / tasks）
- **提交**：`870cda5`（R3a）· `a82eda1`（R3b）· `2210e9d`（R3c）· `26d77d4`（立项）

## 结论

**✅ 全部 15 条 AC 成立** ⇒ 本 change **可归档**。

**核心结论一句话**：核的三个名字（**C++ 类** `Rv32iCoreTop` · **产物模块类型名** `Rv32iCoreTop` ·
**头文件** `rv32i_core_top.h`）已统一；**端口面零变化**（34 根逐字不变）；**行为零变化**
（核侧 flat / 层次化 / 带固件初值三份产物 **sha256 逐字节不变**）。

## 逐条判定

### R3a（模块类型名 · 产物面）

| AC | 判定 | 证据（本轮实跑） |
|---|---|---|
| **AC1** | ✅ | SoC 层次化产物：`grep -c '^module Rv32iCoreTop' top.v` = **1**；`grep -c 'Rv32iCoreComplex' top.v` = **0**；module 集 = `IfStage IdStage ExStage MemStage WbStage HazardUnit ForwardingMux_32_ Rv32iCoreTop SocTraceCounter top`（10） |
| **AC2** | ✅ | `tb_riscv_soc --emit-rtl --check-products --out <dir>` ⇒ `PRODUCTS-CHECK: PASS`（含 module 集相等 + 双形态等价 + 篡改自证 `first_mismatch=cycle 0, observation 'top.store_count'`） |
| **AC3** | ✅ | `scripts/verify_riscv_soc_core_ports.sh` ⇒ **PASS**；`--self-test` ⇒ **PASS**（7 投毒各自必红） |
| **AC4** | ✅ | 核侧三份产物的 `top.v` + `manifest.json` **sha256 与基线逐字节相同**（6/6 IDENTICAL）⇒ 反证「模块类型名只出现在 SoC 侧」 |
| **AC5** | ✅ | SoC 产物 `diff` = **6 行（3 对）**，全部是模块名行（`// ---- module: top.core (Rv32iCoreTop) ----` / `module Rv32iCoreTop (` / `Rv32iCoreTop core (`）；`manifest.json` **零差异** |

### R3b（C++ 类名 · 符号面）

| AC | 判定 | 证据（本轮实跑） |
|---|---|---|
| **AC6** | ✅ | `tb_rv32i_soc` ⇒ `RESULT: checks=24 fails=0`（退出码 0） |
| **AC7** | ✅ | 同 AC4：核侧三份产物 **sha256 逐字节不变** ⇒ 反证「类名不进端口名 / 不进 module 名」 |
| **AC8** | ✅ | 全量重建 RC=0 · `ctest` ⇒ **273/273 全绿** · `run_all_ported_tests.sh` ⇒ **PASS 29 / FAIL 0** · `verify_node_names_single_source.sh` ⇒ **PASS**（构造默认实例名改了 ⇒ 模拟日志节点前缀 `rv32i_pipeline_top.*` → `rv32i_core_top.*`，护栏仍绿） |
| **AC9** | ✅ | `.rddf/checks/public-api-surface.md`「已移除 / 已改名的公开符号」新增 **2** 行（类改名 + 头文件改名）⇒ `verify_public_api_visibility.sh` ⇒ **PASS**，`--self-test` ⇒ **PASS**（行数锚点 30 → 31 → 32 同步） |
| **AC10** | ✅ | `.rddf/checks/examples-rtl-sim-targets.tsv` 两行的 component 列已同步 ⇒ `verify_examples_rtl_sim.sh` ⇒ **PASS** |
| **AC11** | ✅ | `verify_riscv_soc_core_ports.sh` ⇒ **PASS**（**V3** = C6 快照三方闭环 ⇒ 端口面未变）· `examples/riscv-soc/AGENTS.md` 的 34 端口契约表**逐字未动** |

### R3c（文件名 · 路径面）

| AC | 判定 | 证据（本轮实跑） |
|---|---|---|
| **AC12** | ✅ | `verify_no_duplicate_rv32i_headers.sh` ⇒ **PASS**；`--self-test` ⇒ **PASS**（`SOC_TOP_HEADER` + **33** 处 fixture 字面量成批替换后自检仍绿） |
| **AC13** | ✅ | `verify_port_naming.sh` ⇒ **PASS**（豁免计数 **仍 1452**）· §4.1「消化留痕」**历史行未改**（旧路径保留）⇒ ⚠️ **口径订正已回写 proposal AC13**：该表是**具名消化**活账 ⇒ **改名不是消化 ⇒ MUST NOT 追加行**（实施期发现并修正） |
| **AC14** | ✅ | `verify_docs_links.sh` ⇒ **PASS**（两个 dated 架构文档的 `file:line` 指针已更新；**行号不变** —— 被引用文件只改了第 1 行注释 ⇒ 行数与内容位置不变） |
| **AC15** | ✅ | `docs/CHANGELOG.md` 新增 **3** 条（R3a / R3b / R3c 各一条）⇒ 本 change 自带非空 `## 用户可见变更` 段 ⇒ `verify_public_api_visibility.sh` **R1** 成立 |

### R3e（追加 · **核的接口契约表归位**）

| 项 | 判定 | 证据 |
|---|---|---|
| 核侧 34 行表存在且条数 = **34** | ✅ | `awk -F'|' '/^\| *[0-9]+ *\|/' examples/riscv-mini/AGENTS.md` ⇒ **34** |
| SoC 侧该节**不再重复**表（改为指针） | ✅ | `examples/riscv-soc/AGENTS.md` 的「核复合体（`Rv32iCoreTop`）的 **34 端口契约表** ⇒ **已归位到核侧**」节 ⇒ 只留指针 |
| 护栏期望表指针切到核侧 | ✅ | `scripts/verify_riscv_soc_core_ports.sh` 的 `AGENTS_MD="examples/riscv-mini/AGENTS.md"` ⇒ **PASS**；`--self-test` ⇒ **PASS**（7 投毒各自必红 ⇒ 投毒流程与表路径无关） |
| **顺带订正**：原前言「期望表由 `verify_examples_rtl_sim.sh` 解析」**指错了脚本** | ✅ | 实测唯一消费者 = `verify_riscv_soc_core_ports.sh`（`parse_agents_table` 只此一处调用）⇒ ⇒ 前言已改正 |
| 零行为 / 零端口变化 | ✅ | 表内容逐字未动 ⇒ V1/V2 仍绿 ⇒ 34 根端口表不变 |

## 回归（全部本轮实跑 · 全绿）

`ctest` 273/273 · `run_all_ported_tests.sh` PASS 29 / FAIL 0 ·
`verify_examples_rtl_sim.sh` · `verify_examples_rtl_sim_coverage.sh` · `verify_hierarchical_rtl.sh` ·
`verify_rv32i_fw_init_rtl.sh` · `verify_dtcm_read_timing.sh` · `verify_no_duplicate_rv32i_headers.sh`(+self-test) ·
`verify_port_naming.sh` · `verify_public_api_visibility.sh`(+self-test) · `verify_riscv_soc_core_ports.sh` ·
`verify_node_names_single_source.sh` · `verify_arch_ledger_residuals.sh` · `verify_ledger_evidence.sh` ·
`verify_improvement_index.sh` · `verify_guard_reachability.sh` · `verify_roadmap_fragments.sh` ·
`verify_agents_size.sh` · `verify_docs_links.sh` · `verify_test_counts.sh` · `rv32i_rtl_sim_env.sh`

## 遗留清扫（本次范围内）

| 项 | 处置 |
|---|---|
| TODO / 半截实现 / 临时日志 / 被 skip 的测试 | **无**（本 change 纯改名 ⇒ 无新增代码路径） |
| 公开面登记 | ✅ 类改名 + 头文件改名**各一行**（`public-api-surface.md`）+ `public-api-removals.tsv` 的**搜索面路径**同步（R5 要求引用仍可解析） |
| 指针类更新 | ✅ `public-api-removals.tsv`（ext_clk 移除记录）· `firmware-load-chain-2026-11.md` 的 `evidence:` 探针路径（R1）· 两个 dated 架构文档的 `file:line` 指针（docs-links C2） |
| **范围外遗留（只记录 · 未动）** | ① `docs/developer_guide/patterns/COMPONENT-LIFECYCLE-GUIDE.md` 的**教学样例**仍写 `Rv32iPipelineTop` + `rv32i_pipeline_top.h` + 已删除的 `ext_clk` ⇒ ⇒ **既有陈旧**（早于本 change）⇒ ⇒ 改它 = 只改一半 ⇒ ⇒ 归**独立项**（教学样例同步）<br>② `docs/adr/**` · `.rddf/reviews/**` · `.rddf/plans/**` · dated 决策单 · `docs/CHANGELOG.md` 历史行 ⇒ ⇒ **存量不追溯**（旧路径/旧类名是**历史证据**）<br>③ `port-naming-registry.md` §4.1 历史行的旧路径 ⇒ ⇒ **禁改历史行**（该表纪律）<br>④ **TB 改名**（`tb_rv32i_soc`）⇒ ⇒ 已在母 improvement 的 OUT 表登记为 **R3d**（未立项） |

## 已用轮次

**1 / 3**（verifier 轮次；R3a / R3b / R3c 各一次「改 → 验 → 提交」，**无返工轮**）。

## 下一步（母 improvement `rv32i-soc-platform-2026-11` 的批次序列）

```
✅ R3a / R3b / R3c（本次 · 零行为变化 · 核的三个名字统一）
  ↓
🔜 S2  MEM 反压（data_ready 真消费 · ready≡1 逐拍等价 · ready=0 wrapper 手推 stall 拍数）
      ⚠️ 实施期实测（供下一批参考）：`Rv32iPipeline::data_ready`（:69）**零消费者**；
         `MemStage::__io` **没有任何 ready/stall 输入**；`HazardUnit` 的 stall 只覆盖 load-use
         ⇒ ⇒ S2 需要**新增** MEM 级停顿通路（IF/ID 冻结 + MEM/WB 保持），属**时序契约变更** ⇒ ⇒ 架构面
  ↓
G-7  总线类裁决（Lite vs full AXI4）⇒ 决定 S3 的口形态
  ↓
S3   总线化 + D-L1 + burst 升级 ← 【TCM 归属 / 成层与否在这一批一起定】
```
