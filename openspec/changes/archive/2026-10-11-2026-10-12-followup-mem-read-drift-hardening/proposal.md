# 2026-10-12-followup-mem-read-drift-hardening

> **STATUS**: 📋 **PROPOSAL** — 首次 `openspec validate` 已通过；等待执行 + 二次 validate + archive
> **SCOPE**: 钉死 Issue #27 中**真正未完成**的 2 项跟踪任务 + 1 项 docs parity，转化为代码级回归防护
> **REVISED SCOPE NOTE**: 执行期间发现 cleanup `2026-10-11-cleanup-250198a` 已添加 golden Verilog 测试（`tests/test_mem_comb_read.cpp:216-277` 的 `"aread Verilog codegen emits assign (no always_ff)"` tagged `[mem][comb-read][codegen]`），P0 原方案"新增 golden test"调整为**校验现有测试满足 R5 golden 子条款 + 加可选 substring alias**——见下方 What Changes §1.1 与 SPEC R5.first 灵活性表述。
> **RELATED**: Issue #27、cleanup-250198a-aread-combinational-debt（已 archive）、`openspec/specs/mem-read-timing/spec.md`（既有 capability，将追加 R5）

## Why

Issue #27 跟踪 `250198a` 引入的 simulator↔Verilog drift 的**代码级缓解**。`cleanup-250198a-aread-combinational-debt` change 已完成文档化（ARCHITECTURE.md / spec R1-R4）、3 个 FIFO 内联 note、`test_mem_comb_read.cpp` 4 个 TEST_CASE（含 codegen pin + 跨周期 write+read）。执行前审计发现：

- **cleanup #3 已 pin `assign` 发射**（proxy-name 行内匹配）——spec R5.first 直接引用为契约
- **Same-cycle write+read race 仍空**——spec R4 缺执行层 fence
- **`branch_predictor_v2.h` 5 处零文档**——与 3 个 FIFO sample 不一致

修复后使"refactor for cleanliness"不会无意识撤回 `250198a` 的合约。

### 风险评估

| Mitigation | 不做的后果 |
|------------|----------|
| 缺 Same-cycle race 测试 | Simulator `instr_mem_async_read_port::eval` 若被改成 tick 入口读组合值 → dmem 静默错位 1 周期 |
| 缺 `branch_predictor_v2.h` 注释 | 该 BHT/BTB 5 个站点的 drift-1 用户无就地指引，仍需翻 ARCHITECTURE.md |
| 现有 cleanup #3 golden test 命名未来变化 | proxy-name 锚定可能因 codegen 重命名失效——可选用 substring alias 加固（低优先级） |

## What Changes

### 1. 测试新增 (`tests/test_mem_comb_read.cpp`)

新增 1 个测试（强制）+ 可选 1 个 alias（视需要）：

- **TASK 1 — `[mem][aread][race]`** — Same-cycle write+read race fence（**强制**）
  - 同一 tick 内：`we=1, wdata=Y, waddr=X, aread_addr=X`，`tick()` 后断言 `rdata_async == mem_initial_at_X`（写前值）
  - 失败信号：Simulator `instr_mem_async_read_port::eval` 被改为在 tick 入口读组合值
  - 与既有的 `[comb-read][mem][sim][write-read]` 区别：后者是**跨周期**（write at N, aread at N+1），本测试是**同周期**（write+aread 同 tick）

- **TASK 1.5 — `[mem][aread][golden]` alias（可选）** — Substring-level codegen pin
  - 与 cleanup #3 `[comb-read][codegen][mem]` 互为正交 fence
  - 不依赖 proxy name 而是抓**通用 `assign ... = mem[`** 形态
  - **去留判断**：写完后若发现 cleanup #3 已能 complete 覆盖 R5.first 子条款，**取消本任务**避免重复

### 2. 文档同步 (`examples/riscv-mini/src/branch_predictor_v2.h`)

- **TASK 2** — 在 5 处 `.aread(...)` 站点（L88 `bht.aread(bht_index)`、L103 `btb_tag.aread(btb_index)`、L104 `btb_valid.aread(btb_index)`、L108 `btb_target.aread(btb_index)`、L115 `bht.aread(bht_index)`）旁加单行内联 note，措辞与 3 个 FIFO sample 完全一致

### 3. Spec 变更（追加到既有 capability）

在 `openspec/specs/mem-read-timing/spec.md` 追加两个 ADDED Requirement：

- **R5.first（柔性契约）** — 既有 cleanup #3 `[mem][comb-read][codegen]` `"aread Verilog codegen emits assign (no always_ff)"` 已 pin `aread` combinational `assign` 发射。本 spec **不重新定义 fence**，而是把现有 fence 升级为 spec 级别契约；如未来 cleanup #3 失败或被改名，**必须由另一条带 `[mem][aread][golden]` 或 `[mem][comb-read][codegen]` tag 的 TEST_CASE 接力**
- **R5.second（硬性契约）** — 至少 1 条带 `[mem][aread][race]` tag 的 TEST_CASE 必须存在，断言 same-cycle write+aread 返回 pre-write 值。这是本 change 的真正新增 fence

### 4. Issue #27 body 更新

- **TASK 3** — 编辑 GitHub issue body checkbox 状态：
  - M1（fifo_bundle comment）勾选 ✅（bf11ddd 已做）
  - M2（Golden Verilog regression test）勾选 ✅（cleanup #3 已覆盖）
  - M3（Same-cycle write+read race regression test）勾选 ✅（本 change 提供）
  - M4（branch_predictor_v2.h documentation）勾选 ✅（本 change 提供）
  - 删除冗余 M5（原 M4.b changelog entry）— 与项目 convention 冲突
  - 在末尾加一行引用本 change 的 archive commit

