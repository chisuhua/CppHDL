---
name: cache-complete-doa-note
priority: P1
source: 撞墙事件 —— 第一次尝试按 ADR-037 提前物理删 cache stub 时，cmake build probe 显示 *_complete.h 有 11 个 pre-existing compile error
phase: phase-4
category: cache
type: incident-note
created_at: 2026-10-18
---

# Improvement: cache complete rewrite via ch_mem (dead-on-arrival recovery)

- **Date drafted**: 2026-10-18
- **Drafted by**: build-agent (architect role)
- **Source**: 撞墙事件 —— 第一次尝试按 ADR-037 提前物理删 cache stub 时（见 `.rddf/improvements/cache-stub-physical-removal.md` 顶部 status 修订），cmake build probe 显示 `*_complete.h` 有 11 个 pre-existing compile error，**最严重是架构级**（SRAM 用 C-style 数组但用运行时 `ch_uint<7>` 索引），不可能在 ADR-037 scope 内修复。
- **Status**: Draft — backlog（无活跃 change；本文件只是立项登记）
- **Priority**: 中（cache 完整功能不是当前 RV32I 集成阻塞路径；riscv-mini 当前阶段跑 ch_device 顶层失败，cache 不是主因）
- **Related**:
  - `.rddf/improvements/cache-stub-physical-removal.md` — 父 change（docs-only 收尾，等本 rewrite 落地后再回来执行）
  - ADR-037 (`docs/adr/ADR-037-cache-stub-removal.md`) — 物理删除 stub 的 ADR（已 2026-10-18 增补"rewrite 是 hard prerequisite"）
  - Root AGENTS.md §KNOWN LIMITATIONS（已 2026-10-18 增补 `*_complete.h` 是 dead-on-arrival）
  - `include/core/mem.h` — `ch_mem<T, N>::aread/sread(addr)` —— 正确的 cache 内存原语

---

## 1. Why (背景与动机)

**撞墙事件 2026-10-18**：architect 第一次调研报告（`cache-stub-physical-removal.md`）建议"7 步删 stub + 改 3 test 用 complete"，builder 阶段 Step 1 跑 `cmake --build test_cache_simple` 编译失败。cmake build probe 揭示 `*_complete.h` 有 **11 个 pre-existing build error**，包括：

- 1 个 typo：`0xFFFFFFF0_d` literal 宽度 33 → `static_assert` fail
- 2 个 port→ch_uint 显式转换 mismatches（`ch_uint<32>(port)` 不直接走 `operator ch_uint<W>()` 模板）
- 1 个 `ch_uint<128>(ch_in<ch_uint<32>>)` 缺位宽转换
- 4 个 `&&`/`||` on ch_bool+port（2026-10-16 ADR-036 deprecated，但 stub 时代这两个 header 没被编译所以旧代码还在用）
- **1 个架构级设计错误（最严重）**：
  `i_cache_complete.h:84-96` 把 SRAM 声明为 C-style 数组 `ch_reg<ch_uint<DATA_WIDTH>> data_sram[128][2]`，但 `ch_reg::operator[]` 只接受 compile-time `unsigned`，而 `index` 是运行时 `ch_uint<7>`（从 `bits<...>(io().addr)` 推出来）。`d_cache_complete.h` 镜像同样问题。

**根因**：`ch_reg` 不是 C 数组 —— 它是 HDL 寄存器类型，operator[] 只为编译期索引重载。Cache 应当用 `ch_mem<T, N>`（`include/core/mem.h`）原语，它原生支持运行时 `ch_uint<addr_width>` 索引（`aread`/`sread` 方法）。

**为什么 stub 时代没暴露**：
- 3 个 test 文件 `pragma GCC diagnostic ignored "-Wdeprecated-declarations"` 包裹 stub，include `_stub.h`
- 没人 include `_complete.h`（grep 验证 0 production 调用方）
- `i_cache_prefetch.h` / `d_cache_write_buffer.h` 包装了 complete，但**这两个 wrapper 自己也没被任何 test 串进 CMake target**
- 整条 `*_complete.h` → wrapper → riscv-mini 链 = dead-on-arrival，已存在 6 个月（写于 2026-04-10）

