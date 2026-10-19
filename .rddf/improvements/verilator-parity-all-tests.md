---
name: verilator-parity-all-tests
priority: P0
source: 用户在 cache-complete-rewrite 合并后提出"所有单元测试除了 interpreter 路径外，都要有 verilator 后端的路径"
phase: phase-4
category: testing
type: feature
created_at: 2026-10-19
---

# Improvement: 所有单元测试都需走 interpreter + verilator 双后端路径

- **Date drafted**: 2026-10-19
- **Drafted by**: build-agent (cache-complete-rewrite Session B 收尾)
- **Status**: 📋 Handoff document — 5-segment draft, 等待 rdd-planner / rdd-builder 启动
- **Source**: 用户在 cache-complete-rewrite 合并后提出"所有单元测试除了 interpreter 路径外，都要有 verilator 后端的路径"

## 1. Context

**当前状态**（截至 commit `bb286a2`，main HEAD）:

| 测试 | interpreter 路径 | verilator 路径 |
|---|---|---|
| `tests/test_arithmetic*`, `tests/test_basic`, `tests/test_reg*`, `tests/test_literal*` | ✅ | ❌ |
| `tests/test_module*`, `tests/test_operator*` | ✅ | ❌ |
| `tests/test_io_*`, `tests/test_simple_io` | ✅ | ❌ |
| `tests/test_bundle*` (12 个) | ✅ | ❌ |
| `tests/test_ch_stream`, `tests/test_stream_*` (5 个) | ✅ | ❌ |
| `tests/test_mem*` (4 个) | ✅ | ❌ |
| `tests/chlib/test_cache_*` (3 个) | ✅ | ❌ |
| `tests/chlib/test_pipeline*`, `tests/chlib/test_fifo*` 等 | ✅ | ❌ |
| `tests/test_axi_*` (7 个) | ✅ | ❌ |
| `tests/test_rv32i_pipeline`, `tests/test_jit_*` | ✅ | ❌ (JIT 单独测试) |
| **已覆盖 verilator**: | | |
| `tests/test_verilator_trace_parity` | ✅ | ✅ (counter / AXI4-Lite / bundle 3 fixture) |
| `tests/test_verilator_e2e_harness` | ❌ | ✅ (counter 1 fixture) |
| `tests/test_verilator_bundle_e2e` | ❌ | ✅ (bundle 4 fixture) |
| `tests/test_verilator_stream_e2e` | ❌ | ✅ (stream 2 fixture) |
| `tests/test_verilator_memory_e2e` | ❌ | ✅ (sync_fifo + axi4_full 2 fixture) |
| `tests/test_verilator_complex_e2e` | ❌ | ✅ (rv32i_pipeline + spi_fsm 2 fixture) |
| `tests/test_verilator_backend` | ❌ | ✅ (backend 自身 API) |
| `tests/test_verilator_error_flow` | ❌ | ✅ (VerilatorError 类型契约) |

**已覆盖范围** (per `root AGENTS.md §Verilator 三路 perf 对比`):
- perf test 走 interpreter + JIT + Verilator 三路 (但仅 perf, 1-2 fixture)
- 8 个专项 verilator test 覆盖 11 个 fixture (counter/AXI4-Lite/bundle/stream/sync_fifo/axi4_full/rv32i_pipeline/spi_fsm)

**未覆盖**:
- ~150 个 `add_catch_test` 单元测试**仅走 interpreter**, 任何 codegen 对这些 design 走通与否**完全未验证**
- 包括 `ch_mem` / `ch_reg` / bundle / stream / fifo / axi4 peripherals / RV32I 集成 全部子模块

**核心风险**:
- interpreter 与 Verilator codegen 是**两套独立实现路径**, shared-pass 测试只覆盖 ~7% 设计 surface
- cache-complete-rewrite (commit bb286a2) 的 `ch_mem` 走通在 interpreter 上, 但 Verilator codegen 路径**完全没 e2e 验证**
- riscv-mini 集成一旦启用 VerilatorBackend, **潜在 codegen bug 可能首次暴露**, 而非预期集成时一次性通过

