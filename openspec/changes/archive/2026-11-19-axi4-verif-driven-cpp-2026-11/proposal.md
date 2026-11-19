# Proposal: AXI4-Lite example 的 C++ 侧改由 `.verif` 驱动 + 文件改名 `tb_*`（`axi4-verif-driven-cpp`）

- **状态**: **立项（2026-11-18 · planner）** · 未开工 · **metis 第 1 轮已处置（P0×5 / P1×6 / P2×4）**
- **前置**: `rtl-verification-single-source-2026-11`（**已实现并提交** · `957864f` / `c1af967` / `5164188`）
- **后继（依赖本 change）**: `emit-cli-into-example-2026-11`（**硬依赖本 change 的改名**）
- **并行但碰同一文件**: `example-file-boundary-2026-11`（它也改 `tests/test_verif_single_source.cpp`）⇒ ⇒ 见「顺序与所有权」
- **上游决策**: `docs/adr/ADR-074-rtl-verification-single-source.md`（Accepted）· `docs/adr/ADR-075-port-naming-prerequisite.md`（Accepted）

---

## Why

`.verif` 已成为 **AXI4-Lite 示例的唯一期望载体**，但**该示例的 C++ 侧**仍是第二份期望载体，
且两者**内容分叉**：

| # | C++ 侧现状（逐条可核） | 位置 |
|---|---|---|
| ① | `Axi4TimingChecker` 的 **5 个** `check_*` **只 `std::cout` 打印** ⇒ 不断言 ⇒ 不影响退出码 | `examples/axi4/axi4_lite_example.cpp:71-108` |
| ② | 唯一的**真实断言** = 写 `0x12345678`@`0x00` 读回并比 `0x12345678` ⇒ `data_ok` ⇒ `return data_ok ? 0 : 1` | `:187-193` · `:218` |
| ③ | `toVerilog("axi4_lite_example.v", …)` ⇒ 在 **CWD** 落一个**游离** `.v` | `:203-205` |
| ④ | `VcdRecorder` ⇒ 落 `axi4_lite_waveform.vcd`，但 13 个信号里**多数是硬编码常量**（`:127` 全 `0`/`1`；`:141` 除 `awvalid`/`awready` 外皆常量）⇒ **不是仿真的忠实波形** | `:21-68` · `:127` · `:141` |
| ⑤ | 6 行 `set_input_value` 手写初值（含 `bready=1` / `rready=1` / `rst_n=1`） | `:118-124` |

⇒ ⇒ **期望值两处载体且语义重复**：`.verif` 的 `write_read_roundtrip` 已覆盖「写后读回」
（`REPEAT 4` 写 `0xA5000000 + _i`@`_i*4` ⇒ 读回同值，`tb_axi4_lite_example.verif:143-186`），
C++ 侧**又**手写一遍「写后读回」⇒ ⇒ 值与地址还不同 ⇒ ⇒ 违反 ADR-074 D1（单一真相源）。

⇒ ⇒ **该重复当前无机械抓手**：`scripts/verify_example_test_wiring.sh` 的 C3 只判
「退出码 0 且日志无 error/fatal」⇒ ⇒ 对 ① 那种「只打印」**结构性失明**。

⇒ ⇒ **文件名与内容不符**：该文件装的是 `main()` + `Simulator` + 激励 + 检查器
⇒ ⇒ 它**是测试台**；而同 stem 的 `tb_axi4_lite_example.{verif,sv}` 已经是测试台。

⇒ ⇒ **`.verif` 只在护栏的 Verilator 路径上被跑**（`verify_examples_rtl_sim.sh` ⑤）
⇒ ⇒ 「同一份 IR ⇒ 两个后端同结论」这条架构结论在**正常构建**里看不到。

## What Changes

