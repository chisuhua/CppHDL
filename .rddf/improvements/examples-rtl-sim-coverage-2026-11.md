---
name: examples-rtl-sim-coverage-2026-11
priority: P2
phase: phase-6
category: testing
type: feature
状态: in-progress
进度: E1 / E2 / E4 / E5 / E7 / E8 已落 · **E3 = 7/10 verified**（counter 54 · counter_simple 768 · pwm 13 · gpio 9 · quadrature 7 · fifo_stream 62 · uart_tx 34）· **E6 ⬜**（前置 = E3 **10/10**）· E3 余 3 项：`uart_rx` · `axi4_lite` · `riscv-mini` smoke · ✅ **fifo_stream 的 D5 已修复**（`full` 门限 ⇒ 两处同改 ⇒ 容量 = N ⇒ TB 判据正面放开 + 变异承重）
主题: dual-backend 测试完整化
依赖: rtl-emit-product-tooling-2026-11（CLI + manifest）· port-naming-ratchet-2026-11（端口名可读）
来源: 用户 2026-11-10 拍板「③ 对 example 下的示例添加完整的 RTL 生成和独立 Verilog 仿真能力，同时是否也需要创建单元测试覆盖生成 RTL 和独立 Verilator 测试能力」
生成时间: 2026-11-10
evidence: probe:scripts/verify_hierarchical_rtl.sh（既有产物级护栏样板）+ 本轮实测 verilator 编译 ≈8s/设计
---

# Improvement: examples 的 **RTL 生成 + 独立 Verilog 仿真**覆盖（含生成能力的单元测试）

- **Date drafted**: 2026-11-10
- **Drafted by**: planner 角色（build-agent）
- **Status**: 📝 **draft**（待用户拍板 ⇒ 拍板后转 builder 流程）
- **用户裁决（2026-11-10）**: 「#3 按你建议」⇒ ⇒ **10 个**有意义的 example（**不是** 32 个全量）· `--emit-tb-stub` 只发骨架 · **必须**有单元测试覆盖「生成 RTL」这件事 · 计数锚点单一更新入口 · CI 分级 + 复用缓存

---

**主题**: dual-backend 测试完整化

## 架构依据

### A.1 实测事实（2026-11-10）

| # | 事实 | 证据 |
|---|---|---|
| ① | ⚠️ **「生成 RTL」这件事的产物形态零测试** —— 没有任何测试断言过 `toVerilog()` 的**端口集 / 名字 / 确定性 / lint** | `scripts/` 下只有 `verify_hierarchical_rtl.sh`（**层次化**产物）⇒ ⇒ **扁平**产物**无护栏** |
| ② | ⚠️ **「独立 Verilog 仿真」当前只有 1 个实例**，且是本轮 planner 的临时产物（**不在仓内**） | `/tmp/opencode/rv32i_rtl/tb_rv32i.sv`（3149 行 RTL + 独立 TB ⇒ 20/20 golden 通过） |
| ③ | verilator 后端**已经**在跑生成的 RTL（`src/core/verilator_backend.cpp:709` 用同一个 `verilogwriter`）⇒ ⇒ **"跑生成的 RTL"的能力已存在** ⇒ ⇒ 缺的是**独立于 C++ 仿真器**的那条腿 | 既有 `verilator_e2e` 标签的 **77** 个测试 |
| ④ | ⚠️ 但那条腿**测的是"后端行为"**，**不是"产物可独立使用"** ⇒ ⇒ 后端按 **node-id** 访问端口（`port_access_[node->id()]`）⇒ ⇒ **端口名对后端毫无意义** ⇒ ⇒ 后端全绿 **不构成**「产物可被外部 TB 使用」的证据 | `src/core/verilator_backend.cpp:849-930`（`build_port_access_table` 按 node id 建表） |
| ⑤ | ✅ **已有产物级护栏的样板**：`scripts/verify_hierarchical_rtl.sh`（R1–R8，含 `--self-test` 逐条投毒 ⇒ 必红 ⇒ 还原复绿）⇒ ⇒ 扁平侧照此做即可 | `scripts/verify_hierarchical_rtl.sh` |
| ⑥ | ✅ **已有独立 Verilog 仿真的样板**：`scripts/run_all_samples.sh`（运行护栏 · 缺失二进制 = 失败）⇒ ⇒ examples 侧照此做即可 | `scripts/run_all_samples.sh` |
| ⑦ | ⚠️ **CI 成本**：verilator 编译 ≈ **8s/设计**（本轮实测：3149 行 RTL 的编译 ≈ 8.1s）⇒ ⇒ 32 个 example 全量 ≈ **4min+** ⇒ ⇒ 必须分级 | 本轮实测 |
| ⑧ | ⚠️ **计数锚点有 4 个**且全部手维护：`CTEST_TOTAL`(257) · `VERILATOR_E2E_TOTAL`(77) · `DUAL_BACKEND_PARITY_TOTAL`(66) · `DUAL_BACKEND_PARITY_PAIRS`(290) · `EXAMPLES_TEST_SOURCES`(17) ⇒ ⇒ 新增测试 MUST 同步 ⇒ ⇒ 否则 `scripts/verify_test_counts.sh` 等护栏转红 | 根 `AGENTS.md` 的 CI anchor 段 |
| ⑨ | ✅ **verilator SHA-1 缓存已存在**（`compute_cache_key`：verilog 源 + sim_main 模板 + flags + verilator 版本）⇒ ⇒ 独立 TB 可**复用**同一缓存思路 ⇒ ⇒ 第二次编译 ≈ 秒级 | `src/core/verilator_backend.cpp:1128-1135` |
| ⑩ | ⚠️ **examples 的测试源**计数口径**已固化**（17 个），新增 TB 源文件**是否**计入该口径？⇒ ⇒ MUST 先确认口径（见 §范围 C6） | 根 `AGENTS.md` 的 `EXAMPLES_TEST_SOURCES` 锚点注释 |

