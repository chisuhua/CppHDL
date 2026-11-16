# Tasks: SoC 层的 RTL 产物形态 + SoC 层 TB 的交付契约（`soc-rtl-product-and-tb-2026-11`）

> **来源**：`openspec/changes/soc-rtl-product-and-tb-2026-11/proposal.md` 的 task 表（**逐条转写，不新增任何 task**）。
> **架构依据**：`docs/adr/ADR-073-soc-rtl-product-and-tb.md`（Accepted v1.1）。
> **执行顺序**：T0 → T1 → T2 → T3 → T4 → T5 / T7 → T6 → T8–T12 → archive → Purpose 手写。
> ⚠️ **T4 必须早于 T5 / T7 的验收**（AC2 要求「冻结期望集写进期望源」⇒ 依赖 T4）。
> ⚠️ **计数口径**：本文件共 **15** 个 checkbox = **13 个产品 task**（T0–T12 · proposal 的 task 表逐条转写）+ **2 个生命周期项**（**T-0** 执行载体 = 本文件本身 · **T13** 归档与 Purpose 手写）⇒ ⇒ proposal 的 DoD「13 个 task」指前者。

## 0. 执行前置

- [ ] **T-0** **补齐执行载体**：本 change 的 `tasks.md`（本文件）存在 ⇒ `openspec list` 的 task 列不再是 "No tasks" ⇒ ⇒ `scripts/verify_openspec_delta.sh` 的 **R4**（tasks.md 的 bash 块语法可达）从「零信号假绿」恢复为有效判据
      - 判据：`openspec list` 显示 13 个 task · `bash scripts/verify_openspec_delta.sh` 4/4 绿且 R4 **有输出**
      ```bash
      openspec list
      bash scripts/verify_openspec_delta.sh
      ```

## 1. 产物形态与可发射性

- [x] **T0** 🔴 **后代输入驱动源（可发射性前置）** —— `examples/axi4/axi_soc_example.cpp`<br>      - ✅ **已完成（2026-11-16）**：31 根全接（11 tie-off + 19 单向 + 1 透传）· 编译零告警 · example 绿（`GPIO_OUT=0xab` · `DATA_IN=0x5a`）· **变异自证**：还原源码 ⇒ 未驱动 39（后代 31）⇒ 修复后 8（**后代 0**）
      - `matrix.m1_*` **11** 根 tie-off：5 根握手（`awvalid`/`wvalid`/`bready`/`arvalid`/`rready`）仿 `examples/axi4/phase3a_complete.cpp:259-263` + **6 根 payload**（`awaddr`/`awprot`/`wdata`/`wstrb`/`araddr`/`arprot`）用**带位宽常量**
      - `matrix.s1_*` ↔ `uart` **19** 根**单向**接线（请求侧 11：矩阵 ch_out → uart ch_in；响应侧 8：uart ch_out → 矩阵 ch_in）
      - `master.rst_n` **1** 根透传（`master.io().rst_n <<= io().rst_n`；⚠️ `AxiLiteTestMaster::describe()` **不消费** `rst_n` ⇒ 纯发射前置，无行为影响）
      - 合计 **31** 全覆盖
      - 判据：① 编译通过且**本文件零告警** ② **未驱动端口中 `owner != "top"` 的条目数 == 0**（`build_rtl_manifest` 派生 ⇒ 变异体 = 31，修复后 = 0）③ `axi_soc_example` 仍绿（`GPIO_OUT == 0xab` · `DATA_IN == 0x5a`）
      ```bash
      cmake --build build --target axi_soc_example -j$(nproc)
      ./build/examples/axi4/axi_soc_example
      ```
- [x] **T1** **成层声明**：4 个 `ch_module` → `ch_layer<T>`（**档 ③**）<br>      - ✅ **已完成（2026-11-16）**：产物 **5 个 module**（`AxiLiteTestMaster` / `AxiLiteMatrix2x2_32_32_` / `AxiLiteGpio_8_` / `AxiLiteUart_16_` / `top`）· 顶层端口 **14** · **扁平产物逐字节不变**（md5 相同 ⇒ `ch_layer` 只影响层次化发射）⇒ ⇒ 模块类型名**一次定死**：`"AxiLiteTestMaster"` / `"AxiLiteMatrix2x2<32,32>"` / `"AxiLiteGpio<8>"` / `"AxiLiteUart<16>"` ⇒ ⇒ 产物标识符 `AxiLiteTestMaster` / `AxiLiteMatrix2x2_32_32_` / `AxiLiteGpio_8_` / `AxiLiteUart_16_`
      - 依赖：T0
      - 判据：编译通过（module 数判据在 T3 落地）
      ```bash
      cmake --build build --target axi_soc_example -j$(nproc)
      ```
