# Design: cleanup-250198a-aread-combinational-debt

## Context

HEAD commit `250198a` 让 `mem.aread()` 端口在 Verilog codegen 时发出组合逻辑 `assign`，以解锁 ChipForge dmem 0-cycle 读延迟。5-agent 评审给出 FAIL verdict，并由 Oracle 独立验证 + Metis 意图分析。本 design 文档说明修复实施的技术细节。

### 已知事实（Oracle 验证）

| 事实 | 证据 |
|------|------|
| `ch_mem::aread()` 永远 `enable=nullptr` | `include/core/mem.h:145-147` — `create_mem_read_port(..., nullptr, addr, nullptr /*enable*/, ...)` |
| 唯一调用方是 `mem.h`，async_read 结构性不可达 | `context.cpp:257` — `port_type = cd ? sync_read : async_read`；`create_mem_read_port` 仅 `mem.h` 调用 |
| Simulator 把所有 read ports 放入 `sequential_instr_list_` | `src/simulator_init.cpp:116-121` |
| aread 与 sread 在 Simulator 中**完全相同行为** | Oracle 验证：`instr_mem_async_read_port::eval` 无 clk 检查，但 `eval_sequential()` 只在 clk==1 时调用；与 sync port 行为一致 |
| "0-cycle" 观察来自 tick 末尾 `eval_combinational()` | `simulator.cpp:328` — tick() 在末尾再调用一次 `eval_combinational()`，把 proxy 传播到 output port |
| `mem.sread(addr, "sync_port")` 第二参数绑定 enable 而非 name | const char* → bool → ch_bool 隐式转换；name 仍是默认 `"mem_sread"` |
| FIFO 示例无 `rd_data_reg` 下游 register | `samples/fifo_example.cpp:75-76`、`stream_fifo_example.cpp:60-61`：`io().dout = data_out;` 直连 aread 代理 |
| CTest 按可执行文件名匹配，非 Catch2 TEST_CASE 名 | `tests/CMakeLists.txt:15` — `add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})` |

## Goals / Non-Goals

**Goals:**
- 消除 `codegen_verilog.cpp` 中的死代码 + 错误的 simulator 注释
- 修正 `tests/test_mem_comb_read.cpp` 中的命名 typo、overpromise、hardcoded `/tmp`、magic window scan；保留 sread fixture 隐式转换行为（已文档化）
- 同步文档记录 aread vs sread 时序语义差异 + 已知 simulator-vs-Verilog drift
- 新增 `specs/mem-read-timing/spec.md` capability，使 `openspec validate` 通过
- 不影响任何用户可见的 API、Verilog codegen 输出行为、测试通过率

**Non-Goals:**
- 不改变 `ch_mem::aread()` 公开 API
- 不修 VerilatorBackend（`ADR-035 R7` 已 descoped）
- 不为 aread 增加 enable 参数
- 不做全库 ZERO-DEBT 大扫除
- 不增加 golden Verilog 测试（独立 change 范围）

## Decisions

### Decision A: Task 1 死代码删除采用直接删除策略

**方案**:
- 删除 `print_mem_read_port` 的 `if (enable && node_names_.count(enable)) { ... } else { ... }` 二分支结构
- 只保留无 enable 的 `assign proxy = mem[addr];` 单发射路径

**理由**:
- Oracle 已结构性验证：async_read + enable 不可达（`mem.h` 是唯一调用方，aread 永远 `nullptr` enable）
- 删除零回归风险
- 不扩展 API（扩展 API 是 scope creep；正确组合 hold 需要 deliberate latch 设计）

### Decision B: 注释重写策略

**方案**:
- 删除错误声明（"simulator classifies async_read as combinational"）
- 改写为反映真实机制——
  - aread 在 simulator 中位于 `sequential_instr_list_`（posedge 阶段求值）
  - aread 在 Verilog 中是连续 `assign`（0-cycle）
  - 同 cycle write+read race + reg-driven address skew 是已知 simulator-vs-Verilog drift

### Decision C: TEST_CASE 重命名策略（含 test 4 矛盾修复）

**方案**:
- 4 个 `TEST_CASE` 按测试内容正确命名：
  - `isread 0-cycle latency in Simulator` → `aread 0-cycle latency in Simulator`
  - `isread does NOT register: synchronous latency 1-cycle preserved` → `sread retains 1-cycle latency (regression)` （测 sread 不是 aread）
  - `isread Verilog codegen emits assign (no always_ff)` → `aread Verilog codegen emits assign (no always_ff)`
  - `isread read with no write: stable init value` → **`aread cross-cycle write-then-read consistency`**（原命名 "no write" 与 Choice A 加入 write+read 矛盾，必须同步改）

**理由**:
- test 4 原命名 "no write" 在 Choice A 实施后不再准确
- 避免新 commit 内自相矛盾

### Decision D: fixture 隐式转换修复策略

**方案**:
- 把 `mem.sread(ch_uint<4>(io().addr_sync.impl()), "sync_port")` 改为 `mem.sread(ch_uint<4>(io().addr_sync.impl()), "mem_sread")`
- 让 fixture 与实际 proxy 名 `"mem_sread_data_proxy"` 命名一致

### Decision E: TEST_CASE 4 overpromise 处理