**Why now**：
- 公开暴露"complete 是 dead-on-arrival"这件事 —— 当前 root AGENTS.md §KNOWN LIMITATIONS 的 cache stub bullet 误导性地说"完整实现在 `_complete.h`"，事实不是
- 给 `*_complete.h` 一个**真实可执行**的修复路径，而不是依赖人去手动写 11 个 build fix
- 让 ADR-037 物理删除 stub 的 6-month deadline（2027-04-17）有清晰的前置条件

**Why not just delete `*_complete.h` and keep the stubs forever**：
- 根 AGENTS.md §KNOWN LIMITATIONS 误导（"complete 可用"），后续 contributor 会浪费时间
- ADR-037 计划要删 stub，但 stub 删了 = 无任何 cache 实现可用（rv32i 集成更深依赖 stub 当 mock）
- 必须有"先 ch_mem rewrite 落地，再删 stub"的两阶段路径

---

## 2. What Changes (变更范围)

### 2.1 目标：基于 `ch_mem` 重写 `i_cache_complete.h` / `d_cache_complete.h`

| 现状 | 目标 |
|------|------|
| `ch_reg<ch_uint<DATA_WIDTH>> data_sram[128][2]` (C-style 数组) | `ch_mem<ch_uint<32>, 128> data_mem` 4KB SRAM (`num_entries=128` × `data_width=32`bit 32-bit word,或 `ch_uint<128>` 128-bit line) |
| `ch_reg<ch_uint<TAG_BITS>> tag_sram[128][2]` (C-style) | `ch_mem<ch_uint<TAG_BITS>, 128> tag_mem` (2-way: 2 个 mem,index 同 index,way 固定) |
| `ch_reg<ch_bool> valid_sram[128][2]` (C-style) | `ch_mem<ch_bool, 128> valid_mem` × 2 ways |
| `ch_reg<ch_bool> lru_sram[128]` (C-style) | `ch_mem<ch_bool, 128> lru_mem` |
| `auto index = bits<...>(io().addr);` (ch_uint<7>) | 同（保留 —— `ch_mem::aread` 接受 ch_uint<addr_width>）|
| `data_sram[index][0]` (C-style) | `data_mem.aread(index)` / `.sread(index, enable)` |

### 2.2 修剩余 10 个 pre-existing build error

| # | 文件:行 | 修法 |
|---|---------|------|
| 1 | `i_cache_complete.h:200` / `d_cache_complete.h:215` | `0xFFFFFFF0_d` → `0xFFFFFFF0_h` |
| 2 | `i_cache_complete.h:115-118` | `bits<...>(ch_uint<32>(io().addr))` → `bits<...>(io().addr)`（port 有 `operator ch_uint<W>()` 隐式转换, `bits<...>(port)` 是合法用法,见 `clint.h:64`）|
| 3 | `i_cache_complete.h:189` / `d_cache_complete.h:201` | `ch_uint<DATA_WIDTH>(io().axi_data)` → `zext<DATA_WIDTH>(io().axi_data)` (`io_logical.h:168` 提供) |
| 4 | `i_cache_complete.h:155` | `hit \|\| io().axi_ready` → `hit \| io().axi_ready`（port-port 有 `\|`,mixed ch_bool+port 需 `select(hit, hit, io().axi_ready)` 或先 `ch_bool(io().axi_ready.impl())`）|
| 5 | `i_cache_complete.h:163` | `miss && io().req` → 同上策略（`&` 或 `select`）|
| 6 | `i_cache_complete.h:155` (mixed hit ch_bool+port axi_ready) | 同 #4 |
| 7-11 | `d_cache_complete.h` 镜像 #2-6 | 同上 |

### 2.3 至少加 1 个真正 instantiate complete 的 test

- `tests/chlib/test_cache_complete.cpp`（新文件，~100 行）
  - `ch::ch_device<ICacheComplete> ic;` instantiate
  - 简单 1-cycle miss-fill test: `req=1, addr=X, axi_resp_valid=1, axi_data=Y` → next cycle 同一 `addr` 读应 `miss=0, data=cache_data_high32`
  - `ch::ch_device<DCacheComplete> dc;` 同上 (read miss-fill + write-through 基础)

