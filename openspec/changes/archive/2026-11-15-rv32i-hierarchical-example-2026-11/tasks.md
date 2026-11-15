# Tasks

## 1. R21-A · 发射器（`src/hierarchy_verilog.cpp` + `include/hierarchy_verilog.h`）

- [x] 1.1 **H-A**：`add_conn` 冲突循环跳过 `into_child == false` 的行（方向感知）+ 源码注释写反向不变式
- [x] 1.2 **H-B**：兄弟边分支登记 `parent_landing_wire[lca\x1finst\x1fchild_port] = wire`
- [x] 1.3 `HierarchyEmitResult::rewired_pins`（结构化字段 · 成功路径填 · 顺序 = 登记序）
- [x] 1.4 spike：探针 `ok == true` 且边数 **20 → 0**
- [x] 1.5 产物级 lint：`--lint-only --top-module top` ⇒ 退出码 0 · 零 `%Error` · 零 `%Warning`

## 2. R21-B · 成层（`include/cpu/pipeline/rv32i_pipeline.h`）

- [x] 2.1 7 处 `ch_module` ⇒ `ch_layer` + 显式模块类型名（ADR-070 §2.2 的 7 个）
- [x] 2.2 `load_lane` **保持** `ch_module`（ADR 字面集合不含它 ⇒ 不擅自新增契约名）
- [x] 2.3 交付粒度发射：`ok == true` 且 8 个 module（7 stage + `top`）

## 3. R21-C · 判据

- [x] 3.1 `tests/test_hierarchy_verilog.cpp`：负向对照 (b) 跨 LCA fan-out **仍 fail-closed** 且点名
- [x] 3.2 `tests/test_hierarchy_verilog.cpp`：负向对照 (c) 兄弟边 + 父 `output` 端口 ⇒ 映射一次 + `assign` + `rewired_pins`
- [x] 3.3 负向对照 (a) **结构性不可达**（如实登记 · 不写「已覆盖」）
- [x] 3.4 `examples/riscv-mini/tests/test_rv32i_hier_rtl.cpp` `[product]`：粒度 / 确定性 / 单连接不变式 / 顶层端口表 / lint 门 / `rewired_pins`
- [x] 3.5 同文件 `[equivalence]`：逐拍等价（198 拍 × 9 观测）+ 手推期望（写回序列 / `sw` 地址 / ID 指令序列）
- [x] 3.6 同文件 `[mutation]`：篡改产物一条 `assign` ⇒ 等价性转红
- [x] 3.7 `examples/riscv-mini/CMakeLists.txt` 注册 target（`verilator_e2e` + 资源锁 + `TIMEOUT 900`）

## 4. 回归与锚点

- [x] 4.1 `ctest -LE perf` 全绿（数字以 `CTEST_TOTAL` 锚点为准）
- [x] 4.2 `verify_hierarchical_rtl.sh`（R1–R9）+ `--self-test` 全绿
- [x] 4.3 `verify_hierarchical_port_naming.sh` / `verify_hier_corpus_coverage.sh` 全绿
- [x] 4.4 锚点同步（`CTEST_TOTAL` +1 · `VERILATOR_E2E_TOTAL` +1）

## 5. 归档与交回

- [x] 5.1 `openspec archive`（**禁** `--skip-specs`）⇒ `rtl-hierarchical-output` ADDED ×2
- [x] 5.2 主 spec 新增 Requirement 可 grep
- [x] 5.3 残差 `id=25`（语料全套 R2–R8 未接入 ⇒ `已知`）+ 台账 §7 同批 —— **architect 面** ⇒ ⇒ ✅ **2026-11-15 已闭**（登记 + 台账行 25 ⇒ `verify_arch_ledger_residuals.sh` PASS（26 行））
- [x] 5.4 ADR-060 §2.2 G3 的「逐字逐序」依据句订正（实测只对根端口段成立）—— **architect 面** ⇒ ⇒ ✅ **2026-11-15 已闭**（⚠️ **该依据句实际在 ADR-062 §5** ⇒ ⇒ **两侧都在**：ADR-060 **§2.2 G3-1 理由列 + 选项表 row 9**（= **v1.20** 收窄为「根端口段」）**与** ADR-062 **§5**（= **v1.7** 收窄 + 排他句 + 机械切法）⇒ ⇒ ⚠️ **2026-11-15 订正**：原文只提 ADR-062 一侧 ⇒ ⇒ **ADR-060 侧当时漏改** ⇒ ⇒ 已由 critic 第 2 轮 BLOCKER 指出并补齐）
- [x] 5.5 ADR-060 / ADR-066 判据正文（行 2 适用面 + 反向不变式 + 决策⑤ 适用面）—— **architect 面** ⇒ ⇒ ✅ **2026-11-15 已闭**（ADR-060 **v1.19 §2.1.1** + ADR-066 **v1.6 §2.5** ⇒ ⇒ **决策内容零变更**）
- [x] 5.6 ADR-070 §6 ⑦ **契约强度**（产物级契约名 vs 明文非契约）—— **用户裁决** ⇒ ⇒ ✅ **2026-11-15 已闭**（用户裁决 ⇒ **明文非契约** ⇒ change `2026-11-15-hier-module-name-not-contract-2026-11` 已归档）
- [x] 5.7 roadmap R21 行改写（旧文本写「23 处双驱动点单驱动化」，与本 change 冲突）—— planner ⇒ ⇒ ✅ **2026-11-15 已闭**（R21 行已改为「✅ 已实施并归档」+ 范围订正 + 基线订正（23 → **20**））

## Verify

```bash
# ① spike（边数必须为 0）
bash scripts/probe_r21_hier_granularity.sh

# ② 交付物判据（产物 + 等价性 + 判别性自证）
cmake --build build -j"$(nproc)" --target test_rv32i_hier_rtl
PATH="$PWD/build/verilator-install/bin:$PATH" ./build/examples/riscv-mini/test_rv32i_hier_rtl

# ③ 发射器级负向对照
./build/tests/test_hierarchy_verilog "[policy]"

# ④ 既有语料零回归
ctest --test-dir build -LE perf --output-on-failure
bash scripts/verify_hierarchical_rtl.sh
bash scripts/verify_hierarchical_rtl.sh --self-test

# ⑤ 归档判据
test -f openspec/changes/archive/2026-11-15-rv32i-hierarchical-example-2026-11/proposal.md
grep -q "双类消费者" openspec/specs/rtl-hierarchical-output/spec.md
grep -q "RV32I 层次化交付物的成层粒度" openspec/specs/rtl-hierarchical-output/spec.md
```
