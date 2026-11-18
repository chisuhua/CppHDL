# tasks — `rtl-verification-single-source-2026-11`

> **实施记录**（builder）· 2026-11-17
> ⚠️ **设计不在本文件**：决策面 = `docs/adr/ADR-074-rtl-verification-single-source.md` ·
> `docs/adr/ADR-075-port-naming-prerequisite.md`；完整蓝图 =
> `docs/architecture/2026-11-17-rtl-verification-single-source.md`
> ⇒ ⇒ 本文件只记「**做了什么 / 落在哪 / 判据在哪**」⇒ ⇒ 不复制设计正文（防第二份真相）。

## 批次 A — 库层（`.verif` → IR → 两个执行后端）

| # | 任务 | 落点 | 判据 | 状态 |
|---|---|---|---|---|
| A1 | IR 类型 + API（`VerifOp` / `VerifProgram` / `parse_verif` / `run_verif` / `render_tb`） | `include/rtl_emit/verif.h` | — | ✅ |
| A2 | **扫描器**（生成期求值 ⇒ IR 里无表达式） | `src/rtl_emit/verif_scan.cpp` | AC10（`tests/test_verif_single_source.cpp`） | ✅ |
| A3 | **IR 解释器**（驱动 `Simulator`） | `src/rtl_emit/verif_run.cpp` | AC7 / AC8 / AC20 | ✅ |
| A4 | **Verilog 渲染器** | `src/rtl_emit/verif_render.cpp` | AC4 / AC5 / AC6 | ✅ |
| A5 | `Simulator::set_value_by_node_id()` / `get_value_by_node_id()`（未命中 ⇒ `false`/`nullopt`，**不读 0**） | `include/simulator.h` + `src/simulator.cpp` | AC21 | ✅ |
| A6 | **共享 TB 前段**（骨架与渲染式 TB 共用端口连线 / 时钟 / 复位 / 收尾） | `src/rtl_emit/tb_stub.cpp`（`write_tb_prelude` / `write_tb_reset_pulse` / `write_tb_epilogue`） | — | ✅ |
| A7 | CLI：`--emit-tb-verif` / `--tb-verif-out` + 机器可读 `VERIF-CHECKS:` | `include/rtl_emit/cli.h` + `src/rtl_emit/cli.cpp` | AC7 / AC22 | ✅ |

## 批次 B — examples/axi4 改造

| # | 任务 | 落点 | 判据 | 状态 |
|---|---|---|---|---|
| B1 | 顶层组件**上提**（消「双权威」） | `include/axi4/axi4_lite_example_top.h` · `include/axi4/axi_soc_example_top.h` | `tests/test_axi_lite_verilator_e2e.cpp` 不再复制 `AxiLiteTop` | ✅ |
| B2 | **32 个顶层端口具名**（`AxiLiteTop` 20 + `AxiSocTop` 12） | 同上 | AC2 · `scripts/verify_port_naming.sh`（豁免表 **1447 → 1415**） | ✅ |
| B3 | 验证源 `examples/axi4/tb_axi4_lite_example.verif`（7 场景 / 113 断言） | 同上 | AC7（三方等式） | ✅ |
| B4 | 生成式 TB 入仓（`examples/axi4/tb_axi4_lite_example.sv`） | 同上 | AC6（逐字节重现） | ✅ |
| B5 | 发射入口**薄化**（⚠️ **2026-11-18 订正**：原「`emit_rtl_soc.cpp` 压到某个小行数」的估算**作废** —— 实测 388 行 ⇒ ⇒ 目标改为**结构判据**：被下沉的符号在 example 侧**无定义**；见蓝图 §4.3.1 的逐符号切分表） | `include/rtl_emit/product_check.h` · `examples/axi4/tb_axi_soc_example.cpp`（⚠️ 原落点 `examples/axi4/emit_rtl*.cpp` **均已删除** · 2026-11-18） | AC1（`grep toVerilogHierarchical` 归零） | ✅ |
| B6 | SoC TB 改**具名**连线（此前是 `top_unnamed_input_N`） | `examples/axi4/tb_axi_soc_example.sv` | AC18 | ✅ |

## 批次 C — 护栏与登记

