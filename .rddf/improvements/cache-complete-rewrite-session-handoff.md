---
name: cache-complete-rewrite-session-handoff
priority: P1
source: 用户 2026-10-18 session 末尾决议 "A1 启动 R2 spike + F 保留为 fallback + ADR-039 §2 按 R2 实测修订"
phase: phase-4
category: cache
type: handoff
created_at: 2026-10-19
---

# Improvement: cache complete rewrite — Session B 启动包

- **Date drafted**: 2026-10-19
- **Drafted by**: build-agent (general role)
- **Source**: 用户在 2026-10-18 session 末尾决议 "A1 启动 R2 spike + F 保留为 fallback + ADR-039 §2 按 R2 实测修订"，本文件是决议落地后的 Session B 启动资料
- **Status**: 📋 **Handoff document** — 无 openspec change；本文件不写 `## ADDED Requirements`，不触发 spec 流程。目的：把 2026-10-18 R2 spike 闭环 + ADR-039 §6 6 步 TDD 拆成可勾选清单，作为下个 session 启动时第一份读材料
- **Related**:
  - `docs/adr/ADR-039-cache-complete-rewrite-via-ch-mem.md` (commit `bfb3960`) — 本文件的父决策（§2.2 R2 闭环 + §6 Step 3 multi-port coverage）
  - `docs/adr/ADR-037-cache-stub-removal.md` — 父 ADR（6-month deadline 2027-04-17）
  - `.rddf/improvements/cache-complete-doa-note.md` — 本文件的起源（撞墙事件 + 6.1 Re-entry Protocol）
  - `.rddf/improvements/cache-stub-physical-removal.md` — sibling（backlog，等本 rewrite 落地后重启）
  - `tests/test_mem_comb_read.cpp` — R2 spike 的证据来源（5 个 test_case pin 死 aread/sread 时序）

---

## 1. 上下文快读（如果下个 session 只读本文件）

**目标**：把 `include/cpu/cache/{i,d}_cache_complete.h` 从 dead-on-arrival 改成可用 cache 实现，基于 `ch_mem` 原语重写 SRAM 部分。

**起点状态**（截至 2026-10-19 commit `bfb3960`）：
- `*_complete.h` 有 11 个 pre-existing build error（1 个架构级 SRAM C-style 数组 vs runtime index + 10 个 typo / port-conv / `&&`/`||` deprecated）
- stub `*_stub.h` 维持 `[[deprecated]]`，0 production 调用方
- `examples/riscv-mini/src/{i_cache_prefetch.h, d_cache_write_buffer.h}` wrapper 长期未编译

**终点状态**（builder session 完成后）：
- `*_complete.h` instantiate 可用，2-way set-associative, 4KB, LRU 1-bit
- 新 `tests/chlib/test_cache_complete.cpp` 4 个 test_case 全过
- riscv-mini smoke target 实例化 `ICachePrefetch` + `DCacheWriteBuffer` 可 build
- root AGENTS.md §KNOWN LIMITATIONS 删 dead-on-arrival bullet
- stub 维持 `[[deprecated]]`（直到 2027-04-17 触发 ADR-037 §2 重启）

**关键约束**（硬门槛，违反任一直接撞墙）：
- **R1** (cache-complete-doa-note.md §6.1.R1): architect 阶段必须做 cmake build probe 实测，不能仅 grep
- **R2** (ADR-039 §2.2, commit `bfb3960`): R2 已闭环。**cache hit 用 aread；imem 用 sread**（两者不同原语，各司其职）
- **R3** (cache-complete-doa-note.md §6.1.R3): Step 1 必须全量（10 个 bug fix + ch_mem SRAM 重写同时落地），**禁止 Half-Step / partial commit**
- **R4** (cache-complete-doa-note.md §6.1.R4): 2027-01-15 检查点 — 转 F 路径（YAGNI git rm）判定窗口
- **R5** (cache-complete-doa-note.md §6.1.R5): F 路径 fallback = ADR-040 supersede + `git rm` `*_complete.h` + wrapper。**当前未触发**（F 路径保留给 2027-01-15）

