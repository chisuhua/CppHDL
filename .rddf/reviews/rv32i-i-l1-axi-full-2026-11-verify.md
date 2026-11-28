# 验收报告 — `rv32i-i-l1-axi-full-2026-11`（S4c-1a · I-L1 的 AXI4-full 化 + 修「假 line」）

- **角色**：`verifier`（只验不改）· **轮次**：第 **1** 轮 · **日期**：2026-11-29
- **分流判定**：**未启用 `rdd-verifier` skill** ⇒ 走**本地验收**（判据来源 + 1–5 步）。
  依据：`rdd-skills.sh --json` 的 `available[]` 含 `rdd-verifier`（可加载），但**前置条件 2 不成立** ——
  `.rddf/state/iteration.json` 里**没有**该 change 的条目（P3 archive gate 未触发），且 `tasks.md` 的
  `T5.5` / `T6.1` 仍未勾（`tasks_done != tasks_total`）。
- **结论**：**AC1–AC8 全部满足（8/8 PASS）** · 6 条硬约束全守 · **无** `implementation_gap` / `proposal_drift`
  ⇒ **验收通过**（可进入 `T6.1` 归档；归档后 `openspec/specs/rv32i-i-l1-timing/` 由 delta 合并生成）。

---

## 1. 判据来源（按优先级）

1. **用户本次验收条件**：任务清单「必须新增的判据」（lane 寻址 / burst fill / 命中延迟 / **AC8 反向判据**）
2. `openspec/changes/rv32i-i-l1-axi-full-2026-11/proposal.md` 的 `## 验收标准`（AC1–AC8）
3. change 自身声明：`tasks.md` 的 **6 条硬约束** · `design.md` §1.1 / §5.1–§5.4
4. `docs/adr/ADR-078-...md` §5 的 **AC8** + §7.1 的 dated 订正（I-L1 = full **读**通道 11 根 · 决策未变）

四者无冲突（第 4 项的「+ ID」表述已由 §7.1 收敛为「11 根 · 无 ID」，与 1–3 一致）。

---

## 2. 逐条判定