### A.2 为什么"独立 Verilog 仿真"不可省

③④ ⇒ ⇒ 后端那条腿**不看端口名** ⇒ ⇒ 它**无法**发现：
- 端口名错 / 端口缺失 / 端口多出
- 顶层端口选错（80 个内部输出被提升）
- 悬空输入（`mem_is_load`）
- 产物缺 `` `default_nettype none `` 导致的拼写错
⇒ ⇒ 这些正是**用户拿到 RTL 后最先踩**的坑 ⇒ ⇒ 必须有**一条按名字驱动端口**的腿。

---

## 范围

### ✅ 在内

| 编号 | 内容 |
|---|---|
| **E1** | ⚠️ **metis 订正 —— 本项 MUST 归并到 `rtl-emit-product-tooling-2026-11` 的 F1–F6**（同一判据两份实现必然漂移 ⇒ ⇒ 仓内「两份实现会漂移」纪律反复治的病）⇒ ⇒ 本项**只保留**依赖项**没有**的部分：① **跨进程**确定性（同一进程同图 ⇒ 恒等 ⇒ 判别力 ≈ 0）② 产物**非空 / `module`·`endmodule` 配平** / 端口数 == manifest（`ModuleEmitter::print` 是 `try { … } catch (...) {}` ⇒ 抛异常也 exit 0 ⇒ 可产出半截文件）③ **「按名驱动」的机械证据**：改 manifest 的端口名 ⇒ ⇒ TB **编译失败**（TB 编译通过本身就是「名字对」的硬证据 —— 比任何端口表比对都硬）④ **产物变异 ⇒ TB 必红**（删一条 `assign` / 改一个端口宽度 / 断一个内部驱动）⑤ **X 检测**：TB MUST 断言输出 `!== 1'bx`（`$isunknown`）—— 否则「端口名都对、值是 X」照样绿 |
| **E2** | **CLI + `--emit-tb-stub`**：`tools/cpphdl-emit-rtl --emit-tb-stub` 产出**测试台骨架**：模块实例化 + 全部端口连线（含 tie-off）+ 时钟/复位驱动 + VCD 开关 + 端口清单注释 ⇒ ⇒ **不含**激励与断言（语义不可自动生成） |
| **E3** | **每 example 自带 TB**：`<example>/tb_<name>.sv`（**手写**，提供激励 + 断言）⇒ ⇒ 约定：TB 的期望值 MUST 有**出处登记**（TB 头部注释写明来源）⇒ ⇒ ⚠️ **metis 订正**：① 原稿「与该 example 的 C++ 测试断言同一语义」对 **9/10 候选不成立** —— examples 的 Catch2 测试**只在** `examples/riscv-mini/tests/`（`EXAMPLES_TEST_SOURCES = 17` 全部来自那里），`counter`/`pwm`/`gpio`/`uart_rx`/`spi`/`i2c`/`quadrature_encoder`/`fifo` 都是**无断言的 `main()` demo** ⇒ ⇒ MUST 改为「**两侧各自与外部期望源一致**」② ⚠️ **两侧都是同一份手推常量 ⇒ 不算独立证据**（与仓内「两后端一致不能当正确性证据」同型）③ ⚠️ `examples/spinalhdl-ported/counter/counter.cpp:211-214` 自己打印「`(expected: 255)`」+「`Note: enable/clear may not fully work yet`」⇒ ⇒ 把它当期望源 = **固化已知可疑行为** ⇒ ⇒ 该类 demo **不得**入选 |
| **E4** | 🔵 **已裁决（2026-11-10 用户）＝ 不收窄目标，收窄声明**：<br>**target set 仍为 10 个**（按 `examples/CMakeLists.txt:34-76` 的 `CH_TRACKED_EXAMPLE_TARGETS` 取**确切 target 名**）⇒ ⇒ 但交付口径拆成两档：<br>· **已验证 = 1**（`riscv-mini` 的 RV32I SoC —— 本轮 planner 已实测跑通 golden）<br>· **待补期望源 = 9** ⇒ ⇒ **每一项 MUST 列出「缺什么」**（见下表）；缺项补齐前**不得**计入「已验证」<br>⚠️ 准则 ③「有 C++ 侧断言」**不可放宽**：`examples/` 的 Catch2 测试**只在** `examples/riscv-mini/tests/`（`EXAMPLES_TEST_SOURCES = 17` 全部来自那里）⇒ ⇒ 其余 9 个候选的期望源 MUST 来自**外部**（协议时序 / 手推常量 + 推导过程），并登记在 TB 头部注释 |
| **E5** | **运行护栏**：`scripts/verify_examples_rtl_sim.sh`（对标 `run_all_samples.sh`）：缺失 TB / 缺失产物 ⇒ ⇒ **失败**（不跳过）⇒ ⇒ 防止「TB 从未被执行」<br>⚠️ **metis**：① 「断言非空」**MUST NOT** 靠自研 Verilog 静态分析器 ⇒ ⇒ 仓内先例：`scripts/verify_example_assertion_quality.sh` 的自研 C 子集分析器在**第 4/5/6 轮对抗审稿**里 BLOCKER **全部**出在它身上 ⇒ ⇒ 为 SV 重造 = **复现同一失败模式** ⇒ ⇒ 改用**运行时证据**：每个 TB 末尾 MUST 打印 `RESULT: checks=<N> fails=0`，runner 断言 `checks >= <anchor>` 且 `fails == 0` ② 每个 TB MUST 有 **TIMEOUT / 墙钟 / 内存上限**（挂死的 TB 会拖死 job）③ 缺 verilator 时 MUST **区分**「CI job 内 fail-closed」与「本地可 opt-in」，且**禁止静默 skip**（那正是历史事故形态） |
| **E6** | **CI 形态**：🔵 **已裁决（2026-11-10 用户）**：**挂 `pr-verilator-fast` 的一个 step**（**不**新建 job ⇒ **无**第二次完整 build）。依据：`ci.yml:1353-1356` 该 job **已 PR-only 且 `BUILD_VERILATOR: 'ON'`**（`:1377`）、已跑 `ctest -L verilator_e2e`（`:1621`）⇒ ⇒ 增量成本 ≈ 0（verilator 已构建 + 缓存）。<br>⚠️ **MUST NOT** 注册成 CTest 条目：默认 job 的 `Run tests` 是**无 label 的 `ctest`**（`ci.yml:207-209`，`BUILD_VERILATOR=OFF`）⇒ ⇒ 必被它跑到（无 verilator 环境 ⇒ 必红/必 skip）。<br>⚠️ 若最终仍选 `nightly-verilator`（`ci.yml:1943-1945` 的 cron **就在 main 上跑**）⇒ ⇒ MUST 把「PR 阶段无 RTL 仿真证据」登记进 `.rddf/checks/arch-residual-registry.md`。<br>⚠️ **NIT 订正**（critic）：原「三跑」理由混淆对象 —— `verify_example_test_wiring.sh` C3（`:307-365`）跑的是 `build/examples/` 下的 **C++ 二进制**，**不是** `.sv` TB；结论（不注册进 CTest）仍成立。 |
| **E7** | **缓存复用**：独立 TB 的 verilator 编译 MUST 复用 SHA-1 缓存（源码 + TB + flags + verilator 版本）⇒ ⇒ 第二次编译秒级 |
| **E8** | **计数锚点单一更新入口**：`scripts/update_test_count_anchors.sh` ⇒ ⇒ **保留**锚点本身（它们防的是**静默缩面**）⇒ ⇒ 只把「手工同步」变成「一条命令」<br>⚠️ **metis 订正**：① MUST **不重算**计数 ⇒ ⇒ `DUAL_BACKEND_PARITY_*` 的计数实现在 `scripts/lib/dual_backend_parity.py`（`verify_dual_backend_parity.sh:18,59`）⇒ ⇒ 更新脚本 MUST 调既有护栏的 `--print-actual`（**当前无此 flag · 需先加**）⇒ ⇒ 否则两套计数实现必然漂移 ② `--check` 与既有 `verify_test_counts.sh` **重复判定** ⇒ ⇒ 建议**删除** `--check`（检查交给既有护栏）③ 锚点清单**不止 5 个数值**：还有 `CH4_ANCHOR`（`verify_example_assertion_quality.sh:69-85` 的逐文件断言数下限 · 且「新增测试源 MUST 在此登记一行」）· `AGENTS.md §Test counts` 表格行 · `verify_public_api_visibility.sh` · `verify_perf_baseline_anchors.sh` ⇒ ⇒ 「一次更新全部锚点」**名不副实** ⇒ ⇒ MUST 显式声明不覆盖哪些 |