## Impact

### 1. 风险评估

| Task | 风险 | 理由 |
|------|------|------|
| TASK 1 (Same-cycle Race 测试) | **极低** | 仅测试文件新增，零代码路径改动 |
| TASK 2 (BHT/BTB 注释) | **零** | 仅 5 行 inline 注释，零行为变化 |
| TASK 3 (Issue body) | **零** | GitHub issue 元数据，不影响代码 |
| Spec R5 追加 | **零** | 仅追加 capability，R1-R4 不动 |

### 2. 兼容性
- ✅ 公开 API (`ch_mem::aread()`) 不变
- ✅ 现有 4 个 `test_mem_comb_read.cpp` 测试不变（仅追加 1 个新 TEST_CASE）
- ✅ 全部 142 ctest 测试不退化
- ⚠️ 新 TEST_CASE 用 `[mem]` tag → `ctest -L mem` 默认基线从 9 增至 10

### 3. 信号接口
无信号接口变化。

## Non-Goals（明确不做）

为避免 scope inflation，**不**处理：

1. **全库零技术债大扫除**（cleanup 自身已完成范围）
2. **给 `ch_mem::aread()` 增加可选 `enable` 参数**（API 扩展；正确的组合 hold 需要 deliberate latch 设计，超出本 change scope）
3. **VerilatorBackend mem port 支持**（`ADR-035 R7` 已 descope 到 sibling repo ChipForge）
4. **完整的 ROOT `CHANGELOG.md` 文件创建**（项目根无 `CHANGELOG.md`——commit message + spec + ARCHITECTURE.md 三件套已覆盖；issue body 的 M4.b 请求被判定为 redundant，**取消**而非实施）
5. **`samples/fifo_bundle.cpp` 之外的 sample 内联 note**（M1 已完成 3 个 FIFO）
6. **新增 golden Verilog 测试给其他 sample**（如 `samples/counter.cpp` 的 `ch_reg` 输出）——每类 codegen path 单独 fence，超出本 change scope
7. **强制覆盖或重写 cleanup #3**——#3 是本 change 的 fence 引用源，对其本身零修改

## Acceptance Criteria

完成 TASK 1+2+3 + spec R5 追加后：

```bash
# 1. OpenSpec 格式合规
openspec validate 2026-10-12-followup-mem-read-drift-hardening --strict
# 期望: "Change '2026-10-12-...' valid" (exit 0)

# 2. 编译零错零警
cmake --build build -j$(nproc) 2>&1 | grep -Ei "error|warning"
# 期望: 空

# 3. 新增 TEST_CASE 全绿（直接调二进制）
./build/tests/test_mem_comb_read "[mem][aread][race]"
# 期望: 1/1 pass

# 4. 既有的 cleanup #3 仍绿（确认没 break）
./build/tests/test_mem_comb_read "[mem][comb-read][codegen]"
# 期望: 1/1 pass

# 5. 既有 base 测试不退化
ctest -L base --output-on-failure --timeout 300 2>&1 | tail -5
# 期望: 142 tests passed（与 cleanup 后基线一致）

# 6. Spec R5 已注入（含柔性 first + 硬性 second 两个 Requirement）
grep -c "Requirement:" openspec/specs/mem-read-timing/spec.md
# 期望: 6（既 4 + 新 2；既有 R4 在末尾，所以是 6 个 Heading "Requirement:"）

grep -E "aread.*golden|aread.*race" openspec/specs/mem-read-timing/spec.md | head -3
# 期望: R5.first / R5.second 内容命中

# 7. branch_predictor_v2.h 5 处内联 note 全部存在
grep -c "aread codegen note" examples/riscv-mini/src/branch_predictor_v2.h
# 期望: 5

# 8. Race 测试的反向验证（对抗测试：若 simulator race 行为漂移，新测试必须 fail）
# 临时反向: 把测试的 REQUIRE 改成 "post-write"（即 0xEE）期望 fail
sed -i 's/0xC3/0xEE/' tests/test_mem_comb_read.cpp  # 制造"假定 Simulator 已切 post-write"的反向用例
cmake --build build -j$(nproc)
./build/tests/test_mem_comb_read "[mem][aread][race]"
# 期望: fail (说明 fence 不 vacuous)
git checkout tests/test_mem_comb_read.cpp
cmake --build build -j$(nproc)

# 9. Issue #27 body 更新
gh issue view 27 --repo chisuhua/CppHDL --json body | jq -r '.body' | grep -E "\- \[" | head -10
# 期望: M1/M2/M3/M4 均为 "- [x]"; M5（原 M4.b）已删
```

## 工时估计

| Task | 估计 |
|------|------|
| TASK 1 (Same-cycle race 测试, RED+GREEN) | 25-40 min |
| TASK 1.5 (Optional substring golden alias) | 0-20 min（视需要） |
| TASK 2 (branch_predictor_v2.h 5 处注释) | 5-10 min |
| TASK 3 (Issue #27 body 编辑) | 5 min |
| Spec R5 追加（包含 validate 反修） | 10-15 min |
| 验证 + archive 流程 | 10-15 min |
| **合计** | **~1-1.5 h（TASK 1.5 取消时）或 ~1.5-2.5 h（TASK 1.5 实施时）** |

## Rollback Tag

提交后建议打 tag `post-mem-read-drift-hardening`（与 cleanup 的隐式锚点对齐）：

```bash
git tag -a post-mem-read-drift-hardening -m "Hardens Issue #27 drift tracking as code-level fences (race regression test + branch_predictor doc parity)"
```
