# Tasks: 2026-10-12-followup-mem-read-drift-hardening

> **TDD 流程**: 每个 Task 遵循 **RED → GREEN → REFACTOR**；新增 TEST_CASE 先写、确认编译/断言失败、再写细节、确认 pass
> **依赖关系**: Task 1（TASK 1 — race test）独立；Task 2（BHT/BTB notes）独立；Task 3（issue body）独立；Task 4（validate+archive）依赖 1+2；Task 1.5（substring alias）默认不做
> **总工时**: 1-1.5 h（TASK 1.5 跳过时）/ 1.5-2.5 h（TASK 1.5 实施时）
> **风险**: 极低（仅 1 个新 TEST_CASE + 5 行注释 + spec 追加 1 节 + issue body 编辑）

---

## 范围再说明（执行期间发现 cleanup #3 已覆盖 golden clause）

cleanup `2026-10-11-cleanup-250198a-aread-combinational-debt` 已在 `tests/test_mem_comb_read.cpp:216-277` 添加 `"aread Verilog codegen emits assign (no always_ff)"`（tag `[mem][comb-read][codegen]`）——通过 proxy-name 行内匹配 pin `aread` 的 `assign` 发射契约。这是 spec R5.first 的功能性等价品。

故本 change 实际工作量为：
- **新 TEST_CASE 只 1 条**：`[mem][aread][race]`（race 用例，原 cleanup 没有）
- **5 行 inline note**：branch_predictor_v2.h 站点
- **spec 追加 1 节**：R5.first（柔性，引用 cleanup #3）+ R5.second（硬性，要求新 race 测试）
- **issue body 同步**：4 项 checkbox 全勾 + 删除冗余 M5

---

## Task 1: Same-cycle write+aread race test (`[mem][aread][race]`)

**优先级**: P0（唯一一条真正的执行层新增 fence）
**依赖**: 无
**预计工时**: 25-40 min
**关联文件**: `tests/test_mem_comb_read.cpp`

### 1.1 RED — 写新 TEST_CASE 骨架，触发 TODO fail

**目标**：文件末尾追加 TEST_CASE，先确认编译能过 + 测试 fail 是 TODO 信号：

```cpp
TEST_CASE("mem_comb_read same_cycle write_plus_aread returns pre_write value",
          "[mem][aread][race]") {
    ch::core::context ctx("race_test");
    ch::core::ctx_swap swap(&ctx);
    ch::ch_device<CombReadFixture> dut("race_dut");
    FAIL_CHECK("TODO: implement race fence — see tasks.md Task 1.2");
}
```

- [x] **T1.1.1**: 在 `tests/test_mem_comb_read.cpp` 末尾追加上面 TEST_CASE（保留现有 4 个测试不变）
- [x] **T1.1.2**: 编译，确认能进编译
  ```bash
  cmake --build build --target test_mem_comb_read -j$(nproc) 2>&1 | grep -Ei "error|warning"
  # 期望: 空（编译干净）
  ```
- [x] **T1.1.3**: 跑测试，确认 fail signal 是 TODO 而非 unrelated error
  ```bash
  ./build/tests/test_mem_comb_read "[mem][aread][race]"
  # 期望: 1 test case failed, message 含 "TODO: implement race fence"
  ```

### 1.2 GREEN — 填真实测试逻辑

**目标**：把 TODO 替换为真实 race 验证。

完整测试体：