## 2. Why (动机)

**核心问题**: interpreter pass ≠ verilator pass。

interpreter 和 verilator codegen 是两套独立路径:
- interpreter: `src/ast/instr_*.cpp::eval()` 直接计算 `data_map_`
- verilator: `src/codegen_verilog.cpp` emit SystemVerilog → 第三方 verilator 编译 → dlopen Vtop → `VerilatorBackend::eval_*` 通过 QData accessor 读写 port

**已知差异点** (per `root AGENTS.md VERILATOR BACKEND RULES`):
- port bitwidth > 64 → `CHERROR` (silent truncation in QData cast)
- 2-step clock toggle (`clk=1 → eval → clk=0 → eval`) 必须精确
- initialize() 必须 pulse default_reset
- `mkdir` 缓存目录必须 recursive

**已发现的真实 bug** (历史):
- 2026-10-13 ADR-035 verilator-PATH fix: `test_verilator_gen` 等 3 个 test 静默 fail 因 verilator 不在 PATH
- 2026-10-14 strengthen-verilator-e2e-coverage: 7 个 test × 4 phase 才补齐 counter/AXI4-Lite/bundle/stream/memory/complex trace parity
- issue #25 "always_ff may not propagate" — interpreter 没暴露, verilator 才暴露

**预期收益**:
- **测试覆盖率**: 单元测试 verilator 覆盖从 7% → 100% (11 fixture → ~160 fixture)
- **riscv-mini 集成信心**: cache/rv32i_pipeline 走 VerilatorBackend 不再有"首次集成才发现 codegen 错"风险
- **回归速度**: 任何 codegen 回归在对应 test 立即失败, 不等 nightly
- **fmax 测量可信度**: 之前 perf test 走 verilator 但只 1-2 fixture; 全 coverage 后 fmax 数据更接近真实 workload

**预期成本**:
- **CI 资源**: 现有 verilator test 总耗时 ~94s (perf verilator fixture); 新增 ~150 fixture 预估 ~30-60 min CI wall-clock
- **开发工作量**: 详见 §5 估算 2-3 周
- **维护负担**: 每个新单元测试都需 follow 同一 pattern (interpreter + verilator fixture setup)

## 3. What Changes (草案)

### A. 引入统一 dual-backend test fixture (新基础设施)

**新文件**: `tests/test_dual_backend_fixture.h` (新)

提供 `DUAL_BACKEND_TEST(name, tags)` 宏, 接受 lambda `(auto& sim, auto& dev)` 写一次, 自动跑两遍:
- 第一遍: interpreter backend (默认)
- 第二遍: verilator backend (如 `verilator` 在 PATH, 否则 SKIP)

参考模式:
```cpp
DUAL_BACKEND_TEST("ch_uint add 32-bit", "[bitwise][dual]") {
    ch::ch_device<Adder32> dev;
    ch::core::context ctx("test");
    ch::core::ctx_swap swap(&ctx);
    ch::Simulator sim(dev.context());
    auto& dut = dev.instance();

    sim.set_input_value(dut.io().a, 0x1234u);
    sim.set_input_value(dut.io().b, 0x5678u);
    sim.tick();
    REQUIRE(static_cast<uint64_t>(sim.get_port_value(dut.io().sum)) == 0x1234 + 0x5678);
}
```

**宏实现** 内部:
- 创建临时目录 (verilator workdir)
- 第一次: `set_backend(InterpreterBackend())`, 跑 lambda
- 第二次: `set_backend(VerilatorBackend(workdir))`, `initialize(...)`, 跑 lambda
- 比较 trace 或返回值

### B. 重构所有现有单元测试 (迁移工作)

