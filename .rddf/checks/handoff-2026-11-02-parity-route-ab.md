# 交接快照：objective-test-coverage-100pct（路线 A/B 推进）

- **时间**：2026-11-02（builder）
- **objective**：`.rddf/roadmap/objectives/objective-test-coverage-100pct.md`
- **plan**：`.rddf/plans/dual-backend-full-coverage-2026-11.md`
- **用途**：下一次会话**直接从这里继续**，不需要重新盘点。

---

## 1. 当前计数（实测，全部已对齐 AGENTS.md 锚点）

| 量 | 值 | 命令 |
|---|---|---|
| **parity 源文件数** | **48**（目标 60，差 **12**） | `bash scripts/verify_dual_backend_parity.sh` |
| parity macro 调用数（PAIRS） | **241** | 同上 |
| `CTEST_TOTAL` | **225** | `cd build && ctest -N \| tail -1` |
| `VERILATOR_E2E_TOTAL` | **57** | `ctest -N -L verilator_e2e \| tail -1` |
| non-perf | **214** | `ctest -N -LE perf \| tail -1` |
| `tests/test_*.cpp` | **128** | `ls tests/test_*.cpp \| wc -l` |
| `tests/chlib/test_*.cpp` | **41** | `ls tests/chlib/test_*.cpp \| wc -l` |
| 全量测试 | **214/214 绿**（**全量重建后**） | `cmake --build build -j$(nproc) && cd build && ctest -LE perf -j4` |

⚠️ 工作区里有一批**并发会话**的文件（`hierarchical-rtl-output`：`docs/adr/ADR-058..061`、
`docs/usage_guide/05-component-design.md`、`HIERARCHICAL-RTL-AUTHORING-GUIDE.md` 等）
⇒ ⇒ **提交时 MUST 按路径显式 `git add`，MUST NOT 用 `git add -A`**。

## 2. 本会话已完成（11 个 commit，全部未 push）

| commit | 内容 |
|---|---|
| `0cd957a` | 路线 B 第 6 批：`test_pipeline_dual.cpp`（3 pair / 42 断言） |
| `e208c98` | 登记 `sread` 禁读拍分歧（提案） |
| `4ac91ab` | 归档第 6 批 |
| `1fec002` | 台账 44 → 45 |
| `587fcbb` | **修复 `sread` 禁读拍**（Simulator 清零，对齐 Verilog） |
| `718a29c` | 归档 sread change（`mem-read-timing` **+1 Requirement**） |
| `85e674d` | 台账 45 → 47 |
| `a40fa14` | 订正 `test_mem_timing` 的 HOLD 断言 → 清零 |
| `5c31771` | AGENTS.md COMMANDS 段加「改库源码后 MUST 全量重建」护栏 |
| `888fabd` | 路线 A **D 批第 1 个**：`test_mem_connection_dual.cpp`（4 pair / 40 断言） |
| `2fe6b69` | 归档 D 批第 1 个 |

parity 轨迹：**37 → 48**（本会话 +11）。

## 3. 下一步（明确的第一个动作）

### D 批第 2 个：`tests/test_mem_comb_read.cpp`（8 TEST_CASE，**6 可转 / 2 不可转**）

| 行 | 用例 | 可转？ |
|---|---|---|
| 203 | `aread observable same-tick in Simulator (regression for codegen ...)` | 待核实 |
| 247 | `sread retains 1-cycle latency (regression for codegen ...)` | ⚠️ **先核实判别力**（见陷阱 T2） |
| 289 | `aread with register-driven address reflects the new address ...` | 待核实 |
| 323 | `sread with register-driven address retains 1-cycle latency ...` | ⚠️ 同上 |
| 368 | `aread Verilog codegen emits assign (no always_ff)` | ❌ **codegen 文本断言**，双后端无增量 |
| 439 | `generated Verilog references no undeclared signals` | ❌ 同上 |
| 474 | `aread cross-cycle write-then-read consistency` | 可转 |
| 531 | `mem_comb_read same_cycle write_plus_aread returns post_write value` | 可转 |

⚠️ **第一步必须是**：读 203/247/289/323 的断言**实现**，判断它们是不是靠「tick 后读」
来断言时序 —— 若是，按陷阱 **T2**，它们**没有**判别力，转换后要换成
「**tick 之间观测**」的写法（或如实登记该维度不可判别）。

### 之后

| 批 | 文件 | 备注 |
|---|---|---|
| D（剩 1） | `test_mem_timing.cpp`（7 可转） | ⚠️ 其中 `sread holds its value...` **已被本会话订正为 clears** |
| E（3） | `test_bundle_connection`(20) · `test_bundle_master_slave`(9) · `chlib/test_cache_complete`(14) | cache 可能触发 ADR-041 的 Async golden-value 分类 |
| F（6） | `chlib/test_onehot` · `test_operation_results` · `test_rv32i_pipeline` · `test_module_advanced` · `chlib/test_forwarding` · `chlib/test_hazard_complete` | 多 DUT；CI 墙钟风险最大 ⇒ **先实测再决定拆分** |
| B（剩 1–3） | 路线 B 剩余新文件 | |