---

## 2. Session B 启动 Checklist（下个 session 第一件事）

### 2.1 读 3 份材料（5 min）

```bash
# 1. ADR-039（最新版本，含 §2.2 R2 闭环）
git show bfb3960:docs/adr/ADR-039-cache-complete-rewrite-via-ch-mem.md

# 2. 本文件（handoff）
cat .rddf/improvements/cache-complete-rewrite-session-handoff.md

# 3. cache-complete-doa-note.md（撞墙事件起源 + Re-entry Protocol）
cat .rddf/improvements/cache-complete-doa-note.md
```

### 2.2 验证 commit `bfb3960` 已落盘 + 工作区干净（30 s）

```bash
git log -1 --format="%ai %an %h %s"  # 期望: 2026-10-19 ... bfb3960 docs(adr): ADR-039 §2.2 ...
git status                            # 期望: clean (.rddf/ untracked 忽略)
```

### 2.3 确认 R3 hard-gate 仍生效（30 s）

```bash
# 缓存目录现状
ls -la include/cpu/cache/

# 期望: 仍是 4 个文件
#   i_cache_stub.h / d_cache_stub.h
#   i_cache_complete.h / d_cache_complete.h
# 不能变: 只有 ADR-039 / ADR-037 / 3 份 improvement 文档
```

### 2.4 决定 openspec change 包路径（5 min）

**选项**：
- **A. 走 openspec change 流程**：创建 `openspec/changes/cache-complete-rewrite/{proposal.md, design.md, tasks.md}` + `specs/cpu-cache-subsystem/spec.md`（如不存在，proposal 标注 "no spec changes"）。**推荐**（触及公共 API 重写 + 多模块修改，按 root AGENTS.md OPENSPEC §3）
- **B. 走 rdd-quick**：如果用户明确说不写 openspec，按 ADR-0047 走 in-place TDD 5-step（无 worktree）。**不推荐**（3-5 天工作量 + 6 文件改动 + 公共 API，scope 偏大）

**用户已确认路径 A**（见 2026-10-18 session 决议）。

### 2.5 决定 worktree vs main（2 min）

**推荐 worktree**（ADR-0049 + rdd-workflow 标准做法）：
```bash
git worktree add ../cache-rewrite-wt -b feat/cache-complete-rewrite
cd ../cache-rewrite-wt
```

**reason**: 3-5 天 / 5-8 commit，主分支保留 ready-to-deploy 状态；worktree 上 push 不影响 main。

---

## 3. ADR-039 §6 6 步 TDD — 拆成可勾选 checklist

每步独立可验证（commit + ctest 通过）；Step 1+2 必须同时提交（R3 硬门槛）。

### 3.1 Step 1: `i_cache_complete.h` 重写（半天）

**目标文件**：`include/cpu/cache/i_cache_complete.h`

**改法**（按 ADR-039 §2 决策表）：
- [ ] **SRAM 数组改 ch_mem**：
  - `ch_reg<ch_uint<128>> data_sram[128][2]` → `ch_mem<ch_uint<128>, 128> data_mems[2]`
  - `ch_reg<ch_uint<21>> tag_sram[128][2]` → `ch_mem<ch_uint<21>, 128> tag_mems[2]`
  - `ch_reg<ch_bool> valid_sram[128][2]` → `ch_mem<ch_bool, 128> valid_mems[2]`
  - `ch_reg<ch_bool> lru_sram[128]` → `ch_mem<ch_bool, 128> lru_mem`
