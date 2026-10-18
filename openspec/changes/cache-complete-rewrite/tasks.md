# tasks: cache-complete-rewrite

按 `cache-complete-rewrite-session-handoff.md §3` 的 6 步 TDD 拆分，每步独立 commit + ctest 通过。**Step 1+2 必须同一 commit**（R3 hard-gate，handoff §3.2）。

## 0. Preparation (commit 0: read-only audit + worktree + openspec 包)

- [ ] 0.1. worktree 已在 `../cache-rewrite-wt` 分支 `feat/cache-complete-rewrite` 创建（handoff §2.5）
- [ ] 0.2. openspec 包骨架 `openspec/changes/cache-complete-rewrite/{proposal.md, design.md, tasks.md}` 已创建
- [ ] 0.3. 主 spec 路径 `openspec/specs/cpu-cache-subsystem/spec.md` 已创建（首次出现）
- [ ] 0.4. `git log -1 --format="%ai %an %h %s"` 验证起点是 `bfb3960`
- [ ] 0.5. `git status` 验证工作区干净（除 `.rddf/` untracked）

## 1. Step 1+2 combined (commit 1: i_cache_complete.h + d_cache_complete.h 重写)

> **R3 hard-gate (handoff §3.2)**: Step 1 + Step 2 必须同一 commit。架构级 SRAM 改 ch_mem 与 10 个 typo/port-conv/`&&`/`||` 强耦合，Half-Step 会导致"修了一半但仍编译不过"。

### 1.1. `include/cpu/cache/i_cache_complete.h` 重写

**SRAM 改 ch_mem**（架构级 fix，handoff §3.1）：

- [ ] `ch_reg<ch_uint<128>> data_sram[128][2]` → `ch_mem<ch_uint<128>, 128> data_mems[2]`
- [ ] `ch_reg<ch_uint<21>> tag_sram[128][2]` → `ch_mem<ch_uint<21>, 128> tag_mems[2]`
- [ ] `ch_reg<ch_bool> valid_sram[128][2]` → `ch_mem<ch_bool, 128> valid_mems[2]`
- [ ] `ch_reg<ch_bool> lru_sram[128]` → `ch_mem<ch_bool, 128> lru_mem`
- [ ] 删除死代码 L101-110 的 for-loop SRAM 初始化（ch_mem 有 init constructor）
- [ ] 读访问改 aread：`tag_sram[index][0]` → `tag_mems[0].aread(index)`
- [ ] 写访问改 write：`tag_sram[index][victim_way] = ch_reg<...>(tag)` → `tag_mems[victim_way].write(index, tag, fill_enable)`
- [ ] `victim_way` 类型：`ch_bool`（1-bit lru_mem.aread）→ 改为 `ch_uint<1>` + 0/1 字面量
- [ ] LRU 命中更新：`if (hit_way1) lru_mem.write(index, ch_bool(true), ch_bool(true)); else if (hit_way0) lru_mem.write(index, ch_bool(false), ch_bool(true));`

**修复 11 个 typo/port-conv/`&&`/`||` bug**：

- [ ] L115-118: 去掉 `ch_uint<32>(io().addr)` 冗余 wrap（port 自带 `operator ch_uint<W>()`，参考 `clint.h:64`）
- [ ] L131-133: `valid_way0 && (tag_way0 == tag)` / `valid_way1 && (tag_way1 == tag)` / `hit_way0 || hit_way1` → `&`/`|`（bit-identical DAG）
- [ ] L155: `hit || io().axi_ready` → `select(hit, hit, io().axi_ready)`（避免 mixed ch_bool+port `||`）
- [ ] L163: `miss && io().req` → `select(miss, miss, ch_bool(false)) & io().req`
- [ ] L189: `ch_uint<DATA_WIDTH>(io().axi_data)` → `zext<DATA_WIDTH>(io().axi_data)`（`io_logical.h:168` 提供）
- [ ] L200: `0xFFFFFFF0_d` → `0xFFFFFFF0_h`

### 1.2. `include/cpu/cache/d_cache_complete.h` 重写（镜像）

- [ ] 同样 4 个 SRAM 改 ch_mem
- [ ] 同样 10 个 bug fix（位置行号不同，按 d_cache 文件实际行号）
- [ ] L162: `hit && io().we == ch_bool(true)` → `select(hit, hit & io().we, ch_bool(false))` 或先 `ch_bool(io().we)`
- [ ] L190: `(miss && is_read) || io().we` → 拆分：先 `ch_bool(miss & is_read)` 再 `| io().we`
- [ ] L215: `0xFFFFFFF0_d` → `0xFFFFFFF0_h`
- [ ] Write-Through 路径：保持（hit+we 同时写 cache_data + axi_wdata）