---

## 4. 陷阱清单（**本会话新增 3 条**，务必先读）

### 🔴 T1（新增，最重要）：`ctest` **不会**自动重新构建

改了 `src/**` / `include/**` 后，**只重建被点名的 target 就跑 ctest ⇒ 假绿**。
**本会话真实踩过**：改 `instr_mem.cpp` 后只重建 2 个 target ⇒ 「213/213 全绿」是假的
⇒ 全量重建后 `test_mem_timing` 立刻转红。
⇒ ⇒ **跑 ctest 前 MUST `cmake --build build -j$(nproc)`**（已写入 AGENTS.md COMMANDS 段）。

### 🔴 T2（新增）：靠「`tick()` 后读」**无法**区分 0-cycle 与 1-cycle

变异实测：把 `always_ff` 里的 `<=` 改成 `=`（阻塞赋值）**恒绿** ⇒ 在「设置输入 →
`tick()` → 读」的模型下，1-cycle 与 0-cycle **语义等价**（阻塞赋值同样只在沿上生效；
且 TB 从不在 tick 之间观测）。
⇒ ⇒ **要钉时序必须在 `tick()` 之间观测**（`set_input_value` 之后、`tick()` 之前读）。
⚠️ 但注意：框架在 `set_input_value` 后**不会重新求值组合逻辑** ⇒ 两后端都读旧值
⇒ 该写法对**两后端一致性**也不是普适的判别手段 —— 需逐案确认。
⇒ ⇒ 结论：mem 类时序用例的转换要**先判「这条断言在两后端下是否真的可判别」**，
不可判别的维度**如实登记**，不要伪造判别力。

### 🔴 T3（新增）：`DUAL_BACKEND_TEST_PAIR` 的 body 参数里**花括号内的逗号**会被当参数分隔

用例表 MUST 放**文件作用域**（只有圆括号才保护）。本会话踩过一次（9 参数报错）。

### 既有陷阱（仍有效）

- **子组件 MUST 用 `ch::ch_module<T>`**：裸 `PipelineStage stage(this, "x")` 的
  `describe()` **不会被框架调用** ⇒ `ch_reg` 从未创建 ⇒ **SIGSEGV**（本会话实测）。
- **端口连线 MUST 用 `<<=`**（`operator=` 是 deleted）。
- **`ch_in<ch_bool>` 不能绑 `const ch_bool&`** ⇒ 用 `port<ch_uint<1>> == ch_uint<1>(1_d)`。
- **变异 MUST 先确认仍可编译**（否则跑旧二进制 = 假绿）；**还原用 `cp`**（刷新 mtime）。
- **无效变异要如实登记**（本会话登记了 2 条：`<=`→`=`、以及 pipeline 的 rst 只在 reg=0 时测）。
- **N2′ 判据**：断言数(with verilator) == **2 ×** 断言数(interpreter-only)。
- **AGENTS.md 的 hunk 级隔离**：并发会话也在改它 ⇒ 用
  `git show HEAD:AGENTS.md` → 应用自己的替换 → `git hash-object -w` →
  `git update-index --cacheinfo 100644,<blob>,AGENTS.md` ⇒ 工作区另行重放同样替换
  （否则护栏读工作区会红）。

## 5. 关键文件索引

| 路径 | 说明 |
|---|---|
| `tests/dual_backend_fixture.h` | `DUAL_BACKEND_TEST_PAIR` 宏 + R1..R5 不变量 + R6 排除清单 |
| `tests/test_mem_connection_dual.cpp` | 本会话新增（4 连接形态，可作模板） |
| `tests/test_mem_sread_disable_dual.cpp` | 本会话新增（禁读拍清零） |
| `tests/chlib/test_memory_dual.cpp` | 本会话新增（RAM 族写拍 = 0） |
| `tests/chlib/test_pipeline_dual.cpp` | 本会话新增（pipeline 控制语义） |
| `openspec/specs/mem-read-timing/spec.md` | **新增** Requirement「sread 的禁读拍 MUST 清零输出」 |
| `src/ast/instr_mem.cpp` | `instr_mem_sync_read_port::eval` 的禁读拍 = 清零 |
| `src/codegen_verilog.cpp` | `print_mem_read_port`（**dangling-else** ⇒ 禁读拍清零；注释已订正） |
| `.rddf/roadmap/objectives/objective-test-coverage-100pct.md` | §2 判据 + 实测台账（现值 47 ⇒ **本次未跟到 48**，下一步记得跟） |
| `.rddf/plans/dual-backend-full-coverage-2026-11.md` | 批次清单 + §5 墙钟账本 |
| `.github/workflows/ci.yml` | P2 三杠杆 + 分片覆盖护栏 |

⚠️ **遗留**：objective 台账的「现状」还写着 **47**，D 批第 1 个落地后应为 **48** ⇒
下次继续时顺手更新（只改实测数字，判据 owner = planner）。
