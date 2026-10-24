# rv32i-canonical-convergence-2026-10 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use skill_use("execute") to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 把 RV32I 微架构收敛到 `include/cpu/` 唯一权威 —— 删 17 个死头、提升 10 个头、
消除 4 组同名双副本、消除根 `tests/` 对 example 源码的反向依赖、落地单源化护栏、
文档与磁盘对齐。

**Architecture:** 改动集中在 4 个面 —— ①`include/cpu/**`（+10 提升、-0 删除）；
②`examples/riscv-mini/src/**`（32 → **1**）；③根 `tests/**` 与 example 测试的
`#include` 路径改写 + `tests/CMakeLists.txt` 的 6 处反向 `-I` 删除；
④新增 `scripts/verify_no_duplicate_rv32i_headers.sh`。**不动**任何组件的**逻辑**
（唯一例外：`hazard_unit.h` 的数据 mux 拆到已有的 `chlib::ForwardingMux`，
逻辑逐档等价 ⇒ 断言语义不变）。

**Tech Stack:** C++23, Catch2 v3.7.0, CMake 3.20+, bash（护栏脚本）, GitHub Actions。

---

## 设计要点：实测纠正的 3 处（与 improvement 草稿的预诊断不同）

> 本 plan 的所有事实均**先解码编译器的 `.d` 依赖文件**（真相源）确认，再落到代码。

### R1. 「死头」要用编译器的 `.d` 反查，不是源码级 basename 反查

草稿用「源码级 `#include` 反查」判死头。实测该方法**会误判**：
`examples/riscv-mini/tests/test_pipeline_minimal.cpp` 写的是
`#include "../../include/cpu/pipeline/rv32i_pipeline.h"` —— 从 `examples/riscv-mini/tests/`
出发 `../../` = `examples/` ⇒ `examples/include/...` **不存在**；它之所以能解析，
是因为 `-I <root>/include/core` 恰好让 `../../include/...` 落到仓库根
（**路径深度巧合**）。

⇒ 判定 MUST 用编译器的 `.d`（`build/**/*.o.d`），且 MUST 过滤掉**已删除 target 的陈旧 `.d`**。

### R2. `hazard_unit.h` 的语义分叉是**结构性**的 ⇒ ADR-046 D3 #14 与 D5 实测互斥

草稿把「lib 版禁用前推」当成「可能的有意规避，需重新评估」。**实测证明它是必需的**：

把 mux 搬进 lib 版 ⇒ 5 个 target 直接红，同一根因
`FATAL: Cycle detected in IR graph during topological sort (name: top.ex_stage._lt_lt)`。

根因：`hazard_unit` 一旦接入真实流水线装配，`hazard.rs1_data → ex_alu_result →
ex_stage → id_stage → reg_file → hazard` 形成组合环。

⇒ 「删 example 副本 + 消费者重指 lib」= 对 3 个 example 消费者**功能回退**
（它们确实断言「前推后的数据」）⇒ **方案 A 被实测否决**。
⇒ 裁决：数据 mux 拆到**已有的** `chlib::ForwardingMux`（`include/chlib/forwarding.h:184`），
其档位语义与原 mux **逐档等价**（0→REG / 1→EX / 2→MEM）⇒ 零新增组件、断言不变。

### R3. `rv32i_decoder.h` 等 3 组的「4 行 diff」是**名字可见性收窄**，不是等价

lib 版把 `using namespace ch::core;` 放进 `namespace riscv {` **内部**；
example 版放在**全局**。⇒ 重指到 lib 后，依赖全局可见性的 TU 会**编译失败**
（不是行为差异）⇒ 逐 target 编译验证；必要时在 TU 内**显式**补
`using namespace ch::core;`（MUST NOT 回退到「保留 example 副本」）。

---

## 阶段

### Phase 1: 护栏先行（Task 1）

先立尺子（ADR-046 D11 + D12 契约先行）：

- [ ] 1.1 `scripts/verify_no_duplicate_rv32i_headers.sh`（4 条检查）
- [ ] 1.2 允许清单初始 1 条（`rv32i_pipeline_top.h`）
- [ ] 1.3 **RED 验证**（3 个变异：同名副本 / 反向 `-I` / stale 豁免）
- [ ] 1.4 挂 CI

### Phase 2: 死头删除（Task 2）

- [ ] 2.1 `git rm` 17 个
- [ ] 2.2 编译 + `.d` 复核

### Phase 3: 提升（Task 3）

- [ ] 3.1–3.3 9 个 `git mv` + 1 个改名 + 2 个 cache
- [ ] 3.4 namespace 核对
- [ ] 3.5 消费者 `#include` 改写

### Phase 4: 双副本收敛（Task 4）

- [ ] 4.1–4.2 3 组重指 + 可见性收窄处理
- [ ] 4.3–4.5 `hazard_unit.h` mux 拆解（等价性 + 断言语义不变）

### Phase 5: 反向依赖消除（Task 5）

- [ ] 5.1–5.4 6 处 `-I` + 6 个 TU + 作废注释

### Phase 6: 回归（Task 6）

- [ ] 6.1–6.6 编译 / `ctest -LE perf` / `-L verilator_e2e` / `-L perf` / 既有护栏 / 新护栏

### Phase 7: 文档（Task 7）

- [ ] 7.1–7.5 三份 `AGENTS.md` + ADR-046 回填 + KNOWN LIMITATIONS 保留

### Phase 8: 归档（Task 8）

- [ ] 8.1–8.4 validate / delta 自检 / critic / archive + 手写 `## Purpose`

---

## 复现命令（验收用）

```bash
cmake -B build && cmake --build build -j"$(nproc)"
cd build && ctest -LE perf --output-on-failure
cd build && ctest -L verilator_e2e --output-on-failure
cd build && ctest -L perf --output-on-failure
bash scripts/verify_no_duplicate_rv32i_headers.sh
bash scripts/verify_example_test_wiring.sh
bash scripts/verify_example_assertion_quality.sh
bash scripts/verify_test_counts.sh
bash scripts/verify_rst_n_wiring.sh
bash run_all_ported_tests.sh
bash scripts/run_all_samples.sh
openspec validate rv32i-canonical-convergence-2026-10
bash scripts/verify_openspec_delta.sh rv32i-canonical-convergence-2026-10
```

## 验收基线（2026-10-24）

| 指标 | 基线 | 来源 |
|---|---|---|
| `ctest -N` | 199 | root `AGENTS.md` |
| `ctest -LE perf` | 188/188 | C1 归档实测 |
| `ctest -L verilator_e2e` | 34 | root `AGENTS.md` |
| `ctest -L perf` | 11/11（1338 s） | C1 归档实测 |
| `run_all_ported_tests.sh` | 28/28 | C1 归档实测 |
| `run_all_samples.sh` | 35/35 | C1 归档实测 |
| `examples/riscv-mini/src/*.h` | 32 | 本 plan 实测 |
| `include/cpu/**/*.h` | 20 | 本 plan 实测 |
