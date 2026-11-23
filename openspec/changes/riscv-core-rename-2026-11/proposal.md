# Change: RV32I 核改名 —— `Rv32iPipelineTop` / `Rv32iCoreComplex` → **`Rv32iCoreTop`**

> **Change 名**：`rv32i-core-rename-2026-11`
> **母 improvement**：`rv32i-soc-platform-2026-11`（状态 `in-progress` · §范围 OUT 表「改名（`Rv32iPipelineTop` / `tb_rv32i_soc`）| 单列一批（**R3**）」）
> **日期**：2026-11-23 · builder
> **性质**：**纯改名**（零行为变化 · 零端口变化 · 零 spec delta）

## Why

1. **「核 = 可替换 IP」需要一个自洽的名字**。当前同一个核在仓内有**三个名字**：
   C++ 类 = `Rv32iPipelineTop` · 产物 module 类型名 = `Rv32iCoreComplex` · 头文件 =
   `rv32i_pipeline_top.h` ⇒ ⇒ 读产品的人（SoC 装配者 / 下游集成者）拿到的三处线索互不一致
   ⇒ ⇒ **R3 把三者统一为 `Rv32iCoreTop`**（与 `Rv32iSocTop` 对称）。
2. **模块类型名是「当前产品名 · 非契约」**（ADR-077 **D2** 明文）⇒ ⇒ 改名的**唯一义务** =
   「产物**重新 lint 干净**」+「**结构判据重算**」（module 集 / module 数）
   ⇒ ⇒ 改名**不触碰**任何已签批 spec 的 Requirement（`rtl-hierarchical-output` 钉的是
   **成层集合**（7 个 stage）与**交付粒度**（8 module），**不含**顶层类名 / 模块类型名）。
3. **零行为变化 ⇒ 可机械判定**。类名与模块类型名都**不进** Verilog 端口名
   （端口名来自 C++ **成员名**）⇒ ⇒ 核侧产物（flat / hier / fw-init）应**逐字节不变**；
   SoC 侧产物只应差**模块名那一行** ⇒ ⇒ 改名有**强判别力**的验收判据（不是"跑绿了就算"）。

## What Changes

| 批 | 内容 | 产物面影响 |
|---|---|---|
| **R3a** | **模块类型名** `Rv32iCoreComplex` → `Rv32iCoreTop`（SoC 侧 `ch_layer` 的第 2 参 + 冻结期望集 + 护栏常量 + 期望源 + 文档） | SoC 产物 **module 名 1 行**（+ manifest 内同名字段）；**核侧产物零影响** |
| **R3b** | **C++ 类名** `Rv32iPipelineTop` → `Rv32iCoreTop`（5 个 TU + 2 个脚本 + 登记表 + 公开面登记） | **零**（类名不进端口名 / 不进 module 名） |
| **R3c** | **文件名** `rv32i_pipeline_top.h` → `rv32i_core_top.h`（+ `verify_no_duplicate_rv32i_headers.sh` 的 fixture 批量 + `port-naming-registry.md` 活账追加 + 文档） | **零** |

⇒ ⇒ **三批各自独立提交 · 各自可回滚 · 各自有判据**（拆分理由见 `design.md` §3）。

## 验收标准

> 期望值 MUST **手推 / 实测派生**（禁抄仿真输出）· 计数 MUST 从产物派生（禁裸数字）。

### R3a（模块类型名）

- [ ] **AC1** 🔴 SoC 层次化产物**含** `module Rv32iCoreTop` **且不含** `Rv32iCoreComplex`
  ⇒ 证据：`build/examples/riscv-soc/tb_riscv_soc --emit-rtl --hierarchical --out <dir>`
  ⇒ ⇒ `grep -c '^module Rv32iCoreTop' top.v` == 1 且 `grep -c 'Rv32iCoreComplex' top.v` == 0
- [ ] **AC2** 🔴 `tb_riscv_soc` 的**冻结期望集** == 产物 module 集（**集合相等**）
  ⇒ 证据：`tb_riscv_soc --check-products` 退出码 0（期望集在 `tb_riscv_soc.cpp:136-146`）
- [ ] **AC3** 🔴 `scripts/verify_riscv_soc_core_ports.sh` PASS（V1–V5）· `--self-test` PASS（**7** 投毒各自必红）
- [ ] **AC4** 🔴 **核侧三种产物逐字节不变**（`tb_rv32i_soc --emit-rtl` / `--hierarchical` / `--firmware-init`）
  ⇒ 证据：`cmp` 改前改后的 `top.v` 与 `manifest.json` ⇒ ⇒ **零差异**
  ⇒ ⇒ ⚠️ 这是「核侧根本不含 `Rv32iCoreComplex`」的**反证**（模块类型名只出现在 SoC 侧）
- [ ] **AC5** 🔴 SoC 侧产物的 `diff` **只有模块名相关行**（不允许出现任何其它差异）
  ⇒ 证据：改前改后 `top.v` 的 `diff` ⇒ ⇒ 差异行 MUST 全部指向模块名

### R3b（C++ 类名）

- [ ] **AC6** 🔴 24 条核级判据仍绿：`tb_rv32i_soc` ⇒ `RESULT: checks=24 fails=0`（退出码 0）
- [ ] **AC7** 🔴 **核侧产物逐字节不变**（同 AC4 的三份）⇒ ⇒ **类名不进产物**
- [ ] **AC8** 🔴 全量 `ctest` 绿 + `run_all_ported_tests.sh` PASS（**FAIL: 0**）
  ⇒ ⚠️ 头文件重命名会改**模拟日志里的节点名前缀**（`rv32i_pipeline_top.*` → `rv32i_core_top.*`）
  ⇒ ⇒ `scripts/verify_node_names_single_source.sh` MUST 绿