- [ ] **删除** 死代码 L101-110 的 for-loop SRAM 初始化（ch_mem 不需要，因为有 init constructor）
- [ ] **读访问** 改用 aread：`tag_sram[index][0]` → `tag_mems[0].aread(index)`（data 同样）
- [ ] **写访问** 改用 write：`tag_sram[index][victim_way] = ch_reg<...>(tag)` → `tag_mems[victim_way].write(index, tag, fill_enable)`
- [ ] **修复 10 个 typo/port-conv bug**（cache-stub-physical-removal.research.md §3 详细列表）：
  - L200: `0xFFFFFFF0_d` → `0xFFFFFFF0_h`
  - L115-118: 去掉 `ch_uint<32>(io().addr)` 冗余 wrap（port 有 `operator ch_uint<W>()`，参考 `clint.h:64`）
  - L189: `ch_uint<DATA_WIDTH>(io().axi_data)` → `zext<DATA_WIDTH>(io().axi_data)`（`io_logical.h:168` 提供）
  - L155: `hit || io().axi_ready` → `select(hit, hit, io().axi_ready)`（避免 mixed ch_bool+port `||`）
  - L163: `miss && io().req` → `select(miss, miss, ch_bool(false)) & io().req` 或先 `ch_bool(io()._.impl())`
  - L131-133, L155, L163: 4 个 `&&`/`||` on ch_bool+port 全部重写（参考 ADR-036 迁移路径）
- [ ] **保留** `ICacheConfig` 数值（CAPACITY=4096, ASSOCIATIVITY=2, LINE_SIZE=16, DATA_WIDTH=128）
- [ ] **保留** `__io` 接口（外部 behavior 不变）

**验收**：
```bash
cmake --build build -j$(nproc)  # 0 error 0 warning
ctest -R test_cache_simple --output-on-failure  # 现有的 stub test 仍 PASS
```

### 3.2 Step 2: `d_cache_complete.h` 重写（半天，与 Step 1 同一 commit）

**目标文件**：`include/cpu/cache/d_cache_complete.h`

**改法**：镜像 Step 1，10 个 bug 列表 + ch_mem SRAM 重写 + write-through 路径保留

**⚠️ R3 hard-gate**：Step 1 + Step 2 **必须同一 commit**。架构级 fix 与非架构 bug fix 强耦合，Half-Step 会导致"修了一半但仍编译不过"。

**验收**：
```bash
cmake --build build -j$(nproc)
ctest -R test_cache_simple --output-on-failure
ctest -R test_cache_pipeline --output-on-failure  # 现有 5 个 test_case 仍 PASS
ctest -R test_phase4_chlib --output-on-failure    # 现有 7 个 test_case 仍 PASS
```

### 3.3 Step 3: 新增 `tests/chlib/test_cache_complete.cpp`（半天）

**目标文件**：`tests/chlib/test_cache_complete.cpp`（新文件，~250 行）+ `tests/CMakeLists.txt` 加 1 行 `add_catch_test()`

**4 个 TEST_CASE 必须覆盖**（覆盖 ADR-039 §2.2 R2 缺口 G1/G2）：

#### 3.3.1 `icache miss-fill-then-hit` `[cache][complete][icache][miss-fill]`
```cpp
ch::ch_device<ICacheComplete> ic;
ch::core::Simulator sim(ic.context());

// 周期 1: 触发 miss fill
sim.set_input_value(ic.instance().io().addr,           0x1000u);
sim.set_input_value(ic.instance().io().req,            1u);
sim.set_input_value(ic.instance().io().axi_data,       0xDEADBEEFu);
sim.set_input_value(ic.instance().io().axi_resp_valid, 1u);
sim.set_input_value(ic.instance().io().axi_ready,      1u);
sim.tick();

// 周期 2: 关 AXI 应答，同地址再读 → 应 hit
sim.set_input_value(ic.instance().io().axi_resp_valid, 0u);
sim.tick();
REQUIRE(sim.get_port_value(ic.instance().io().miss) == 0u);
// 注意: 0xDEADBEEF 是 32-bit，DATA_WIDTH=128 → axi_data 零扩展成 [127:0]
// cache_data(63_d, 32_d) = axi_data[63:32] = 0（高位零扩展）
// builder 阶段根据实测调整断言
```

