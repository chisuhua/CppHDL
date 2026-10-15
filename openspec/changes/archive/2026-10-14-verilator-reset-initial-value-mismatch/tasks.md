# verilator-reset-initial-value-mismatch

> **STATUS (2026-10-14)**: 起草中。4 阶段 TDD 结构。
>
> **Phase 依赖**：阶段 1 独立（RED 基线）；阶段 2 依赖阶段 1（修 backend）；阶段 3 依赖阶段 2（fixture 改回非零初值）；阶段 4 = verify gate + archive。

## 阶段 1 — RED: 复现 bug

- [ ] 1.1 在 `tests/test_verilator_trace_parity.cpp::TraceParity - BundleMultiFieldTwentyCycles` fixture 中临时把 `reg_b(0_d)` 改为 `reg_b(10_d)`（保留 `reg_a(0_d)` / `reg_c(0_d)`）
      ⏳ Pending
- [ ] 1.2 跑 `ctest -R test_verilator_trace_parity --output-on-failure` —— 期望 cycle 1 b 字段失败 `REQUIRE(b_a_v == b_b_v)`，interpreter=11 verilator=1（差异 10）
      ⏳ Pending
- [ ] 1.3 把 1.1 临时改动 git stash 掉（保留作为后续 RED 钉死）
      ⏳ Pending

## 阶段 2 — GREEN: 修 backend

- [ ] 2.1 提取 `static void pulse_reset(VerilatorBackend *self)` helper 到 `src/core/verilator_backend.cpp` 匿名 namespace（4-step 序列：reset=1→eval→reset=0→eval）
      ⏳ Pending
- [ ] 2.2 `VerilatorBackend::reset()` (lines 791-813) 替换为调用 `pulse_reset(this)`，diff 限制为函数体改写，行为不变
      ⏳ Pending
- [ ] 2.3 `VerilatorBackend::try_initialize()` 现有 force-low 块（lines 365-373）后追加 `pulse_reset(this)` 调用
      ⏳ Pending
- [ ] 2.4 `src/codegen_verilog.cpp::print_reg()` (lines 604-610) 字面量宽度规范化：用 `sdata_type(static_cast<uint64_t>(lit->value()), node->size())` 替代 `get_literal_str(lit->value())`
      ⏳ Pending
- [ ] 2.5 `src/codegen_verilog.cpp::print_reg()` 对非 `type_lit` 的 init_node 加 `CHWARN` 警告（替代静默 `1'b0` 回落）
      ⏳ Pending
- [ ] 2.6 把 1.3 stash 还原（即恢复 1.1 的 reg_b(10_d) 改动）
      ⏳ Pending
- [ ] 2.7 跑 `ctest -R test_verilator_trace_parity --output-on-failure` —— 期望 3/3 fixture 全过（含非零初值 bundle fixture）
      ⏳ Pending
- [ ] 2.8 跑 `ctest -R test_verilator_bundle_e2e --output-on-failure` —— 期望 4/4 全过（无回归）
      ⏳ Pending
- [ ] 2.9 跑 `ctest -R test_verilator_e2e_harness --output-on-failure` —— 期望 4/4 全过（0_d fixture 行为不变）
      ⏳ Pending
- [ ] 2.10 跑 `ctest -R test_axi_lite_verilator_e2e --output-on-failure` —— 期望 2/2 全过（AXI4-Lite 不动）
      ⏳ Pending
- [ ] 2.11 跑 `ctest -R test_verilator_backend --output-on-failure` —— 期望所有 verilator backend 测试全过
      ⏳ Pending
- [ ] 2.12 **commit 1**：`fix(verilator): pulse default_reset at initialize() to load ch_reg init values + normalize reset literal width to reg width`

## 阶段 3 — REFACTOR: fixture 清理 + 文档同步

- [ ] 3.1 `tests/test_verilator_trace_parity.cpp::BundleMultiField<8>::describe()` 改 `ch_reg(0_d/10_d/20_d)`（移除 D8 (b) workaround）
      ⏳ Pending
- [ ] 3.2 `tests/test_verilator_trace_parity.cpp` 删除注释 "All three registers start at 0 to avoid the VerilatorBackend ch_reg initial-value mismatch bug (D8 (b))"（约 line 227-237 注释块）
      ⏳ Pending
- [ ] 3.3 `tests/test_verilator_bundle_e2e.cpp::BundleAssignmentChain` 保留 `ch_reg(0_d)`——它本来就是 0 初值设计；移除 fixture 内 D8 (b) 注释
      ⏳ Pending
- [ ] 3.4 跑 `ctest -R "test_verilator_(trace_parity|bundle_e2e|e2e_harness|backend)" -j$(nproc)` —— 期望所有 test 全过
      ⏳ Pending
- [ ] 3.5 AGENTS.md "VERILATOR BACKEND RULES" 增加条目 "initialize MUST pulse default_reset"（CRITICAL 区块）
      ⏳ Pending
- [ ] 3.6 **commit 2**：`test(verilator): revert D8(b) trace parity reg-init workaround + lock pending step (verilator-reset-initial-value-mismatch)`

## 阶段 4 — VERIFY: 全量 ctest + openspec validate + archive

- [ ] 4.1 `cmake --build build -j$(nproc)` 0 errors, 0 warnings on changed files
      ⏳ Pending
- [ ] 4.2 `ctest -E "perf_tests|perf_three_way" -j$(nproc)` 158 + alias 全过
      ⏳ Pending
- [ ] 4.3 `ctest -R "test_verilator_(trace_parity|bundle_e2e)" --output-on-failure` 至少 7 个 TEST_CASE 全部实际执行（非 SKIP）
      ⏳ Pending
- [ ] 4.4 `ctest -R "test_verilog_(arith|compare|shift|gen)" --output-on-failure` 8 个 lint 升级全过（无回归）
      ⏳ Pending
- [ ] 4.5 `openspec validate verilator-reset-initial-value-mismatch --strict` 通过
      ⏳ Pending
- [ ] 4.6 `openspec validate --specs --strict` 10/10 passed（含新 capability `verilator-reg-init`）
      ⏳ Pending
- [ ] 4.7 `openspec archive verilator-reset-initial-value-mismatch` 成功

## 关联基线

- **ADR-035 §M5 e2e tier**：本 change 是 §M5 的 reg init 覆盖完善
- **verilator-backend-e2e spec**：本 change 在其上新增 reset 脉冲时序契约
- **oracle 诊断**：详见 `openspec/changes/verilator-reset-initial-value-mismatch/design.md` Bug A/B 描述
- **D8 (b) descope** (strengthen-verilator-e2e-coverage)：本 change 是其 follow-up