- [x] **T2** **发射入口**：新建 `examples/axi4/emit_rtl_soc.cpp` + CMake target `emit_rtl_axi_soc`<br>      - ✅ **已完成（2026-11-16）**：EXIT 0 · manifest 161 条 · **诊断 0 条** · fail-closed（诊断 ⇒ 退出码 1 · `--allow-diagnostics` 放行）· `--out` / `--emit-hygiene` / `--allow-diagnostics` / `--check-products`
      - 复用 example 的**同一份定义**（照 `examples/axi4/emit_rtl.cpp:22-24` 的先例）：`#define main cpphdl_example_main_disabled` + `#include "axi_soc_example.cpp"` + `#undef main` ⇒ ⇒ **不抽头文件 · example 源码零结构改动**
      - ⚠️ **不用** `run_emit_cli`（**只发扁平**：`src/rtl_emit/cli.cpp:177-187`）⇒ ⇒ 自己解析 `--out` / `--emit-hygiene` ⇒ ⇒ 调 `toVerilogHierarchical(out+"/top.v", *root, opts)` + `build_rtl_manifest(*root, ctx)`
      - ⚠️ **`result.ok == false` MUST 映射为非零退出**（否则护栏读到残留文件 ⇒ 假绿）
      - 依赖：T0 · T1
      - 判据：AC1 正臂（发射成功 + `origin == Bubbled` 计数 0）+ **失败路径非零退出**
      ```bash
      cmake --build build --target emit_rtl_axi_soc -j$(nproc)
      build/examples/axi4/emit_rtl_axi_soc --out build/rtl_products/soc
      ```
- [x] **T3** ✅ **产物级判据（载体 = examples 侧自检子命令 · 2026-11-16 用户裁决「选 (a)」）**<br>      - ✅ **① ② ③ 已完成（2026-11-16）**：`CHECK 1 module 集 PASS (5/5)` · `CHECK 2 顶层端口 PASS (14/14 · 后代未驱动 0)` · `CHECK 3 lint PASS` ⇒ `PRODUCTS-CHECK: PASS` · EXIT 0<br>      - ✅ **变异自证**：把 `uart` 的 `ch_layer` 改回 `ch_module` ⇒ **CHECK 1 FAIL (5→4) + CHECK 2 FAIL (14→24)** ⇒ 双红 ⇒ 判据非恒绿<br>      - ⚠️ **④ 双形态等价待 T7**（当前输出 `CHECK 4 双形态等价: TODO (T7)`）<br>      - ⚠️ **判据措辞订正（feedback-20261116-001）**：「零后代冒泡」（不是「零 `Bubbled`」）—— 根自有的 8 个顶层输入**本就被标为 `Bubbled`** ⇒ ⇒ `CHECK 2` 判「owner != 根的未驱动端口 == 0」
      - **载体 = `emit_rtl_axi_soc --check-products`**（与 T2 同一 target ⇒ ⇒ **不碰 `tests/`** ⇒ 不触发 `verify_no_duplicate_rv32i_headers.sh` 的 **C3**（`:998`）/ **C7**（`:170-196`）⇒ ⇒ 无需复刻 fixture ⇒ 无双权威）
      - **4 项 MUST 逐条输出并以非零退出**：① 产物 module 数 == **成层 IP 数 + 1**（5）② 顶层端口数 == **14** 且 `origin ∈ {Self, ClockReset}` ③ `verilator --lint-only --top-module top` **exit 0** · 零 `%Error` ④ 双形态逐拍等价（`ch::hier_eq::emit` + `compare`，`ports` **由产物派生**）
      - ⚠️ **产物目录 MUST 独立**（如 `build/rtl_products/soc/`）—— 主产物目录受 `scripts/verify_hierarchical_rtl.sh:94-97` 的「**恰好 3 个 `.v`**」约束
      - ⚠️ **AC4 的 lint 执行者 = `scripts/verify_examples_rtl_sim.sh` 的 kind=soc 专用判据**（与 T6 同批）
      - 依赖：T2
      - 判据：4 项逐条 PASS；**负向对照**：改回 `ch_module` ⇒ module 数必减 ⇒ 必红
      ```bash
      build/examples/axi4/emit_rtl_axi_soc --check-products --out build/rtl_products/soc
      ```