**方案**（选择 A）:
- 加入 write+read 跨 cycle 验证（`we=1` 写 mem[5]=0xFF，下一 cycle read aread at addr=5 验证看到 0xFF）

### Decision F: hardcoded `/tmp` 修复

**方案**:
- 使用 `std::filesystem::temp_directory_path()` + `getpid()` 构造唯一路径
- 测试结束处 `std::filesystem::remove()` 清理

### Decision G: magic window 简化

**方案**:
- 不再 600/1500 字节窗口 + ±100 回溯
- 改为行 split 后精确查找 `assign proxy_name =` 字符串
- 额外加否定断言：确认 sread 仍 emit `always_ff` 块（通过查找 `Read port <proxy>` 注释行）

### Decision H: 文档同步策略

**方案**:
- 修改既有 `docs/simulation/ARCHITECTURE.md`（已存在），新增 "Memory read port timing" 小节
- 在 root `AGENTS.md` CONVENTIONS 列表加一行 aread vs sread 说明（措辞调整：避免 "registered downstream" 强约束——两个现有 FIFO 示例无下游 register）
- 在 2 个 shipping FIFO 示例的 `toVerilog()` 调用附近加注释，**真实描述组合读 FIFO 行为**（`dout = mem[rd_ptr]` 直连，无 `rd_data_reg`）

### Decision I: Spec delta 策略

**方案**:
- 新增 capability `specs/mem-read-timing/spec.md`
- 含 4 个 ADDED Requirements（aread/sread Verilog 发射契约 + simulator 分类契约 + drift 文档契约）
- 每条 Requirement 用 SHALL/MUST 关键字 + RFC 2119 Scenario 格式
- 该 spec 与 `codegen_verilog.cpp` 修改对齐，使 `openspec validate` 通过

**理由**:
- OpenSpec spec-driven schema 要求至少一个 delta
- 本次 change 引入可追溯时序契约（aread 0-cycle vs sread 1-cycle），适合用 spec 固化
- 避免依赖 `--skip-specs` 豁免（AGENTS.md 禁止）

## Risks / Trade-offs

### 风险 R1: 删除 enable 分支可能影响未来扩展
**概率**: 极低
**缓解**: Oracle 验证 `create_mem_read_port` 仅有 `mem.h` 调用方；重构会触发所有 caller 更新

### 风险 R2: TEST_CASE 重命名影响 CI/脚本
**概率**: 低
**缓解**: 提交后 grep 验证；项目内 ctest 使用可执行文件名，不受影响

### 风险 R3: docs 修改措辞与未来漂移
**概率**: 中
**缓解**: 文档描述**机制**而非**具体输出**

### 风险 R4: FIFO 注释错误（被 Metis + Oracle 一致发现）
**概率**: 高（已发生）
**缓解**: Decision H 重写注释为真实结构描述，删除虚构 `rd_data_reg`

### 风险 R5: spec delta 内容过时
**概率**: 低
**缓解**: spec 描述**契约**（SHALL），实现变化时同步更新 spec

## Open Questions

无。

## Verification Strategy

### Task 1 验收
```bash
# 编译零错零警
cmake --build build -j$(nproc) 2>&1 | grep -Ei "error|warning"
# 期望: 空

# mem 测试 5/5（按可执行文件名匹配）
cd build && ctest -R "test_memory|test_mem$|test_mem_connection|test_mem_timing|test_mem_comb_read" --output-on-failure
# 期望: 5/5 pass
```

### Task 2 验收
```bash
# 新测试按 Catch2 filter 直接调用
./build/tests/test_mem_comb_read "[comb-read]"
# 期望: 4 个 TEST_CASE 全部运行

# 无遗留 "isread" typo
grep -rn "isread" tests/
# 期望: 空

# 文件末尾有 newline
tail -c 1 tests/test_mem_comb_read.cpp | od -c | head -1
# 期望: \n
```

### Task 3 验收
```bash
# ARCHITECTURE.md 包含新小节
grep "Memory read port timing" docs/simulation/ARCHITECTURE.md
# 期望: 匹配

# AGENTS.md 包含 aread/sread 时序描述
grep -E "aread.*combinational|sread.*always_ff" AGENTS.md
# 期望: 匹配

# FIFO 注释真实描述组合读 FIFO（不虚构 rd_data_reg）
grep -E "combinational.*aread|dout.*mem\[rd_ptr\]" samples/fifo_example.cpp examples/spinalhdl-ported/fifo/stream_fifo_example.cpp
# 期望: 匹配
```

### Task 4 验收
```bash
# OpenSpec validate
openspec validate cleanup-250198a-aread-combinational-debt
# 期望: "Change 'cleanup-...' valid"

# Spec delta 含 SHALL
grep "SHALL" openspec/changes/cleanup-250198a-aread-combinational-debt/specs/mem-read-timing/spec.md
# 期望: 至少 1 行
```

## Rollback Plan

每 Task 独立可回滚：
- Task 1 commit: revert 即可恢复 dead enable 分支 + 错误注释
- Task 2 commit: revert 即可恢复原 typo 命名
- Task 3 commit: revert 即可恢复原文档
- Task 4 commit: revert 即可删除 spec delta（但 archive 后会合并到 openspec/specs/）

整体回滚 tag: `post-250198a-cleanup`。