**目标**:
- `tests/test_basic.cpp`, `tests/test_arithmetic*.cpp`, `tests/test_module*.cpp` 等基础 test → 走 dual backend
- `tests/test_bundle*.cpp` (12 个) → 走 dual backend
- `tests/test_ch_stream*`, `tests/test_stream*.cpp` → 走 dual backend
- `tests/test_mem*.cpp` (含 `test_mem_comb_read.cpp` 已有 aread/sread parity) → 走 dual backend
- `tests/chlib/test_cache_*.cpp` (3 个) → 走 dual backend (cache-complete-rewrite 关键)
- `tests/chlib/test_pipeline*.cpp`, `tests/chlib/test_fifo*.cpp` → 走 dual backend
- `tests/test_axi_*.cpp` (7 个) → 走 dual backend
- `tests/test_rv32i_pipeline.cpp` → 走 dual backend

**迁移策略**:
- 不重写 test 内容, 改用 `DUAL_BACKEND_TEST` 宏替换 `TEST_CASE` 宏
- 依赖 fixture 自动跑两遍
- 行为验证统一: interpreter 已知正确, verilator 必须产出相同结果

### C. 新增特定 fixture 覆盖已知风险点

- 64-bit port boundary test (verilator QData cast 风险)
- 2-step clock toggle timing test (verilator backend rules)
- default_reset pulse test
- hierarchical input driver test (`verilator-hier-input` spec §R1)
- cache 走 VerilatorBackend e2e (cache-complete-rewrite follow-up)

### D. CMake 集成

`tests/CMakeLists.txt`:
- 每个 `add_catch_test` 加 `RESOURCE_LOCK "verilator_backend"` (避免 verilator build 并发)
- 加 `ENVIRONMENT "${_vl_path_env}"` (verilator PATH 注入)
- 加 `LABELS "base"` (默认 ctest -LE perf 不跑)
- 新增 `add_catch_test(test_dual_backend_template ...)` 验证 macro 工作

### E. CI 矩阵调整

- 现有 nightly: 增加 `ctest -LE perf -L verilator_e2e` 全跑
- 新 PR matrix: `BUILD_VERILATOR=OFF` (快速反馈) 仍只跑 interpreter
- 新 PR matrix: `BUILD_VERILATOR=ON` 跑 dual backend (增加 ~30 min PR feedback)

## 4. Acceptance

1. **覆盖率 100%**:
   - 所有 `add_catch_test` 单元测试都通过 `DUAL_BACKEND_TEST` 宏或显式 dual-backend fixture 跑两遍
   - `ctest -L verilator_e2e` 输出 ≥ 100 个 test (基线 11, 目标 ≥ 100)

2. **零回归**:
   - `ctest -LE perf` 仍 149/149 PASS
   - `ctest -L perf` 仍 11/11 PASS
   - CTEST_TOTAL 不下降

3. **基础设施稳定**:
   - `DUAL_BACKEND_TEST` 宏 1 行 setup, 0 行 teardown
   - 新增 test 模板 ≤ 现有 TEST_CASE 复杂度
   - 编译时间增量 ≤ 10%

4. **riscv-mini 集成可走 verilator**:
   - `tests/test_rv32i_pipeline.cpp` 走 verilator backend pass
   - `examples/riscv-mini/src/i_cache_prefetch.h` 实例化 ICacheSync 通过 verilator e2e

5. **资源锁正确**:
   - 多个 dual-backend test 并发不踩 verilator build 目录
   - skip 行为 (verilator not on PATH) 与现有 test_verilator_*.cpp 一致

## 5. 工作量估算

| Step | 工作量 | 备注 |
|---|---|---|
| 1. 设计 DUAL_BACKEND_TEST macro + fixture | 1 天 | 参考 `test_verilator_trace_parity.cpp` 模式 |
| 2. 实现 tests/test_dual_backend_fixture.h | 1 天 | trace 收集, 临时目录管理, skip 行为 |
| 3. 迁移基础 test (test_basic, test_arithmetic, test_module) | 2 天 | ~30 个 test, 验证 macro 稳定 |
| 4. 迁移 cache test (test_cache_simple, test_cache_pipeline, test_cache_complete) | 1 天 | 13 test_case / 53 assertions, 验证 cache e2e |
| 5. 迁移 bundle / stream / fifo / axi4 test | 3 天 | ~50 个 test, 复杂度中等 |
| 6. 迁移 RV32I pipeline test + riscv-mini integration | 2 天 | 1 test but 复杂; 需 ch_module 容器支持 |
| 7. 64-bit boundary / clock toggle / reset pulse 专项 fixture | 1 天 | 新增 ~5 个 fixture |
| 8. CMake 集成 + CI matrix 调整 | 0.5 天 | RESOURCE_LOCK / ENVIRONMENT 注入 |
| 9. 全量回归 (interpreter + verilator) | 1 天 | ctest -L verilator_e2e 全跑, 修 bug |
| **总计** | **~12.5 天** | 单人工作量, 含 debug |