- [ ] **T7** **双形态一致性（J-4）** —— 载体随 T3 落在 examples 侧（`--check-products` 第 ④ 项）
      - `ch::hier_eq::emit` + `compare`（`ports` **由产物派生**）· 激励 **≥10 拍** · 观测集 {`rdata`, `gpio_out`, `uart_tx`}（⚠️ **不含 `done`** —— 它**粘滞**）
      - ⚠️ **这只是 parity，不是正确性** ⇒ 正确性锚点唯一地压在 AC6 的手推断言上
      - 依赖：T3 · T4
      - 判据：逐拍全等 + 变异自证（篡改层次化产物的一条 `assign` ⇒ 必红）
      ```bash
      build/examples/axi4/emit_rtl_axi_soc --check-products --out build/rtl_products/soc
      ```

## 2. 期望源与独立 TB

- [x] **T4** **期望源登记**：`.rddf/checks/soc-rtl-expectations.md`（端口映射 + 地址映射 + 事务序列 + **手推结果** + **冻结的模块名期望集**）<br>      - ✅ **已完成（2026-11-16）**：§1 端口映射（14 端口 · 产物名 ↔ C++ 成员 ↔ 语义 · **位宽序列唯一确定** ⇒ 不是猜编号）· §2 冻结 module 集（5）· §3 地址映射（matrix 2×2 默认窗口 + GPIO/UART 寄存器表）· §4 事务序列 + **手推结果**（含事务延迟推导 ≤4 拍）· §5 TB 输入常量 · §6 判别力自证 · §7 已知盲区（`uart_rx` 反向 8 根未覆盖 / DECERR 不可观测 / `timer_irq` 死端口）
      - 依赖：T1（模块名须先定死）
      - 判据：文件在册且可 grep；`EXPECT-SRC:` 指针指向它
      ```bash
      test -f .rddf/checks/soc-rtl-expectations.md
      ```
- [x] **T5** **独立 TB**：`examples/axi4/tb_axi_soc_example.sv`（**14** 个顶层端口 + **4** 个可观测场景）<br>      - ✅ **已完成（2026-11-16）**：`RESULT: checks=14 fails=0` · EXIT 0 · 14 端口全连（`timer_irq`=0 · `uart_rx`=1 · `gpio_in`=0x5A）<br>      - 断言分布：① `gpio_out==0xA5` · ② `rdata==0x0000_005A` · ③ UART 8N1 帧（起始位 + **8 个数据位逐位** + 停止位 = 10 条）· ④ `gpio_out==0` + `uart_tx` 空闲高<br>      - ✅ **变异自证**：切断 `matrix.s1_wdata → uart.wdata`（DUT 侧）⇒ **fails=4** ⇒ TB 真的在验证 uart↔matrix 的数据通路<br>      - ⚠️ **实测修正**：采样窗 MUST **从写脉冲起算**（matrix 的 m0→s1 路由是**组合**的 ⇒ 帧紧贴写脉冲启动 ⇒ 先等 16 拍再采会丢掉起始位）<br>      - ⚠️ 帧定位用「**先逐拍采样成数组 → 找第一个低电平**」（**不用**下降沿检测 —— 实测那有竞态，不同编译结果不稳定）
      - ⚠️ **14 个端口 MUST 全部连接**（否则严格 flags 下 `%Warning-PINMISSING` **致命** —— 仓内实证 `examples/axi4/tb_axi4_lite_example.sv:33-34`）⇒ 其中 **`timer_irq`（零消费死端口）接 `0`** · **`uart_rx` 接 `1`**（否则 RX 引擎把低电平当起始位）· **`gpio_in` 给固定值**
      - 场景：① 写 GPIO DATA_OUT ⇒ `gpio_out` == 手推值 ② 读 GPIO DATA_IN ⇒ `rdata` == `gpio_in` ③ 写 UART TX_DATA ⇒ `uart_tx` 出现 8N1 帧（UART 场景需 **≥160 拍** = 10 bit × `UART_BIT_PERIOD` = 16）④ `rst_n` 拉低后读 GPIO DATA_OUT == 0
      - ⚠️ 完成判定 MUST 用「按已知延迟采样」（**禁**等 `done` 上升沿）
      - 依赖：T0 · T4
      - 判据：`RESULT: checks=<N> fails=0`（N ≥ 4）· 失败时**非零退出** · **变异自证**：改坏一个期望值 ⇒ 必红
      ```bash
      verilator --lint-only --top-module top examples/axi4/tb_axi_soc_example.sv
      ```

## 3. 覆盖登记与护栏