#### 3.3.2 `dcache read miss-fill` `[cache][complete][dcache][miss-fill]`
同上策略，write-through 路径下读 miss-fill。

#### 3.3.3 `2-way multi-port aread 隔离性` `[cache][complete][isolation][g1]`
**覆盖 R2 缺口 G1**（2 个独立 aread 端口是否互相污染）：
```cpp
// 实例化 2-way cache, 同一 tick 内 aread way0 一个 set, aread way1 另一个 set
// 确认两条 aread 路径返回不同 set 的数据（互不干扰）
```

#### 3.3.4 `同周期 hit-aread + miss-fill-write` `[cache][complete][concurrency][g2]`
**覆盖 R2 缺口 G2**（hit 通路 + fill write 通路并发）：
```cpp
// 周期 1: 读 set X (way0) hit + 读 set Y (way1) miss 同时发生
// 同时 fill write 向 set Y (way1) 写数据
// 周期 2: 验证 set X hit 数据稳定（没被 fill 干扰）+ set Y (way1) 现在 hit
```

**验收**：
```bash
cmake --build build -j$(nproc)
ctest -R test_cache_complete --output-on-failure  # 4 个新 test_case 全过
ctest -N | tail -1  # 验证 CTEST_TOTAL: 159 + 1 = 160 (新增 test_cache_complete)
```

### 3.4 Step 4: riscv-mini 串接验证（半天）

**目标文件**：`examples/riscv-mini/CMakeLists.txt`（加 smoke target）

**改法**：最小化 — 仅实例化 `ICachePrefetch` + `DCacheWriteBuffer`，不跑实质行为（runtime 阻塞不在 cache）：
```cpp
// examples/riscv-mini/tests/test_cache_smoke.cpp （新文件，~50 行）
TEST_CASE("ICachePrefetch instantiation", "[riscv-mini][cache][smoke]") {
    ch::ch_device<chlib::ICachePrefetch> icp;
    REQUIRE(true);
}
// 同上 DCacheWriteBuffer
```

**验收**：
```bash
cmake --build build --target riscv_mini_cache_smoke  # 0 error 0 warning
ctest -R riscv_mini_cache_smoke --output-on-failure  # PASS
```

### 3.5 Step 5: 文档同步（30 min）

**改 3 个文件**：
- [ ] `root AGENTS.md §KNOWN LIMITATIONS`：删 `*_cache_complete.h` is dead-on-arrival bullet；保留 stub bullet（仍 `[[deprecated]]`）
- [ ] `docs/adr/ADR-037-cache-stub-removal.md §5 Status`：增补 "rewrite 已完成 2026-XX-XX by ADR-039 §6 6-step TDD；stub 删除可恢复执行（deadline 保持 2027-04-17）"
- [ ] `include/cpu/AGENTS.md STRUCTURE 节`：加一行"`*_complete.h` 是 ch_mem-based 4KB L1 2-way set-associative，实例化用法见 `examples/riscv-mini/src/i_cache_prefetch.h`"

### 3.6 Step 6: archive + ADR-037 §2 重启（30 min）

**改 2 个文件 + 1 个 openspec 操作**：
- [ ] 创建 openspec change 包（如果走 openspec）：`openspec/changes/cache-complete-rewrite/`（proposal / design / tasks）+ 跑 `openspec validate --strict`
- [ ] `openspec archive cache-complete-rewrite`（spec 合并到 `openspec/specs/cpu-cache-subsystem/`）
- [ ] 把 `.rddf/improvements/cache-stub-physical-removal.md` status 从"Superseded by docs-only phase"改回"Active"
- [ ] `docs/adr/ADR-037-cache-stub-removal.md §5 Status` 改为 "Executed 2026-XX-XX (stub deletion 重启)"

---

## 4. R2 缺口跟踪（G1/G2 → Step 3）