### 2.4 riscv-mini 串接验证

- 在 `examples/riscv-mini/CMakeLists.txt` 加 1 个 test：实例化 `ICachePrefetch` + `DCacheWriteBuffer`（这两个 wrapper 已经存在，只是没被编译）
- 验证 wrapper 自己能 build（之前没测过 —— 链上 `_complete.h` → wrapper → top 全是 dead）

### 2.5 不在范围
- 不实现 BTB / branch predictor / 完整 LRU 行为（仅保证 Hit/Miss 状态机正确 + LRU bit update）
- 不做 multi-level cache（L1 only）
- 不做 cache coherency（M-cache 单核，无 coherency 需求）

### 2.6 公共 API 影响
**有变更**（fix bug）:
- `ICacheComplete::describe()` 行为在重写后保持语义一致（"Hit: cache_data; Miss: axi_data; ready: hit ? true : axi_ready"）—— **外部行为不变**
- 接口（`__io` 列表）不变
- 类名 / 头文件名不变

**有删除**:
- 无（保留 `*_complete.h` 作为唯一入口）

---

## 3. Acceptance (验收标准)

### 3.1 编译 + 测试
- [ ] `cmake --build build -j$(nproc)` 0 error 0 warning（含 `--Werror`-equivalent: `-Wall -Wextra -Wpedantic` 全过）
- [ ] `ctest -L base -j$(nproc)` 全绿（159 tests 保持）
- [ ] 新增 `tests/test_cache_complete.cpp` 至少 2 个 test_case（icache miss-fill-then-hit + dcache read miss-fill）通过
- [ ] riscv-mini smoke target（新增）能 build 且 `ctest -R riscv_mini_smoke` 通过
- [ ] `*_stub.h` 维持 `[[deprecated]]` 状态,**不**删除（ADR-037 6-month deadline 2027-04-17 触发本 rewrite 落地后再删）

### 3.2 代码清洁
- [ ] `*_complete.h` `describe()` 0 pre-existing build error（probe 验证：include 进一个空 test 文件能 compile）
- [ ] 11 个 bug 列表（§2.2 + §1 架构级）全部修复
- [ ] `grep -rn '&&\|||' include/cpu/cache/ include/cpu/pipeline/ examples/riscv-mini/src/i_cache_prefetch.h examples/riscv-mini/src/d_cache_write_buffer.h` —— cache 相关文件不出现 deprecated 运算符（chlib 整体不要求）

### 3.3 文档同步
- [ ] root AGENTS.md §KNOWN LIMITATIONS `*_complete.h` dead-on-arrival bullet 改写为"修复完成,2026-XX-XX 由 change cache-complete-rewrite 落地"
- [ ] ADR-037 §5 Status 增补"rewrite 已完成 2026-XX-XX, stub 删除可以恢复执行（deadline 保持 2027-04-17）"
- [ ] `include/cpu/AGENTS.md` STRUCTURE 节加"`*_complete.h` 是 ch_mem-based 4KB L1,实例化用法见 `examples/riscv-mini/src/i_cache_prefetch.h`"
- [ ] `.rddf/improvements/cache-stub-physical-removal.md` status 改回"Active",恢复执行

---

## 4. Capabilities (涉及的能力维度)

| Capability | 影响 | spec 文件 |
|------------|------|-----------|
| `cpu-cache-subsystem` | 完整重写 | `openspec/specs/cpu-cache-subsystem/spec.md`（如不存在,提案中声明 no spec changes）|
| `chlib-test-coverage` | 新增 1 test 文件,加 1 riscv-mini smoke target | `openspec/specs/chlib-test-coverage/`（如存在）|

---

## 5. Impact (影响面分析)

### 5.1 风险

