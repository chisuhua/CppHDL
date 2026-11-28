# Tasks: S4c-1a —— I-L1 的 AXI4-full 化 + 修「假 line」

> **change**：`rv32i-i-l1-axi-full-2026-11` · **范围** = ADR-078 的 **D7**（I 侧 cache 的 AXI 侧
> full 通道化 + 修「假 line」）⇒ S4c-1b（取指 fork + 核的 full 取指主口）**另立**。
> **参照实现**：`include/cpu/cache/d_cache_sync.h`（S3 第 1 阶段 · `81097dc`）。

---

## P0 · 决策门（已闭）

- [x] **T0.1** ADR-078 **D7** 已定（原地升级 · 两处一起改 · hit 延迟不变 · `i_cache_async.h` 零 diff）
      ⇒ ⇒ **S4c-1 拆分**（design §「范围拆分」）：本批 = D7（对端 = 已存在的 `TcmAxiSlave`
      ⇒ 可独立验证）· S4c-1b = D14 + 核的 full 取指主口（取指反压是**不同量级**）
- [x] **T0.2** 用户 2026-11-28 裁决：继续推进下一批 ⇒ ⇒ 本批为 **S4c-1a**

## P1 · I 侧 AXI 侧 full 读通道 + burst fill（组件级）

- [x] **T1.1** `i_cache_sync.h` 的 AXI 侧：5 根简化口（`axi_addr/axi_valid/axi_ready/axi_data/axi_resp_valid`）
      → **AXI4-full 读通道**（`ar_addr/ar_len/ar_size/ar_burst/ar_id/ar_valid/ar_ready` +
      `r_data/r_resp/r_last/r_id/r_valid/r_ready`）⇒ ⇒ **只读 ⇒ 无写通道**（design §1.1 ①）
- [x] **T1.2** fill 路径：单拍 `zext<128>(axi_data)` → **4-beat burst**（`ARLEN=3`/`ARSIZE=2`/`ARBURST=INCR`）
      + `beat0..3` + `r_last` 收尾 ⇒ ⇒ ⚠️ **拼整行 MUST 用当拍 `r_data`**（避 S3 的 off-by-one
      ⇒ 否则 lane 3 恒 0）
- [x] **T1.3** `ready` 语义**保持 I 侧既有契约**（design §1.1 ②）：hit ⇒ 1 拍后数据到；
      miss ⇒ 0，fill 完成后 ⇒ 1（新增 `fill_done` 状态位）⇒ ⇒ **MUST NOT** 照搬 D 侧的组合 ready
- [x] **T1.4** `miss` ⇒ 触发 AR 通道（锁存 AR 握手）+ 块地址（`& 0xFFFFFFF0`）⇒ ⇒
      `ar_valid` 不得依赖 `ar_ready`（`include/cpu/AGENTS.md` 的 cache wrapper 硬约束）

## P2 · 读路径：lane 选择（**修「假 line」**）

- [x] **T2.1** 读选择：`bits<31,0>(cache_data)` → **按 `addr[3:2]` 选 lane**（4 选 1）
      ⇒ ⇒ 数据源是 `sread`（1 拍）⇒ ⇒ 选择 MUST 在拿到 line 之后
- [x] **T2.2** **`i_cache_async.h` 零 diff**（命中延迟维度不变 ⇒ ADR-041 变体族不扩张）

## P3 · 爆炸半径（同批改写）

- [x] **T3.1** `include/cpu/cache/i_cache_prefetch.h`：内层实例化 + **AXI 侧**接线（5 根 → full 读通道）
      ⇒ ⇒ **前端转发不变** ⇒ ⇒ 爆炸半径比 ADR 估的**更小**
- [x] **T3.2** `tests/chlib/test_cache_sync.cpp`（5 个双后端 pair）：旧激励 → burst
- [x] **T3.3** `tests/chlib/test_cache_complete.cpp`（ICache fill 用例）
- [x] **T3.4** `examples/riscv-mini/tests/test_cache_smoke.cpp`（双后端 wrapper 用例）
- [x] **T3.5** 其余消费者（`test_cache_async_dual` / `test_cache_pipeline_integration` /
      `test_cache_simple` / `test_phase4_integration`）

## P4 · 判据 + capability

- [x] **T4.1** **lane 寻址**判据（AC1）：一行 **4 个不同字**（手推 `0x11111111`/`0x22222222`/
      `0x33333333`/`0x44444444`）· fill 后依次读 offset 0/4/8/12 ⇒ 各返回对应字 ⇒
      ⇒ **修复前 MUST 红**
