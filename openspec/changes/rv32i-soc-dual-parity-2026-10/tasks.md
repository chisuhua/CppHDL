# 任务：rv32i-soc-dual-parity-2026-10（C3）

> ## ⏸ 状态：**挂起**（T2/T3 部分完成，产物存于 `drafts/`）
>
> 阻塞于 `.rddf/improvements/rv32i-pipeline-commit-path-2026-10.md`。
> 恢复条件见 `proposal.md` 顶部的状态块。任务编号保留（恢复后从 T2 继续）。
>
> | 任务 | 状态 |
> |------|------|
> | T1 契约 | ✅ 完成（spec delta 5 条 ADDED，`--strict` valid） |
> | T2 DUT 改造 | ✅ 代码完成（`drafts/rv32i_pipeline_top.h.draft`，编译通过）——**未入树** |
> | T3 verilator 能编译含 TCM 的 SoC | ✅ 风险排除（冷编译 13.7 s） |
> | T4 固件 | ✅ 41 条 + 编码器交叉校验（12 断言绿）——在草稿测试里 |
> | T5 双路逐周期 parity | ⛔ **失败**（暴露 5 个前置缺陷 ⇒ 本 change 挂起） |
> | T6/T7/T8 | ⏸ 未开始 |
>
> 执行模式：**lightweight**（就地，无 worktree）—— 与 C2 一致。
> 依赖：`.rddf/improvements/rv32i-soc-dual-parity-2026-10.md`（5 段草稿）+
> `openspec/changes/rv32i-soc-dual-parity-2026-10/{proposal,design}.md`。
> 设计裁决见 `design.md` 的 D1–D11。

## T1 契约先行：spec delta（D1/D2/D4）

- [x] 新建 `specs/verilator-rv32i-e2e/spec.md`（ADDED ×5：观测端口契约 /
      固件写端口与死端口禁令 / 双路逐周期 parity / JIT 归因 / JIT 回落断言）
- [x] `openspec validate rv32i-soc-dual-parity-2026-10 --strict` valid

verify:
```bash
cd "$(git rev-parse --show-toplevel)" && openspec validate rv32i-soc-dual-parity-2026-10 --strict
```

## T2 DUT 改造：`Rv32iPipelineTop`（D2/D3）

- [ ] 删除 4 个死端口（`ext_instr_data` / `ext_instr_ready` / `ext_data_read_data` / `ext_data_ready`）
- [ ] 加固件写端口 `prog_we` / `prog_addr` / `prog_data` → `itcm.io().write_*`
- [ ] 加 10 个观测输出（见 `design.md` D2 表）
- [ ] TCM 规模 `ADDR_WIDTH = 12`（`InstrTCM<12,32>` / `DataTCM<12,32>`），
      并在文件头注释写明「pc / 数据地址高位被截断 ⇒ 固件与数据必须 < 16 KB」
- [ ] 无死端口、无未驱动端口；`describe()` 里每个端口都被引用

verify:
```bash
cd "$(git rev-parse --show-toplevel)" && \
  grep -c "ch_out<" examples/riscv-mini/src/rv32i_pipeline_top.h | \
    awk '{ if ($1 < 10) { print "FAIL: 观测输出不足 10 个 (" $1 ")"; exit 1 } }'
```

## T3 早期风险验证：verilator 能编译含 TCM 的 SoC（D3）

> **先做这一步**：若 12 位 TCM 在 verilator 下编译失败或冷编译过慢，
> 整个方案的 D1 需要重开（退回 fixture wrapper）。

- [ ] 写最小 DUT 冒烟（2 条指令 + 1 个 store），双后端各跑 30 拍，
      只断言 `obs_perf_instructions` 终值一致
- [ ] 实测并记录：verilator 冷编译耗时、verilog 行数、运行内存

verify:
```bash
cd "$(git rev-parse --show-toplevel)" && \
  cmake --build build -j"$(nproc)" --target test_verilator_complex_e2e 2>&1 | \
    grep -E "error|Error" && exit 1 || true
```

## T4 固件（D4/D5）

- [ ] 测试内建 `enc_r` / `enc_i` / `enc_s` / `enc_b` + 手算常量交叉校验
      （`addi x1,x0,1` == `0x00100093`；`sw x3,16(x1)` == `0x0030A823`）