| # | 变更 | 依据 |
|---|---|---|
| **C1** | **`main()` 改为「跑 `.verif`」**：调 `parse_verif()` + `build_rtl_manifest()` + `run_verif()` 执行 `examples/axi4/tb_axi4_lite_example.verif` | ADR-074 D1/D6 |
| **C2** | **fail-closed + 自洽退出码**：`.verif` 打不开 / `parse_verif` 失败 / `manifest.diagnostics` 非空 ⇒ **非零退出 + stderr**，且**不得**打印 `RESULT:` 行；成功路径 MUST 满足 `res.ok && res.checks == verif_check_count(prog) && res.fails == 0` 才 `return 0` | 「checks=0 fails=0 被当绿」正是要消灭的形态 |
| **C3** | **路径 MUST 与 CWD 无关**：`.verif` 路径 = `std::string(CPPHDL_SOURCE_DIR) + "/examples/axi4/tb_axi4_lite_example.verif"`，`CPPHDL_SOURCE_DIR` 由 **CMake 注入** | 三面 CWD 不一致（见下） |
| **C4** | **改名**：`axi4_lite_example.cpp` → `tb_axi4_lite_example.cpp`；**CMake target** `axi4_lite_example` → `tb_axi4_lite_example` ⇒ ⇒ **CTest 条目名 `spinalhdl_axi4_lite` 不变** | 用户 2026-11-18 诉求 |
| **C5** | **改名面同步**（**含上一次漏掉的两处**）：`examples/axi4/CMakeLists.txt` 的 `add_executable` / `target_*` / `install(TARGETS …)` / emitter 的 `target_link_libraries` 若引用；`tests/CMakeLists.txt:1455` 的 **`add_spinalhdl_ported_test` 第二参数**；`examples/CMakeLists.txt` 的 `CH_TRACKED_EXAMPLE_TARGETS`；tsv 的 `target` / `source` 两列；`include/axi4/AGENTS.md`；`examples/axi4/AGENTS.md`；`run_all_ported_tests.sh:100-105` 的注释（点名 `axi4_lite_waveform.vcd`）；`tests/test_axi_lite_verilator_e2e.cpp:73-75` 的「mirror `axi4_lite_example.cpp`」旧注释；`docs/CHANGELOG.md` | 仓内纪律 |
| **C6** | **期望值零改动**：`.verif` 逐字不变 ⇒ ⇒ 生成式 TB **逐字节不变**（⑥' 新鲜度仍绿） | ADR-074 D4 |
| **C7** | **护栏升级（判别力 ⇒ `verify_examples_rtl_sim.sh` 新增 ⑨）**：对 tsv 里 `generated=yes` 的行，**运行该行的 example 二进制**并断言其 `RESULT: checks=<min_checks> fails=0`（按列名取 tsv 字段；**找不到二进制 ⇒ fail-closed**）+ 两条 `--self-test` 投毒 ⇒ ⇒ 「1 条断言的 `.verif`」不再能过 | 护栏只判退出码 ⇒ 无判别力 |
| **C7b** | **已登记的遗留（本 change 不修）**：`verify_example_test_wiring.sh` 的 C3 **对本 example 空跑** —— 它按 *CTest 条目名* 找 `build/examples/**` 下同名可执行文件，而 `spinalhdl_axi4_lite` 不是二进制名；落到 `is_declared_example_target()` 时其正则（`_test$\|_demo$\|_example$\|...`）不匹配 ⇒ **静默 continue** ⇒ ⇒ 同样形态影响全部 26 个 `add_spinalhdl_ported_test` 条目 ⇒ ⇒ **独立立项**（本条只登记，不在本 change 修） | 实测（`verify_example_test_wiring.sh:310-345`） |
| **C8** | **结论行同协议**：打印 `RESULT: checks=<N> fails=<M>`（**十进制**、无 ANSI 颜色 ⇒ ⇒ 护栏正则可达） | ADR-074 D6 |

### 实施约束（已核实，避免实现期返工）

1. **三面 CWD 不一致（实测）**：`run_all_ported_tests.sh:106` `cd "$BUILD_DIR_ABS"`（= `build/`）；
   CTest 的 CWD = `build/tests`（`tests/CMakeLists.txt:1423-1426` 的 `add_test` **无** `WORKING_DIRECTORY`）；
   `verify_example_test_wiring.sh:69` `cd` 到**仓库根**。
   ⇒ ⇒ 相对路径必然在其中两面失败 ⇒ ⇒ **只能**用编译期绝对路径（`CPPHDL_SOURCE_DIR`）。
2. **`CPPHDL_SOURCE_DIR` 现有定义只在 `tests/CMakeLists.txt:1138`**（`test_verif_single_source`）
   ⇒ ⇒ example target **MUST 新增** `target_compile_definitions(<target> PRIVATE CPPHDL_SOURCE_DIR="${PROJECT_SOURCE_DIR}")`
   ⇒ ⇒ 这不是「纯改名」，是**新增构建接线**。
3. **`install(TARGETS …)` 的后果**：`examples/axi4/CMakeLists.txt:27` 把该 target 装到 `bin/axi4/`
   ⇒ ⇒ 装出去的二进制读的是**构建机源码树绝对路径** ⇒ ⇒ 源码清理后运行会 **fail-closed**
   ⇒ ⇒ **接受**（示例可执行文件本就依赖源码树），但 MUST 写进「用户可见变更」。
