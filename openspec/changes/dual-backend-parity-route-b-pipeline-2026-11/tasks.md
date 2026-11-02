# Tasks：路线 B 第 6 批 —— `chlib::pipeline` 流水线控制语义双后端

## C1 —— 新增 dual 文件（**原文件 MUST NOT 修改**）

- [x] `tests/chlib/test_pipeline_dual.cpp`（**3** pair）
      ⇒ pair 1 `PipelineStage`（8 拍）· pair 2 函数式 `pipeline_reg`（7 拍）·
      pair 3 `PipelineChain<3>`（6 拍）
- [x] `tests/CMakeLists.txt`：1 条 `add_dual_backend_test`
- [x] 原文件 `tests/chlib/test_pipeline.cpp` **未修改**（`git diff` 空）

## C2 —— 判据（N2′：断言数 == 2 ×）

- [x] `test_pipeline_dual`：**42 == 2 × 21**（有/无 verilator 实测）
- [x] 期望值**全部手推**：文件头逐拍列出（`kStageSteps` 8 / `kFuncSteps` 7 /
      `kChainOut` 6），语义前提逐条引自 `include/chlib/pipeline.h`
- [x] ⚠️ 时序语义**实测确认**（不是假设）：`Simulator::eval_sequential` 先跑时序
      指令 ⇒ 组合输出在 `tick()` 后呈现**沿后**的寄存器值

## C3 —— 实施期修正（如实登记）

- [x] 🔴 **SIGSEGV**：裸 `PipelineStage stage(this, "x")` 构造的子组件其 `describe()`
      **不被框架调用**（`Component::build_internal` 只对顶层组件调）
      ⇒ 子组件 `ch_reg` 从未创建 ⇒ 首次运行即 SIGSEGV
      ⇒ 已改用 `ch::ch_module<T>` ⇒ ⇒ 该用法纪律在既有测试里**不可被发现**
      （它只做 `REQUIRE(true)` 编译性检查）
- [x] 🔴 **初稿表述被实测推翻**：曾写「函数式 `pipeline_reg` 的 `flush` 会清寄存器」
      ⇒ **错** —— `flush` 只让 `update = 0` ⇒ `select(update, d, reg)` 取 `reg`
      ⇒ 寄存器保持。⇒ ⇒ `pipeline_reg` 与 `PipelineStage` 的语义差异
      **只有 `rst` 一项**（前者清零 / 后者只抑制更新）⇒ 已订正文件头
- [x] 🔴 **判别力缺口（变异 M3 首次未被抓住）**：初版两张表都**只在 `reg == 0` 时
      施加 `rst`** ⇒ 「保持 0」与「清零成 0」**不可区分** ⇒ 变异 M3 全绿
      ⇒ ⇒ 重推两张表：`rst`/`flush` 之后**紧跟一个 `stall` 拍**再观测输出
      （`update=0` ⇒ 寄存器保持 ⇒ 才暴露其真实值）⇒ 现已 6/6 全抓
- [x] `PipelineChain` 的输入由**全同值**改为**递增不同值** ⇒ 才能精确钉住
      「恰好 3 级延迟」（同值序列只能钉「延迟 ≥ 3」）

## C4 —— 变异证明判别力（6/6 全抓，每条先确认仍可编译）

| 变异 | 内容 | 结果 |
|---|---|---|
| M1 | `PipelineStage` 的 `stall` 门控失效 | 2 failed |
| M2 | `PipelineStage` 的 `flush` 组合清零失效 | 2 failed |
| M3 | `pipeline_reg` 的 `rst` 不再清零 | 2 failed |
| M4 | `PipelineStage` 的 `rst` 也清零（与 `pipeline_reg` 混同） | 2 failed |
| M5 | `PipelineStage` 的 `flush` 也清寄存器 | 2 failed |
| M6 | `PipelineChain` 退化为单级 | 2 failed |

- [x] 每条变异均**先确认仍可编译**（error 计数 = 0）⇒ 非假绿
- [x] 还原用 `cp`（刷新 mtime ⇒ 强制重建）⇒ 复验回到 **42 断言全绿**

## C5 —— ⚠️ 同批被**推迟**的部分（`chlib::memory`）

- [x] 已写 `tests/chlib/test_memory_dual.cpp`（192 行）⇒ ⇒ **未注册、未落地**
      （留档 `/tmp/opencode/sread-divergence/`）
- [x] 原因：`sread` 的**禁读拍**存在**真 parity 分歧**
      （interpreter 保持上一拍值 / Verilog 清零）⇒ 两后端不可能有同一期望值
      ⇒ 无法满足 N2′
- [x] 用户 2026-11-02 裁定 = **选项 A**：独立 change 修分歧，`memory` dual 等修好后补
- [x] 已建 `openspec/changes/sread-disable-cycle-divergence-2026-11/` 登记该分歧

## C6 —— 计数与验收

- [ ] 锚点：`CTEST_TOTAL` **222** · `VERILATOR_E2E_TOTAL` **54** ·
      `DUAL_BACKEND_PARITY_TOTAL` **45** · `PAIRS` **233** · non-perf **211** ·
      `tests/chlib/test_*.cpp` **40**
- [ ] `ctest -LE perf` **211/211** 零回归
- [ ] `verify_test_counts.sh` / `verify_dual_backend_parity.sh` 双绿
- [ ] 全部 `scripts/verify_*.sh` + `--self-test` exit 0
- [ ] 归档：**用** `--skip-specs`（`.openspec.yaml` 已显式声明 no spec changes）