### ❌ 不在本项

- **端口命名规则** ⇒ 归 `port-naming-ratchet-2026-11`（本项只**消费** manifest 的 `name`）
- **CLI / manifest / 产物护栏本身** ⇒ 归 `rtl-emit-product-tooling-2026-11`
- **全量 32 个 example 的 RTL 仿真** ⇒ ⇒ 首批 10 个跑通后**再评估**扩展
- **`samples/`（40 个）的 RTL 仿真** ⇒ ⇒ 独立立项（`samples/` 是单文件 demo，形态更简单 ⇒ ⇒ 适合第二批）
- **自动生成 TB 的语义断言** ⇒ ⇒ 不可行 ⇒ ⇒ 只发骨架

---

## Why

### W.1 「生成能力」的零测试 = 一次退化不会有人知道

① ⇒ ⇒ 仓内"两后端一致不能当正确性证据"的纪律（同源 ⇒ 同构缺陷一致地错）**同样适用于 codegen**：没有任何独立信号能发现「端口多出 80 个」「名字漂移」「lint 被关掉」⇒ ⇒ 必须有产物级护栏。

### W.2 后端那条腿**看不见**用户会踩的坑

④ ⇒ ⇒ 后端按 node-id 访问端口 ⇒ ⇒ 端口名错、端口多、悬空输入，**后端一律全绿**⇒ ⇒ 而这恰恰是外部 TB 用户最先遇到的 ⇒ ⇒ 必须补一条**按名字驱动**的腿。

### W.3 全量 32 个 example 的 CI 成本不可接受，但**零覆盖**更不可接受

⑦ ⇒ ⇒ 全量 ≈ 4min+ ⇒ ⇒ 而收益集中在**有实质逻辑**的 example ⇒ ⇒ ⇒ 首批 10 个是性价比拐点。
⇒ ⇒ ⚠️ **但**"先做 10 个"最容易变成"永远只有 10 个"⇒ ⇒ E5 的运行护栏 + 扩展清单 MUST 在本项里**留痕**（哪些没做、为什么）。

### W.4 计数锚点是**静默缩面**的防线，不能手工维护

⑧ ⇒ ⇒ 新增测试必然要动锚点 ⇒ ⇒ 手工同步 = 下次就忘了 ⇒ ⇒ E8 把它变成**一条命令**（但**保留**锚点语义）。

---