| R2 缺口 | 来源 | 覆盖位置 | 验收命令 |
|---|---|---|---|
| **G1: 2-way multi-port aread 隔离性** | `cache-complete-doa-note.md §6.1.R2`（arch 阶段漏测 multi-port 隔离）| §3.3.3 test_case | `ctest -R test_cache_complete --output-on-failure` |
| **G2: 同周期 hit-aread + miss-fill-write** | `cache-complete-doa-note.md §6.1.R2`（arch 阶段没测同周期并发）| §3.3.4 test_case | 同上 |

**如果任一 test_case fail**：回退到 ADR-039 §6.1.R5 F 路径（YAGNI git rm），不要死磕（risk=cache complete 不是 RV32I runtime 阻塞）。

---

## 5. 风险与 fallback

| 风险 | 等级 | 缓解 |
|---|---|---|
| Step 1+2 撞新 bug（除 11 个已知外）| 🟡 中 | builder 阶段遇到新 bug 先实测再修，不要纯纸面；遇到架构级问题立即停下回退到 R5 F 路径 |
| Step 3 4 个 test_case 写不出来（行为不可断言）| 🟡 中 | 改 `SKIP()` 保留 test 编号，不删 test（research.md §A3.2.4 同样的 fallback） |
| riscv-mini wrapper 自身 bug（长期未编译）| 🟡 中 | Step 4 独立可回退：wrapper bug 不阻塞 cache 重写成功，单独 follow-up |
| ADR-037 deadline 2027-04-17 紧逼 | 🟢 低 | 当前 2026-10-19，~6 个月缓冲；R4 检查点 2027-01-15 转 F 路径判定 |
| 用户切到 F 路径（"cache 不是 RV32I 关键路径，干脆删") | 🟢 低 | F 路径已在 §6.1.R5 完整设计，1 个 session 即可执行 |

---

## 6. 不在范围内（明确划线）

- ❌ 不实现 BYPASS mode（ADR-037 §2 variant (a) 已废，research.md §A4.1 4 个理由）
- ❌ 不实现 multi-level cache / coherency
- ❌ 不实现 BTB / branch predictor
- ❌ 不改 stub（stub 维持 `[[deprecated]]`，等 2027-04-17 触发 ADR-037 重启）
- ❌ 不实现 write-back / write buffer（`DCacheWriteBuffer` wrapper 已存在，本 change 仅保证它能 build，不动其内部逻辑）
- ❌ 不改 ADR-039 的"aread vs sread"决策（R2 已闭环于 commit `bfb3960`）

---

## 7. 时间预算

| 步骤 | 工作量 | 累计 |
|---|---|---|
| Session B 启动 (读 3 文件 + 验证 + worktree + openspec 包) | 1 小时 | 1 h |
| Step 1+2 合并 commit | 1 天 | 1 d 1 h |
| Step 3 test_case 编写 + 调试 | 半天 | 1 d 5 h |
| Step 4 riscv-mini 串接 | 半天 | 2 d 1 h |
| Step 5 文档同步 | 半小时 | 2 d 1.5 h |
| Step 6 archive + 重启 ADR-037 | 半小时 | 2 d 2 h |
| **总计** | **2-3 天** | (与 cache-complete-doa-note.md §5.2 估算"3-5 天"一致) |

---

## 8. 给下个 session 的人（或下次回到这个分支的我）的最后一句

如果你发现 builder 阶段撞了新架构问题（比如 `ch_mem` 在 ch_out/port 边界类型不匹配）：

1. **先停下**，不要继续堆代码 —— R3 hard-gate 不允许"半修 broken build"
2. 回读 `cache-complete-doa-note.md §6.1` 5 条 hard-gate，确认没违反
3. 如果问题超出 ADR-039 scope，**直接转 R5 F 路径**（git rm + ADR-040 supersede）—— 这是为"cache 不是 RV32I runtime 阻塞"这个事实设计的 fallback

**F 路径的入口**：cache-complete-doa-note.md §6.1.R5 有完整 git rm 清单 + 风险评估。