| AC | 判定 | 证据（工具 + 输出摘要） |
|---|---|---|
| **AC1 lane 寻址** | **PASS** | ① 代码：`i_cache_sync.h:146` `lane_sel = bits<3,2>(io().addr)` + `:179-183` 4 选 1（`bits<31,0>/<63,32>/<95,64>/<127,96>`）；② 用例 `ICacheSync - line has 4 independently addressable lanes`（`test_cache_sync.cpp:241-272`）= 一行 4 个**手推**字 `0x11111111/0x22222222/0x33333333/0x44444444` ⇒ 依次读 offset 0/4/8/12 各返回对应字 ⇒ `test_cache_sync` **28 用例 / 126 断言全绿**；③ 判别力见 AC8（改回 `bits<31,0>` ⇒ 该用例红） |
| **AC2 burst fill** | **PASS** | ① 代码：`i_cache_sync.h:215-217` `ar_len=3` / `ar_size=2` / `ar_burst=1(INCR)`，`:237` `fill_complete = r_fire & fill_active & r_last`；② 用例断言逐值（`test_cache_sync.cpp:88-95` cold-miss 用例 + `test_cache_pipeline_integration.cpp:70-73`）；③ 旧单拍形态**已不存在**：`grep "io()\.axi_" include/cpu/cache/i_cache_sync.h` ⇒ **0 命中**（端口已不存在 ⇒ 旧激励在编译期即不可完成 fill） |
| **AC3 hit 延迟不变 + Async 零 diff** | **PASS** | ① `git diff HEAD -- include/cpu/cache/i_cache_async.h` ⇒ **空**（零 diff）；② 命中路径仍 `sread`（`:152-155` tag/valid、`:168-169` data 均 `sread`）· 用例 `hit returns ready and data one cycle after the address`（`test_cache_sync.cpp:296-306`）**恰好 1 拍**后断言 lane 2 的字；③ 反向：把读选择改回 `bits<31,0>` 时**该用例也转红**（返回上一拍锁存的 lane 0 字）⇒ 证明「1 拍」不是空断言 |
| **AC4 爆炸半径同批清零** | **PASS** | ① 全仓 `io()\.axi_*` 残留**仅**落在 `ICacheAsync` / `DCache*`（`test_cache_complete.cpp:350,434` 等均属 `ch::ch_device<ICacheAsync>` 块 · 与 K3 一致）；`i_cache_sync.h` / `i_cache_prefetch.h` **0 处**；② wrapper 硬约束：**9 个 `ch_out` 全部无条件驱动**（`i_cache_prefetch.h:142-144,155-161`）· **7 个内层 `ch_in` 全驱动**（`:119-125`）· `ar_valid` 纯转发（不引 `ar_ready`）；③ 4 个测试文件均已改写（`git diff --stat`：sync 281 行 / complete 431 行 / pipeline 60 行 / smoke 188 行） |
| **AC5 与 D 侧 S3 形态同构 + 避开 off-by-one** | **PASS** | ① **逐式相同**：`ar_len/ar_size/ar_burst`（I `:215-217` vs D `:211-213`）· `beat3_final = select(fill_complete, io().r_data, beat3)`（I `:260` vs D `:254`）· `full_line = concat(concat(beat3_final, beat2), concat(beat1, beat0))`（I `:261` vs D `:255`）· `lane_sel = bits<3,2>`（I `:146` vs D `:159`）；② **off-by-one 判别力实测**：把 `beat3_final` 换成「完成拍取 0」（= 用未装载的 `beat3`）⇒ lane 用例 **2 条转红**（`test_cache_sync.cpp:268` · 两后端 ⇒ lane 3 = 0）⇒ 还原复绿 |
| **AC6 mini 线零行为变化** | **PASS**（含环境性红的甄别） | ① `ctest -LE perf -j4`：**271/275 绿** + 4 条 `test_rv32i_official_suite_g*` **Timeout**（**环境性**：当时 `load average 12.6` / `nproc=4`，且有**会话外**进程 `cc1plus` / `cpptlm_tests` 在编译）⇒ ② 同 4 条经 `ctest -R test_rv32i_official_suite -j1 --timeout 900` ⇒ **4/4 Passed**（196 s / 115 s …）；③ 逐条直跑二进制 ⇒ `g1..g4` 全 `rc=0`（145 / 230 / 139 / 245 s · 各 22 断言）⇒ **非回归**；④ 护栏（本轮**亲自**跑）：`verify_examples_rtl_sim`（18 项 PASS · SoC 产物端口面/lint · `checks=16 fails=0`）· `verify_examples_rtl_sim_coverage`（守恒 30 == 17 ∪ 13）· `verify_hierarchical_rtl`（R1–R8 + R8′ + R9 · **53 根手推端口表**）· `verify_no_duplicate_rv32i_headers`（**C6 快照 51 一致**）· `verify_riscv_soc_core_ports`（53 端口 + 兄弟边）· `verify_product_lines`（C1/C2/C3 + `--self-test` 5 投毒全红）；⑤ 锚点：`verify_test_counts.sh` 三方一致（`lib=283, riscv-mini=2, riscv-full=1` = 286 · `verilator_e2e` = 82） |
| **AC7 I-L1 capability 缺口** | **PASS（归档时实体化）** | ① delta 存在：`changes/.../specs/rv32i-i-l1-timing/spec.md`（Purpose 点明 I-L1 · 3 个 ADDED Requirement：`addr[3:2]` 选 lane / 命中 1 拍 / `ready` 语义）；② 悬空声明原文 = `openspec/specs/rv32i-dcache-timing/spec.md:13`「取指侧的 I-L1（各有独立 capability）」⇒ 新 capability 名 = `rv32i-i-l1-timing` ⇒ **归档合并后成立**（当前 `openspec/specs/` 下尚无该目录，属归档机制的正常时序，不是缺陷）；③ `openspec validate --strict` ⇒ `is valid` · `verify_openspec_delta.sh` **4/4** |
| **AC8 反向判据** | **PASS（**独立复现**）** | 变异（**只**改读选择为 `bits<31,0>(cache_data)`）⇒ 先确认编译 0 error ⇒ `test_cache_sync` **4 条断言转红** = lane 用例 × 2 后端（`cpp:268`）+ hit-latency 用例 × 2 后端（`cpp:303`）；还原（`grep -c` 变异标记 = 0）⇒ **重建** ⇒ **126 断言复绿** |

### 6 条硬约束逐条

| # | 约束 | 判定 | 证据 |
|---|---|---|---|
| 1 | D7 两处一起改（fill + 读选择） | ✔ | fill = `:212-267` · 读选择 = `:179-183`（两处在同一文件同批落地）|
| 2 | `ready` 保持 I 侧语义，**不**照搬 D 侧组合 ready | ✔ | `:284` `ready <<= select(hit, true, fill_done_r)`（`fill_done_r` 是 `ch_reg`，`:207` 声明 · `:281` 装载）· 用例 `Miss holds ready low until fill completes` + 完成拍 `ready/data` 同拍断言 |
| 3 | 拼整行用**当拍** `r_data` | ✔ | `:260`（见 AC5 的 off-by-one 变异实测）|
| 4 | `i_cache_async.h` 零 diff | ✔ | `git diff HEAD -- …` 空 · `git status` 无该文件 |
| 5 | 爆炸半径同批清零 | ✔ | 见 AC4 |
| 6 | 不动 `pipeline/**` · `examples/riscv-mini/src/**` · `examples/riscv-full/**` | ✔ | `git status --short` 三处**均为空** |