| 风险 | 概率 | 影响 | 缓解 |
|------|------|------|------|
| ch_mem L1 行为与原 C-array 意图不同（fill 时序、LRU 状态机差异）| 中 | 中 | builder 阶段跑 test_cache_complete + 行为 diff（如果原 `_complete.h` 不可编译,行为基线用 stub 的 fill-then-read 对比）|
| riscv-mini 串接后暴露新 bug（wrapper 自身长期未编译）| 高 | 中 | 串接作为本 change 的 1 个独立 step,失败可回退到 "wrapper 仍 dead" 状态|
| ADR-037 deadline 2027-04-17 紧逼 | 低 | 低 | 当前是 2026-10-18,有 6 个月缓冲,本 change 估 1-2 周工作量 |
| ch_mem 是否有 LRU/Hit-Miss 友好 API | 需调研 | 中 | 走读 `include/core/mem.h` 全文（125+ 行）确认 `aread`/`sread` 行为;若不友好,降级到 `ch_reg` 数组 + `mux` for 循环展开 |

### 5.2 工作量估算
- §2.1 SRAM 重写: 1-2 天
- §2.2 10 个 bug fix: 0.5 天
- §2.3 新 test 文件: 0.5 天
- §2.4 riscv-mini 串接: 0.5-1 天
- 文档同步: 0.5 天
- 总计: **3-5 天**,1 个 builder change,~5-8 个 commit

### 5.3 利益
- `*_complete.h` 从 dead-on-arrival 变真正可用的 cache 实现
- ADR-037 物理删除 stub 6-month plan 重新可执行
- root AGENTS.md §KNOWN LIMITATIONS 少 1 个 bullet（dead-on-arrival 消失）
- riscv-mini 集成 path 不再依赖 stub 当 mock
- 11 个 pre-existing build error 一并消失（zero-debt 政策）

### 5.4 不在范围内
- 不实现 write-back / write buffer（`DCacheWriteBuffer` 那个 wrapper 已经存在,本 change 只确保它能 build,不动其内部逻辑）
- 不实现 multi-level cache / coherency
- 不改 stub（stub 维持 deprecated,等 deadline 2027-04-17 触发 ADR-037 重启）

---

## 6. 决策 checklist (待 review)

| # | 决策项 | architect 推荐 |
|---|--------|----------------|
| 1 | SRAM 选 `ch_mem` 还是 `ch_reg` 数组 + mux | **ch_mem**（原生支持 runtime index）|
| 2 | DATA_WIDTH 64 bit 还是 32 bit | 64（保留原 header 数值 128 bit line,但 cache 内部 word 32）|
| 3 | LRU 2-way 实现 | 1-bit LRU（保留原设计,不全 LRU）|
| 4 | test 文件名 | `tests/test_cache_complete.cpp`（`tests/` 根,与 `test_basic.cpp` 等并列,不是 `tests/chlib/`）|
| 5 | riscv-mini smoke target | 最小化：仅实例化 `ICachePrefetch` + `DCacheWriteBuffer`,不跑实质行为 |
| 6 | 走 openspec change 流程 | **走**（触及公共 API 重写 + 多模块修改）|
| 7 | 走 rdd-workflow 完整 rdd-arch → planner → builder → verifier | **跳过 rdd-arch**（已有 ADR-037 父决策 + 本 improvement 是 ADR-037 §2 路径变体,arch 决策已做完）|

---

## 6.1 Re-entry Protocol (2026-10-18 oracle 评估追加)

**本节是硬门槛（hard gate）**,任何后续 session 启动 ADR-039 §6 6 步 TDD 前**必须**满足以下 5 条。违反任一条直接撞墙(2026-10-18 architect/builder 已经验证)。

### R1. 禁止跳过 build probe
architect 第一次调研（见 `cache-stub-physical-removal.research.md`）只 grep 调用方就下结论"`*_complete.h` 可用",导致 builder 撞 11 个 pre-existing build error。**修正**:任何"清理 / 重写 / 替换"类 change, architect 阶段必须做 `cmake --build build` 实测目标 header 能 instantiate,**不能**仅凭 grep 调用方下结论。

### R2. 禁止跳过 runtime probe (Step 0 spike)
architect 第二次调研(2026-10-18 cmake build probe)只验证**编译可行性**,**没验证 simulator 运行时行为**。**未识别的关键风险**:根 AGENTS.md "Mem read port timing" 明确——`aread` 在 Verilog 是 0-cycle combinational, 在 Simulator 是 posedge 采样(1-cycle skew)。Cache hit 数据通路 = combinational read, 选 `aread` 还是 `sread` 是**未决设计问题**,ADR-039 §2 决策表"`data_mems[0].aread(index)`"未经实证。**修正**:任何 ch_mem 路径 change, 启动前必须先做 30-60 min Step 0 spike:
- 实例化 `ch_mem` 在 InterpreterBackend 和 VerilatorBackend 下分别验证 `aread` / `sread` 的 cycle 时序
- 据此裁决 cache hit 通路用哪个原语
- 结果回写进 ADR-039 §2(决策表修正)