## What Changes

### E1 — 生成能力的单元测试

- 落点：`tests/test_rtl_emit_contract.cpp`（**仓内** · 进 ctest）⇒ ⇒ 断言 E1 的 6 条。
- ⚠️ **断言必须可判别**：每条 MUST 有**变异自证**（改一处 ⇒ 转红）⇒ ⇒ 对标 `verify_hierarchical_rtl.sh --self-test` 的做法。
- ⚠️ **端口集判据**需要一份**期望端口表**（每个被测 DUT 的）⇒ ⇒ 由 manifest 承载 ⇒ ⇒ **但** manifest 与产物同源 ⇒ ⇒ ⚠️ **自证风险**！⇒ ⇒ 必须另有一份**独立手写**的期望端口表（人工列出）⇒ ⇒ 否则"manifest == 产物"是同源自证 ⇒ ⇒ **本项 MUST 同时维护手写期望表**。

### E2 — `--emit-tb-stub`

```
tools/cpphdl-emit-rtl --top Counter --emit-tb-stub -o build/rtl/counter
```
产出 `tb_counter_stub.sv`：
- `module tb_counter_stub;` + `top dut (...);` + **全部端口连线**（输入按 `role` tie-off / 由 `semantic` 决定注释、输出接 `logic`）
- 时钟/复位驱动骨架（`always #5 clk = ~clk;`）
- VCD 开关（`ifdef DUMP_VCD`）
- 端口清单注释块（来自 manifest）
- ⚠️ **明确留空**：激励 + 断言位置（`// TODO: 由 <example> 的 tb_<name>.sv 提供`）

### E3 — 每 example 自带 TB

- 约定目录：`<example_dir>/tb_<name>.sv` ⇒ ⇒ 骨架复制过来后补激励与断言。
- ⚠️ **断言与 C++ 侧同语义** ⇒ ⇒ 两侧互为对照 ⇒ ⇒ 若 C++ 侧与 TB 侧期望值**不同** ⇒ ⇒ 判"不一致"为红 ⇒ ⇒ 防止"TB 自己写错了"（仓内纪律：期望值 MUST 手推）。
- ⚠️ **不得**让 TB 只是 `SUCCEED` / 空断言（仓内既有护栏 `scripts/verify_example_assertion_quality.sh` 管 C++ 侧的这件事 ⇒ ⇒ **RTL 侧需要等价护栏**）。

### E4 — 首批 10 个 example（**精选准则**）

**准则**（按优先级）：① 有**可观测语义**（输出随输入变化）② 覆盖**不同结构类别**（组合 / 时序 / 流控 / 总线 / CPU）③ C++ 侧已有**断言质量**合格的测试 ④ RTL 规模适中（verilator 编译 ≲ 10s）。

**候选**（**待实施期按准则筛选并登记**）：
| 类别 | 候选 | 备注 |
|---|---|---|
| **CPU（核心）** | `riscv-mini` 的 RV32I SoC | ✅ **本轮 planner 已实测跑通**（20/20 golden）⇒ ⇒ **直接转正** |
| 组合 | `counter` | 极简 ⇒ 适合做「最小闭环」 |
| 组合 | `gpio` / `pwm` | 组合逻辑 + 宽度处理 |
| 时序 | `uart_tx` / `uart_rx` | 状态机 ⇒ ⇒ 验证「时钟/复位」在独立 TB 里被正确驱动 |
| 流控 | `stream_fifo` | 握手 / ready-valid ⇒ ⇒ 验证端口方向正确 |
| 流控 | `quadrature_encoder` | 状态 + 计数 |
| 总线 | `axi4_lite` | AXI 握手 ⇒ ⇒ 验证多端口方向 |
| 其它 | `i2c_controller` / `spi_controller` | 复杂状态机（篇幅大 ⇒ ⇒ 视编译成本决定） |

⇒ ⇒ ⚠️ **实施期 MUST 按准则重新筛选并登记最终名单 + 每条的理由 + 落选的排除理由**（仓内既有先例：`dual-backend-full-coverage-inventory` 的 166 个源的逐个判定）。

### E4′ — 「待补期望源」清单（10 项逐条：缺什么）

> 口径：**`已验证` = 1**（`riscv-mini`）· 其余 9 项为 **`待补期望源`** ⇒ 补齐前不算已验证。
> ⚠️ 「缺什么」MUST 是**外部**期望源（协议时序 / 手推常量 + 推导），**不能**是「另一个同样没断言的 demo」。

| # | target（**权威确切名**，取自 `examples/CMakeLists.txt:34-76`） | 状态 | 缺什么 |
|---|---|---|---|
| 1 | **`riscv-mini` 的 RV32I SoC**（⚠️ 精确定位为 **test DUT** `Rv32iPipelineTop`，**不是** `CH_TRACKED_EXAMPLE_TARGETS` 条目） | ✅ **已验证** | —（本轮 planner 已实测：独立 Verilog TB 跑生成 RTL，golden 全过） |
| 2 | `counter_example` | ⬜ 待补 | C++ 侧**无断言** ⇒ 缺「计数/溢出/使能」手推表；且 `counter.cpp:211-214` 自认「`enable/clear` may not fully work yet」⇒ **MUST 先裁决该行为是否为已知缺陷**，否则固化可疑行为 |
| 3 | `counter_simple_example` | ⬜ 待补 | 同 2（同目录姊妹 demo） |
| 4 | `fifo_stream_example` | ⬜ 待补 | C++ 侧无断言 ⇒ 缺 **ready/valid 握手时序期望**（须来自 `chlib-stream-semantics` spec） |
| 5 | `uart_tx_example` | ⬜ 待补 | C++ 侧无断言 ⇒ 缺 **波特率/帧格式时序期望**（须手推位时间表） |
| 6 | `uart_rx_example` | ⬜ 待补 | 同 5 + **采样点相位**的期望 |
| 7 | `pwm_example` | ⬜ 待补 | C++ 侧无断言 ⇒ 缺 **占空比/周期手推表** |
| 8 | `gpio_example` | ⬜ 待补 | C++ 侧无断言 ⇒ 缺 **方向/读回语义手推表** |
| 9 | `quadrature_encoder_example` | ⬜ 待补 | C++ 侧无断言 ⇒ 缺 **正交序列 → 计数**的手推表 |
| 10 | `axi4_lite_example` | ⬜ 待补 | C++ 侧无断言 ⇒ 缺 **AXI4-Lite 握手时序期望**（须来自 `axi-slave-conformance` / `axi4-*` spec） |