### 1.3. 验收（commit 1 末）

- [ ] `cmake --build build -j$(nproc)` → 0 error
- [ ] `cmake --build build -j$(nproc) 2>&1 | grep -E "warning|error"` → 0 line on changed files
- [ ] `ctest -R test_cache_simple --output-on-failure` → PASS（existing stub test 不回归）
- [ ] `ctest -R test_cache_pipeline --output-on-failure` → PASS（existing 5 test_case）
- [ ] `ctest -R test_phase4_chlib --output-on-failure` → PASS（existing 7 test_case）
- [ ] `git commit -m "feat(cache): rewrite *_complete.h via ch_mem (R2 spike)"`

## 2. Step 3 (commit 2: tests/chlib/test_cache_complete.cpp 新增)

**4 个 TEST_CASE 必须覆盖**（handoff §3.3，覆盖 ADR-039 §2.2 R2 缺口 G1/G2）：

### 2.1. `icache miss-fill-then-hit` `[cache][complete][icache][miss-fill]`

- [ ] 创建 `tests/chlib/test_cache_complete.cpp`（~250 行）
- [ ] TEST_CASE 用 `ch::ch_device<ICacheComplete>` + `ch::core::Simulator sim(ic.context());`
- [ ] 周期 1：触发 miss fill（addr=0x1000, req=1, axi_data=0xDEADBEEF, axi_resp_valid=1, axi_ready=1）
- [ ] 周期 2：关 AXI 应答（axi_resp_valid=0），同地址再读 → 应 hit
- [ ] 断言：`sim.get_port_value(ic.instance().io().miss) == 0u`
- [ ] **注意**：0xDEADBEEF 是 32-bit，DATA_WIDTH=128 → axi_data 零扩展成 [127:0]，cache_data(63_d, 32_d) = axi_data[63:32] = 0
- [ ] 测试期间若实际行为与预期不符（零扩展 / 写穿时机 / lru update 时序），**先停下实测再调断言**，不要硬塞 PASS

### 2.2. `dcache read miss-fill` `[cache][complete][dcache][miss-fill]`

- [ ] 同上策略，write-through 路径下读 miss-fill
- [ ] `ch::ch_device<DCacheComplete>` + 写使能=0 验证读 miss-fill
- [ ] 额外加一个 `dcache write hit-through` test 验证 write-through 路径

### 2.3. `2-way multi-port aread 隔离性` `[cache][complete][isolation][g1]`

**覆盖 R2 缺口 G1**（2 个独立 aread 端口是否互相污染）：

- [ ] 实例化 2-way cache
- [ ] 同一 tick 内 aread way0 一个 set，aread way1 另一个 set
- [ ] 验证两条 aread 路径返回不同 set 的数据（互不干扰）
- [ ] 用 R2 spike 已建立的 `ch_mem::aread` 0-cycle 契约（spec 已开）

### 2.4. `同周期 hit-aread + miss-fill-write` `[cache][complete][concurrency][g2]`

**覆盖 R2 缺口 G2**（hit 通路 + fill write 通路并发）：

- [ ] 周期 1：读 set X (way0) hit + 读 set Y (way1) miss 同时发生
- [ ] 同时 fill write 向 set Y (way1) 写数据
- [ ] 周期 2：验证 set X hit 数据稳定（没被 fill 干扰）+ set Y (way1) 现在 hit
- [ ] **覆盖 spec R4 已知 race**（已闭环：`aread` 返回 pre-write 写前值）

### 2.5. 注册新 test

- [ ] `tests/CMakeLists.txt` 加 1 行 `add_catch_test(test_cache_complete chlib/test_cache_complete.cpp)` （参考同目录已有 test_cache_pipeline 等的注册模式）
- [ ] `cmake --build build -j$(nproc)` → 0 error
- [ ] `ctest -N | tail -1` 验证 CTEST_TOTAL = 160（159 + 1）
- [ ] `ctest -R test_cache_complete --output-on-failure` → 4/4 PASS
- [ ] **若任一 test_case fail**：按 handoff §4 fallback "回退到 ADR-039 §6.1.R5 F 路径（YAGNI git rm）"，不要死磕
- [ ] 若 test_case 写不出来（行为不可断言）：改 `SKIP()` 保留 test 编号，不删 test（research.md §A3.2.4 同样 fallback）
- [ ] `git commit -m "test(cache): add test_cache_complete covering R2 G1/G2 gaps"`