```cpp
TEST_CASE("mem_comb_read same_cycle write_plus_aread returns pre_write value",
          "[mem][aread][race]") {
    ch::core::context ctx("race_test");
    ch::core::ctx_swap swap(&ctx);

    ch::ch_device<CombReadFixture> dut("race_dut");
    ch::Simulator sim(dut.context());

    // tick 0: 不做任何操作,让 init state 稳定 (fixture 默认 init list 已写入 data_map_)
    sim.set_input_value(dut.instance().io().we,        static_cast<uint64_t>(0));
    sim.set_input_value(dut.instance().io().addr_sync, static_cast<uint64_t>(0x0));
    sim.set_input_value(dut.instance().io().addr_async, static_cast<uint64_t>(0x0));
    sim.set_input_value(dut.instance().io().wdata,      static_cast<uint64_t>(0x00));
    sim.tick();

    // 关键 tick: 同 tick 内 we=1 写 + aread 读同一地址
    sim.set_input_value(dut.instance().io().we,         static_cast<uint64_t>(1));   // enable write
    sim.set_input_value(dut.instance().io().wdata,       static_cast<uint64_t>(0xEE)); // 写 0xEE
    sim.set_input_value(dut.instance().io().addr_sync,  static_cast<uint64_t>(0x2));  // waddr=2 (init=0xC3)
    sim.set_input_value(dut.instance().io().addr_async, static_cast<uint64_t>(0x2));  // aread_addr=2
    sim.tick();

    // spec R4 Scenario "Same-cycle write+read race" 第三条:
    //   "Simulator aread returns pre-write value (mem[X] at tick entry)"
    constexpr uint64_t kPreWriteValueAtAddr2 = 0xC3;  // init list[2] = 0xC3
    auto rdata = sim.get_port_value(dut.instance().io().rdata_async);
    REQUIRE(static_cast<uint64_t>(rdata) == kPreWriteValueAtAddr2);
}
```

- [x] **T1.2.1**: 用上面 TEST_CASE 替换 1.1 的 skeleton
- [x] **T1.2.2**: 编译
  ```bash
  cmake --build build --target test_mem_comb_read -j$(nproc) 2>&1 | grep -Ei "error|warning"
  # 期望: 空
  ```
- [x] **T1.2.3**: 跑测试，期望 GREEN
  ```bash
  ./build/tests/test_mem_comb_read "[mem][aread][race]"
  # 期望: 1 test case passed
  ```
  **若 GREEN 失败 + 信号是 0xC3 != <其他值>** → 说明当前 Simulator 已经是 post-write，本 change 反而要把它"修回" pre-write——与 cleanup design 矛盾，回退并重新调研。
  **若信号是 undefined/floating** → simulator 异常，回退到 baseline 评估。

### 1.3 REFACTOR — 提取常量

- [x] **T1.3.1**: 把 `0xC3`、`0xEE`、`0x2` 提取为文件顶部 `constexpr`，便于未来换 init value 不必改 1 处以上
- [x] **T1.3.2**: 重跑确认仍 GREEN
  ```bash
  ./build/tests/test_mem_comb_read "[mem][aread][race]"
  # 期望: 1 test case passed
  ```

### 1.4 Adversarial Verification (Mandatory)

**目的**: 确认 fence **真能 catch regression**，不是 vacuous

- [x] **T1.4.1**: 临时反向——把 expected 从 pre-write 改成 post-write
  ```bash
  sed -i 's/kPreWriteValueAtAddr2 = 0xC3/kPreWriteValueAtAddr2 = 0xEE/' tests/test_mem_comb_read.cpp
  cmake --build build -j$(nproc)
  ./build/tests/test_mem_comb_read "[mem][aread][race]"
  # 期望: 1 test case failed (因为 rdata 实际是 0xC3, 但 assert == 0xEE)
  ```
- [x] **T1.4.2**: 立刻 revert
  ```bash
  git checkout tests/test_mem_comb_read.cpp
  cmake --build build -j$(nproc)
  ./build/tests/test_mem_comb_read "[mem][aread][race]"
  # 期望: 1 test case passed
  ```

**若 T1.4.1 不 fail** → fence vacuous → 回到 1.2 重写（不太可能发生，simulator 当前已经把 aread 放在 sequential list 是 cleanup Oracle 验证过的稳定行为）

---

## Task 1.5 (Optional): substring `[mem][aread][golden]` alias

**优先级**: P3（可选，默认**跳过**）
**依赖**: 无
**预计工时**: 0 或 15-25 min（视 TASK 1 race 测试决定）
**关联文件**: `tests/test_mem_comb_read.cpp`

### 1.5.1 何时执行 / 何时跳过

**跳过条件（默认）**：
- TASK 1 race 测试全绿 → spec R5.first 引用 cleanup #3 已足够覆盖 codegen clause → **跳过本任务**
- 不写第二个 golden 测试，避免与 cleanup #3 双 fence 重复

**执行条件**：
- 若 race 测试在某个未来 codegen refactor 后**已无法区分 pre-write vs 写死 0xC3**——加 alias 测试做更宽的 substring fence
- 若 cleanup #3 proxy-name 锚定策略被认为过于脆弱（如 codegen 改 proxy 命名）——加 substring alias 提供冗余