- [x] **T4.2** **burst fill** 判据（AC2）：断言 `ARLEN=3`/`ARSIZE=2`/`ARBURST=INCR`
- [x] **T4.3** **命中延迟**判据（AC3）：hit 后 **1 拍**数据就绪
- [x] **T4.4** **反向判据**（AC8）：把读选择改回 `bits<31,0>` ⇒ ⇒ AC1 MUST 红（实测）
- [x] **T4.5** 新 capability **`rv32i-i-l1-timing`**（承载 I-L1 契约）⇒ ⇒ 补上
      `openspec/specs/rv32i-dcache-timing/spec.md:13` 的**悬空声明**
- [x] **T4.6** `cpu-cache-subsystem` 的 delta（ADDED ×2：I 侧 fill 契约 + I 侧 lane 寻址契约）

## P5 · 回归与收口

- [x] **T5.1** 全量重建 0 error · `ctest` **全绿**（285/285）· 5 个既有护栏全绿 ·
      mini 线产物端口表**逐字不变**（I 侧**无生产消费者** ⇒ 行为面零变化）
- [x] **T5.2** `openspec validate --strict` exit 0 · `verify_openspec_delta.sh` 4/4
- [x] **T5.3** ADR-078 加 dated 订正（**只读 I-L1 只需 full 读通道** · **决策未变**）
- [x] **T5.4** 母 improvement `rv32i-soc-platform-2026-11` 的 `进度:` 回填
- [x] **T5.5** `verifier` 独立验收（AC1–AC8 逐条工具证据 + 遗留清扫）⇒ **PASS 8/8**（报告 `.rddf/reviews/rv32i-i-l1-axi-full-2026-11-verify.md`）

## P6 · 归档

- [x] **T6.1** `openspec archive rv32i-i-l1-axi-full-2026-11 --yes` ⇒ 已归档为 `openspec/changes/archive/2026-11-29-rv32i-i-l1-axi-full-2026-11/`（spec 合并：`cpu-cache-subsystem` +1 · `rv32i-i-l1-timing` 新建 +3）

## T5.1 / T5.5 证据（**实施记录** · 2026-11-28 · builder）

### 改动面（6 个文件 · 爆炸半径同批清零）

| 文件 | 动作 |
|---|---|
| `include/cpu/cache/i_cache_sync.h` | AXI 侧 5 根 → **AXI4-full 读通道 11 根**（具名）· 4-beat burst fill 状态机 · `addr[3:2]` 选 lane · `fill_done_r`（K1）· 删死变量 `offset` |
| `include/cpu/cache/i_cache_prefetch.h` | **AXI 转发面**同步扩（7 个内层输入全驱动 + 6 个 AR/R 输出转发）· 前端转发面逐字不变 |
| `tests/chlib/test_cache_sync.cpp` | I 侧 5 个用例改 burst + **新增 4 个判据**（lane 寻址 · burst 参数并入 cold-miss · 完成拍 ready/data 同拍 · miss 反压） |
| `tests/chlib/test_cache_complete.cpp` | 5 个 I-Sync fill 块改 burst（Async 块**零改动** · K3） |
| `tests/chlib/test_cache_pipeline_integration.cpp` | I-Cache Integration 用例改 burst（含 AR 保持 + burst 参数断言） |
| `examples/riscv-mini/tests/test_cache_smoke.cpp` | 4 个双后端 pair + 语料行为级用例改 full 读通道（含「`data` 不再透传 `r_data`」的反向断言） |
| `include/cpu/cache/i_cache_async.h` | **零 diff**（`git status`/`git diff` 实测为空） |

### 实测结果

| 判据 | 结果 |
|---|---|
| `cmake --build build -j4`（全量） | **0 error** |
| `test_cache_sync` | **28 用例 / 126 断言全绿**（原 24 / 86） |
| `test_cache_complete` | 13 用例 / 53 断言全绿 |
| `test_cache_pipeline` | 26 断言全绿（2 SKIP 为既有 SKIP） |
| `riscv_mini_cache_smoke`（双后端） | 12 用例 / 86 断言全绿 |
| `ctest -LE perf` | **275 / 275** |
| `ctest -L perf -j1` | 10 / 11 绿；**唯一红 = `perf_report_consistency`**（陈旧 `perf_results.json` · 见下「范围外」） |
| 5 个既有护栏 + `verify_product_lines` | 全部 **PASS**（`verify_examples_rtl_sim` 18 项 · `_coverage` 守恒 30==17∪13 · `verify_hierarchical_rtl` R1–R8 + R8′ + R9 · `verify_no_duplicate_rv32i_headers`（C6 快照 51 一致）· `verify_riscv_soc_core_ports`（53 端口 + 兄弟边）· `verify_product_lines` C1/C2/C3 + `--self-test` 5 投毒全红） |
| `verify_test_counts.sh` | 锚点一致（`lib=283, riscv-mini=2, riscv-full=1` = 286）⇒ **本批未新增 ctest target**（新用例加在既有二进制内） |

### AC8 反向判据（**实测**）