- [ ] **T6** **覆盖登记 + 护栏**：tsv +1 行 `kind=soc`（`target` = `axi_soc_example`）+ 锚点 `# REGISTERED_SOC: 1` + 护栏 ⓪ **三桶**判定 + **kind=soc 专用判据**（跑 `--check-products` ⇒ = AC4 的执行者）+ `EXPECT-SRC:` **锚点存在性**判据
      - ⚠️ **共用工件**（与 `rtl-productization-surface-2026-11` 的 T5/T13 串行）⇒ ⇒ **本 change 先落**
      - 依赖：T3 · T5
      - 判据：`bash scripts/verify_examples_rtl_sim.sh`（含 `--self-test`）绿 · `kind ∈ {target,dut}` 行数 == `REGISTERED_TARGETS` · `kind=soc` 行数 == `REGISTERED_SOC` · 三桶之和 == 数据行总数
      ```bash
      bash scripts/verify_examples_rtl_sim.sh
      ```

## 4. 簿记与文档

- [ ] **T8** **事实漂移订正（四处同批）**：`examples/axi4/axi_soc_example.cpp:4` 源码头注释 + `:24` 零使用 include + `examples/axi4/AGENTS.md` OVERVIEW 表 + `.rddf/checks/examples-rtl-sim-exclusions.md:36`（SoC TB 交付后该行 **MUST 移除**）
      - 判据：AC10 的四条断言逐条可判
      ```bash
      grep -c "Timer" examples/axi4/axi_soc_example.cpp examples/axi4/AGENTS.md
      grep -c "axi_soc_example" .rddf/checks/examples-rtl-sim-exclusions.md
      ```
- [ ] **T9** **残留登记**：`uart.irq` + 根的 `timer_irq` ⇒ ⇒ 台账 §7 + `.rddf/checks/arch-residual-registry.md` **两侧同批**
      - ⚠️ **architect 域**（`docs/architecture/`）⇒ ⇒ 由 architect 执行，builder 只验收
      - 判据：`bash scripts/verify_arch_ledger_residuals.sh` PASS（**行数由台账派生 · 禁写死**）
      ```bash
      bash scripts/verify_arch_ledger_residuals.sh
      ```
- [ ] **T10** **账本锚点与全局校验**：① `AGENTS.md` 的 `CTEST_TOTAL` / `VERILATOR_E2E_TOTAL`（若带标签；锚点值**从实测派生**，可用 `scripts/update_test_count_anchors.sh`）② **顺序无回归复核**：`git diff --name-only` **不含** `src/**` / `include/**` + `ctest -LE perf` 全绿
      - 判据：`bash scripts/verify_test_counts.sh` 绿 · `verify_dual_backend_parity.sh` 绿
      ```bash
      bash scripts/verify_test_counts.sh
      ```
- [ ] **T11** **文档同步**：① `docs/usage_guide/05-component-design.md` 的「SoC 装配」小节（产物怎么接线 · 端口怎么找 · TB 怎么搭）② `examples/axi4/AGENTS.md` 的 FILES 表 ③ 🔴 **根 `AGENTS.md:196` 的 capability 计数** —— ⚠️ **实测当前已是 48**（该处写 47 ⇒ 本身已陈旧）⇒ ⇒ 归档后应为 **49**
      - 判据：「SoC 装配」小节可 grep 到；计数订正到位
      ```bash
      grep -c "SoC 装配" docs/usage_guide/05-component-design.md
      ```
- [ ] **T12** ✅ **能力面**：`rtl-soc-output` 的 ADDED Requirements —— ⚠️ **delta 已落**（G-1 已闭）⇒ ⇒ 本 task = **校验并保持未漂**
      - ⚠️ **两处措辞义务**：① 正文 **MUST NOT** 指称已退役符号（`UndrivenPolicy::Bubble` / `Bubbled`）——「冒泡端口」只可作为**禁止态措辞** ② **`## Purpose` 归档后手工写**（`openspec archive` **不合并** delta 的 Purpose ⇒ 新 capability 会留 `TBD - created by archiving …` 占位符）
      - 判据：`openspec validate soc-rtl-product-and-tb-2026-11` = valid · `bash scripts/verify_openspec_delta.sh` 4/4 绿
      ```bash
      openspec validate soc-rtl-product-and-tb-2026-11
      bash scripts/verify_openspec_delta.sh
      ```

## 5. 归档与收口

- [ ] **T13** **archive** ⇒ ⇒ 归档后手写 `openspec/specs/rtl-soc-output/spec.md` 的 `## Purpose`（**非占位符**）⇒ ⇒ 清理 `.rddf/checks/examples-rtl-sim-exclusions.md` 的 SoC 行（已由 T8 移除）
      - 判据：`grep -c "TBD - created by archiving" openspec/specs/rtl-soc-output/spec.md` == 0
      ```bash
      openspec archive soc-rtl-product-and-tb-2026-11
      ```