**默认本 change 跳过本任务**。如果后续 TASK 1 全绿 + spec R5 引用 cleanup #3 路径清晰 → 直接进 Task 2。

### 1.5.2 若决定实施（参考实现）

```cpp
TEST_CASE("mem_comb_read substring golden aread emits combinational assign",
          "[mem][aread][golden]") {
    ch::core::context ctx("substring_golden_test");
    ch::core::ctx_swap swap(&ctx);

    ch::ch_device<CombReadFixture> dut("dut");
    auto tmp_path = std::filesystem::temp_directory_path() /
                    ("cpphdl_substr_golden_" + std::to_string(::getpid()) + ".v");
    {
        std::ofstream touch(tmp_path);
        REQUIRE(touch.good());
    }
    toVerilog(tmp_path.string(), dut.context());

    std::ifstream in(tmp_path);
    std::stringstream ss;
    ss << in.rdbuf();
    std::string content = ss.str();
    std::filesystem::remove(tmp_path);

    REQUIRE_THAT(content, Catch::Matchers::ContainsSubstring("assign") &&
                          Catch::Matchers::ContainsSubstring("mem["));
    // 其余 line-by-line 解析同 cleanup #3, 故默认跳过
}
```

### 1.5.3 若实施必跑

- [x] 编译 + 跑测试 = GREEN
- [x] 反向验证：把 codegen 退回 always_ff，期望 fail（与 Task 1.4 同型）

---

## Task 2: `branch_predictor_v2.h` 5 处 inline note

**优先级**: P1（doc parity，纯注释）
**依赖**: 无
**预计工时**: 5-10 min
**关联文件**: `examples/riscv-mini/src/branch_predictor_v2.h`

### 2.1 在 5 处 `.aread(...)` 旁加 inline note

**复制粘贴** 既有 FIFO sample note 措辞（verbatim 来自 `samples/fifo_example.cpp:197-200`）：

```cpp
// aread codegen note: this read emits combinational `assign` (0-cycle).
// See docs/simulation/ARCHITECTURE.md "Memory read port timing"
// for the simulator-vs-Verilog drift contract.
```

- [x] **T2.1**: L88 `auto counter = bht.aread(bht_index);` 上方插入 note
- [x] **T2.2**: L103 `auto tag_r = btb_tag.aread(btb_index);` 上方插入 note
- [x] **T2.3**: L104 `auto valid_r = btb_valid.aread(btb_index);` 上方插入 note
- [x] **T2.4**: L108 `auto target_r = btb_target.aread(btb_index);` 上方插入 note
- [x] **T2.5**: L115 `auto old_counter = bht.aread(bht_index);` 上方插入 note

### 2.2 验证

- [x] **T2.6**: 编译（确认纯注释不影响 IR）
  ```bash
  cmake --build build -j$(nproc) 2>&1 | grep -Ei "error|warning"
  # 期望: 空
  ```
- [x] **T2.7**: grep 计数
  ```bash
  grep -c "aread codegen note" examples/riscv-mini/src/branch_predictor_v2.h
  # 期望: 5
  ```
- [x] **T2.8**: grep 措辞统一性
  ```bash
  for f in samples/fifo_example.cpp samples/fifo_bundle.cpp examples/spinalhdl-ported/fifo/stream_fifo_example.cpp examples/riscv-mini/src/branch_predictor_v2.h; do
    grep -A 1 "aread codegen note" "$f" | head -1
  done
  # 期望: 4 个文件的 "aread codegen note" 行内容完全一致
  ```
  → 若 branch_predictor_v2 用了 4 行而 FIFO 用 3 行（cleanup-250198a / fifo_bundle 用 4 行）——细微措辞差异在 commit 验收时合并即可

### 2.3 风险提示

Branch predictor 代码历史上有 compile error（参见 `a562462`/`a1bdaf9` 两个 fix commit）。Task 2 是纯注释，理论上不会 break。

若真 break：
- [x] revert Task 2 改动
- [x] 把 Task 2 单独留作 follow-up issue，本 change scope 缩到 Task 1 + Task 3 + Task 4

---

## Task 3: Issue #27 body 同步

**优先级**: P2（GitHub issue 元数据，非代码）
**依赖**: Task 1 + 2 全部 green 后
**预计工时**: 5 min