4. **`RESULT:` 行格式**：老 `main()` 里有 `std::hex` 切换（`:183`）⇒ ⇒ 新实现 MUST **不残留** `std::hex`
   （否则 `checks=71` 之类以十六进制打印 ⇒ 护栏正则失配）。
5. **`main` 签名提前改为 `int main(int argc, char** argv)`**（argv 暂不用）⇒ ⇒ 后继 change 加
   `--emit-rtl` 时不必再改签名。

---

## 验收标准

**16 条**

| AC | 判据（**可机械判定**） | 载体 |
|---|---|---|
| AC1 | `.verif` 含失败断言时 example 退出码 **非 0**，且 stderr 含失败的 `check_id`（`res.failure_lines`） | 投毒 |
| AC2 | example 源码里 `.verif` 的**相对后缀字面量** `/examples/axi4/tb_axi4_lite_example.verif` 出现**恰好 1 次**，且其仓内相对路径 == tsv `tb` 列把 `.sv` 换成 `.verif`（**不是**与护栏 `--emit-tb-verif` 的绝对路径逐字比 —— 那永远不可能成立） | grep |
| AC3 | **条数等式**：既有护栏 **⑥**（`.md` `VERIF-CHECKS` ↔ 渲染 TB 运行期 `checks` ↔ `min_checks`）仍绿 ⇒ ⇒ **⑥''（键集合）不承担数值**，判据用**运行期口径**（源码行数 ≠ 113） | 护栏 |
| AC4 | 零残留（**限定该文件**）：`grep -c 'Axi4TimingChecker\|VcdRecorder\|toVerilog\|axi4_lite_waveform' examples/axi4/tb_axi4_lite_example.cpp` == 0 | grep |
| AC5 | 零端口访问（**限定该文件**）：`grep -c 'set_input_value\|get_port_value\|\.io()\.' <该文件>` == 0 | grep |
| AC6 | **零落盘**：在 `mktemp -d` 新建的空目录里运行 ⇒ 该目录运行后仍为空 | shell |
| AC7 | 改名不改期望：生成式 TB **逐字节不变**（⑥' 新鲜度绿） | 护栏 |
| AC8 | 同步面全绿：`verify_examples_rtl_sim_coverage.sh`（**27 == 10 ∪ 17**；⚠️ **2026-11-18 订正**：原 28/18 未计入 change `example-file-boundary-2026-11` 的 `axi4_full_example` 退役 ⇒ ⇒ 该数字无判别力（护栏比的是**活值**），但字面 MUST 与实测一致）· `ctest -N` 仍有 `spinalhdl_axi4_lite` · `install(TARGETS)` 行已改 · **`source` 列路径存在于磁盘**（新增判据 —— 该列目前**读了不判**：`verify_examples_rtl_sim.sh:140` 取出后作为 `$3` 传入而 `check_one` 从未使用它） | shell |
| AC9 | 护栏 C3 仍绿（**注**：C3 对 `spinalhdl_*` 这类「CTest 名 ≠ 二进制名」的条目**空跑** ⇒ 它对本 example 无判别力 ⇒ 已登记为独立遗留）；**本 change 的判别力由 AC17 承担** | 护栏 |
| AC10 | `./run_all_ported_tests.sh` 绿（**27** tracked · PASS 计数 **27**）⇒ ⇒ ⚠️ **2026-11-18 订正（用户裁决 D4）**：原「仍 28」与 change `example-file-boundary-2026-11` 的 AC27（`axi4_full_example` 退役 ⇒ tracked 28 → 27）**互斥** ⇒ ⇒ 以 **27** 为准；⇒ ⇒ 「绿」的前提 = **无 `UNTRACKED`**（既有缺口 `emit_rtl_rv32i_soc_fw_init` ⇒ 由 `examples/disabled_examples.txt` 登记处置） | shell |
| AC11 | **fail-closed**：`.verif` 路径指向不存在的文件 ⇒ 退出非 0 · stderr 有说明 · stdout **无** `RESULT:` 行（且不得以其它形态写出 `checks=/fails=`） | 投毒 |
| AC12 | **自洽 + 格式**：成功路径输出 `RESULT: checks=113 fails=0`（**十进制** · 单行 · 无 ANSI） | grep |
| AC13 | **CWD 无关**：从仓根 / `build/` / `build/tests/` / `mktemp -d` 新建空目录各跑一次 ⇒ 都 exit 0 且 `RESULT:` 行相同 | shell |
| AC14 | **运行时长** < 30s（CTest `TIMEOUT 30` 不变） | 实测 |
| AC15 | **`CH_JIT_ENABLE=OFF`**：本地 `cmake -B build-nojit -DCH_JIT_ENABLE=OFF` 构建该 target 并运行 ⇒ exit 0（记录命令与输出；**不**声称由 CI 保证 —— 实测 CI 无该配置的 job） | 实测 |
| AC16 | **旧名归零**：①「旧名」= 两个精确模式（`axi4_lite_example\.cpp` 与词边界 `axi4_lite_example`）；② 作用域 = `examples/ scripts/ tests/ docs/*.md AGENTS.md`；③ 白名单 = `scripts/verify_examples_rtl_sim.sh` 的 self-test fixture 行 · `.rddf/checks/**`（含 `examples-rtl-expectations.md` 的 `## axi4_lite_example` 锚点 —— `.verif` 的 `@expect-src` 被 AC7 冻结，锚点**不能**改名）· `openspec/changes/**` · `docs/adr/**` · `docs/architecture/**` · `docs/reports/**` · `docs/CHANGELOG.md` · `docs/archive/**` | grep |
| AC17 | **本 change 的判别力**：`verify_examples_rtl_sim.sh` 新增 **⑨**（C++ 侧条数 == tsv `min_checks`）+ `--self-test` 两条投毒（⑨ 条数不符 ⇒ 必红；`source` 列指向不存在路径 ⇒ 必红）⇒ ⇒ `--self-test` **整轮绿**（不得用 `\|\| true` 吞失败）⇒ ⇒ **⑨ 的作用域 MUST 由清单表达**（「该行有解释器入口」）⇒ ⇒ **MUST NOT** 假设「凡 `generated=yes` 必有 C++ 侧」（将来只有 Verilog 侧的产品会被误判为违规） | 护栏 |