⚠️ **命名订正（critic 第 2 轮）**：原表用 `uart_tx` / `pwm` / `gpio` / `quadrature_encoder` / `counter` 等**候选名**，与权威 target 名（均带 `_example` 后缀）不符 ⇒ 已按 `examples/CMakeLists.txt:34-76` 改正；`riscv-mini` 也**不是** target 名，已注明它是 test DUT。

⚠️ **最终 10 项 MUST 在实施期按 `CH_TRACKED_EXAMPLE_TARGETS` 复核**（本表是**候选**，不是冻结名单：`examples/CMakeLists.txt:34-76` 是权威）。目标名与候选名不一致的（如 `counter` → `counter_example`）以 CMake 为准。

### E5 — 运行护栏

```
scripts/verify_examples_rtl_sim.sh            # 全部首批 example
scripts/verify_examples_rtl_sim.sh --list     # 列出应跑集合
scripts/verify_examples_rtl_sim.sh --self-test
```
- 判据：① 首批集合**每个**都有 TB 文件 ② 每个都能编译 ③ 每个都**跑过**（退出码 0）④ TB 的断言**非空**（对标 `verify_example_assertion_quality.sh` 的静态分析思路）⑤ `--self-test` 投毒必红。
- ⚠️ **缺失 = 失败**（不跳过）⇒ ⇒ 防"TB 存在但从未执行"。

### E6 — CI 分级

- 🔵 **已裁决（E6 · critic 第 2 轮：本行原与 E6 冲突，已改）**：**不新增 ctest label、不注册 CTest 条目**；改为 `pr-verilator-fast` job 的**一个 step**（调 `scripts/verify_examples_rtl_sim.sh`）
- ⚠️ **CI 墙钟登记**：新增 job 的墙钟 MUST 记入 `.rddf/checks/ci-wallclock-cold-vs-warm-2026-11.md`（既有台账）⇒ ⇒ 避免"CI 变慢无人知道"。

### E7 — 缓存复用

- 缓存 key = `SHA-1(verilog 源 + TB 源 + flags + verilator 版本 + tb 文件名)` ⇒ ⇒ 落在 `~/.cache/cpphdl/rtl_sim/<key>/` ⇒ ⇒ 复用 `compute_cache_key()` 的思路（**MUST NOT** 与既有后端缓存**冲突**：key 里含 TB 名 ⇒ ⇒ 独立目录）。
- ⚠️ **缓存命中 MUST 仍验证产物存在** ⇒ ⇒ 缺失 ⇒ ⇒ 重编译（防"缓存指向已删文件"）。

### E8 — 锚点更新入口

```
scripts/update_test_count_anchors.sh            # 更新并打印 diff
scripts/update_test_count_anchors.sh --check    # 只检查是否需要更新（CI 用）
```
- 覆盖：`CTEST_TOTAL` · `VERILATOR_E2E_TOTAL` · `DUAL_BACKEND_PARITY_TOTAL` · `DUAL_BACKEND_PARITY_PAIRS` · `EXAMPLES_TEST_SOURCES`（根 `AGENTS.md` 的 CI anchor 段）。
- ⚠️ **保留**锚点的**语义**（防静默缩面）⇒ ⇒ 只自动化**同步**，**不**改为自动推导 ⇒ ⇒ `--check` 在 CI 里应当**红**（当锚点与实际不符时）⇒ ⇒ 与既有 `scripts/verify_test_counts.sh` 的关系 MUST 明确（避免两个护栏重复 / 冲突）。

---

## Acceptance

### 生成能力有测试

- [ ] ⚠️ **metis 订正**：原稿的 `tests/test_rtl_emit_contract.cpp` 六条与依赖项 F1–F6 **重复** ⇒ ⇒ **删除**该文件的重复部分（否则两份实现）⇒ ⇒ 本项只保留依赖项没有的四条：**跨进程**确定性 / 产物**非空 + module 配平** / **改 manifest 名 ⇒ TB 编译失败** / **产物变异 ⇒ TB 必红** / **`$isunknown` X 检测**
- [ ] ⚠️ **metis 订正**：原稿的「独立手写期望端口表」是「`io_type` → manifest → 表」的**第三份转录** ⇒ ⇒ 不提供新信息、只提供新的漂移面 ⇒ ⇒ **更轻的独立锚**：① 用 `verilator --json-only` 从**产物文件**抽端口 vs `io_type`（跨编译单元 ⇒ 真独立）② 每个 DUT 一份 **golden 端口快照**（`<name>.ports.golden`，与 TB 同目录 ⇒ 改名即必须改快照 ⇒ 这才是「不许静默改名」的真实需求）⇒ ⇒ 结构比对复用 `verify_hierarchical_rtl.sh:464-548` 的 R8 解析，**不自造第三种表格式**
- [ ] 每条断言有**变异自证**（改一处 ⇒ 转红）

