# cleanup-250198a-aread-combinational-debt

> **STATUS**: 📋 **PROPOSAL** — 等待 Metis + Oracle 审查通过后执行
> **TARGET COMMIT**: `250198a feat(mem): aread emits combinational \`assign\` instead of \`always_ff\``
> **SCOPE**: 钉死为 HEAD commit `250198a` 引入的债务清理；不涉及全库 ZERO-DEBT 大扫除

## Why

HEAD commit `250198a` 让 `mem.aread()` 端口在 Verilog codegen 时发出组合逻辑 `assign`（之前错误地发出 `always_ff`，导致 1-cycle 读延迟），解锁 ChipForge dmem 的 0-cycle 读延迟。

5-agent review 给出 **FAIL verdict**（3 FAIL / 2 PASS），并经 Oracle 深度验证 + Metis 意图分析，确认存在 **4 项 Blocking Issues** + 5-agent 遗漏的 **3 项重要发现**。本次清理实施这些修复，并新增 `mem-read-timing` 能力 spec 记录可追溯的时序契约。

### Blocking Issues（必修）

1. **死代码 + 潜在组合反馈环**：`src/codegen_verilog.cpp:1097-1105` 的 enable 分支是死代码（`ch_mem::aread()` API 永远传 `nullptr` 给 enable，见 `include/core/mem.h:145-147`）。若可达，`assign proxy = en ? mem[addr] : proxy;` 是组合自引用（Verilator UNOPTFLAT / 综合推断意外 latch）。
2. **注释自相矛盾**：`codegen_verilog.cpp:1071-1106` 注释声称 "simulator classifies async_read ports as combinational"，但 `simulator_init.cpp:116-121` 实际把所有 `type_mem_read_port` 放入 `sequential_instr_list_`。
3. **TEST_CASE 命名 typo + 自相矛盾**：4 个 `TEST_CASE` 全叫 `isread`（应为 `aread`），test 2 测 sread 却命名 `isread does NOT register`；test 4 原命名 "aread read with no write" 在加入 write+read 验证后也需要修改。
4. **文档未同步**：aread 从 1-cycle → 0-cycle 是用户可见语义变化；`AGENTS.md` / `docs/` 无对应记录。违反零技术债政策的"文档同步"门。

### 5-agent 遗漏的重要发现（Oracle 独立验证发现）

5. **3/4 新测试对本次 commit 是 vacuous 的**：simulator 行为未变，test 1/2/4 在 parent commit 上也会通过；**只有 test 3（codegen 字符串匹配）能真正 fail**。测试 1/2 的注释声称 "0-cycle" 和 "1-cycle"，但实际在 simulator 中 aread 和 sread **完全相同行为**（都在 sequential list + clk high 阶段）。
6. **Simulator↔Verilog 时序分歧比提交所述更广**：提交信息仅承认 "same-cycle write+read race"，但 **register-driven address 场景**（典型 FIFO raddr）也存在分歧——simulator 1-cycle vs Verilog 0-cycle。
7. **2 个 shipping FIFO 示例的 generated Verilog 被悄悄改变**：
   - `samples/fifo_example.cpp:197` 调用 `toVerilog()`
   - `examples/spinalhdl-ported/fifo/stream_fifo_example.cpp:168` 调用 `toVerilog()`
   - 二者 Verilog 输出从 `always_ff` → `assign`，**无 golden Verilog 检查**。
   - 正面：`chlib::ch_rom`（`include/chlib/memory.h:347`）现在变成正确的 comb ROM。
   - **重要澄清**：两个 FIFO 的 `io().dout = data_out;`（aread 直连 output 端口）**无任何下游 register**。它们本质是组合读 FIFO（first-word-fall-through 风格）。注释**不能**虚构 `rd_data_reg` 描述。

## What Changes

### 1. 代码修正
- `src/codegen_verilog.cpp` — 删除 dead enable 分支 + 重写注释为真实机制（Task 1）

### 2. 测试修正
- `tests/test_mem_comb_read.cpp` — 重命名 TEST_CASE、修正 fixture、修正 overpromise、改 `/tmp` 路径、简化 magic window、加 trailing newline（Task 2）

### 3. 文档同步
- `docs/simulation/ARCHITECTURE.md`（已存在） — 加 "Memory read port timing" 小节
- `AGENTS.md` — CONVENTIONS 加一行 aread vs sread 时序说明（措辞调整避免与现有 FIFO 示例矛盾）
- `samples/fifo_example.cpp` + `examples/spinalhdl-ported/fifo/stream_fifo_example.cpp` — 加注释说明 generated Verilog 变化（描述真实组合读 FIFO 结构）