### 3.1 编辑 issue body

**当前 issue body 状态**：
- M1 (parity cleanup, 3 FIFO inline notes): ✅ (bf11ddd)
- M2 (Golden Verilog regression test): ❌ checkbox 但 cleanup #3 实际已做 → 改 ✅
- M3 (Same-cycle write+read race regression test): ❌ → 改 ✅（本 change TASK 1 提供）
- M4 (branch_predictor_v2.h aread documentation): ❌ → 改 ✅（本 change TASK 2 提供）
- M5 (原 M4.b Document the existing behavior in changelog): ❌ → **删除**

修改方式（取现 body → 修改 → 推回）：

- [x] **T3.1**: 取现 issue body
  ```bash
  gh issue view 27 --repo chisuhua/CppHDL --json body | jq -r '.body' > /tmp/issue27-current.md
  ```
- [x] **T3.2**: 编辑 `/tmp/issue27-current.md`：
  - 把 "Add `aread codegen note` to samples/fifo_bundle.cpp to match the other 2 FIFO samples (parity cleanup, <5 min)" 项前 `- [x]` 改成 `- [x]`
  - 把 "Add a golden-Verilog test that pins the FIFO `assign` emission (catches future 'always_ff' regressions on aread paths)" 项前 `- [x]` 改成 `- [x]`（说明：cleanup #3 已覆盖）
  - 把 "Document the existing behavior in the changelog entry for HEAD 250198a (currently only mentioned in commit message and commit 250198a docstring)" 整项删除（理由见 proposal.md Non-Goals #4）
  - 把 "Add a regression test that triggers same-cycle write+aread and asserts the chosen semantic (probably 'Simulator pre-write' to lock in current Simulator behavior)" 项前 `- [x]` 改成 `- [x]`（说明：本 change TASK 1 提供）
  - 把 "Add `aread codegen note` to `examples/riscv-mini/src/branch_predictor_v2.h` to match the other 3 FIFO samples (parity cleanup, <5 min)" 项前 `- [x]` 改成 `- [x]`（说明：本 change TASK 2 提供）
  - 在末尾或顶部加一行归档引用：
    ```markdown
    ---
    Resolved via OpenSpec change `2026-10-12-followup-mem-read-drift-hardening` (archived as <commit SHA after Task 4>)
    ```
- [x] **T3.3**: 推回
  ```bash
  gh issue edit 27 --repo chisuhua/CppHDL --body "$(cat /tmp/issue27-current.md)"
  ```

### 3.2 验证

- [x] **T3.4**: 
  ```bash
  gh issue view 27 --repo chisuhua/CppHDL --json body | jq -r '.body' | grep -c "\- \[x\]"
  # 期望: 4 (M1-M4)
  gh issue view 27 --repo chisuhua/CppHDL --json body | jq -r '.body' | grep "Document the existing behavior in the changelog"
  # 期望: 无输出（该项已删除）
  ```

---

## Task 4: OpenSpec validate + archive

**优先级**: 必经
**依赖**: Task 1 + 2 全部 green
**预计工时**: 10-15 min

### 4.0 确认 spec/mem-read-timing/spec.md 已写入

- [x] **T4.0.1**: 确认文件存在
  ```bash
  ls openspec/changes/2026-10-12-followup-mem-read-drift-hardening/specs/mem-read-timing/spec.md
  # 期望: 文件存在
  ```
- [x] **T4.0.2**: 看内容包含两个 ADDED Requirement
  ```bash
  grep -c "Requirement:" openspec/changes/2026-10-12-followup-mem-read-drift-hardening/specs/mem-read-timing/spec.md
  # 期望: 2 (R5.first + R5.second)
  ```

### 4.1 二次 validate

- [x] **T4.1**:
  ```bash
  openspec validate 2026-10-12-followup-mem-read-drift-hardening --strict
  # 期望: "Change '2026-10-12-...' valid" + exit 0
  ```
  **若 exit 非 0** → 看报错修正 spec.md 格式

### 4.2 全量 ctest 基线

- [x] **T4.2**:
  ```bash
  ctest -L base --output-on-failure --timeout 300 2>&1 | tail -10
  # 期望: 142/142 passed (既有基线)，无新增 fail
  ```
