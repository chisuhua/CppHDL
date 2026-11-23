# Design: RV32I 核改名（R3a / R3b / R3c）

> **母 change**：`rv32i-core-rename-2026-11` · **判据见** `proposal.md` 的 AC1–AC15
> **本文只承载**：逐文件落点 · 拆分理由 · 判据重算清单 · 取舍登记 · 回滚

## 1. 三个名字的关系（改名前 → 改名后）

| 名字 | 载体 | 改名前 | 改名后 | 进产物？ |
|---|---|---|---|---|
| **C++ 类名** | `examples/riscv-mini/src/rv32i_pipeline_top.h` | `Rv32iPipelineTop` | **`Rv32iCoreTop`** | ❌（不进端口名 · 不进 module 名）|
| **模块类型名** | `examples/riscv-soc/src/rv32i_soc_top.h` 的 `ch_layer` 第 2 参 | `Rv32iCoreComplex` | **`Rv32iCoreTop`** | ✅（SoC 产物的 module 名）|
| **文件名** | `examples/riscv-mini/src/` | `rv32i_pipeline_top.h` | **`rv32i_core_top.h`** | ❌（但进**脚本 fixture** 与**登记表**）|

⚠️ **库层类 `riscv::Rv32iPipeline`（`include/cpu/pipeline/rv32i_pipeline.h`）不动**
⇒ ⇒ 已签批 spec `rtl-hierarchical-output` 引用的就是它 ⇒ ⇒ **spec 零 delta**。

## 2. 逐文件落点（**只列活载体**；历史文档一律不动）

### 2.1 R3a —— 模块类型名

| # | 文件 | 落点 |
|---|---|---|
| 1 | `examples/riscv-soc/src/rv32i_soc_top.h` | `:81` `core{"core", "Rv32iCoreComplex", …}` → `"Rv32iCoreTop"` |
| 2 | `examples/riscv-soc/tb_riscv_soc.cpp` | `:136-146` `kExpectedModules` 里的 `"Rv32iCoreComplex"` |
| 3 | `scripts/verify_riscv_soc_core_ports.sh` | `:49` `CORE_MODULE="Rv32iCoreComplex"`（+ `:6/:11/:20` 的注释） |
| 4 | `.rddf/checks/soc-rtl-expectations.md` | `:264`（module 集）· `:266`（端口表标题）· `:312`（备注） |
| 5 | `examples/riscv-soc/AGENTS.md` | `:60`（module 集行）· `:68`（34 端口契约表标题）|
| 6 | `docs/usage_guide/02-component-design.md` | `:239`（代码样例）|
| 7 | `docs/CHANGELOG.md` | **新增**一行（禁改历史行）|

### 2.2 R3b —— C++ 类名

| # | 文件 | 落点 |
|---|---|---|
| 1 | `examples/riscv-mini/src/rv32i_pipeline_top.h` | 类名 + 头注释 + **构造默认 name**（`"rv32i_pipeline_top"` → `"rv32i_core_top"`）|
| 2 | `examples/riscv-mini/tb_rv32i_soc.cpp` | `#include` 注释 · `make_emit_design*` 工厂 · `ch_device<>` |
| 3 | `examples/riscv-mini/tests/test_rv32i_hier_rtl.cpp` | `#include` · `ch_device<>` |
| 4 | `examples/riscv-mini/tests/test_rv32i_soc_dual_parity.cpp` | `#include` · 注释 · `ch_device<>` |
| 5 | `examples/riscv-soc/src/rv32i_soc_top.h` | `ch_layer<Rv32iPipelineTop, …>` → `ch_layer<Rv32iCoreTop, …>` |
| 6 | `scripts/probe_r21_hier_granularity.sh` | `:30` 探针的 `#include` + `ch_device<>` |
| 7 | `scripts/verify_examples_rtl_sim.sh` | `:957` 自检 fixture 行的 **DUT 列** |
| 8 | `.rddf/checks/examples-rtl-sim-targets.tsv` | `kind=hier` 行的 **DUT 列** |
| 9 | `.rddf/checks/examples-rtl-expectations.md` | DUT 名 + 推导源说明 |
| 10 | `.rddf/checks/public-api-surface.md` | 「⚠️ **已移除 / 已改名的公开符号**」节**新增一行** |
| 11 | `docs/CHANGELOG.md` | **新增**一行 |