### AC ↔ task 映射

| AC | task |
|---|---|
| AC1 · AC11 | **C0** · **C1** |
| AC2 · AC4 · AC5 · AC6 · AC12 | **B1**–**B6** |
| AC3 · AC14 | **D1** · **D2** |
| AC7 | **E7** |
| AC8 | **E2**–**E5** · **F1b** |
| AC9 | **F2**（并见 C7b 的登记） |
| AC10 | **F3** |
| AC13 | **C2** |
| AC15 | **G1** |
| AC16 | **E6** · **F4** |
| AC17 | **F1** |

---

## Capabilities

- **`examples-rtl-sim-coverage`**（**新能力** · 由前置 change 首次 ADDED）
  ⇒ ⇒ 本 change **ADDED** 两条 requirement（见 `specs/examples-rtl-sim-coverage/spec.md`）：
  1. 「C++ 侧 MUST 与生成式 TB 共享同一验证源」（含「无第二载体」「无 print-only 检查器」「fail-closed」）
  2. 「由 `.verif` 驱动的 example 断言失败 MUST 以非零退出码结束」（**限定作用域** ⇒ ⇒ 已登记但未迁移的产品不受约束）

---

## 用户可见变更

| 项 | 前 | 后 |
|---|---|---|
| CMake target / 二进制 | `axi4_lite_example` | `tb_axi4_lite_example`（**CTest 条目名 `spinalhdl_axi4_lite` 不变**） |
| 产物 | `axi4_lite_waveform.vcd`（**手喂常量 · 非忠实波形**）· `axi4_lite_example.v`（游离） | **无** |
| 结论 | 5 个时序检查只打印 · 1 个数据断言 | **113 条断言**（`RESULT: checks=113 fails=0`）· 失败/解析错 ⇒ **非零退出**且不打印 `RESULT:` |
| `install` 后的二进制 | 自包含 | **依赖构建机源码树**（读 `.verif` 的绝对路径）⇒ ⇒ 源码清理后 fail-closed（**接受**） |

⚠️ **波形**：Verilator 侧的渲染式 TB **仍有**真波形
（`src/rtl_emit/tb_stub.cpp:160-162` 的 `ifdef DUMP_VCD` ⇒ `tb_axi4_lite_example.sv:856` 的 `$dumpfile`）
⇒ ⇒ 本 change 只删 C++ 侧那个**手喂常量**的误导性产物，**不**提供替代的 C++ 侧波形
（那属独立立项）。

---

## Out of Scope（明确不做）