**如果 Step 0 spike 跳过 R2,ADR-039 §6 6 步 TDD 仍可能在 InterpreterBackend 通过但 VerilatorBackend 跑挂 trace parity**(典型"编译过 ≠ 运行对")。

### R3. Step 1 必须全量执行(禁止 Half-Step)
oracle 评估 2026-10-18 指出:11 个 pre-existing error 中, **架构级那一个**(`ch_reg arr[128][2]` runtime 索引)是**编译阻塞点**——即使把其他 10 个非架构 bug 都修完, 文件**仍不可 instantiate**。**没有"半个 Step 1"**。**修正**:Step 1 必须全量(10 fixes + ch_mem SRAM 重写),禁止拆 Half-Step / partial commit。如果一次 session 时间不够, **直接停下,下次再来**,不要交付"改了一半、仍编译不过、且无测试"的代码(违反 ZERO-DEBT POLICY "no broken build / no orphan files")。

### R4. 2027-01 检查点(转 F 路径的判定窗口)
ADR-037 6-month deadline = 2027-04-17。**2027-01-15**(距 deadline 3 个月)设强制检查点,根据预算/状态选择路径:

| 状态 | 决策 |
|------|------|
| rewrite 已落地或接近完成 | 维持原计划,继续完成 §6 6 步 TDD |
| rewrite 已启动但远未完成 | 推迟 ADR-037 deadline 到下一周期(2027-10-17 之类) |
| rewrite 未启动且预算仍紧张 | **转 F 路径(见 §6.1.R5)**,避免 ADR-037 裸奔到 2027-04-17 |

**日历触发**:在 2027-01-01 启动的 session 第一件事是检查本文件状态 + git log,执行 R4 决策表。

### R5. F 路径(YAGNI 物理删除)作为 fallback
若 2027-01 检查点判定预算持续紧张,**F 路径是出口**:
- 立 ADR-040 supersede ADR-039: `git rm` `include/cpu/cache/{i,d}_cache_complete.h`
- 处理 wrapper 依赖: `examples/riscv-mini/src/{i_cache_prefetch.h, d_cache_write_buffer.h}` 也删除(它们 include complete, 自身不在任何 CMake target, 0 调用方)
- stub 维持 `[[deprecated]]`,存活到 2027-04-17(按 ADR-037 原计划)
- 真 cache 等 riscv-mini 集成**真正需要时**按 ch_mem 重写(pull-driven, YAGNI)

**F 论据**:
- `_complete.h` 教科书定义的 dead code, ZERO-DEBT "no dead code" **支持删除而非修复**
- 0 production callers(grep 验证)
- cache 不在 RV32I 关键路径(根 AGENTS.md "riscv-mini: Pipeline compile-time fixes complete, runtime requires ch_device wrapper"——cache 不是 runtime 阻塞)
- 损失 = hit/miss/LRU 参考实现(git history 可找回) + 一轮 ADR supersede 文书

**F 路径门槛**: R4 触发的 F 路径**仍是 architect 范围**(改 ADR 树 + 改 `include/cpu/` 是 build agent 角色矩阵的 build 操作,**但 F 路径是 0 代码量,只是 git rm + 改 docs,1 个 session 足够**)。

---

## 7. 后续链接

- `.rddf/improvements/cache-stub-physical-removal.md` — 父 change,docs-only 收尾后等本 rewrite 落地重启
- ADR-037 §5 Status 修订 — 增补"rewrite 是 hard prerequisite"
- root AGENTS.md §KNOWN LIMITATIONS 新增 bullet — 透明化 `*_complete.h` dead-on-arrival
- §6.1 Re-entry Protocol (本节) — oracle 2026-10-18 评估的硬门槛,避免第三次撞墙