### 独立 Verilog 仿真

- [ ] `--emit-tb-stub` 产出可编译的骨架（模块实例化 + 全端口连线 + 时钟/复位 + VCD 开关）
- [ ] **target set = 10**（确切 target 名）；其中 **`已验证` = 1**（`riscv-mini`）· **`待补期望源` = 9**（每项登记「缺什么」）⇒ ⇒ 缺项补齐前**不得**计入已验证（名单 MUST 登记入选理由 + 落选的排除理由）
- [ ] TB 的断言**非空**（以 `RESULT: checks=<N> fails=0` 的**运行时证据**判定 ⇒ ⇒ **禁**自研 Verilog 静态分析器）且期望值有**出处登记** ⇒ ⇒ ⚠️ 两侧期望值同源 ⇒ ⇒ **不构成**独立证据（与仓内「两后端一致不能当正确性证据」同型）
- [ ] ⚠️ **手写期望表登记**：最终 10 个名单 + 每条的入选理由 + 落选的排除理由
- [ ] 运行护栏 `scripts/verify_examples_rtl_sim.sh` 存在；缺失 TB / 缺失产物 ⇒ ⇒ **失败**（不跳过）
- [ ] 护栏 `--self-test` 投毒必红 ⇒ 还原复绿

### CI 与成本

- [ ] 🔵 **已裁决**：RTL-sim 作为 **`pr-verilator-fast` job 的一个 step**（**不**新建 job · **不**注册 CTest 条目 · **不**加 `rtl_sim` label）⇒ 无第二次完整 build；墙钟增量按「不含 build」单独登记
- [ ] 新 job 的墙钟**已登记**到 `.rddf/checks/ci-wallclock-cold-vs-warm-2026-11.md`
- [ ] 第二次编译命中缓存（≈ 秒级）；缓存命中但产物缺失 ⇒ ⇒ 重编译（不自绿）
- [ ] 首批的**总墙钟**已测量并登记（分「不含 build 的仿真时间」/「含 build 的 step 总时间」两项）—— 🔵 已裁决挂 `pr-verilator-fast` ⇒ **无**第二次完整 build（原「≈5–8 min 新 job」的前提已作废）⇒ ⇒ MUST 分项登记（「不含 build 的仿真时间」与「含 build 的 job 总时间」）
- [ ] ⚠️ **metis 新增**：`.sv` TB **MUST NOT** 放进 `examples/**/tests/`（`verify_example_test_wiring.sh` 的 C1 语义是「必须被 CTest target 编译」）⇒ ⇒ 放 `examples/<x>/tb_<name>.sv` ⇒ ⇒ 并在两个 examples 护栏各加一行「`.sv` 不在本护栏域」

### 计数锚点

- [ ] `scripts/update_test_count_anchors.sh` 存在；一次更新全部锚点并打印 unified diff
- [ ] `--check` 模式在 CI 中能抓「锚点与实际不符」
- [ ] 全部受影响锚点**已同步**（`CTEST_TOTAL` / `VERILATOR_E2E_TOTAL` / `DUAL_BACKEND_PARITY_TOTAL` / `DUAL_BACKEND_PARITY_PAIRS` / `EXAMPLES_TEST_SOURCES`）
- [ ] `scripts/verify_test_counts.sh` 等既有锚点护栏全绿

### 不回归

- [ ] 既有 **257** 个 ctest 全绿
- [ ] `scripts/verify_hierarchical_rtl.sh`（R1–R8）全绿
- [ ] `scripts/verify_example_assertion_quality.sh` / `verify_example_test_wiring.sh` 全绿
- [ ] 既有 `run_all_samples.sh` 语义**不变**

---

## Capabilities

| capability | 动作 | 说明 |
|---|---|---|
| **`rtl-sim-example-coverage`**（**新**） | ADDED | ① 「生成 RTL」的**产物契约**必须被单元测试核验（含「期望端口表 MUST 独立手写」这条反自证规则）② 独立 Verilog 仿真的覆盖义务（首批名单 + 名单登记 + 落选理由）③ TB 断言质量义务（非空 + 与 C++ 侧同语义）④ 运行护栏义务（缺失 = 失败）⑤ CI 分级与缓存义务 |
| `dual-backend-test-coverage` | MODIFIED | 追加「**独立于 C++ 仿真器**的那条腿」作为覆盖面的第三个维度（interpreter / verilator 后端 / **独立 RTL 仿真**）⇒ ⇒ 既有判据**不删** |
| `example-assertion-quality` | MODIFIED | 追加「RTL 侧 TB 的断言质量」面（现有分析器只覆盖 C++ 侧）⇒ ⇒ 既有条款**不删** |
| `example-runtime-coverage` | **不动**（引用） | 本项是它的**第三条腿**（独立 RTL 仿真）⇒ ⇒ 既有语义**不删** |

---

## Impact

### 爆炸半径

| 面 | 影响 |
|---|---|
| `tests/` | 新增 `test_rtl_emit_contract.cpp` ⇒ ⇒ `CTEST_TOTAL` +1 |
| `examples/*/` | 新增 TB 源文件 ⇒ ⇒ **`EXAMPLES_TEST_SOURCES` 口径待确认**（TB 是 `.sv` 不是 `.cpp` ⇒ ⇒ 按现有口径「Catch2 用例宏 / `CATCH_` / `examples/**/tests/` 下的 .cpp」⇒ ⇒ `.sv` **不计入** ⇒ ⇒ **MUST 明确** ⇒ ⇒ 否则锚点口径含糊 ⇒ ⇒ 建议：**不计入**（它是 Verilog 源）⇒ ⇒ 但**另立**一个锚点 `EXAMPLES_RTL_TB_SOURCES` 记录 TB 数量 ⇒ ⇒ 防"TB 被删 ⇒ 护栏静默缩面" |
| `scripts/` | 新增 2 个脚本（`verify_examples_rtl_sim.sh` · `update_test_count_anchors.sh`）⇒ ⇒ **MUST 有 `--self-test`** |
| CI | 新增 job ⇒ ⇒ 墙钟 +≈1.5min（PR）⇒ ⇒ 登记 |
| 根 `AGENTS.md` | 锚点段更新 ⇒ ⇒ **CI anchor 注释块** MUST 同步（"Do not reformat"） |