| 项 | 原因 |
|---|---|
| **SoC example 改名 / 改 `.verif` 驱动** | UART 8N1 帧解码超出封闭原语集（ADR-074 D2）⇒ SoC 的 `.verif` 不存在 ⇒ 改名只是换皮 ⇒ 独立立项 |
| `.verif` 原语集扩容 | 属 ADR-074 决策面 ⇒ 需 architect |
| 给 C++ 侧补「忠实波形」 | 与「去碎片化」无关 ⇒ 独立立项 |
| `examples/axi4/emit_rtl.cpp` 的删除 / `--emit-rtl` 模式 | 属 `emit-cli-into-example-2026-11` |
| 两个顶层头的归位 / `tests/` 的 DUT 切换 | 属 `example-file-boundary-2026-11` |
| 其余 26 个 example 的 C++ 侧改造 | 本 change 只做 1 个 ⇒ 其余属后续范围（清单 `generated=no` 已逐行登记原因） |
| 期望值的任何改动 | AC7 ⇒ 期望值改动 MUST 走独立 change + `.md` 登记 |
| `tests/test_axi_lite_verilator_e2e.cpp` 的**手写激励面** | 该文件自己也有一套手写驱动/期望 ⇒ 「AXI4-Lite 的期望只剩一份」**不成立**，本 change **不**主张这个结论 |
| **修 `verify_example_test_wiring.sh` 的 C3 空跑**（C7b） | 该洞影响**全部 26 个** `add_spinalhdl_ported_test` 条目 ⇒ ⇒ 修它需要「CTest 名 → 二进制」的映射机制（`spinalhdl_*` 前缀剥除规则）⇒ ⇒ 与本 change 的目标无关 ⇒ ⇒ **独立立项**（本 change 只在 `verify_examples_rtl_sim.sh` 侧提供 ⑨ 覆盖本 example） |

---

## 🚪 门（**开工前 MUST 闭**）

| 门 | 内容 | 闭法（**可机械核**） |
|---|---|---|
| **G-1** | **归档顺序**：capability `examples-rtl-sim-coverage` 由前置 change 首次 ADDED ⇒ ⇒ 本 change 的 delta 指向**主 spec 里还不存在**的 capability | 开工前置：`test -f openspec/specs/examples-rtl-sim-coverage/spec.md`；且**前置 change 的归档 MUST 是独立 commit**（不得夹带本 change 的未提交改动） |
| **G-2** | **顺序与所有权**：三个 change 都动 `examples/axi4/` | 见下「顺序与所有权」；`example-file-boundary` 改 `tests/test_verif_single_source.cpp` ⇒ ⇒ **本 change 先落**（否则拆分基线漂移） |

---

## 顺序与所有权

| 文件 / 面 | 本 change | `emit-cli-into-example` | `example-file-boundary` |
|---|---|---|---|
| `examples/axi4/tb_axi4_lite_example.cpp`（改名后） | **owner**（驱动改造 + 改名） | 加 `--emit-rtl` 模式 | — |
| `examples/axi4/CMakeLists.txt` | 改 target / install | 删 emitter target | 加 `axi4_catch_test` helper |
| `tests/CMakeLists.txt:1455` | **改第二参数** | — | — |
| `tests/test_verif_single_source.cpp` | **不动**（本 change 零改动该文件） | — | **owner**（拆分） |
| `examples-rtl-sim-targets.tsv` 第 53 行 | `target` / `source` 两列 | `emitter` / `emitter_args` 两列 | — |

⇒ ⇒ **硬顺序**：本 change（批次 A–G）→ `emit-cli-into-example` → `example-file-boundary` 的 3.1。

---

## 风险与回滚

| 风险 | 处置 | 回滚 |
|---|---|---|
| 改后 example 在 CTest 下读不到 `.verif` ⇒ 退出非 0 ⇒ C3 红 | 实施约束 1–2（编译期绝对路径 + 新增 `target_compile_definitions`）；AC13 四 CWD 实测 | 回到手写路径（**单 commit**） |
| `RESULT:` 被 `std::hex` 污染 ⇒ 护栏正则失配 | 实施约束 4；AC12 判「十进制单行」 | — |
| 改名漏 `tests/CMakeLists.txt:1455` ⇒ configure 期直接红 | C5 逐项列全；AC8 判「`ctest -N` 仍有 `spinalhdl_axi4_lite`」 | — |
| 改后跑超 30s ⇒ 红在超时（掩盖断言） | AC14 实测 | — |
| 删产物影响外部使用 | 用户可见变更已声明；仓内零消费者 | 恢复两处调用 |

## 关联 change

- **前置**: `rtl-verification-single-source-2026-11`
- **后继**: `emit-cli-into-example-2026-11`
- **其后**: `example-file-boundary-2026-11`（3.1）
