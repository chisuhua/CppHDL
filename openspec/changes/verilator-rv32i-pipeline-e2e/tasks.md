# verilator-rv32i-pipeline-e2e

> **STATUS (2026-10-14)**: 起草中。4 阶段 TDD 结构。
>
> **Phase 依赖**：阶段 1 独立（RED：编写 fixture）；阶段 2 依赖阶段 1（GREEN：fix 或 descope）；阶段 3 依赖阶段 2（REFACTOR：清理）；阶段 4 = verify gate + archive。

## 阶段 1 — RED: 实现 RV32I fixture

- [ ] 1.1 在 `tests/test_verilator_complex_e2e.cpp` 实现 `Rv32iPipelineFixture` —— `ch::ch_module<riscv::Rv32iPipeline>` 内嵌 + imem ch_mem + dmem stub
      ⏳ Pending
- [ ] 1.2 imem 装 3 条短指令编码：`addi x1, x0, 1` (0x00000094) / `addi x2, x0, 2` (0x00000113) / `add x3, x1, x2` (0x002081B3) + 1 个 padding (0x00000013)
      ⏳ Pending
- [ ] 1.3 dmem stub：write=0, read_data=0x0（3 条 ALU 指令不触发 load/store）
      ⏳ Pending
- [ ] 1.4 改写 `ComplexE2E - Rv32iShortInstructionTraceParity` TEST_CASE，移除 documented SKIP，interpreter + verilator 各 tick 20 cycle，对比 `perf_instructions` 终值
      ⏳ Pending
- [ ] 1.5 跑 `ctest -R test_verilator_complex_e2e --output-on-failure` —— 期望可能失败（fixture 实现细节错误 / 编码错）或通过
      ⏳ Pending

## 阶段 2 — GREEN: 修复或 descope

- [ ] 2.1 **如果 1.5 失败**：分析失败原因（interpreter vs verilator 数值差 / fixture 编译错 / verilator 编译失败 / TIMEOUT）
      ⏳ Pending
- [ ] 2.2 **如果是 fixture 实现 bug**（如 include path、imem 编码、端口绑定错）→ 修复 fixture，本 change 内继续
      ⏳ Pending
- [ ] 2.3 **如果是 VerilatorBackend bug**（如 register propagation issue #25 影响 commit_count / always_ff reset 与 verifier 不一致）→ D8 (b) descope：
      - TEST_CASE 转 `SKIP("backend bug #N; see follow-up change <name>")`
      - tasks.md 记录 `⏸️ Deferred to <follow-up-change-name>`
      - **本 change 不阻塞 archive**
      ⏳ Pending
- [ ] 2.5 跑 `ctest -R test_verilator_complex_e2e --output-on-failure` —— 期望通过（绿）
      ⏳ Pending
- [ ] 2.6 跑 `ctest -R test_verilator_e2e_harness --output-on-failure` —— 0 回归
      ⏳ Pending
- [ ] 2.7 跑 `ctest -R test_verilator_backend --output-on-failure` —— 0 回归
      ⏳ Pending
- [ ] 2.8 跑 `ctest -R "test_verilator_(trace_parity|bundle_e2e|stream_e2e|memory_e2e|complex_e2e)" --output-on-failure` —— 5 个测试间态无回归
      ⏳ Pending
- [ ] 2.9 **commit 1**：`test(verilator): rv32i pipeline e2e trace parity (3 short instructions)`

## 阶段 3 — REFACTOR: 清理 documented SKIP 注释

- [ ] 3.1 移除 `ComplexE2E - Rv32iShortInstructionTraceParity` TEST_CASE 名末尾的 `[deferred]` tag（如果 2.3 descope 已加）
      ⏳ Pending
- [ ] 3.2 移除 TEST_CASE 文件 header 注释中的 "DESCOPE (R10)" 段（如果 descope）
      ⏳ Pending
- [ ] 3.3 添加简短注释指明本 fixture 来源：`// Follow-up to strengthen-verilator-e2e-coverage D8 (b) (2026-10-14 archive).`
      ⏳ Pending
- [ ] 3.4 AGENTS.md "VERILATOR BACKEND RULES" 加条目："RV32I e2e test 用 ch_mem sread 加载 imem，不允许 aread（verilator-e2e-coverage §Requirement 1 invariant 3）"
      ⏳ Pending
- [ ] 3.5 **commit 2**：`test(verilator): rv32i e2e test cleanup + AGENTS.md sread rule`

## 阶段 4 — VERIFY: 全量 ctest + openspec validate + archive

- [ ] 4.1 `cmake --build build -j$(nproc)` 0 errors, 0 warnings on changed files
      ⏳ Pending
- [ ] 4.2 `ctest -E "perf_tests|perf_three_way" -j$(nproc)` 158 + alias 全过
      ⏳ Pending
- [ ] 4.3 `ctest -R test_verilator_complex_e2e --output-on-failure` 至少 2 个 TEST_CASE 全部实际执行
      ⏳ Pending
- [ ] 4.4 `ctest -L verilator -R perf_three_way` 全过（verilator 三路子集不变）
      ⏳ Pending
- [ ] 4.5 `openspec validate verilator-rv32i-pipeline-e2e --strict` 通过
      ⏳pending
- [ ] 4.6 `openspec validate --specs --strict` 11/11 passed（含新 capability `verilator-rv32i-e2e`）
      ⏳ Pending
- [ ] 4.7 `openspec archive verilator-rv32i-pipeline-e2e` 成功

## 关联基线

- **verilator-e2e-coverage** change (archive 2026-10-14) —— D8 (b) descope 起源
- **verilator-reset-initial-value-mismatch** change (archive 2026-10-14) —— 提供 t=0 reset pulse，本 fixture 受益
- **ADR-035 §M5 e2e tier** —— 本 change 是 §M5 的代表性复杂设计覆盖完善
- **rv32i_pipeline.h:54** —— `perf_instructions` 观测点定义
- **rv32i_decoder.h / id_stage.h** —— RV32I 指令编码 / 字段位置