- [ ] 固件 ≥ 20 条（含 NOP 填充），覆盖 ALU-only / `sw`→`lw` / taken + not-taken branch
- [ ] 固件按「无数据前推」构造（生产者→消费者之间插 NOP，数量实测确定）
      —— 在构造处写注释指向 ADR-046 D5
- [ ] 固件加载：`ext_rst=1` 期间逐拍写 imem（D6）

verify:
```bash
cd "$(git rev-parse --show-toplevel)" && \
  test -f tests/test_rv32i_soc_dual_parity.cpp && \
  grep -c "enc_i\|enc_r\|enc_s\|enc_b" tests/test_rv32i_soc_dual_parity.cpp
```

## T5 双路逐周期 parity（D7）

- [ ] `Rv32iSoc - DualBackendTraceParity`：2×`ch_device` + 2×`Simulator`，
      ≥ 200 拍，每拍比对 10 个观测值
- [ ] 语义 golden：至少断言 3 条（ALU 结果经 store 落 dmem / load 读回值再 store /
      branch 决定哪条 store 执行）
- [ ] `perf_instructions` / `perf_branch_count` 终值断言（数值由实测确定 + 推导记录在注释）
- [ ] verilator 缺席 / `initialize()` false ⇒ `WARN` + `return`（不用 `SKIP`）

verify:
```bash
cd "$(git rev-parse --show-toplevel)" && \
  ctest --test-dir build -R test_rv32i_soc_dual_parity --output-on-failure
```

## T6 JIT 归因 + 回落断言（D8）

- [ ] `src/jit/jit_compiler.cpp::generate_ir`：在 `r != SUCCESS` 的中心失败点写
      `last_error_msg_ = "unsupported node type: <type> (node_id=N)"`
- [ ] `Rv32iSoc - JitFallbackContract`：`#if __has_include("jit/jit_compiler.h")` 包裹；
      断言 ①`is_jit_enabled()==true` && `is_jit_compiled()==false` ②`std::cout` 捕获到
      `JIT skipped` + `mem_read_port` ③回落 trace == 显式 interpreter trace
- [ ] `CH_JIT_ENABLE=OFF` 构建仍编译通过

verify:
```bash
cd "$(git rev-parse --show-toplevel)" && \
  ctest --test-dir build -R test_rv32i_soc_dual_parity --output-on-failure && \
  grep -q "unsupported node type" src/jit/jit_compiler.cpp
```

## T7 护栏 + 计数 + 文档（D9/D10/D11）

- [ ] `scripts/verify_no_duplicate_rv32i_headers.sh`：C6 快照 → 新 15 端口集合；
      删除零消费者豁免条目；`EXPECTED_DEAD_ALLOWANCES 1 → 0`；`mk_fixture` 的假顶层头同步
- [ ] `--self-test` 全绿（fixture 数量不变，判据 ⑭/⑮/⑯ 仍各有判别力）
- [ ] `tests/CMakeLists.txt` 注册 `test_rv32i_soc_dual_parity`
      （LABELS `base;verilator_e2e` + `RESOURCE_LOCK verilator_backend` + TIMEOUT 600
      + `${_vl_path_env}` + `include/cpu/pipeline` 的 include 路径）
- [ ] `CTEST_TOTAL 199 → 200`；`tests/test_*.cpp 115 → 116`（root `AGENTS.md` NOTES）
- [ ] `include/cpu/AGENTS.md` / `COMPONENT-LIFECYCLE-GUIDE.md` 端口示例同步
- [ ] root `AGENTS.md` KNOWN LIMITATIONS 的 JIT 回落条目 → 「已断言契约」

verify:
```bash
cd "$(git rev-parse --show-toplevel)" && \
  bash scripts/verify_no_duplicate_rv32i_headers.sh && \
  bash scripts/verify_no_duplicate_rv32i_headers.sh --self-test && \
  bash scripts/verify_test_counts.sh
```

## T8 门禁 + 对抗审稿 + 归档

- [ ] `cmake --build build -j$(nproc)` 0 error 0 warning
- [ ] `ctest -LE perf` 全绿（零回归）
- [ ] `run_all_ported_tests.sh` 28/28；`run_all_samples.sh` 35/35
- [ ] 既有 7 个护栏脚本 exit 0
- [ ] 对抗审稿（critic）通过
- [ ] `openspec archive` + 手写主 spec 的 `## Purpose`

verify:
```bash
cd "$(git rev-parse --show-toplevel)" && \
  ctest --test-dir build -LE perf --output-on-failure -j"$(nproc)"
```
