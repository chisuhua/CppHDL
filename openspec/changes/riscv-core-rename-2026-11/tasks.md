# Tasks: RV32I 核改名（R3a / R3b / R3c）

> **判据** = `proposal.md` 的 AC1–AC15 · **落点** = `design.md` §2
> ⚠️ 纪律：每批 = 一次「改 → 验 → 提交」· 提交 MUST **按路径显式 `git add`**（禁 `git add -A`）

## 0. 前置（**开工前**）

- [ ] **T0.1** 确认工作区干净（`git status --short` 为空）⇒ ⇒ 不夹带他人 WIP
- [ ] **T0.2** 基线记录（**用于 AC4 / AC7 的逐字节比对**）：
  ```bash
  rm -rf /tmp/opencode/r3-base && mkdir -p /tmp/opencode/r3-base/{flat,hier,fw}
  build/examples/riscv-mini/tb_rv32i_soc --emit-rtl --out /tmp/opencode/r3-base/flat
  build/examples/riscv-mini/tb_rv32i_soc --emit-rtl --hierarchical --out /tmp/opencode/r3-base/hier
  build/examples/riscv-mini/tb_rv32i_soc --emit-rtl --firmware-init --out /tmp/opencode/r3-base/fw
  ```
  ⇒ ⇒ 三份 `top.v` + `manifest.json` 的 **sha256** 记录进本文件的「实测记录」段
- [ ] **T0.3** SoC 侧基线：`build/examples/riscv-soc/tb_riscv_soc --emit-rtl --hierarchical --out /tmp/opencode/r3-base/soc`

---

## 1. R3a —— 模块类型名 `Rv32iCoreComplex` → `Rv32iCoreTop`

- [ ] **T1.1** `examples/riscv-soc/src/rv32i_soc_top.h:81` ⇒ `"Rv32iCoreComplex"` → `"Rv32iCoreTop"`
- [ ] **T1.2** `examples/riscv-soc/tb_riscv_soc.cpp` 的 `kExpectedModules` ⇒ 同名项替换
- [ ] **T1.3** `scripts/verify_riscv_soc_core_ports.sh` ⇒ `CORE_MODULE=` + 3 处注释
- [ ] **T1.4** `.rddf/checks/soc-rtl-expectations.md` ⇒ 3 处
- [ ] **T1.5** `examples/riscv-soc/AGENTS.md` ⇒ 2 处（module 集行 · 34 端口表标题）
- [ ] **T1.6** `docs/usage_guide/02-component-design.md:239` ⇒ 代码样例
- [ ] **T1.7** 验证：
  ```bash
  bash scripts/verify_riscv_soc_core_ports.sh            # AC3
  bash scripts/verify_riscv_soc_core_ports.sh --self-test # AC3（7 投毒）
  bash scripts/verify_examples_rtl_sim.sh                # AC10（回归）
  # AC1 / AC5：重发 SoC 产物 ⇒ module 名 + diff 只允许模块名行
  # AC4：核侧三份产物 cmp 基线 ⇒ 零差异
  ```
- [ ] **T1.8** 提交（单 commit · 显式路径）

---

## 2. R3b —— C++ 类名 `Rv32iPipelineTop` → `Rv32iCoreTop`

- [ ] **T2.1** `examples/riscv-mini/src/rv32i_pipeline_top.h` ⇒ 类名 + 头注释 + 构造默认 name
- [ ] **T2.2** `examples/riscv-mini/tb_rv32i_soc.cpp` ⇒ include 注释 + 2 个工厂 + `ch_device<>`
- [ ] **T2.3** `examples/riscv-mini/tests/test_rv32i_hier_rtl.cpp` ⇒ include + `ch_device<>`
- [ ] **T2.4** `examples/riscv-mini/tests/test_rv32i_soc_dual_parity.cpp` ⇒ include + 注释 + `ch_device<>`
- [ ] **T2.5** `examples/riscv-soc/src/rv32i_soc_top.h` ⇒ `ch_layer<Rv32iPipelineTop, …>` → `ch_layer<Rv32iCoreTop, …>`
- [ ] **T2.6** `scripts/probe_r21_hier_granularity.sh` ⇒ include + `ch_device<>`
- [ ] **T2.7** `scripts/verify_examples_rtl_sim.sh` ⇒ 自检 fixture 行的 DUT 列
- [ ] **T2.8** `.rddf/checks/examples-rtl-sim-targets.tsv` ⇒ `kind=hier` 行 DUT 列
- [ ] **T2.9** `.rddf/checks/examples-rtl-expectations.md` ⇒ DUT 名 + 推导源说明
- [ ] **T2.10** `.rddf/checks/public-api-surface.md` ⇒ 「已移除 / 已改名的公开符号」节**新增一行**
- [ ] **T2.11** `docs/CHANGELOG.md` ⇒ **新增**一行
- [ ] **T2.12** 验证：
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
- [ ] **T2.13** 提交（单 commit · 显式路径）

---

## 3. R3c —— 文件名 `rv32i_pipeline_top.h` → `rv32i_core_top.h`

- [ ] **T3.1** `git mv examples/riscv-mini/src/rv32i_pipeline_top.h examples/riscv-mini/src/rv32i_core_top.h`
- [ ] **T3.2** 5 处 `#include "rv32i_pipeline_top.h"` ⇒ 新名（TB · 2 测试 · SoC 顶层 · 探针脚本）
- [ ] **T3.3** `scripts/verify_no_duplicate_rv32i_headers.sh` ⇒ `SOC_TOP_HEADER=` + **~25 处 fixture 字面量**（**成批替换** ⇒ ⇒ 逐条 `grep -c` 复核归零）
- [ ] **T3.4** `.rddf/checks/port-naming-registry.md` ⇒ **活账追加新行**（对象 = 新路径 · 32 端口）· ⚠️ **禁改 / 禁删历史行**
- [ ] **T3.5** `examples/riscv-mini/CMakeLists.txt` ⇒ 3 处注释
- [ ] **T3.6** 文档 8 处（`design.md` §2.3 #7）
- [ ] **T3.7** 验证：
  ```bash
  bash scripts/verify_no_duplicate_rv32i_headers.sh           # AC12
  bash scripts/verify_no_duplicate_rv32i_headers.sh --self-test
  bash scripts/verify_port_naming.sh                          # AC13（计数仍 1452）
  bash scripts/verify_docs_links.sh                           # AC14
  bash scripts/verify_node_names_single_source.sh
  cd build && ctest --output-on-failure
  ```
- [ ] **T3.8** 提交（单 commit · 显式路径）

---

## 4. 收口

- [ ] **T4.1** 母 improvement `.rddf/improvements/rv32i-soc-platform-2026-11.md` 的 OUT 表「改名」行 ⇒ 指向本 change
- [ ] **T4.2** `docs/CHANGELOG.md` 的三批合并为**一条**用户可见变更（或三条，逐批记）
- [ ] **T4.3** 验收报告落 `.rddf/reviews/riscv-core-rename-2026-11-verify.md`（**逐条 AC + 工具证据**）

## 5. 实测记录（**执行时回填**）

| 判据 | 实测 | 证据 |
|---|---|---|
| AC4 / AC7 核侧产物逐字节 | ⏳ | sha256（三份 × 2 组）|
| AC5 SoC 产物 diff 行数 | ⏳ | `diff` 输出 |
| AC6 核级判据 | ⏳ | `RESULT: checks=24 fails=0` |
| AC8 ctest | ⏳ | `ctest` 末行 |
| AC13 豁免计数 | ⏳ | `verify_port_naming.sh` 输出 |
