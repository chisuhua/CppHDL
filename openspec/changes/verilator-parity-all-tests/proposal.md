# Change: verilator-parity-all-tests — 单元测试 interpreter + verilator 双后端全覆盖

## Why

**现状**（截至 commit `bb286a2`，main HEAD）：项目内 ~150 个 `add_catch_test` 单元测试**仅走 interpreter 路径**，而 codegen（`src/codegen_verilog.cpp` → verilator → Vtop → QData accessor）是与 interpreter 并行的**独立实现路径**。`verilator-e2e-coverage` spec 已固化 9 个专项 verilator 测试覆盖 11 个 fixture（counter / AXI4-Lite / bundle / stream / sync_fifo / axi4_full / rv32i_pipeline / spi_fsm），但占 design surface 不到 7%。

**真实代价**：

| 历史痛点 | 暴露点 |
|---|---|
| `2026-10-13` ADR-035 verilator-PATH fix | 3 个 verilator 测试因 PATH 静默 fail；codegen ≠ interpreter 的"双后端统一"需求被首次提出 |
| `2026-10-14` strengthen-verilator-e2e-coverage | 7 个 test × 4 phase 才补齐 counter/AXI4-Lite/bundle/stream/memory/complex trace parity，证明"逐 fixture 迁移"成本 ≈ 5 天 |
| issue #25 "always_ff may not propagate" | interpreter 完全没暴露，verilator 才暴露；codegen bug 延迟到 RV32I 集成 nightly 才发现 |
| `2026-10-13` cache-complete-rewrite（commit `bfb3960`，已合并） | `ch_mem` 走通在 interpreter，但 Verilator codegen 路径**完全没 e2e 验证**，与 riscv-mini 真集成的潜在 codegen bug 阻断了 dual-backend 信心 |

**预期收益**：

1. **覆盖率 100%**：单元测试 verilator 覆盖从 7% → 100%（11 fixture → ~12-15 fixture，含 Phase 1 落地）
2. **riscv-mini 集成信心**：cache/rv32i_pipeline 走 VerilatorBackend 不再有"首次集成才发现 codegen 错"风险
3. **回归速度**：任何 codegen 回归在对应 TEST_CASE 立即失败（≤45 min PR feedback），不等 nightly
4. **fmax 数据可信**：perf 三路对比扩面后基线更接近真实 workload

**预期成本**：

- 一次性 CI 墙钟增量 ~30-45 min/PR（per R10 决策）
- 改造工作量 ~12.5 人天（单 builder，含 debug）
- 维护模式：每个新单元测试都需 follow "dual-backend 可行性"判定 + 走 macro 或排除清单

**Out of Scope**（明确划线）：
- ❌ JIT 后端同步纳入（interpreter + verilator 已足够；JIT 走 `tests/jit/`，正交 concern）
- ❌ 重构现有 9 个 `test_verilator_*.cpp`（它们是 macro 的 reference impl，迁移会破 `verilator-e2e-coverage` 等既有 spec 的 file-level / count-level contracts）
- ❌ 重写 codegen 让 `>64-bit` 端口被支持（属 ADR-035 R3 物理上限，需独立 ADR 才能动）
- ❌ 替代现有 `test_verilator_e2e_harness` / `test_verilator_*_e2e` 模式（本 change 是补充而非替代）

## What Changes

### 1. 新建 `openspec/specs/dual-backend-test-coverage/spec.md`

定义永久 capability 契约，包含 13 个 `## Requirement`（已通过 `openspec validate --strict`）。详见该 spec。

### 2. 新建 `tests/dual_backend_fixture.h`

提供 `DUAL_BACKEND_TEST(DutType, name, tags)` macro 与 `DUAL_BACKEND_GOLDEN_TEST(DutType, name, tags)` macro（Async cache / aread 设计的 golden-value 用）。实现细节见 `design.md` §API。

### 3. 新建 `tests/CMakeLists.txt::add_dual_backend_test()` 函数

封装现有 `add_catch_test`，外部追加 `RESOURCE_LOCK "verilator_backend"` + `ENVIRONMENT "${_vl_path_env}"` + `LABELS "verilator_e2e"` + 可选 `TIMEOUT` kwarg。

### 4. 迁移 ~10 个基础测试（Phase 1 范围）

从草案 §3.B 的 ~150 个候选中，按 R6 排除清单扫描后挑出 **10 个可 dual 的基础测试**，用 `DUAL_BACKEND_TEST` 替换 `TEST_CASE`（仅改 1 行）。候选名单（按 explore subagent 调研）：

