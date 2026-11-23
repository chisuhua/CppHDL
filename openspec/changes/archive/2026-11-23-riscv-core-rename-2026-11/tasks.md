# Tasks: RV32I 核改名（R3a / R3b / R3c）

> **判据** = `proposal.md` 的 AC1–AC15 · **落点** = `design.md` §2
> ⚠️ 纪律：每批 = 一次「改 → 验 → 提交」· 提交 MUST **按路径显式 `git add`**（禁 `git add -A`）

## 0. 前置（**开工前**）

- [x] **T0.1** 确认工作区干净（`git status --short` 为空）⇒ ⇒ 不夹带他人 WIP
- [x] **T0.2** 基线记录（**用于 AC4 / AC7 的逐字节比对**）：
  ```bash
  rm -rf /tmp/opencode/r3-base && mkdir -p /tmp/opencode/r3-base/{flat,hier,fw}
  build/examples/riscv-mini/tb_rv32i_soc --emit-rtl --out /tmp/opencode/r3-base/flat
  build/examples/riscv-mini/tb_rv32i_soc --emit-rtl --hierarchical --out /tmp/opencode/r3-base/hier
  build/examples/riscv-mini/tb_rv32i_soc --emit-rtl --firmware-init --out /tmp/opencode/r3-base/fw
  ```
  ⇒ ⇒ 三份 `top.v` + `manifest.json` 的 **sha256** 记录进本文件的「实测记录」段
- [x] **T0.3** SoC 侧基线：`build/examples/riscv-soc/tb_riscv_soc --emit-rtl --hierarchical --out /tmp/opencode/r3-base/soc`

---

## 1. R3a —— 模块类型名 `Rv32iCoreComplex` → `Rv32iCoreTop`

- [x] **T1.1** `examples/riscv-soc/src/rv32i_soc_top.h:81` ⇒ `"Rv32iCoreComplex"` → `"Rv32iCoreTop"`
- [x] **T1.2** `examples/riscv-soc/tb_riscv_soc.cpp` 的 `kExpectedModules` ⇒ 同名项替换
- [x] **T1.3** `scripts/verify_riscv_soc_core_ports.sh` ⇒ `CORE_MODULE=` + 3 处注释
- [x] **T1.4** `.rddf/checks/soc-rtl-expectations.md` ⇒ 3 处
- [x] **T1.5** `examples/riscv-soc/AGENTS.md` ⇒ 2 处（module 集行 · 34 端口表标题）
- [x] **T1.6** `docs/usage_guide/02-component-design.md:239` ⇒ 代码样例
- [x] **T1.7** 验证：
  ```bash
  bash scripts/verify_riscv_soc_core_ports.sh            # AC3
  bash scripts/verify_riscv_soc_core_ports.sh --self-test # AC3（7 投毒）
  bash scripts/verify_examples_rtl_sim.sh                # AC10（回归）
  # AC1 / AC5：重发 SoC 产物 ⇒ module 名 + diff 只允许模块名行
  # AC4：核侧三份产物 cmp 基线 ⇒ 零差异
  ```
- [x] **T1.8** 提交（单 commit · 显式路径）

---

## 2. R3b —— C++ 类名 `Rv32iPipelineTop` → `Rv32iCoreTop`

- [x] **T2.1** `examples/riscv-mini/src/rv32i_pipeline_top.h` ⇒ 类名 + 头注释 + 构造默认 name
- [x] **T2.2** `examples/riscv-mini/tb_rv32i_soc.cpp` ⇒ include 注释 + 2 个工厂 + `ch_device<>`
- [x] **T2.3** `examples/riscv-mini/tests/test_rv32i_hier_rtl.cpp` ⇒ include + `ch_device<>`
- [x] **T2.4** `examples/riscv-mini/tests/test_rv32i_soc_dual_parity.cpp` ⇒ include + 注释 + `ch_device<>`
- [x] **T2.5** `examples/riscv-soc/src/rv32i_soc_top.h` ⇒ `ch_layer<Rv32iPipelineTop, …>` → `ch_layer<Rv32iCoreTop, …>`
- [x] **T2.6** `scripts/probe_r21_hier_granularity.sh` ⇒ include + `ch_device<>`
- [x] **T2.7** `scripts/verify_examples_rtl_sim.sh` ⇒ 自检 fixture 行的 DUT 列
- [x] **T2.8** `.rddf/checks/examples-rtl-sim-targets.tsv` ⇒ `kind=hier` 行 DUT 列
- [x] **T2.9** `.rddf/checks/examples-rtl-expectations.md` ⇒ DUT 名 + 推导源说明
- [x] **T2.10** `.rddf/checks/public-api-surface.md` ⇒ 「已移除 / 已改名的公开符号」节**新增一行**
- [x] **T2.11** `docs/CHANGELOG.md` ⇒ **新增**一行
- [x] **T2.12** 验证：
  ```bash
  cmake --build build -j$(nproc)                              # 全量重建（AGENTS.md §COMMANDS）
  build/examples/riscv-mini/tb_rv32i_soc                      # AC6 ⇒ RESULT: checks=24 fails=0
  cd build && ctest --output-on-failure                       # AC8
  bash scripts/verify_public_api_visibility.sh                # AC9
  bash scripts/verify_node_names_single_source.sh             # AC8（节点名前缀变了）
  bash scripts/verify_riscv_soc_core_ports.sh                 # AC11（V3 三方闭环）
  ./run_all_ported_tests.sh                                   # AC8（FAIL: 0）
  # AC7：核侧三份产物 cmp 基线 ⇒ 零差异
  ```