⚠️ **构造默认 name 改了 ⇒ 模拟日志的节点名前缀变**（`rv32i_pipeline_top.*` → `rv32i_core_top.*`）
⇒ ⇒ 判据 = `scripts/verify_node_names_single_source.sh` MUST 绿（AC8）。

### 2.3 R3c —— 文件名

| # | 文件 | 落点 |
|---|---|---|
| 1 | `examples/riscv-mini/src/rv32i_pipeline_top.h` → `rv32i_core_top.h` | `git mv` |
| 2 | 5 个 `#include "rv32i_pipeline_top.h"` | TB · 2 个测试 · SoC 顶层 · 探针脚本 |
| 3 | `scripts/verify_no_duplicate_rv32i_headers.sh` | `SOC_TOP_HEADER=` 路径 + **~25 处自检 fixture 字面量**（含 `sed` 模式 ⇒ ⇒ MUST 用**成批替换** + `--self-test` 复核）|
| 4 | `.rddf/checks/port-naming-registry.md` | **活账追加新行**（对象 = 新路径 · 32 端口）· ⚠️ **禁改 / 禁删历史行** |
| 5 | `scripts/verify_port_naming.sh` | 豁免计数锚点（**1452** ⇒ 改名后 MUST 仍为 1452）|
| 6 | `examples/riscv-mini/CMakeLists.txt` | `:22` / `:111` / `:131` 注释 |
| 7 | 文档 8 处 | `examples/riscv-mini/AGENTS.md` · `examples/riscv-soc/AGENTS.md` · `include/cpu/AGENTS.md` · 根 `AGENTS.md` · `docs/architecture/rv32i-microarchitecture.md` · `docs/developer_guide/rv32i-rtl-delivery-and-sim.md` · `docs/developer_guide/patterns/COMPONENT-LIFECYCLE-GUIDE.md` · `docs/usage_guide/02-component-design.md` |

## 3. 为什么拆成三批（**不是**为了省事，是为了**判据面不重叠**）

| 批 | 触碰的判据面 | 爆炸半径 | 风险 | 回滚粒度 |
|---|---|---|---|---|
| **R3a** | **产物面**（SoC module 名）+ 2 个期望源 + 1 个护栏常量 | 小（7 文件）| 低 | 单 commit |
| **R3b** | **C++ 符号面**（5 TU）+ 登记面 + 公开面登记 | 中（11 文件）| 低（**产物零变化** ⇒ AC7 强判别）| 单 commit |
| **R3c** | **路径面**（1 个 1700 行护栏的 fixture + 棘轮活账 + 8 处文档）| **大** | **中**（fixture 批量替换易漏）| 单 commit |

⇒ ⇒ **合并做 ⇒ 三类判据同时动 ⇒ 失败时无法二分定位** ⇒ ⇒ 拆分后每批失败都能**单点归因**。

## 4. 取舍登记（**已决 / 待裁**）