### 已知风险

| 风险 | 缓解 |
|---|---|
| **"manifest == 产物"是同源自证** ⇒ ⇒ 护栏恒绿 | E1 强制**独立手写**期望端口表 ⇒ ⇒ 变异自证 |
| **TB 自己写错了** ⇒ ⇒ 恒绿 | TB 与 C++ 侧**同语义** + 两侧期望值一致 ⇒ ⇒ 不一致即红 |
| **TB 变成"只编译不跑"**（历史事故形态） | E5 运行护栏：缺失 = 失败 + 非 0 退出码即红 |
| **CI 变慢** | 分级（不进 `base`）+ 缓存 ⇒ ⇒ 墙钟登记 |
| **锚点口径含糊**（`.sv` 算不算） | E8 明确口径 + 另立 `EXAMPLES_RTL_TB_SOURCES` 锚点 |
| **首批 10 个变成"永远 10 个"** | 名单 + 落选理由**必须登记** ⇒ ⇒ 缺口可见 ⇒ ⇒ 扩展属后续立项 |

### 与其他 improvement 的关系

| 项 | 关系 |
|---|---|
| `rtl-emit-product-tooling-2026-11` | **强依赖**：CLI + manifest + 产物护栏是本项的地基 ⇒ ⇒ **它先做** |
| `port-naming-ratchet-2026-11` | 端口名可读 ⇒ ⇒ TB 才写得出来 ⇒ ⇒ **建议至少首批消化先做**（`rv32i_pipeline_top.h` 32 个端口）⇒ ⇒ 否则 TB 的端口名仍是 `top_unnamed_input_3` |
| `firmware-load-chain-2026-11` | RV32I example 的 TB 若要装固件 ⇒ ⇒ 依赖新的装载链路 ⇒ ⇒ 本项的 RV32I TB 可先用现有 `prog_*` 路径 |
| `memory-timing-contract-2026-11` | 若先改了 D-TCM 时序 ⇒ ⇒ RV32I TB 的 golden 需重算 ⇒ ⇒ 建议**本项在其后**（或与 RV32I TB 一起在时序项之后） |

## metis 第 1 轮 gap 分析与处置（2026-11-10）

> **轮次**：1（该 improvement 总计 1 轮）· **审查者**：metis（预规划 gap 分析）

### ✅ 已订正

| # | metis finding | 处置 |
|---|---|---|
| 1 | 🔴 **候选名不是 target 名**（`counter` → `counter_example` **和** `counter_simple_example`；`stream_fifo` → `fifo_stream_example`）⇒ ⇒ 原稿 10 行实为 **12–13 个 target** ⇒ 「首批 10 个」不可判定 | E4 追加：名单 MUST 用**确切 target 名**；权威清单 = `examples/CMakeLists.txt:34-76` 的 `CH_TRACKED_EXAMPLE_TARGETS` |
| 2 | 🔴 **9/10 候选没有 C++ 侧测试**（examples 的 Catch2 只在 `examples/riscv-mini/tests/`）⇒ ⇒ E3「两侧同语义」对它们**无 C++ 侧可比** | E3 改为「**两侧各自与外部期望源一致**」+ 期望值须有**出处登记** |
| 3 | 🔴 **两侧同源 ⇒ 不算独立证据**（与仓内「两后端一致不能当正确性证据」同型） | E3 追加明示；验收同步 |
| 4 | `counter.cpp:211-214` 自己打印「`(expected: 255)`」+「`may not fully work yet`」⇒ ⇒ 固化**已知可疑行为** | E3 追加：该类 demo **不得**入选 |
| 5 | 🔴 **E1⑥ 与依赖项 F6 完全同义** ⇒ ⇒ 判据归属不明 | E1 归并到依赖项 F1–F6；本项只保留依赖项**没有**的四条（跨进程确定性 / 产物非空+配平 / **改 manifest 名 ⇒ TB 编译失败** / **产物变异 ⇒ TB 必红** / **`$isunknown` X 检测**） |
| 6 | 「两次发射逐字节相同」若**同进程同图** ⇒ 恒等 ⇒ 判别力 ≈ 0 | E1 改为**跨进程** |
| 7 | 🔴 **自研 Verilog 静态分析器 = 复现历史失败模式**（`verify_example_assertion_quality.sh` 的 C 子集分析器在**第 4/5/6 轮对抗审稿**里 BLOCKER **全部**出在它身上） | E5 改为**运行时证据**（TB 打印 `RESULT: checks=<N> fails=0`）+ runner 断言 |
| 8 | 🔴 **CI 默认 job 是无 label 的 `ctest`**（`ci.yml:209`，`BUILD_VERILATOR=OFF`）⇒ ⇒ 注册 `rtl_sim` **照样被跑到**；且 `verify_example_test_wiring.sh` C3 会**直接跑** `build/examples/` 下的二进制 ⇒ ⇒ 同一 TB 跑三遍 | E6 改为「**脚本 + job**，MUST NOT 注册进 CTest」（照 `run_all_samples.sh` + `Samples Regression Guard` 先例） |
| 9 | 「加个 job」**不是免费**：新独立 job = **第二次完整 build**（≈ 5–8 min）⇒ ⇒ 原稿「≈1.5min」**低估** | E6 / 验收追加分项登记要求 |
| 10 | `pr-verilator-fast` 已是 **PR-only**（`ci.yml:1356`）⇒ ⇒ 「push 可选」= **push 不跑** ⇒ ⇒ main 永远无 RTL 仿真证据 | E6 追加为**待裁决点** |
| 11 | **墙钟硬约束**：`pr-verilator-fast` 预算 45 min · cold ≈ 37.8 min | E6 追加 |
| 12 | 「独立手写期望端口表」是**第三份转录** ⇒ ⇒ 只增加漂移面 | 验收改为「`verilator --json-only` 从**产物文件**抽端口 vs `io_type`」+ 每 DUT 一份 **golden 端口快照**（复用 R8 解析，不造第三种表格式） |
| 13 | 缺「按名驱动」的机械证据 / 缺 X 检测 / 缺产物非空判定 / 缺变异自证 | E1 追加四条 |
| 14 | E8 与既有护栏**重复判定**；`DUAL_BACKEND_PARITY_*` 的计数实现在 `scripts/lib/dual_backend_parity.py` ⇒ ⇒ 更新脚本若自己数 ⇒ 两套实现 | E8 改为「调既有护栏的 `--print-actual`（需先加该 flag）」+ 建议**删除** `--check` + 显式声明不覆盖 `CH4_ANCHOR` / `§Test counts` 表格等 |
| 15 | `.sv` TB 放进 `examples/**/tests/` 会被 C1 判成「必须被 CTest target 编译」 | 验收追加：TB 放 `examples/<x>/tb_<name>.sv` + 两个 examples 护栏各加一行「`.sv` 不在本护栏域」 |