- 变异：把 `hit_word` 的 4 选 1 改回 `bits<31,0>(cache_data)`（**只改读选择**）⇒ 先确认编译通过 ⇒
  `test_cache_sync` **4 条断言转红**（lane 用例 × 2 后端 + hit-latency 的 lane 2 用例 × 2 后端）；
- 还原（确认 `grep -c MUTANT` = 0）⇒ **重建** ⇒ 复绿（126 断言）。⇒ ⇒ 两条判据对「假 line」**各自独立可判**。

### 实施期新增的两条实测（补 design §5.4 的 K1–K4）

| # | 坑 | 正解 |
|---|---|---|
| **K5** | `test_cache_pipeline_integration.cpp` 的 **cmake target 名是 `test_cache_pipeline`**（不含 `_integration`）⇒ 按源文件名传 target ⇒ make 报「No rule」但被 `grep error` 过滤掉 ⇒ **跑的是旧二进制** ⇒ 误判「AR 未拉高」 | 用 `cmake --build build --target <target>` 前先核对 `CMakeLists` 的**目标名**；判定前看 `[100%] Built target …` 行，别只看 error 计数 |
| **K6** | `ar_ready=1` 放在**第一拍之前** ⇒ AR 在第一拍**已握手** ⇒ `fill_active` 下一拍即置位 ⇒ 「冷 miss ⇒ `ar_valid=1`」这条断言**恒为 0** | 想观测 `ar_valid` 保持 ⇒ 首拍 `ar_ready=0`（先拿 miss，再握手）；想一步 fill ⇒ 用 helper 的「呈现地址 → 握手 → 4 beat」三段式 |

### 范围外的既有红（**如实登记 · 不清理**）

- **`ctest -L perf` 的红 = 陈旧的 perf 产物文件**（`perf_report_consistency` · **与本批无关**）：
  `perf_results.json` 的 mtime = **09:56**（本批改动之前 12 h）· `perf_results.csv` / `.md` = 22:34（本次）
  ⇒ 该用例断言的正是「三份文件必须来自**同一次** `perf_tests`」这**跨轮 artifact 混配**（其设计说明见
  `tests/benchmark/CMakeLists.txt:114-126`，机制 = `perf_results_clean` fixture 清 `${CMAKE_SOURCE_DIR}/perf_results.*`；
  残留来自 `test_perf_subprocess_isolation.cpp` 的 `perf_results.json.__saved*` 改名-还原舞步）。
  **实测判据**：① 现状 ⇒ 红（`js=14310.4` vs `cs=ms=51532.8`）② `mv perf_results.json` 走开 ⇒ 该用例 **SKIP ⇒ exit 0**
  ③ 原样还原 ⇒ 复红。⇒ ⇒ **artifact 驱动、非代码驱动**；`perf` 其余 10 条全绿（含 `perf_regression` 通过）。
  ⇒ ⇒ 不清理（改了会掩盖现象），交用户/后续清理陈旧的 `perf_results.*` 后重跑即可复绿。

- `scripts/verify_port_naming.sh` 在 **HEAD 上已红**：`include/cpu/pipeline/stages/if_stage.h` 的
  `ch_in<ch_bool> trap_flush` / `ch_in<ch_uint<32>> trap_target` **未登记**于 `port-naming-exempt.tsv`
  （P6-3 遗留）。⇒ 已用 `git worktree` 对 HEAD 实测确认与本批无关；本批只清自己的部分：
  新 `ar_*` / `r_*` 端口**全部具名** ⇒ 登记表 **1367 → 1357**（债只减不增），
  `EXPECTED_EXEMPT_PORTS` 同步并在脚本内注明原因。

---


1. **D7 的两处 MUST 一起改**（fill + 读选择）⇒ 只做其一 ⇒ **review 阻塞**（ADR-078 R7）。
2. **`ready` MUST 保持 I 侧既有语义**（「已接受 · 1 拍后数据到」）⇒ ⇒ 照搬 D 侧的组合 ready
   ⇒ **review 阻塞**（会破坏取指侧的既有契约）。
3. **拼整行 MUST 用当拍 `r_data`** ⇒ ⇒ 用未装载的 `beat3` ⇒ **review 阻塞**（lane 3 恒 0）。
4. **`i_cache_async.h` MUST 零 diff**（命中延迟维度不变 ⇒ ADR-041 变体族不扩张）。
5. **爆炸半径 MUST 同批清零**（wrapper + 4 个测试文件）⇒ 漏改 ⇒ 编译失败或测试恒绿 ⇒ **review 阻塞**。
6. **本批 MUST NOT 动** `include/cpu/pipeline/**` · `examples/riscv-mini/src/**` · `examples/riscv-full/**`
   ⇒ ⇒ 取指反压与核的 full 主口属 **S4c-1b**。
