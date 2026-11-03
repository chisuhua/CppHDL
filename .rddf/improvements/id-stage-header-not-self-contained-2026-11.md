---
name: id-stage-header-not-self-contained-2026-11
priority: P3
source: 路线 C 第 2 批（`IdStage` 双后端 parity）实施期实测挖出
phase: phase-5
category: cpu
type: bugfix
created_at: 2026-11-03
---

# Improvement: `include/cpu/pipeline/stages/id_stage.h` **非自包含**（单独 include 会编译失败）

- **Date drafted**: 2026-11-03
- **Drafted by**: build-agent (builder role)
- **Status**: 📋 **待实施**（**零行为影响**（只改 include 路径）⇒ ⇒ 不需 ADR）
- **来源**：路线 C 第 2 批把 `riscv::IdStage` **直接当 dual DUT** ⇒ ⇒ 首次以
  「外部使用者」的视角 include 它 ⇒ ⇒ **编译失败**。

## 1. Why (背景与动机)

`include/cpu/pipeline/stages/id_stage.h:19`：

```cpp
#include "rv32i_decoder.h"
```

⚠️ `rv32i_decoder.h` 实际位于 **`include/cpu/pipeline/rv32i_decoder.h`**
⇒ ⇒ 与 `id_stage.h` 所在目录（`include/cpu/pipeline/stages/`）**差一级**
⇒ ⇒ 预处理器按「**先找当前文件所在目录**」的规则**找不到**它
⇒ ⇒ **必须**额外把 `include/cpu/pipeline` 加进 include 路径才能编译。

### 实测证据

```
$ g++ -c -I include ... tests/test_rv32i_timing_dual.cpp
include/cpu/pipeline/stages/id_stage.h:19:10: fatal error: rv32i_decoder.h: No such file or directory
```

⇒ ⇒ 本仓**已有**一个目标在用同样的补丁：
`tests/CMakeLists.txt` 的 `target_include_directories(test_rv32i_pipeline_behavior PRIVATE
${PROJECT_SOURCE_DIR}/include/cpu/pipeline)` ⇒ ⇒ **同一个补丁要打两处**
⇒ ⇒ 这是「**症状被压住**」的典型形态（根因在头文件，补丁在构建脚本）。

⚠️ 根 `AGENTS.md` 的 CONVENTIONS 明写「**Headers are self-contained**」
⇒ ⇒ ⇒ **违反**。

⇒ ⇒ **影响面**：任何**仓外**使用者（或任何不想改构建脚本的人）想直接用
`riscv::IdStage` ⇒ ⇒ **编译不过** ⇒ ⇒ 该组件的**可复用性被这条 include 挡住**。

## 2. What Changes (变更范围）

- **`include/cpu/pipeline/stages/id_stage.h:19`**：
  `#include "rv32i_decoder.h"` ⇒ ⇒ `#include "../rv32i_decoder.h"`
  ⇒ ⇒ **零行为变更**（同一个头文件，同一个文件）⇒ ⇒ **不需要 ADR**。
- ⚠️ **配套**：修好后 `tests/CMakeLists.txt` 的**两个**补丁
  （`test_rv32i_pipeline_behavior` 与 `test_rv32i_timing_dual`）**仍可保留**
  （无害）⇒ ⇒ 但**可以**删掉 ⇒ ⇒ 删除前 MUST 确认没有别的目标依赖它。
- ⚠️ **顺带**：全仓其它 `include/cpu/**` 头**是否也有同类问题** ⇒ ⇒
  **本批只实测了这三个**（`id_stage.h` · `rv32i_regfile.h` · `rv32i_tcm.h`）
  ⇒ ⇒ 后两个**自包含** ✔ ⇒ ⇒ **建议**修本批时顺带做一次
  「每个 `include/cpu/**` 头单独编译」的全量扫描（可做成脚本）。

## 3. Acceptance (验收标准）

- [ ] `#include "cpu/pipeline/stages/id_stage.h"` 在**只有** `-I include` 时**编译通过**
- [ ] `include/cpu/pipeline/stages/id_stage.h` **不再**需要
      `include/cpu/pipeline` 在 include 路径上
- [ ] **全量扫描**：每个 `include/cpu/**/*.h` 都能**单独**编译
      （⇒ ⇒ 建议做成 `scripts/verify_*.sh` 护栏 ⇒ ⇒ 防止复发）
- [ ] `ctest -LE perf` 零回归

## 4. Capabilities (契约归属)

- ⚠️ **不涉及 capability**（这是**构建/头文件卫生**问题，不是行为契约）。
- ⇒ ⇒ 若要立契约，可并入既有的「头文件自包含」类条款（本仓目前**没有**这一条
  capability ⇒ ⇒ **planner 裁决是否值得新建**）。

## 5. Impact (影响面)

- **生产消费者**：**0 行为影响**（`include/cpu/pipeline/stages/id_stage.h` 被
  `rv32i_pipeline.h` include ⇒ ⇒ 而后者在 `include/cpu/pipeline/` ⇒ ⇒
  **在流水线内部**这个相对 include 能解析（因为 `rv32i_pipeline.h` 与
  `rv32i_decoder.h` **同目录**）⇒ ⇒ ⇒ **只有「从 `stages/` 目录直接 include
  `id_stage.h`」这条路会失败** ⇒ ⇒ ⇒ 这就是为什么它一直没人发现）。
- ⇒ ⇒ **爆炸半径为零** ⇒ ⇒ 可以独立修。
- ⚠️ **本批（parity 批）不改它**：本批用**构建脚本补丁**绕过（沿用既有先例）
  ⇒ ⇒ 缺陷登记 + 独立修。