- [x] **T2.13** 提交（单 commit · 显式路径）

---

## 3. R3c —— 文件名 `rv32i_pipeline_top.h` → `rv32i_core_top.h`

- [x] **T3.1** `git mv examples/riscv-mini/src/rv32i_pipeline_top.h examples/riscv-mini/src/rv32i_core_top.h`
- [x] **T3.2** 5 处 `#include "rv32i_pipeline_top.h"` ⇒ 新名（TB · 2 测试 · SoC 顶层 · 探针脚本）
- [x] **T3.3** `scripts/verify_no_duplicate_rv32i_headers.sh` ⇒ `SOC_TOP_HEADER=` + **~25 处 fixture 字面量**（**成批替换** ⇒ ⇒ 逐条 `grep -c` 复核归零）
- [x] **T3.4** `.rddf/checks/port-naming-registry.md` ⇒ **活账追加新行**（对象 = 新路径 · 32 端口）· ⚠️ **禁改 / 禁删历史行**
- [x] **T3.5** `examples/riscv-mini/CMakeLists.txt` ⇒ 3 处注释
- [x] **T3.6** 文档 8 处（`design.md` §2.3 #7）
- [x] **T3.7** 验证：
  ```bash
  bash scripts/verify_no_duplicate_rv32i_headers.sh           # AC12
  bash scripts/verify_no_duplicate_rv32i_headers.sh --self-test
  bash scripts/verify_port_naming.sh                          # AC13（计数仍 1452）
  bash scripts/verify_docs_links.sh                           # AC14
  bash scripts/verify_node_names_single_source.sh
  cd build && ctest --output-on-failure
  ```
- [x] **T3.8** 提交（单 commit · 显式路径）

---

## 4. 收口

- [x] **T4.1** 母 improvement `.rddf/improvements/rv32i-soc-platform-2026-11.md` 的 OUT 表「改名」行 ⇒ 指向本 change
- [x] **T4.2** `docs/CHANGELOG.md` 的三批合并为**一条**用户可见变更（或三条，逐批记）
- [x] **T4.3** 验收报告落 `.rddf/reviews/riscv-core-rename-2026-11-verify.md`（**逐条 AC + 工具证据**）

## 5. 实测记录（**执行时回填**）

| 判据 | 实测 | 证据 |
|---|---|---|
| AC4 / AC7 核侧产物逐字节 | ✅ **6/6 IDENTICAL** | `flat/hier/fw` × `top.v/manifest.json` 的 sha256 与基线逐字节相同 |
| AC5 SoC 产物 diff 行数 | ✅ **6 行（3 对）· 全部模块名行**；`manifest.json` 零差异 | `diff` 输出 |
| AC1 产物 module 名 | ✅ `module Rv32iCoreTop` = 1 · `Rv32iCoreComplex` 残留 = 0 | `grep -c` |
| AC2 冻结期望集 | ✅ `PRODUCTS-CHECK: PASS` | `tb_riscv_soc --emit-rtl --check-products` |
| AC3 / AC9 / AC10 / AC11 护栏 | ✅ 全 PASS（含 3 个 `--self-test`） | 各脚本输出 |
| AC6 核级判据 | ✅ `RESULT: checks=24 fails=0` | `tb_rv32i_soc` |
| AC8 ctest | ✅ **273/273** · `run_all_ported_tests` PASS 29 / FAIL 0 | `ctest` 末行 |
| AC12 / AC13 / AC14 / AC15 | ✅ 全 PASS | 各脚本输出 |
| 验收报告 | ✅ `.rddf/reviews/riscv-core-rename-2026-11-verify.md` | —— |

> ⚠️ **实施期口径订正（已回写 proposal AC13）**：`port-naming-registry.md` §4.1「消化留痕」是
> **具名消化**的活账 ⇒ ⇒ **改名不是消化 ⇒ MUST NOT 追加行** ⇒ ⇒ 只改当前态描述；
> §4.1 历史行保留旧路径（禁改 / 禁删）。豁免计数 **仍 1452**。