| # | 任务 | 落点 | 判据 | 状态 |
|---|---|---|---|---|
| C1 | 列号**从 `#COLUMNS:` 推导**（禁位置绑定） | `scripts/verify_examples_rtl_sim.sh` | AC11 | ✅ |
| C2 | ⑥ 由**下界**改**等式** + `.md` 的 `VERIF-CHECKS:` 为第一边 | 同上 + `.rddf/checks/examples-rtl-expectations.md` | AC7 | ✅ |
| C3 | ⑥' 生成物**新鲜度**（逐字节重生成） | 同上 | AC6 | ✅ |
| C4 | ⑥'' 期望源锚点 ↔ 验证源 scenario **双向 1:1** | 同上 + `.md` 的 `#### scenario:` × 7 | AC3 | ✅ |
| C5 | ⑥‴ TB **无 TODO 残留** + 场景数 ≥ 1 + 每行断言带 `check_id` | 同上 | AC5 | ✅ |
| C6 | ⑥⁗ **生成覆盖度三方一致**（清单 `generated` ↔ TB 标记 ↔ `.verif`） | 同上 + `.rddf/checks/examples-rtl-sim-targets.tsv` | AC12 | ✅ |
| C7 | ⑧'' TB **可判定为生成物** + ⑧⁗ 顶层端口身份 **1:1** | 同上 + `scripts/lib/verif_tb_shape_check.py` | AC2 / AC4 | ✅ |
| C8 | `--self-test` 投毒 8 → **14**（新增 ⑥'/⑥ 等式/⑥''/⑥⁗/⑧''/刻意失败注入） | 同上 | AC13 | ✅ |
| C9 | 计数锚点 `CTEST_TOTAL` 266 → **267** | `AGENTS.md` | AC15 | ✅ |
| C10 | 公开面登记（端口改名 + 新增 `Simulator` 访问器）+ CHANGELOG | `.rddf/checks/public-api-surface.md` + `docs/CHANGELOG.md` | `scripts/verify_public_api_visibility.sh` | ✅ |
| C11 | **顶层组件上提 ⇒ 「必需端口」登记表**同步（`AxiLiteTop` / `AxiSocTop` / `AxiLiteTestMaster` 的 `rst_n` 现在落在 `include/**` ⇒ ⇒ R2 要求登记） + 新增 `AxiLiteTestMaster` 的**直接**判别性用例 | `.rddf/checks/input-port-required-registry.md` + `tests/test_axi_lite_test_master.cpp` | `scripts/verify_input_port_required_registry.sh` | ✅ |
| C12 | `verify_rst_n_wiring.sh` 识别**新的 TB 级驱动形态**（`.verif` 验证源里的 `SET rst_n`） ⇒ ⇒ 否则「用验证源驱动复位」的用例会被判成「未驱动」 | `scripts/verify_rst_n_wiring.sh` | 负向对照：改掉 `.verif` 引用 ⇒ 5 处必红（见 D6） | ✅ |

## 批次 D — 判别力自证（判据「不是恒绿」）

| # | 变异 | 载体 | 期望 | 状态 |
|---|---|---|---|---|
| D1 | **M1** 删掉 AW 握手后的那一拍 ⇒ `wready` 判据转红 | 单元测试（源级） | `bringup_and_low_active_reset#1` | ✅ |
| D2 | **M2** 复位后少等一拍 ⇒ 读回值判据转红 | 单元测试（源级） | `bringup_and_low_active_reset#9` | ✅ |
| D3 | **M3** DUT 侧 `rdata` 延迟 2 拍 ⇒ 读回值判据转红 | 单元测试（DUT 级） | `aw_w_phase_split_and_addr_latch#12` | ✅ |
| D4 | **刻意失败注入** ⇒ 渲染式 TB 与解释器报**同一条** `check_id` | 护栏投毒 14 + 单元测试 | `aw_w_phase_split_and_addr_latch#8` | ✅ |
| D5 | 两后端 **check_id 集合相等** | 单元测试 | 113 == 113 | ✅ |
| D6 | `.verif` 驱动识别的**负向对照**（把 `.verif` 引用改坏 ⇒ 护栏必红） | 手工对照 | 5 处 `rst_n` 未驱动 ⇒ 红 | ✅ |

## ⚠️ 未做 / 本次不做（**如实登记**，交用户裁决）

| 项 | 原因 | 影响 |
|---|---|---|
| `examples/axi4/tb_axi_soc_example.sv` 仍是**手写** TB（清单 `generated=no`） | UART 8N1 帧解码需要「逐拍采样成数组 + 首个低电平定位 + 变长位偏移」⇒ ⇒ **封闭原语集**（ADR-074 D2）无此构造 ⇒ ⇒ 硬塞进 DSL 会把「UART 语义」漏进**通用**原语集 ⇒ ⇒ 不做 | 该单元的「单一语义来源」**尚未**达成 ⇒ ⇒ 其期望值仍登记在 `.rddf/checks/soc-rtl-expectations.md#soc-example`（手写 TB 的 `EXPECT-SRC` 不变） |
| 删 `examples/axi4/emit_rtl.cpp`（蓝图 §4.3 的「并入 example 的 `--emit-rtl` 模式」）✅ **已由 change `emit-cli-into-example-2026-11` 执行**（2026-11-18） | 清单的 `emitter` 列**按 target 名**寻址 ⇒ ⇒ 删掉它 ⇒ ⇒ 护栏找不到发射器 ⇒ ⇒ 需要**另加一列**（本次不做的成本 > 收益） | 发射入口仍有两个 target（**各自 40 行薄壳**）⇒ ⇒ 「碎片化」已消除（重复实现没了），但**文件数**未减 |
| 其余 9 个清单行的 TB 迁移到 `.verif` | 本 change 只改造 `examples/axi4/` ⇒ ⇒ 其余 9 行 `generated=no` + 具体原因 | 「生成覆盖度」当前 = **1 / 10**（axi4_lite）⇒ ⇒ 其余属**后续改造范围** |
| `docs/developer_guide/**` · `docs/usage_guide/**` 的同步 | ADR-074 **明确不覆盖**（蓝图 §6 Step 6） | 用户侧文档尚未讲 `.verif` |
| `perf_regression` 的 **6 条 verilator 行**红（TC-07 / TC-08） | **既有问题**（与本次改动无关）⇒ ⇒ 证据：在**纯净 HEAD** 的独立 worktree 上跑同一用例 ⇒ ⇒ 同样红（比值 5.6× / 19.3×，同型）⇒ ⇒ 根因 = 基线录于 **2026-10-28**（sha `0758fa0`）⇒ ⇒ 已陈旧于本机环境 | ⇒ ⇒ **AC14 只判 `ctest -LE perf` 绿**（perf 标签被排除）⇒ ⇒ 不阻塞本 change；但该基线**陈旧**这件事本身**应当单独立项**（perf 标签目前无判据） |