| # | 取舍 | 裁决 | 理由 |
|---|---|---|---|
| ① | `docs/adr/ADR-077` 的 Verification 章（V2/V5）提到旧 module 名 | ✅ **不动 ADR** | 模块类型名 = **非契约**（ADR-077 **D2**）⇒ ⇒ 判据的**意图**（module **集合**相等 · 端口表 == 34）不变 ⇒ ⇒ 集合判据按「**除名字外**相等」判定 ⇒ ⇒ **改名不需要 architect** |
| ② | 已签批 spec `rtl-hierarchical-output`（成层集合 7 · 交付粒度 8 module）| ✅ **零 delta** | 本批**不改成层集合、不改交付粒度** ⇒ ⇒ spec 文本零触碰 |
| ③ | 构造默认 name 是否跟着改 | ✅ **改** | 否则文件 / 类 / 节点前缀三处仍不一致（名字债只还了一半）⇒ ⇒ 判据 = node-names 护栏绿 |
| ④ | TB 改名（`tb_rv32i_soc`）| 🔴 **本批不做**（R3d）| 会动 tsv / CMake / CI / 多护栏 fixture ⇒ ⇒ 单列 |
| ⑤ | TCM / `Rv32iPipeline` 成层 | 🔴 **不在本批** | 撞已签批 spec ⇒ ⇒ 归 S3 的「TCM 归属」决策 |

## 5. 判据重算清单（**改名前后必须逐条核对**）

| 判据 | 受影响？ | 处理 |
|---|---|---|
| 核侧产物（flat / hier / fw-init）| ❌ 不受影响 | **AC4 / AC7**：逐字节不变（`cmp`）|
| SoC 侧产物 module 名 | ✅ | **AC1 / AC5**（diff 只允许模块名行）|
| SoC 侧 34 端口契约表 | ❌ | **AC11**（`top_core_*` 前缀由**实例名**决定 ⇒ 不变）|
| `verify_no_duplicate_rv32i_headers.sh` C6 快照 | ❌ | 端口面不变 ⇒ 快照不变（R3c 只改路径常量）|
| `verify_port_naming.sh` 豁免计数（1452）| ⚠️ R3c 路径迁移 | **AC13**：追加活账行 ⇒ 计数**必须仍为 1452** |
| `verify_examples_rtl_sim.sh` 自检 fixture | ⚠️ R3b DUT 列 | **AC10** |
| `verify_riscv_soc_core_ports.sh` | ⚠️ R3a 常量 | **AC3** |
| `verify_public_api_visibility.sh` R1 / R2 | ⚠️ R3b | **AC9 / AC15** |

## 6. 执行顺序（**每批 = 一次「改 → 验 → 提交」**）

```
R3a（模块类型名 · 产物面）
  ├─ 改 7 处（§2.1）
  ├─ 验：AC1 · AC2 · AC3(+self-test) · AC4 · AC5
  ├─ 验（回归）：verify_examples_rtl_sim.sh · verify_riscv_soc_core_ports.sh
  └─ 提交（单 commit · 显式路径 add）

R3b（C++ 类名 · 符号面）
  ├─ 改 11 处（§2.2）
  ├─ 验：AC6 · AC7 · AC8 · AC9 · AC10 · AC11
  ├─ 验（回归）：ctest 全量 · run_all_ported_tests.sh · verify_public_api_visibility.sh
  └─ 提交

R3c（文件名 · 路径面）
  ├─ git mv + 改 §2.3 的 7 组
  ├─ 验：AC12 · AC13 · AC14 · AC15
  ├─ 验（回归）：verify_no_duplicate_rv32i_headers.sh(+self-test) · verify_port_naming.sh
  │             · verify_node_names_single_source.sh · verify_docs_links.sh
  └─ 提交

⇒ 之后回到母 improvement 的批次序列：S2（MEM 反压）→ G-7（总线类裁决）→ S3（含 TCM 归属决策）
```

## 7. 回滚

- 每批 = 单 commit ⇒ `git revert <sha>` 即回滚该批
- **R3c 的回滚需连带**回滚 `port-naming-registry.md` 的追加行（⇒ ⇒ 追加行 MUST 与代码同 commit ⇒ ⇒ 保证同批回滚）
- ⚠️ **不得**用 `git add -A`（本仓多人 / 多会话并行 ⇒ 见根 `AGENTS.md` §COMMANDS）