- [ ] **AC9** 🔴 **公开面登记**：`.rddf/checks/public-api-surface.md` 的「⚠️ **已移除 / 已改名的公开符号**」节
  MUST 含 `Rv32iPipelineTop` → `Rv32iCoreTop` 一行（**改名**属符号级消失 ⇒ 必须登记）
  ⇒ 证据：`scripts/verify_public_api_visibility.sh` PASS（**R2** + **R1** 两向）
- [ ] **AC10** 🔴 登记面同步：`.rddf/checks/examples-rtl-sim-targets.tsv`（`kind=hier` 行的 DUT 列）
  · `.rddf/checks/examples-rtl-expectations.md`（DUT 名）⇒ ⇒ `verify_examples_rtl_sim.sh` PASS
- [ ] **AC11** 🔴 **端口面零变更**：`verify_riscv_soc_core_ports.sh` **V3**（C6 快照三方闭环）仍绿
  ⇒ ⇒ 34 端口契约表（`examples/riscv-soc/AGENTS.md`）**逐字不变**

### R3c（文件名）

- [ ] **AC12** 🔴 `scripts/verify_no_duplicate_rv32i_headers.sh` PASS + `--self-test` PASS
  ⇒ ⇒ ⚠️ 该护栏的**自检 fixture 里有 ~25 处** `rv32i_pipeline_top.h` 字面量 ⇒ ⇒ MUST 成批替换且自检仍绿
- [ ] **AC13** 🔴 `scripts/verify_port_naming.sh` PASS 且**豁免计数不变**（**1452**）
  ⇒ ⇒ `.rddf/checks/port-naming-registry.md` 的**活账 MUST 追加新行**（对象 = 新路径 · 32 端口）
  ⇒ ⇒ ⚠️ **禁改 / 禁删历史行**（该表的活账纪律）⇒ ⇒ **历史行指向的旧路径保持不动**
- [ ] **AC14** 🔴 文档同步：`examples/riscv-mini/AGENTS.md` · `examples/riscv-soc/AGENTS.md` ·
  `include/cpu/AGENTS.md` · 根 `AGENTS.md` · `docs/architecture/rv32i-microarchitecture.md` ·
  `docs/developer_guide/rv32i-rtl-delivery-and-sim.md` · `docs/developer_guide/patterns/COMPONENT-LIFECYCLE-GUIDE.md` ·
  `docs/usage_guide/02-component-design.md` ⇒ ⇒ `scripts/verify_docs_links.sh` PASS
- [ ] **AC15** 🔴 `docs/CHANGELOG.md` **新增一行**（改名 = 用户可见变更 ⇒ `verify_public_api_visibility.sh` **R1** 要求
  本 change 自带非空 `## 用户可见变更` 段）

## Out of Scope

| 项 | 理由 |
|---|---|
| **TB 改名**（`tb_rv32i_soc` → `tb_rv32i_core` 等） | 会动 tsv / CMake / CI / 多个护栏的 fixture ⇒ ⇒ **单列一批**（R3d）· 本批不动 |
| **历史文档**（`openspec/changes/archive/**` · `.rddf/reviews/**` · `.rddf/plans/**` · `.rddf/improvements/**` 的旧条目 · `docs/adr/**` 的历史正文 · `docs/CHANGELOG.md` 历史行） | **存量不追溯** |
| **`docs/adr/ADR-077` 正文** | 模块类型名 = **非契约**（ADR-077 **D2**）⇒ ⇒ 改名**不需要**修订 ADR ⇒ ⇒ ⚠️ 其 Verification 章的 **V2 / V5** 提到的旧名**按"名字非契约"理解**（判据的**意图** = module **集合**相等 / 端口表 == 34 ⇒ ⇒ 集合判据按"除名字外相等"判定）⇒ ⇒ **登记在 `design.md` §4 的取舍表** |
| **TCM 成层 / `Rv32iPipeline` 成层** | 撞已签批 `rtl-hierarchical-output`（成层集合 7 · 交付粒度 8 module）⇒ ⇒ **架构面** ⇒ ⇒ 归 **S3 的「TCM 归属」决策**同批 |
| **端口面任何变化** | ADR-046 **D10** 冻结 32 端口快照 ⇒ ⇒ 本批**零端口变化** |
| **S2 / S3–S7** | 母 improvement 的其它批 |

## 用户可见变更

- **C++ 类 `Rv32iPipelineTop` → `Rv32iCoreTop`**（`examples/riscv-mini/src/` 下 ⇒ **example 侧**符号
  ⇒ ⇒ 下游若直接 include 该头并实例化该类 ⇒ ⇒ **MUST 改类名**）
- **头文件 `rv32i_pipeline_top.h` → `rv32i_core_top.h`**（⇒ ⇒ 下游 `#include "rv32i_pipeline_top.h"` **MUST 改**）
- **产物里的 module 类型名 `Rv32iCoreComplex` → `Rv32iCoreTop`**（SoC 层次化产物）
  ⇒ ⇒ 下游若按 module 名**抓取/校验**产物 ⇒ ⇒ **MUST 改**（但名字本身**非契约** ⇒ 不改也不破坏功能）
- ⚠️ **端口面零变化**（34 根顶层端口的名字 / 方向 / 位宽 / 顺序**逐字不变**）
- ⚠️ **行为零变化**（核侧三种产物**逐字节不变**）
