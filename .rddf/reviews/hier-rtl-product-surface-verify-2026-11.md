# 验收报告：层次化 RTL 进入产品面（诉求 ⑤）

- **任务**：用户 2026-11-17 诉求「RTL 生成能力产品化」中的 **⑤ 层次化输出示例**
- **实施角色**：builder（角色内建 verifier 环节，按「审查配对」`builder → verifier`）
- **已用轮次**：**1 / 3**
- **判据来源**：用户本次诉求（无 openspec change ⇒ 按 quick 路径：本次声明什么就验什么）
- **日期**：2026-11-17

## 缺口（实施前）

层次化输出只有**测试级**载体（`examples/riscv-mini/tests/test_rv32i_hier_rtl.cpp`
在用例内发射）⇒ ⇒ 用户**拿不到** `.v` 产物（无 CLI）、**没有**独立 Verilog TB 能跑它、
**不在**覆盖登记里 ⇒ ⇒ 停在「特性」层，未到「产品」层。

## 逐条判定

| # | 判据 | 判定 | 工具证据 |
|---|---|---|---|
| AC1 | 通用生成器支持层次化（`--hierarchical` flag ⇒ `toVerilogHierarchical`） | ✅ | `include/rtl_emit/cli.h`（`CliOptions::hierarchical`）· `src/rtl_emit/cli.cpp`（parse + 分支 + fail-closed 判 `ok`）· `--help` 含该选项（实测 1 处命中） |
| AC2 | RV32I 层次化产物可生成，且**确实是多 module** | ✅ | `emit_rtl_rv32i_soc_hier --out <dir>` ⇒ `--modules` 输出 = `IfStage IdStage ExStage MemStage WbStage HazardUnit ForwardingMux_32_ top`（**8**） |
| AC3 | 顶层端口表 == manifest `port_table[]`（**逐字逐序**） | ✅ | 产物顶层 **34**（6 in + 28 out）· `manifest.json` `port_table` **34** · Python 比对 `names == [e['name']]` ⇒ `True`；护栏 ⑧ 亦断言通过 |
| AC4 | **独立 Verilog TB** 能跑层次化产物 | ✅ | `verilator --binary --timing ... tb_rv32i_soc.sv top.v` ⇒ `RESULT: checks=24 fails=0`（与扁平**完全相同**） |
| AC5 | 覆盖登记（`kind=hier` 桶 + 锚点） | ✅ | `.rddf/checks/examples-rtl-sim-targets.tsv` 新增 1 行（11 列）· `REGISTERED_HIER: 1` ⇒ `verify_examples_rtl_sim_coverage.sh` PASS（守恒 `28 == 10 ∪ 18` 不变） |
| AC6 | 护栏接管 + **形态判据 ⑧'** + **判别力自证** | ✅ | `verify_examples_rtl_sim.sh` PASS（**12 项**，含 `⑧' 层次化形态成立（module 数 8 · 根 = top）`）· `--self-test` PASS（**8 条投毒各自转红**，含投毒 8 `flatten_hier` ⇒ ⑧' 转红） |
| AC7 | **零扰动**（扁平默认路径逐字节不变） | ✅ | 改前/改后 `emit_rtl_rv32i_soc` 的 `top.v` + `manifest.json` `cmp` ⇒ **IDENTICAL**；`ctest -LE perf` ⇒ **255/255 passed**（含 `verilator_e2e` 77 项） |
| AC8 | 边界与错误路径 | ✅ | `--hierarchical --emit-tb-stub` ⇒ 骨架 `module tb_top_stub;` + `top dut (` + **34** 端口连接（== `port_table[]` 34）· `--hierarchical --no-verilog` ⇒ 仅 manifest · 层次化失败分支**显式判 `ok`** ⇒ （该入口不抛异常，不判则发射失败会静默 exit 0） |

## 遗留清扫（复核）

| 项 | 结果 |
|---|---|
| TODO / FIXME / XXX / HACK / 调试日志 | **零**（改动文件全扫） |
| 注释掉的代码块 | **零** |
| 被 skip 的测试 | **零** |
| 未替换的 mock / 临时 fallback | **零** |
| 临时代码 | **零**（`emit_rtl_hier.cpp` 48 行 · 纯薄壳） |

## 未纳入本次（如实登记）

| 项 | 原因 | 归属 |
|---|---|---|
| 残差 `id=25`（RV32I 语料接入 `verify_hierarchical_rtl.sh` 的 R2–R8 全套） | 状态格在**双向 1:1** 的登记表 + 架构台账（`docs/architecture/**` = `arch_paths`）⇒ 属 architect 域 | architect |
| 层次化 + 固件初值的组合变体（`emit_rtl_fw_init` 的层次化版） | 另一维（固件链路），不在「层次化输出示例」范围内 | 独立排期 |
| `HierarchyEmitOptions::top_module_name` 未暴露为 CLI 选项 | **刻意**：顶层 module 名必须与扁平同为 `top`，否则「同一 TB 吃两种产物」的前提失效 | 不新增 |
| 特权级（CSR/ECALL/EBREAK/MRET/中断/trap） | ADR-072 判为 G2 独立立项；涉及架构取舍 | architect |

## 结论

**PASS**（8/8 判据）。层次化 RTL 从「测试级特性」升为「产品级交付面」：
CLI 入口 + 独立 TB（复用、同源）+ 覆盖登记 + 形态护栏（⑧' 有判别力）+ 零扰动。