---

## 3. 失败项与路由建议

**零** `implementation_gap` / **零** `proposal_drift`。三条**非本批**的红/超时，如实登记（**不路由回 builder**）：

1. **`perf_report_consistency` 红 = 陈旧的 perf 产物文件**（`ctest -L perf -j1` 的 11 条里唯一红）。
   证据：`perf_results.json` 的 mtime = **09:56**（早于本批首次改动 12 h）而 `.csv` / `.md` = 22:34；
   实测 ① 现状红（`js=14310.4` vs `cs=ms=51532.8`）② `mv perf_results.json` 走开 ⇒ 该用例 **SKIP ⇒ exit 0**
   ③ 原样还原 ⇒ 复红。该用例契约本身就是「三份文件必须来自**同一次** `perf_tests`」
   （设计说明见 `tests/benchmark/CMakeLists.txt:114-126`）⇒ **artifact 驱动**。
   处置建议：清理陈旧的 `perf_results.*` 后重跑（属维护动作，非本 change 范围）；本轮**不清理**以免掩盖现象。
2. **`verify_port_naming` 红 = 2 条 P6-3 遗留**（`include/cpu/pipeline/stages/if_stage.h` 的
   `trap_flush` / `trap_target` 未登记）。证据：在 **HEAD 的独立 worktree** 上跑同一护栏 ⇒ **同样 2 条**
   ⇒ 与本批无关；本批已把自身部分清干净（新 `ar_*`/`r_*` 端口**全部具名** ⇒ 登记表 1367 → **1357**，
   `EXPECTED_EXEMPT_PORTS` 同步并在脚本内注明）。**范围外不清理**（按时段纪律）。
3. **`test_rv32i_official_suite_g1..g4` 的 ctest `Timeout`** = 机器负载（见 AC6 证据链），**非缺陷**。

---

## 4. 遗留清扫（第 5 步复核）

| 项 | 结果 |
|---|---|
| 新增 `TODO` / `FIXME` / `XXX` / `HACK` | **0**（`git diff -U0` 扫描）|
| 新增调试输出（`printf` / `std::cout`）| **0** |
| 新增 `SKIP(` | **0**（既有的 2 个 SKIP 属历史：`Rv32iPipelineWithCache not implemented`）|
| 注释掉的代码 / `#if 0` | **0**（本次只删了死变量 `offset`）|
| 临时 fallback / 未替换的 mock | **0** |
| 文档同步 | `include/cpu/AGENTS.md` 声称 **7742 行** ⇒ `wc -l` 实测 **7742** ✓；`public-api-surface.md` 已登记端口改名 + 迁移指引；ADR-078 **§7.1** dated 订正已落；母 improvement `进度:` 已回填；`tasks.md` 已勾 T1.1–T4.6 / T5.1–T5.4 |
| 公开面 | 端口面变更（5 → 11 根）已登记，迁移指引含「`ready` 语义**未变**」「lane 新语义」「`addr` 保持契约」三条易错点 |

---

## 5. NIT（**不阻塞** · 供后续触碰时顺手处理）

1. `include/cpu/cache/i_cache_prefetch.h:7` 的历史注释仍以旧名 `io().axi_ready` 描述 2026-10-28 的缺陷形态
   ⇒ 读者可能误以为是**当前**端口 ⇒ 建议改为「已退役的旧简化口」措辞。
2. `i_cache_prefetch.h:119` 的 `<<=` 对齐列与相邻行不齐（纯排版）。
3. **既有漂移**：`tests/chlib/test_cache_complete.cpp` 的 `ICacheSync g1 strict` 注释写「2 different sets /
   set 16 / tag=0x80」，实测 `0x100000` 与 `0x200000` 的 index 均 = **set 0**（tag `0x200` / `0x400` 不同
   ⇒ 走 way0/way1）。行为正确（用例绿），仅注释失真；**非本批引入**，不在本批修。

---

## 6. 已用轮次

**1 / 3**（`RDDF_VERIFIER_MAX_LOOPS=3`）。无打回 ⇒ 未消耗重试额度。