## 3. Step 4 (commit 3: riscv-mini smoke target)

**最小化**：仅实例化 `ICachePrefetch` + `DCacheWriteBuffer`，不跑实质行为：

- [ ] 创建 `examples/riscv-mini/tests/test_cache_smoke.cpp`（~50 行）
- [ ] 2 个 `TEST_CASE`：`ICachePrefetch instantiation` `[riscv-mini][cache][smoke]` + `DCacheWriteBuffer instantiation`
- [ ] 用 `ch::ch_device<chlib::ICachePrefetch>` + `REQUIRE(true);`
- [ ] `examples/riscv-mini/CMakeLists.txt` 加 smoke target `add_catch_test(riscv_mini_cache_smoke ...)`
- [ ] `cmake --build build --target riscv_mini_cache_smoke` → 0 error
- [ ] `ctest -R riscv_mini_cache_smoke --output-on-failure` → PASS
- [ ] **wrapper 自身 bug 不阻塞 cache 重写成功**：若 wrapper 编译失败，单独 follow-up，本 commit 可只 cover cache header rewrite + test_cache_complete
- [ ] `git commit -m "test(riscv-mini): smoke target for cache wrappers"`

## 4. Step 5 (commit 4: 文档同步 3 处)

- [ ] root `AGENTS.md §KNOWN LIMITATIONS`：删 `*_cache_complete.h` is dead-on-arrival bullet；保留 stub bullet（仍 `[[deprecated]]`）
- [ ] `docs/adr/ADR-037-cache-stub-removal.md §5 Status`：增补 "rewrite 已完成 2026-XX-XX by ADR-039 §6 6-step TDD；stub 删除可恢复执行（deadline 保持 2027-04-17）"
- [ ] `include/cpu/AGENTS.md STRUCTURE 节`：加一行 "`*_complete.h` 是 ch_mem-based 4KB L1 2-way set-associative，实例化用法见 `examples/riscv-mini/src/i_cache_prefetch.h`"
- [ ] `git commit -m "docs: sync cache-complete-rewrite status across AGENTS.md / ADR-037 / cpu AGENTS"`

## 5. Step 6 (commit 5: archive + ADR-037 §2 重启)

- [ ] `openspec validate cache-complete-rewrite --strict` → PASS
- [ ] `openspec validate --specs --strict` → 14/14 PASS（13 + 1 新 cpu-cache-subsystem）
- [ ] `openspec archive cache-complete-rewrite`（无 `--skip-specs`，走默认 spec 合并）
- [ ] 验证 `openspec/specs/cpu-cache-subsystem/spec.md` 已合并 `## ADDED Requirements`
- [ ] `.rddf/improvements/cache-stub-physical-removal.md` status 从"Superseded by docs-only phase"改回"Active"
- [ ] `docs/adr/ADR-037-cache-stub-removal.md §5 Status` 改为 "Executed 2026-XX-XX (stub deletion 重启)"
- [ ] `git commit -m "chore(openspec): archive cache-complete-rewrite + restart ADR-037 §2"`

## 6. Regression after each commit

- [ ] 6.1. `cmake --build build -j$(nproc)` 0 errors
- [ ] 6.2. `ctest -LE perf --output-on-failure -j$(nproc)` 100% green（基线 + 增量）
- [ ] 6.3. `ctest -L perf` 100% green（per root AGENTS.md "perf 11/11 全绿"基线）
- [ ] 6.4. `./run_all_ported_tests.sh` 28/28 PASS
- [ ] 6.5. `ctest -N | tail -1` CTEST_TOTAL 增量符合预期

## 8. Builder 阶段撞墙 fallback

如发现 builder 阶段撞了新架构问题（比如 `ch_mem` 在 ch_out/port 边界类型不匹配）：

1. **先停下**，不要继续堆代码 —— R3 hard-gate 不允许"半修 broken build"
2. 回读 `cache-complete-doa-note.md §6.1` 5 条 hard-gate，确认没违反
3. 如果问题超出 ADR-039 scope，**直接转 R5 F 路径**（git rm + ADR-040 supersede）—— 这是为"cache 不是 RV32I runtime 阻塞"这个事实设计的 fallback
4. F 路径的入口：cache-complete-doa-note.md §6.1.R5 有完整 git rm 清单 + 风险评估