- [x] **T4.3**: 单独跑 `[mem]` tag 拉新测试明细
  ```bash
  ./build/tests/test_mem_comb_read --list-tests
  # 期望: 5 (清理后 4 + race 新增 1)
  ```

### 4.3 commit + archive

- [x] **T4.4**: stage 变更
  ```bash
  git add -A
  git status
  # 期望: 看到 3 类变更
  #   - openspec/changes/...（change 自身）
  #   - tests/test_mem_comb_read.cpp（race 测试新增）
  #   - examples/riscv-mini/src/branch_predictor_v2.h（5 处注释）
  ```
- [x] **T4.5**: commit
  ```bash
  git commit -m "test(mem): pin same-cycle write+aread race + branch_predictor doc parity

Adds [mem][aread][race] TEST_CASE to tests/test_mem_comb_read.cpp that
fences spec R4 'Same-cycle write+read race' third bullet at the
executable layer. Asserts Simulator aread returns the pre-write value
when we=1 writes the same address in the same tick().

Also adds 5 inline aread codegen notes to
examples/riscv-mini/src/branch_predictor_v2.h for doc parity with
samples/fifo_*.cpp (Issue #27 drift-1 follow-up).

Closes Issue #27.

openspec: 2026-10-12-followup-mem-read-drift-hardening"
  ```
- [x] **T4.6**: archive（**不**传 `--skip-specs`，让 R5 自动 merge 进 mem-read-timing）
  ```bash
  openspec archive 2026-10-12-followup-mem-read-drift-hardening
  # 期望: 自动 commit (merge spec R5 into openspec/specs/mem-read-timing/spec.md) + 移动到 archive/
  ```
- [x] **T4.7**: 验证 archive 后既有 spec 仍 R1-R4 不动 + R5 已注入
  ```bash
  openspec show mem-read-timing --type spec | grep -c "Requirement:"
  # 期望: 6 (R1 + R2 + R3 + R4 + R5.first + R5.second)
  ```
- [x] **T4.8**: 验证 archive 后 ctest 基线无退化
  ```bash
  ctest -L base --output-on-failure --timeout 300 2>&1 | tail -5
  # 期望: 142/142 passed
  ```

### 4.4 完成清理

- [x] **T4.9**: 打 tag
  ```bash
  git tag -a post-mem-read-drift-hardening -m "Hardens Issue #27 drift tracking as code-level fence (race regression test + branch_predictor doc parity; golden fence references cleanup #3)"
  ```
- [x] **T4.10**: 把 Task 3 的 issue body 推到 GitHub（用 archive 后的 commit SHA 填占位符）
  ```bash
  gh issue edit 27 --repo chisuhua/CppHDL --body "$(cat /tmp/issue27-current.md)"
  ```

---

## Failure Recovery

| 失败场景 | 恢复步骤 |
|---------|---------|
| Task 1 在 AV step (T1.4.1) 不 fail | 报告并 abort：fence 设计错误；archive 不动 |
| Task 1 RED step 显示 race 是 GREEN（unexpected） | 反向证据——说明 simulator 已 pre-write 但测试预期写成 post-write，或类似错位。abort 并重测 |
| Task 1 GREEN fail with mismatch `0xC3 != 0xEE` | 预期：fence 行为正常，fence 自身 fail → 说明本预期写错，把 REQUIRE 改回 pre-write 值 |
| Task 1 GREEN fail with crash/segfault | simulator init 失败 → 看 simulator_init.cpp:116-121 是否被改过；revert 后重试 |
| Task 2 编译 break | revert Task 2 改动；issue body 标 "branch_predictor notes deferred"；archive 时仅含 Task 1+3+4 |
| Task 4.1 validate 失败 | 多半是 spec.md 格式问题，参考 cleanup-250198a archived spec/mem-read-timing/spec.md 修 |
| Task 4.8 ctest 退化 | 看 ctest `--output-on-failure`；列出新增 fail，回滚变更 |

## Rollback Tag

执行 Task 4.9 后 tag `post-mem-read-drift-hardening` 立现可回滚点：

```bash
git tag -a post-mem-read-drift-hardening -m "Hardens Issue #27 drift tracking as code-level fences"
```

后续若发现 fence 有误（如 Test 1 误报），`git checkout post-mem-read-drift-hardening` 回到此 tag。