| 测试名 | Fixture 形态 | 是否可 dual |
|---|---|---|
| `test_basic` (部分 case, 有 ch_device 的) | ch_device + ch_out | ✅ |
| `test_arithmetic` *实际为 chlib/test_arithmetic | 裸 context | ❌ (R6) |
| `test_module` (ch_device+ch_module 段) | ch_device + ch_out | ✅ |
| `test_reg_timing` | ch_device + ch_out | ✅ |
| `test_io_*` (partial, 有 simulator 的) | ch_device + ch_out | ✅ |
| `test_simple_io` (有 simulator 部分) | ch_device + ch_out | ✅（分类后）|
| `test_ch_stream` (部分 case, 有 ch_device 的) | ch_device + ch_out + ch_stream | ✅ |
| `test_ch_reg_bitwise` | 裸 context + ch_reg | ❌ (R6) |
| `test_mem_comb_read` | 含 aread | ❌ (R6) |
| `test_state_machine` | DSL lambda | ❌ (R6 Catch2 SECTION) |

**Phase 1 目标**：≥ 8 个可 dual 基础 test，**不强求 10 个**（按 explore 实测 4 个真能跑的，本提案"~10 个"是宽松目标；7+ 即符合 R6 ≥ 100% 允许 boundary）。

### 5. 新建 3 个专项 fixture

- `tests/test_dual_backend_64_bit.cpp` — 64-bit 端口边界 + `>64-bit` 降级路径
- `tests/test_dual_backend_clock.cpp` — 2-step 时钟 toggle
- `tests/test_dual_backend_reset.cpp` — default_reset pulse

### 6. CI matrix 调整（**显式列出 ci.yml 改动**）

`.github/workflows/ci.yml` 新增 `pr-verilator-fast` job：
- 触发条件：`pull_request`
- 命令链：`cmake -B build -DBUILD_VERILATOR=ON && cmake --build build -j$(nproc) && ctest -L verilator_e2e -LE perf`
- timeout 45 min
- 与现有 `pr-build` job 并行

### 7. 现有 9 个 verilator 测试补充 `LABELS "verilator_e2e"` + 顶注释

不迁移测试体，仅补 `set_tests_properties` + 文件首行 `// Reference implementation for DUAL_BACKEND_TEST macro (see openspec/specs/dual-backend-test-coverage)`。

### 8. 五阶段拆分

| Phase | 内容 | 单独 change 名 | 风险等级 |
|---|---|---|---|
| 1 | macro + 8-10 基础 test + 3 专项 fixture + ci.yml PR job + 9 个旧 test 补 label | `dual-backend-test-fixture` | 🟢 低 |
| 2 | cache Sync test 迁移（`test_cache_complete` 等） | `dual-backend-cache-tests` | 🟡 中（ADR-041 Sync/Async 分类） |
| 3 | bundle / stream / fifo / axi4 peripherals 迁移 | `dual-backend-chlib-tests` | 🟡 中（~50 个 test） |
| 4 | RV32I pipeline + riscv-mini integration | `dual-backend-rv32i-integration` | 🔴 高（i_cache_prefetch.h wrapper SIGSEGV 先单独立项修） |
| 5 | 全量覆盖 + cache-stub-removal 触发 | `dual-backend-full-coverage` | 🟡 中（依次） |

**Phase 1 风险最低，先 ship**。每个 Phase 独立评审、独立 commit、独立 archive。后续 phase 等 Phase 1 实测墙钟 / 回归稳定后再开。

**注意**：本 change 只落地 Phase 1。Phase 2-5 是 backlog / 提案，不在本 proposal scope。

## Acceptance

### Phase 1 验收（仅本 change）

1. ✅ **capability spec 落地**：14 个 spec 通过 `openspec validate --specs --strict`
2. ✅ **macro + CMake function 落地**：
   - `tests/dual_backend_fixture.h` 存在
   - `add_dual_backend_test` 在 `tests/CMakeLists.txt` 中定义并被至少 1 个新 test 引用
3. ✅ **8-10 个基础 test 完成 dual-backend 迁移**：
   - `grep -L "STATIC_REQUIRE\|aread\|ch_out<ch_uint<[0-9]\{3,\}>>" tests/test_*.cpp` 输出名册中 ≥ 8 个文件通过 `DUAL_BACKEND_TEST` 替代 `TEST_CASE`
   - `ctest -N -L verilator_e2e --test-dir build` 输出 ≥ 12 个 test target（9 旧 + ≥ 3 新，spec R7 阈值）
4. ✅ **3 个专项 fixture 注册**：见 spec R10
5. ✅ **CI PR job 落地**：`.github/workflows/ci.yml` 出现 `pr-verilator-fast` job
6. ✅ **现有 9 个 verilator 测试保留原状**：仅加 `LABELS verilator_e2e` + 顶注释 2 类变更；`git diff` 限 4 行/文件
7. ✅ **CTEST_TOTAL 同步**：根 `AGENTS.md §NOTES` 的数字由实测 `ctest -N | tail -1` 回填
8. ✅ **零回归**：
   - `ctest -LE perf` 仍 N/N PASS（N = 当前 baseline，0 退化）
   - `ctest -L perf` 仍 11/11 PASS
   - `ctest -L verilator_e2e -LE perf` 全绿（含新注册 test 不出现 verilator-only 失败）

### Phase 1 实测基线

以下数字由 Phase 1 builder 实测回填（禁止拍脑袋）：