**vs 现状**:
- 8 个 verilator test × 4 phase (per strengthen-verilator-e2e-coverage spec) 历史 5 天
- 本提案规模 ~3 倍, 因覆盖 100% 而非 4 fixture

## 6. Risks & Mitigation

| Risk | 等级 | Mitigation |
|---|---|---|
| VerilatorBackend 暴露 cache / bundle / stream / axi4 codegen 多个 bug | 🟡 中 | 提案第 9 步预留 1 天 debug; 每个 bug 单独 follow-up ADR (类似 ADR-035 issue #25) |
| CI 时间增量 30-60 min 不可接受 | 🟡 中 | 提案分阶段: P0 基础 test → P1 cache → P2 bundle/stream → P3 完整; 每阶段独立 ship |
| DUAL_BACKEND_TEST macro 抽象泄漏 verilator 细节 | 🟡 中 | 提案 §3.A 设计阶段咨询 oracle + critic; 优先参考 `test_verilator_trace_parity.cpp` 模式 |
| 资源锁 (RESOURCE_LOCK) 增加 CI 串行 | 🟢 低 | `verilator_backend` 锁已存在, 加 test 不增新锁 |
| verilator cold build 10-30 min 拉长首次 PR feedback | 🟡 中 | CI matrix 仍允许 `BUILD_VERILATOR=OFF` 快速反馈; verilator 完整 run 推到 nightly |
| Trace parity 比较失败 → 改用 port value 比较 (简单) | 🟢 低 | 提案 §3.A 描述 "比较 trace 或返回值", 优先返回值, 简化 macro |
| 每个新单元测试都需 follow 同一 pattern (interpreter + verilator fixture setup) | 🟡 中 | 维护负担, 但由 macro 自动化 |

## 7. 阶段化 (建议)

避免一次性 ship 100% 覆盖, 分 4 个独立 change:

### Phase 1: DUAL_BACKEND_TEST 基础设施
- 提案 §3.A 完整落地
- 迁移 ~10 个基础 test (test_basic, test_arithmetic) 验证 macro
- 单独 change: `dual-backend-test-fixture`
- 不动其他 test

### Phase 2: cache + chlib 关键 test 迁移
- 迁移 test_cache_complete (13 test_case) — cache-complete-rewrite 收尾
- 迁移 test_cache_simple, test_cache_pipeline
- 单独 change: `dual-backend-cache-tests`
- 暴露 cache 在 verilator 的 codegen bug (预期)

### Phase 3: bundle / stream / fifo / axi4 test 迁移
- ~50 个 test 迁移
- 单独 change: `dual-backend-chlib-tests`

### Phase 4: RV32I pipeline + riscv-mini integration
- test_rv32i_pipeline + rv32i fixtures
- 单独 change: `dual-backend-rv32i-integration`

### Phase 5: 全量 + 64-bit boundary 专项
- 100% 单元测试覆盖
- 64-bit port / clock toggle / reset 专项
- 单独 change: `dual-backend-full-coverage`

## 8. Out of Scope (明确划线)

- ❌ JIT 后端强制走 (JIT 路径已 `tests/jit/test_jit_*.cpp` 单独覆盖, 与本提案正交)
- ❌ 重构现有 test_verilator_*.cpp (它们已有 verilator 路径, 仍可作为"全 e2e fixture" reference)
- ❌ codegen 性能优化 (per fixture per codegen 优化是单独 concern)
- ❌ 替代现有 `test_verilator_e2e_harness` 模式 (本提案补充而非替代)

## 9. Open Questions

1. **资源锁粒度**: 用现有 `verilator_backend` 全局锁 (简单但串行) 还是 per-test 锁 (并行但复杂度)?
2. **临时目录管理**: 每次 test 单独目录 (隔离好但慢) 还是 per-test-suite 共享 (快但有冲突风险)?
3. **trace vs port value**: 简单 port value 比较 vs 完整 trace parity (更严但 macro 复杂度高)?
4. **CI matrix 调整**: 新 PR 是否强制 verilator run? 还是 nightly only?
5. **是否本 change 关闭 RISK/Out-of-Scope 中的 verilator 已知风险** (port > 64 等)?

## 10. See also

- `root AGENTS.md` §VERILATOR BACKEND RULES (port 64-bit, clock toggle, reset pulse, mkdir recursive)
- `openspec/specs/verilator-backend-e2e/spec.md` (已有 verilator backend 端到端 spec)
- `openspec/specs/verilator-e2e-coverage/spec.md` (7 个 test × 4 phase 历史决策)
- `openspec/specs/verilator-rv32i-e2e/spec.md` (RV32I + Verilator e2e spec)
- `openspec/specs/verilator-hier-input/spec.md` (hierarchical input driver spec)
- `openspec/specs/verilator-reg-init/spec.md` (default_reset pulse spec)
- `tests/test_verilator_trace_parity.cpp` (推荐 macro 实现参考)
- `tests/test_verilator_e2e_harness.cpp` (verilator build + dlopen 流程参考)
- ADR-035 (VerilatorBackend 整体设计)
- ADR-041 (cache Sync/Async variant) — 本提案 §3.C cache e2e 直接受益
- `.rddf/improvements/cache-stub-physical-removal.md` (backlog, 等本提案 Phase 2 落地)

## 11. 关键决策点 (请 user 拍板)

1. **Macro 抽象深度**:
   - 选项 A: 简单 `DUAL_BACKEND_TEST` macro (lambda + trace 比较)
   - 选项 B: 完整 `BackendPair<T>` template fixture (类型安全但复杂)

2. **CI matrix**:
   - 选项 A: 新 PR 强制 verilator run (~30 min feedback)
   - 选项 B: 快速 PR 跳过, nightly 跑 (现状)
   - 选项 C: 分层 — 基础 test PR 必跑, 完整 coverage nightly 跑

3. **Phase 1 范围**:
   - 选项 A: 仅 macro 基础设施, 不迁 test (单独 change)
   - 选项 B: macro + 10 个基础 test 验证 (推荐)
   - 选项 C: macro + 50 个 test 一次性迁

4. **是否需要 JIT 后端同步覆盖**? (历史 issue #25 cache JIT 路径未验证)

5. **trace parity vs port value parity**? (影响 macro 实现复杂度)

## 12. 后续行动

按 5-segment 格式已完整, 可进入 rdd-planner 阶段:
- `/rdd-planner` 启动: 写 openspec change 包 (proposal + design + tasks)
- 决策关键点 §11 的 5 个问题
- 按 §7 阶段化分 5 个独立 change ship

---

## 13. Planner 阶段落地 (2026-10-19 session)

**角色**: planner (用户切换)

**三个并行 subagent 调研**:
- `oracle` —— macro 抽象架构建议 (推荐 C: A 优先 + B 演进位)
- `metis` —— 草案 gap 分析 (10 个隐藏意图 / 雷区)
- `explore` —— 物理可行性 file:line 级摸底

**关键决策收口** (用户拍板):

| § | 决策点 | 选择 |
|---|---|---|
| §11.1 | Macro 抽象深度 | **C**: A 优先 + B 演进位 (`BackendKind` + `DUAL_SNAPSHOT` 留接口不实现) |
| §11.2 | CI matrix | **A**: PR 必跑 macro dual-backend (~30-45 min feedback)，配合 .github/workflows/ci.yml 加 `pr-verilator-fast` job |
| §11.3 | Phase 1 范围 | **B**: macro + 8-10 基础 test + 3 专项 fixture + ci.yml 改动 |
| §11.4 | JIT 同步 | **C**: interpreter + verilator 优先；JIT 留 `BackendKind` 枚举位扩展 |
| §11.5 | 比较策略 | **A**: 端口值比较 (macro 内部 per-cycle 断言) |
| 新 #6 | Macro 抽象最终 | C (同 §11.1) |
| 新 #7 | 覆盖面 | 广: "所有可 dual" (~10-15 个，~130 个写进 spec 排除清单) |
| 新 #8 | >64-bit 端口 | 字段检测 + SKIP verilator half (interpreter 仍跑全) |
| 新 #9 | ci.yml 冲突 | 显式列出 `pr-verilator-fast` job 改动 |
| 新 #10 | 9 个现有 verilator test | 保留原状 + 补 `LABELS verilator_e2e` + 顶注释 ref impl |

**openspec 产出** (本提案三件套):

```
openspec/specs/dual-backend-test-coverage/spec.md                              ← 永久 capability 契约 (13 requirements, 通过 validate --strict)
openspec/changes/verilator-parity-all-tests/
├── proposal.md                                                                 ← Why + What Changes + Acceptance + Open Q
├── design.md                                                                   ← Macro API + CMake function + ci.yml 改动 + 5 phase 拆分
└── specs/dual-backend-test-coverage/spec.md                                   ← ADDED Requirements delta (用于 archive 合并)
```

**校验状态**:
- `openspec validate --specs --strict` → **14/14 PASS**
- `openspec validate verilator-parity-all-tests --strict --type change` → **is valid**

**对原草案 §3 / §5 / §6 / §7 的关键修订**:

1. **草案 §3.A macro 签名 `(auto& sim, auto& dev)` 修订**：必须双 device (`dev_a`/`dev_b`)，否则 node_id 是 context-local 复用会破 parity (oracle 共识)
2. **草案 §3.A "第一遍 interpreter 默认" 修订**：必须显式 `set_jit_enabled(false) + set_backend(InterpreterBackend)` 双保险钉死 interpreter 路径 (`is_native()==false` 仍走 JIT 分支)
3. **草案 §3.B "现有 ~150 个测试都迁" 修订**：实际只能迁 ~10-15 个 device+port 形态；其余 ~130 个 STATIC_REQUIRE / 裸 context / aread-based / 无 Component 写进 spec 排除清单 (R6)
4. **草案 §4 验收 `ctest -L verilator_e2e ≥ 100` 修订**：改为 `≥ 12` (9 旧 + ≥ 3 新)；`CTEST_TOTAL` 数字由实测 `ctest -N | tail -1` 回填，禁止拍脑袋
5. **草案 §5 工估"~12.5 天"修订**：单 builder 落地 Phase 1 预估 ~5-7 天 (8-10 基础 test + 3 专项 fixture + ci.yml + 9 旧 test label)；Phase 2-5 不在本 change scope
6. **草案 §6 RISK 补 5 条**：JIT 钉不死 / 现有 9 个 verilator test 漏列 `test_axi_lite_verilator_e2e` / global RESOURCE_LOCK 串行导致墙钟爆炸 / SPEC validate 需补 MUST 关键字 / `add_catch_test` 函数不支持 RESOURCE_LOCK/ENVIRONMENT/TIMEOUT 需新 function

**Phase 1 落地状态**: ✅ openspec change 包已就绪；下一阶段 (builder) 由用户切换后开工。

**隐藏阻塞项（planner 阶段识别，需先立项）**:
- i_cache_prefetch.h wrapper SIGSEGV (root AGENTS.md §KNOWN LIMITATIONS) — **阻塞 Phase 4**，建议单独立项修 wrapper 后再走 dual-backend-RV32I
- cache-stub-physical-removal backlog (ADR-037) — 必须在 dual-backend-cache-tests (Phase 2) 之后，避免同一批测试被改两次
- `.rddf/roadmap.md` 当前不存在 — 5 phase 无 sprint view 可插，需先建 roadmap 或改为"独立 improvement 排队"