### 4. Spec 变更（强制）
新增 capability `mem-read-timing`（`specs/mem-read-timing/spec.md`），含 4 个 ADDED Requirements：
- aread emits combinational assign in Verilog
- sread emits always_ff in Verilog
- Simulator classifies mem read ports as sequential
- known simulator-vs-Verilog drift is documented

## Impact

### 1. 风险评估

| Task | 风险 | 理由 |
|------|------|------|
| Task 1 (死代码删除) | **零** | `mem.h:145-147` + `context.cpp:257` 结构性保证 enable 分支不可达 |
| Task 2 (测试修正) | **低** | 仅测试文件改动，不影响代码路径 |
| Task 3 (文档同步) | **零** | 仅文档；FIFO 注释改为真实描述（无 `rd_data_reg`，是组合读 FIFO）|
| Task 4 (Spec delta) | **零** | 新增 capability，不修改既有 spec |

### 2. 信号接口
无信号接口变化——本次清理保持 `ch_mem::aread()` 公开 API 不变，仅修正内部死代码、注释、文档，并新增可追溯 spec。

### 3. 兼容性
- ✅ 既有用户不受影响（API 不变）
- ✅ 既有测试不受影响（test_mem, test_mem_connection, test_mem_timing 行为不变）
- ✅ Verilog codegen 输出行为不变（task 1 删除 dead code，行为等价）
- ⚠️ 测试名变更：4 个 `TEST_CASE` 重命名（影响 Catch2 `-R "<name>"` 调用方）

## Non-Goals（明确不做）

为避免 scope inflation，本次**不**处理:
1. 全库 ZERO-DEBT 大扫除
2. 既有 aread 用户的迁移脚本
3. VerilatorBackend mem port 支持（`ADR-035 R7` 已 descoped 到 sibling repo ChipForge）
4. 给 aread API 增加 enable 参数（扩展 API 是 scope creep；正确组合 hold 需要 deliberate latch 设计）
5. 修复其他历史债务（perf_tests `bv/utils.h:842 ch_check`、JIT 测试关闭等）
6. 增加 golden Verilog 回归测试（用注释 + spec 文档化 FIFO 行为变化；如需 golden test 走独立 change）

## Acceptance Criteria

完成 Task 1+2+3+4 后：

```bash
# 1. OpenSpec 格式合规
openspec validate cleanup-250198a-aread-combinational-debt
# 期望: "Change 'cleanup-...' valid"

# 2. 编译零错零警
cmake --build build -j$(nproc) 2>&1 | grep -Ei "error|warning"
# 期望: 空

# 3. mem 测试 5/5（按可执行文件名匹配）
cd build && ctest -R "test_memory|test_mem$|test_mem_connection|test_mem_timing|test_mem_comb_read" --output-on-failure
# 期望: 5/5 pass

# 4. 新测试按新名命中（直接调用二进制 + Catch2 filter）
./build/tests/test_mem_comb_read "[comb-read]"
# 期望: 4 个 TEST_CASE 全部运行并通过

# 5. 全量 ctest 基线
ctest -L base --output-on-failure --timeout 300 2>&1 | tail -10
# 期望: 118 tests passed（base 标签，无新失败）

# 6. 文档同步
grep "Memory read port timing" docs/simulation/ARCHITECTURE.md
grep -E "aread.*combinational|sread.*always_ff" AGENTS.md
grep "combinational.*aread\|dout.*mem\[rd_ptr\]" samples/fifo_example.cpp examples/spinalhdl-ported/fifo/stream_fifo_example.cpp
# 期望: 全部匹配

# 7. Spec delta 存在
cat openspec/changes/cleanup-250198a-aread-combinational-debt/specs/mem-read-timing/spec.md | grep "SHALL"
# 期望: 至少 1 行 SHALL
```

## 工时估计

| Task | 估计 |
|------|------|
| Task 1 (死代码 + 注释) | 30 min |
| Task 2 (测试修正) | 45 min |
| Task 3 (文档同步) | 1-2 h |
| Task 4 (Spec delta 已完成) | 0 |
| **合计** | **3-4 h** |

## Rollback Tag

提交清理后建议打 tag `post-250198a-cleanup`（与既有 `post-ast-split` 等 tag 风格一致）:

```bash
git tag -a post-250198a-cleanup -m "Cleanup of HEAD 250198a aread-combinational debt"
```