### ⚠️ 待用户裁决

1. ✅ **已裁决（2026-11-10 · 用户拍板 = ⑨）**：**main 分支是否接受「无 RTL 仿真证据」** ⇒ ⇒ **CI 形态 = 挂 `pr-verilator-fast` 的一个 step**（**不**新建 job ⇒ ⇒ 无第二次完整 build · **不**注册 CTest 条目 · **不**加 `rtl_sim` label）⇒ ⇒ ⇒ ⇒ 后果 = main 分支**确实**无 RTL 仿真证据 ⇒ ⇒ ⇒ ⚠️ 该缺口 MUST 登记进 `.rddf/checks/arch-residual-registry.md`（⇒ ⇒ **ledger 侧属 architect 域** ⇒ ⇒ ⇒ 落点见 §E6 行）
2. ✅ **已裁决（2026-11-10 · 用户拍板）**：**首批名单** ⇒ ⇒ **target set 仍为 10 个**（按 `examples/CMakeLists.txt:34-76` 的 `CH_TRACKED_EXAMPLE_TARGETS` 取**确切 target 名**）⇒ ⇒ 但**收窄声明而非收窄目标**：**已验证 = 1**（`riscv-mini` 的 RV32I SoC）· **待补期望源 = 9** ⇒ ⇒ ⇒ **每一项 MUST 列出「缺什么」**（见 §E4′ 表）⇒ ⇒ ⇒ 缺项补齐前**不得**计入「已验证」⇒ ⇒ ⇒ 详见 §E4 行的「已裁决」说明

## critic 第 1 轮（plan, build）—— **REJECT**（随整体）· mergeable: no · scope_expeded: false

> **轮次**：1/2 · 审查者：critic

### ⚠️ SHOULD_FIX（已采纳）：⑨ nightly-only **放弃了几乎免费的 PR 证据**，且 metis#10 的事实陈述有误

| 事实 | 证据 |
|---|---|
| `pr-verilator-fast` **已经是 PR-only 且已构建 verilator**（`BUILD_VERILATOR: 'ON'`） | `.github/workflows/ci.yml:1353-1356,1377` |
| 它已跑 `ctest -L verilator_e2e` | 同上 `:1621` |
| ⇒ 把 RTL-sim 脚本挂为它的**一个 step**，增量成本 ≈ 0（verilator 已构建 + 缓存） | 同上 |
| `nightly-verilator` 的 cron **就是在 main 上跑** | 同上 `:1943-1945` |

⇒ ⇒ metis#10 的「main 分支将永远没有 RTL 仿真证据」**不成立**；而选 nightly-only 意味着 **codegen 回归可合并进 main 后最多 24h 才被发现**。
**处置**：改为 **挂 `pr-verilator-fast` 的一个 step**（非独立 job ⇒ 无第二次 build）；若最终仍选 nightly-only ⇒ **MUST 把「PR 阶段无 RTL 仿真证据」登记进 `.rddf/checks/arch-residual-registry.md`**。

### ⚠️ SHOULD_FIX（需用户裁决）：⑨「v1 = 1 个」**与用户已拍板的「首批 10 个」冲突**

intent 明载用户 2026-11-10 拍板「#3 **首批 10 个** example」；oracle 改为 v1=1。⇒ ⇒ **改用户已签的范围需回到用户**，不能由批次计划自行收窄。

### ⚠️ NIT（已采纳）：⑨ 的「三跑」理由**混淆了对象**

`verify_example_test_wiring.sh` C3（`:307-365`）跑的是 `build/examples/` 下的 **C++ 二进制**，**不是**新增的 `.sv` TB。结论（**不注册进 CTest**）仍成立（`ci.yml:207-209` 无 `-L` 过滤 + `BUILD_VERILATOR=OFF`），但**理由需订正**。