- `ctest -N | tail -1` → `CTEST_TOTAL: <TBD>`
- `ctest -N -L verilator_e2e -LE perf | tail -1` → `<TBD> verilator_e2e non-perf test`
- `time ctest -L verilator_e2e -LE perf` → 墙钟 `<TBD> 秒`（必须 ≤ 45 min）
- `bash scripts/verify_test_counts.sh` → exit 0
- `openspec validate verilator-parity-all-tests --strict --type change` → exit 0

## Open Questions

1. **Phase 1 实测墙钟超 45 min 怎么办**？
   - 选项 A：自动降级——不动 Phase 1 基础 test 数量，仅延后 Phase 2-5（已在 design.md / R10 §"PR feedback 时间不爆 45 min" 锁定）
   - 选项 B：Phase 1 缩减到 5 个 test（β 版先 ship）
   - 选项 C：保留 10 个但 `RESOURCE_LOCK` 改为 per-cache-key 锁（与现有全局锁不同名）
   - 默认选项 A（已在 R10 Scenario "PR feedback 时间不爆 45 min" 锁定，避免 reviewer 反复决策）

2. **9 个现有 verilator 测试 > 4 行 diff 的容忍**？
   - 选项 A：极严格（≤ 4 行/文件，仅 label + 注释 2 类）
   - 选项 B：宽松（≤ 20 行/文件，允许顺手清理 dead code）
   - 默认选项 A

3. **DUAL_BACKEND_GOLDEN_TEST 真的需要独立 macro，还是同 macro 加 tag**？
   - 选项 A：独立 macro（语义清晰）
   - 选项 B：同 macro + `[backend-shift-N]` tag（macro 列表小）
   - 默认选项 A（已在 R11 写明，Phase 1 落地独立 macro）

## Sub-spec changes

- **新建**：`openspec/specs/dual-backend-test-coverage/spec.md`（已通过 validate）
- **未改**：现有 13 个 spec（`verilator-e2e-coverage` / `verilator-backend-e2e` / `verilator-rv32i-e2e` / `verilator-hier-input` / `verilator-reg-init` / `mem-read-timing` / `cpu-cache-subsystem` 等）一律保留原状，不在本次 archive 时合并

## Capabilities affected

- **新增 capability**：`dual-backend-test-coverage`（永久 spec）

## Out of Scope（再列一遍）

- ❌ JIT 后端纳入（interpreter+verilator 已足够；JIT 正交）
- ❌ codegen >64-bit 端口接受（独立 ADR 决策，本 change 不动 ADR-035 R3）
- ❌ 重构 9 个现有 test 体
- ❌ Phase 2-5 实施（仅本 change 落地 Phase 1）

## See also

- `.rddf/improvements/verilator-parity-all-tests.md` — 用户原始需求 draft
- `openspec/specs/dual-backend-test-coverage/spec.md` — capability 永久契约
- `openspec/specs/verilator-e2e-coverage/spec.md` — 既有 9 个 verilator 测试契约
- `openspec/specs/verilator-backend-e2e/spec.md` — VerilatorBackend 接口契约（dlopen/cache/3-eval）
- `openspec/specs/verilator-rv32i-e2e/spec.md` — RV32I + Verilator e2e 契约（Phase 4 续接）
- `openspec/specs/cpu-cache-subsystem/spec.md` — cache Sync/Async 分类（Phase 2 续接）
- `openspec/specs/mem-read-timing/spec.md` — aread/sread 时序契约（R11 cache 分类依据）
- `.rddf/improvements/cache-stub-physical-removal.md` — 关联 backlog（必须在 dual-backend cache test 后执行）
- ADR-035 — VerilatorBackend 设计决策
- ADR-041 — cache Sync/Async variant split
- `docs/developer_guide/verilator-integration.md` — Verilator 安装与 PATH
- 根 `AGENTS.md §VERILATOR BACKEND RULES` — 64-bit cap / clock toggle / reset pulse

## 决策收口（planner session 2026-10-19）

| 决策点 | 决策 | 来源 |
|---|---|---|
| Macro 抽象深度 | C：A 优先 + B 演进位 | oracle 推荐 + 用户拍板 |
| CI matrix | A：PR 必跑 macro dual，~30-45 min feedback | 用户拍板 |
| Phase 1 范围 | B：macro + ~10 个基础 test | 用户拍板 + draft §3.B 子集 |
| JIT 同步 | C：A 优先，JIT 留 BackendKind 枚举位 | 用户拍板 |
| 比较策略 | A：端口值比较（per-cycle） | 用户拍板 + metis §1.4 |
| 覆盖面 | 广：所有可 dual（~10-15 个） | 用户拍板 |
| >64-bit 处理 | 字段检测 + SKIP（interpreter 仍跑全） | 用户拍板 |
| 现有 9 个 test 关系 | 保留原状 + 补 label + 标注 ref impl | 用户拍板 + 3 个 subagent 共识 |
| Phase 1 范围 base list | 8-10 个 | explore 调研后修订（draft 估 10 个偏高） |
| 5 阶段拆分风险 | Phase 4 隐藏 SIGSEGV 阻塞，已显式标 backlog | metis §5 |
