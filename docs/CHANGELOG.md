# CppHDL Changelog

User-visible changes to the CppHDL HDL library. Entries are grouped by
the change that introduced them (see `openspec/changes/` and
`openspec/specs/`).

---

## 2026-11-11 — B2 批（E2 测试台骨架 + E8 锚点单一更新入口）

### Added
- **`--emit-tb-stub`**（`examples-rtl-sim-coverage` **E2**）：`tools/cpphdl-emit-rtl`
  额外产出 `<out>/tb_<module>_stub.sv` —— **只发骨架**：
  端口清单注释块（来自 manifest）· 端口信号（输入 tie-off）· DUT 实例化 ·
  时钟/复位驱动 · VCD 开关 · **留空的激励/断言位**（`// TODO:`）。
  ⚠️ **为什么只发骨架**：激励与期望值是**领域知识**（UART 波特率 / AXI 握手 /
  PWM 占空比表）⇒ ⇒ 自动生成只能产出「编译通过但什么都没验」的 TB。
  新增 `include/rtl_emit/tb_stub.h` + `src/rtl_emit/tb_stub.cpp`（库侧，可复用）。
- **收尾约定**：补完的 TB MUST 打印 `RESULT: checks=<N> fails=0`
  —— 运行护栏（E5）按该行判红绿（**不靠退出码**：`$finish` 恒返回 0）。
- `scripts/update_test_count_anchors.sh`（**E8**）：**一次命令同步 5 个计数锚点**
  （`CTEST_TOTAL` / `VERILATOR_E2E_TOTAL` / `DUAL_BACKEND_PARITY_TOTAL` /
  `DUAL_BACKEND_PARITY_PAIRS` / `EXAMPLES_TEST_SOURCES`）· `--check`（CI 用）·
  `--dry-run`。⚠️ **只同步、不推导** —— 锚点本身是「静默缩面」的防线，必须留在
  文件里可 review；计数一律**复用既有脚本的口径**（不重算，避免第二套实现）。

### 口径（实测发现，已在代码注释登记）
- **manifest 的 `ports[]` ⊋ 产物的顶层端口表**：manifest 收全部
  `type_input`/`type_output`（含**子组件的有 driver input**）⇒ ⇒ 后者在扁平化后是
  **内部 wire**。TB 骨架因此需要过滤：
  `role ∈ {clock,reset}` ∪ `dir == output` ∪ `is_top_io`
  （与 F3 的端口契约对齐）。⚠️ 不过滤 ⇒ Verilator 报
  `Instance attempts to connect to 'X', but it is a variable`（实测踩中）。
  ⚠️ `openspec/specs/rtl-emit-portability/spec.md` **未规定** `ports[]` 的范围
  ⇒ ⇒ 「manifest == 产物端口表」这条判据**当前不成立**（B2 的 E1 需据此调整）。

### 测试
- `tests/test_rtl_tb_stub.cpp`（**22 断言 / 5 用例**）：只连顶层端口（内部 wire
  不得连线）· 骨架不含实际断言 · 收尾 `RESULT` 行 · 时钟/VCD 骨架 · 确定性（无时间戳）
- **判别力自证**：去掉顶层端口过滤 ⇒ 判据 1 转红 ✓（还原后重建复绿）

---

## 2026-11-11 — C6 返工（B1 验收 BLOCKER-1 + 4 项判据子项）

> 来源：`.rddf/reviews/b1-batch-verify-2026-11.md`（B1 验收 **不通过**）⇒ 修完再复审。

### Fixed
- **BLOCKER-1：剪枝误剪「仍被引用」的字面量 ⇒ 卫生产物无法编译**
  （实测：RV32I **扁平 + 层次化双双** `Can't find definition of variable`，exit=1；
  最小复现 = **一句 `select()`**）。
  **根因**：剪枝曾按「使用者**类型**」一刀切（假设 `op`/`mux`/`bitsupdate` 都内联常量）
  ⇒ ⇒ 实际 `print_mux` **按名字引用**、`print_binary_op` **只内联 RHS**
  ⇒ ⇒ 前提是假的。
  **修法**：新增 `is_literal_referenced_by(user, lit)` —— **逐使用者、逐位置**
  判定「文本里会不会出现这个名字」，与各 `print_*` 的实现对齐
  （`binary` 的 rhs · `concat` 两侧 · `rotate`/`bit_sel`/`bits_extract` 的 rhs ·
  `mux`/`bitsupdate` 不内联 · 不发射的 `op` 路径）。
- **剪枝的三条「非图边引用」保护**（每一条都是实测踩出来的）：
  ① 模块端口；② `body_epilogue`（跨 module 实例连接）+ **`port_drives_node` 的目标名**
  （驱动重定向）；③ **端口 `assign` 的终点 driver**（`print_logic` 的 walk 终点）。
- **walk 逻辑统一**：`print_logic` 的 output / input 分支与剪枝原先**各写一份**
  ⇒ ⇒ 三处**不一致**（input 只穿透未命名 proxy · output 穿透全部 · 重定向 proxy 停在自身）
  ⇒ ⇒ 抽出 `port_driver_endpoint()`，**三处共用** ⇒ 从此不会漂移。
- **子组件输入端口可剪**：扁平化后它是**内部 wire**（不是模块端口）⇒ ⇒ 无消费者时
  其声明与驱动 `assign` 都是死代码（实测 RV32I 最后 6 个零引用全是这一类）。

### Changed（判据补齐 · 全部仅在 `emit_hygiene` 开启时生效）
- **C6-③ 复位写法规范化**：`if (!reset) q <= next; else q <= INIT;`
  ⇒ ⇒ `if (reset) q <= INIT; else q <= next;`（逐拍语义等价，分支互换）
  · 两处：`emit_always_ff` 与 `print_reg` 的「无 next」分支。
- **C6-④ `lint_off` 收窄**：卫生模式下**不发**任何 blanket `lint_off`
  ⇒ ⇒ 用**显式 size cast**（`N'(expr)`）把隐式的宽度扩展/截断写成意图
  （新增 `hygiene_cast()`，接入 `print_binary_op` / `print_proxy` /
  `print_logic` 的 output+input 分支 / mem 读口复位值）
  ⇒ ⇒ **RV32I 的 23 条宽度告警归零**（19 `WIDTHEXPAND` + 4 `WIDTHTRUNC`）。
- **C6-① 补 `default_nettype wire`**：`` `default_nettype `` 是**编译指令**
  ⇒ ⇒ 宏状态**跨文件延续** ⇒ ⇒ 不恢复会让后续 include 的文件里所有隐式线网报错
  ⇒ ⇒ 文件末尾恢复（**每文件一次**，与头部 `none` 配对）。

### 护栏（C5）
- **F1 改为「两段式基线」且靶改为卫生产物**：段① 原样 lint（基线）· 段② 剥掉
  `lint_off` 后仍绿。⚠️ 原 F1 跑**默认产物**，其隐含前提「工具设计恰好无宽度告警」
  已破 —— 而**默认产物在真实设计上必然有 23 条宽度告警**（那正是 blanket 要盖的东西）
  ⇒ ⇒ 「剥掉 blanket 后干净」对默认产物是**不可满足**的判据。
- **F7 的 M4 从空操作改为真判定**（旧实现 `if 绿; then : # 只记录; fi` ⇒ 两条分支都不失败）
  ⇒ ⇒ 投毒 = 用 `lint_off` **盖住一条真实宽度告警** ⇒ 剥掉后 F1-b **必红**。
- **F8 新增「无未声明引用」**（`--undeclared-refs` · **不依赖 verilator**）
  ⇒ ⇒ BLOCKER-1 的**直接**判据，与 lint 门互为兜底。
- 工具自带设计加入 **mux 选字面量**形态 ⇒ ⇒ F8 成为 BLOCKER-1 的**常驻回归靶**。

### 验证
- **RV32I 卫生产物（扁平 + 层次化）**：零引用 **0** · 恒等直通 **0** ·
  **未声明引用 0** · verilator 告警 **0** · `--lint-only` **exit=0**
  （返工前：扁平/层次化均 `exit=1`，≥12 处 `Can't find definition`）
- **默认路径逐字节不变**（扁平 + 层次化的 RV32I 产物与 B1 前基线 `diff` 为空）
- **行为等价**：同一 testbench 驱动 `flat_std.v` 与 `flat_hyg.v`
  ⇒ ⇒ **200 拍 × 107 个输出端口逐拍完全一致**
- **判别力自证（3 条变异各自转红，还原后重建复绿）**：
  M1 恢复「mux 内联」⇒ 用例 5 红 · M2 去掉链上 proxy 种子 ⇒ 用例 6 红 ·
  M3 不发 `default_nettype wire` ⇒ 用例 7 红
- `tests/test_rtl_emit_hygiene.cpp` **38 断言 / 7 用例**

---

## 2026-11-11 — C6 产物卫生（`emit_hygiene` 开关）

### Added
- **`EmitOptions::emit_hygiene` / `HierarchyEmitOptions::emit_hygiene`** —— 产物卫生开关
  （`rtl-emit-product-tooling` **C6**）⇒ ⇒ **默认 `false`** ⇒ ⇒ **默认路径产物逐字节不变**
  （实测：RV32I SoC 的层次化产物与改动前**逐字节相同**）。
  ⚠️ **为什么 MUST 默认 off**：「层次化产物逐字节不变」这条 AC **只在**该开关默认 off 时成立。
  CLI 侧：`tools/cpphdl-emit-rtl --emit-hygiene`。
- 开启后做四件事（**全部可判别**，C5 的 **F8** 判据）：
  1. **`` `default_nettype none ``** + **确定性**元数据头
     （⚠️ **时间只进 manifest** ⇒ ⇒ 产物**不含时间戳** ⇒ ⇒ 仍然确定性）；
  2. **不发 blanket `lint_off`** ⇒ ⇒ 产物必须**自身** lint 干净
     （那两条 `WIDTHEXPAND` / `WIDTHTRUNC` 豁免不再靠一条全局注释盖住）；
  3. **剪零引用信号**（**不动点**：`dead(N) ⟺ 纯组合类型 ∧ N 的全部使用者也 dead`）
     ⇒ ⇒ 一趟做不到（实测：层次化路径一趟剩 **156** 个）⇒ ⇒ 不动点后扁平产物**归零**；
     ⚠️ 含**字面量内联**特例（`print_op` 把常量内联成 `5'h0` ⇒ 图里「有使用者」
     但**文本里是死的** ⇒ ⇒ 用「users 全死」规则永远剪不掉这类）；
  4. **proxy 恒等直通**（只被转发的 proxy 与零引用信号一并剪掉）。
  ⚠️ **两条保护 MUST NOT 剪**：① **模块端口**（外部可观测面）；
  ② 出现在**实例连接文本**（`.port(net)`）里的名字 —— 那是**跨 module** 的连接
  ⇒ ⇒ 图里**看不到这条边** ⇒ ⇒ 剪掉会**静默断线**（本项有专门的判别性用例）。

### Changed
- **无**（默认路径零变化 ⇒ ⇒ 既有 golden / 快照 / Verilator 缓存全部不受影响）。

### 已知残留
- ~~层次化产物仍有 12 个零引用 `ch_in`~~ ⇒ ⇒ **已于同日返工中消除**（见上方
  「C6 返工」条目：子组件输入端口在扁平化后是内部 wire ⇒ 无消费者即可剪；
  模块端口由 `view_.ports` 保护，不受影响）。

---

## 2026-11-11 — B1 批（端口命名地基 · N1 / N2 / N3 / P1 / C4 / C1 / C3 / C5）

### Added
- **`tools/cpphdl-emit-rtl`** —— **库 + 薄 CLI** 的 RTL 发射器（C1）。
  ⚠️ **形态**：`ch::rtl_emit::run_emit_cli()` 是**库**（`include/rtl_emit/cli.h`）⇒ ⇒
  用户在自己的可执行文件里**登记**自己的设计 ⇒ ⇒ 退出码 / 失败语义**全仓一致**；
  本仓的二进制只是**一个使用者**（+ 自测载体）⇒ ⇒ **MUST NOT** 链 `examples/**`。
  ⚠️ **退出码（fail-closed）**：**0** 成功 · **1** 运行失败或**存在诊断** · **2** 用法错误
  ⇒ ⇒ **任何异常都不得返回 0**。
- **`include/rtl_emit/manifest.h`** —— 端口清单（manifest）的**参考实现**
  （capability `rtl-emit-portability` 的 schema `rtl-emit-manifest-v1`）：
  10 个字段 + `diagnostics[]` + JSON 序列化 ⇒ ⇒ **端口按名字排序** ⇒ ⇒ **确定性**。
- **`scripts/verify_port_naming.sh`** —— 端口命名的**棘轮**护栏（N2）：
  product 侧（`include/**` + `examples/**`）逐端口判定 · 存量 **1484** 条登记在
  `.rddf/checks/port-naming-exempt.tsv` ⇒ ⇒ **债只许减不许增**。
  ⚠️ P1 额外判「**标识符 == 显式名字串**」⇒ ⇒ 框架**无反射** ⇒ ⇒ 运行期唯一存活的
  是**显式命名串** ⇒ ⇒ manifest 的 `cpp_member` 只能是它。
- **`scripts/verify_flat_rtl_emit.sh`** —— 扁平产物的**产物级**护栏（C5）：
  F1 剥掉 blanket `lint_off` 后重 lint · F2 确定性 · F3 顶层端口集 == 清单 ·
  F4 无悬空顶层输入 · F5 无自赋值死逻辑 · F6 backend 产物 == 产品产物（**当前不适用**）。

### Changed
- **显式命名的端口**的 Verilog 名**绕开** eval-list 序计数器（N3）⇒ ⇒ 端口名
  **对图的无关改动稳定** ⇒ ⇒ 「按名引用」的外部资产（TB / wrapper / 文档）不再**静默失配**。
  ⚠️ **零 churn**：现有端口**全部未具名** ⇒ ⇒ 产物**逐字节不变**（实测）。
  ⚠️ 冲突判定是**端口作用域**的局部判定（点名报告 + 退回计数器拿唯一名）⇒ ⇒
  **不**拒绝整份设计。

### Fixed
- ⚠️ **端口名冲突的误报**（N3 实施期实测）：输出端口的框架默认名是
  **`unnamed_output`**（不是 `unnamed_input`）⇒ ⇒ 第一版判据漏了它 ⇒ ⇒ 多个
  `unnamed_output` 被误判成「显式名」⇒ ⇒ 日志刷出假 ERROR ⇒ ⇒ 被
  `verify_example_test_wiring.sh` 的「跑但报错 ≠ 通过」判据抓到 ⇒ ⇒ 已修。

---

## 2026-11-11 — `rtl-emit-manifest-schema-2026-11`（B1 批 · C4 · 纯规格）

### Added
- **端口清单（manifest）schema `rtl-emit-manifest-v1`** —— 新 capability `rtl-emit-portability`
  的第一条 Requirement（change `rtl-emit-manifest-schema-2026-11`）。
  字段（10 个，零缺失）：`name` / `dir` / `width` / `role`（`clock`·`reset`·`data`）/
  `driven` / `effective` / `owner` / `is_top_io` / `cpp_member` / `semantic`（v1 恒 `null`）。
  - ⚠️ **`driven`（**节点级**「是否被消费」）与 `effective`（**功能级**「是否产生可观测行为」）
    是两个独立字段** —— 「有连线但无功能」（标准样例 = `ext_clk`：接到 `pipeline.io().clk`，
    而流水线内对 `io().clk` 零消费者）这一类**单布尔表达不了**。
  - ⚠️ `is_top_io` 的筛选是**消费方的选择**，MUST NOT 成为发射器的**默认行为**。
  - **库代码零改动** ⇒ ⇒ **端口面、端口顺序、命名规则一字未改**。

### Changed
- **无**（本 change 只新增规范，不改任何既有 Requirement）。

---

## 2026-11-11 — `rv32i-dead-component-consolidation`（B0 批）

### Removed
- **`riscv::ForwardingUnit`**（`include/cpu/pipeline/rv32i_forwarding.h`）——
  零交付路径消费者 + 独有语义为空集（WB 档无覆盖 · `id_reg_read` 死输入）。
  改用仓内权威的 `riscv::HazardUnit`（已接入真实流水线）+ `chlib::ForwardingMux`。
- **`riscv::HazardDetectionUnit`**（`include/cpu/pipeline/hazard_detection_unit.h`）——
  零交付路径消费者 + 其 MEM 级 load-use 检测点在流水线里**永不触发**。改用 `riscv::HazardUnit`。
- **`riscv::HazardUnitComplete`**（`include/cpu/pipeline/rv32i_hazard_complete.h`）——
  零交付路径消费者 + 2 个死输入 + 3 档语义；唯一存在理由是 parity 夹具。改用 `riscv::HazardUnit`。

> ⚠️ **这是显式批准的覆盖缩面**（用户 2026-11-11 批准）：
> `DUAL_BACKEND_PARITY_PAIRS` 290→284 · `DUAL_BACKEND_PARITY_TOTAL` 66→65 ·
> `VERILATOR_E2E_TOTAL` 77→76 · `CTEST_TOTAL` 257（增删相抵）。

### Changed
- **`ch::build_hierarchical_name()` / `ch::Component::name()` 的子路径拼接不再按父路径
  的「字面值」剥离路径段**（`node-name-prefix-consistency` P1）。唯一受影响的情形：
  根组件的 name 为空（归一成 `"unnamed"`）⇒ ⇒ 其子组件的 `name()` 从裸局部名
  （`"kid"`）变为带父前缀（`"unnamed.kid"`）⇒ ⇒ **按 `name()` 精确匹配裸局部名的下游
  代码 MUST 更新**；常规路径（父名非空）**逐字节不变**。
  ⚠️ **已登记代价**：空名与用户真名 `"unnamed"` 在 `name()` 上**永久不可区分**
  ⇒ ⇒ 彻底解决须另立 item。
- **顶层 RTL 端口表（RV32I SoC）：顶层 input 8 → 7**（`rv32i-dead-component-consolidation` C8）。
  `mem_is_load` 的装配点补上 driver ⇒ ⇒ 它不再是「**无 driver 的** input」⇒ ⇒ 按契约
  不再出现在顶层端口面。**行为零变化**（该路在流水线里永不触发）。
  下游若按端口**数量/序号**硬编码 ⇒ ⇒ MUST 改为**按名**取。

### Fixed
- **`riscv::Rv32iAlu` 的 SLTU 是无符号比较**（此前 `bits<31,31>(sub_result)` 取的是
  **结果位**而非**借位位**）⇒ ⇒ `0 <u 0xFFFFFFFF` 曾得 **0**（期望 1）。
- **`sanitize_verilog_name` 的关键字表补全为 IEEE 1800-2017 保留字全集（246 词 ·
  verilator 实测）**（`port-naming-ratchet` N3）。此前 `bit` / `time` / `event` / `table`
  等**合法 C++ 成员名会产出非法 Verilog**（`logic bit;` ⇒ 语法错误）。
  **实测对现网产物零影响**（8585 个标识符中 0 个与关键字完全相等）。

---

## 2026-10-15 — `verilator-hier-input-flattening-fix`

### Fixed
- **`VerilatorBackend` now propagates driven submodule `ch_in` ports
  end-to-end.** `codegen_verilog.cpp::print_header` previously flattened
  driven submodule `ch_in`s (anything with `inputimpl::driver() != nullptr`,
  e.g. `sub.io().x <<= some_wire`) to top-level Verilog `input` ports that
  nothing drove. The matching `print_logic` switch had no `type_input`
  branch, so there was no `assign` to feed the driver value into the
  Vtop field. Verilator initialised those ports to X (=0 after the
  `initialize()` reset pulse), silently disabling every `ch_in` below
  the top component. After the fix:
  - `print_header` filters out driven inputs from the top-port list.
  - `print_decl` declares them as internal `logic` (via `emit_signal_decl`).
  - `print_logic` adds a `type_input` branch that walks the unnamed-proxy
    chain and emits `assign <in> = <driver>;` (mirroring the `type_output`
    branch).
  - `VerilatorBackend::emit_sim_main_postlude` skips them in
    `set_input_Vtop` / `get_field_ptr_Vtop` (the Vtop field does not
    exist), and `build_port_access_table` does not add them to
    `port_access_` so `sync_inputs_to_vtop` will not try to write them.
  JIT already handled this correctly (`src/jit/jit_ir_gen_lnode_io.cpp:31-46`).
  This brings the Verilog codegen path into parity with JIT semantics.

### Resolved (was a known limitation of `verilator-issue-25-fix`)
- The `verilator-issue-25-fix` entry below mentioned that
  "RVALID / RDATA readback via `data_map_` returns 0" and was tracked
  under issue #25 / option A in the Oracle audit. That symptom was
  actually the same hierarchical-input bug, not a separate issue #25
  register-propagation problem: in `test_axi_lite_verilator_e2e.cpp`,
  `slave.io().awaddr <<= io().awaddr` etc. flatten `slave.io().awaddr`
  to a top-level `input` port the testbench never drives, which then
  propagates through to `rvalid`/`rdata` via the `always_ff` busy
  latch. With the codegen fix, the AXI4-Lite WriteReadReg0ThroughBackend
  fixture now asserts `rvalid == 1 && rdata == 0xDEADBEEF` as a real
  REQUIRE, and the RV32I 3-instruction parity fixture runs
  `perf_instructions == 12` on both interpreter and verilator (was 0).

### AGENTS.md
- New VERILATOR BACKEND RULE: hierarchical input driver MUST be emitted
  by codegen. The four-site contract is now enforced (filter, declare,
  assign, backend skip). Reflects the actual root cause of the
  previously-tracked symptom.

---

## 2026-09-23 — `verilator-issue-25-fix`

### Fixed
- **`VerilatorBackend::eval_sequential` performs a 2-step clock toggle** so
  Verilator's `VlClockSig`/`__Vclklast__` edge detection fires
  `always_ff @(posedge clk)` blocks. The clock Vtop field is written
  directly via the new `clock_field_ptr_` (NOT round-tripped through
  `data_map_`); `sync_inputs_to_vtop(exclude_clock=true)` skips clock/reset
  on the input pass, and `sync_outputs_from_vtop()` skips clock/reset on
  the output pass (they are infrastructure, never user-visible). Before
  the fix, `always_ff` blocks never fired — counter-style regressions
  read 0 even though `eval()` was called, AXI4-Lite `awready`/`wready`/
  `bvalid` combinational handshakes did not propagate to `data_map_`, and
  sequential `rdata` reads returned 0. After the fix, the e2e tier
  (`test_verilator_e2e_harness`, `test_axi_lite_verilator_e2e`) passes with
  `REQUIRE` assertions, and TC-07 depth=1000 verilator runs at
  ~580 ticks/sec (was ~34 ticks/sec in the broken baseline). Also fixed
  `emit_sim_main_postlude` (`src/core/verilator_backend.cpp`) to use the
  codegen's `get_verilog_name(node)` instead of `cpp_safe_name(node->name())`
  — the codegen applies a `_N` uniqueness suffix that `lnode->name()`
  does not carry, and without the fix the generated accessor code had
  `&v->top.slave.unnamed_input` (with literal dots) which verilator
  rejected as a hard compile error.

---

## 2026-09-22 — `verilator-perf-three-way`

### Fixed
- **`get_literal_str` masks literal value to target width.**
  `src/codegen_verilog.cpp::verilogwriter::get_literal_str` (single-arg
  overload, `src/codegen_verilog.cpp:142-166`) now masks `value` to
  `width` bits before emitting the SystemVerilog `<width>'h<hex>` literal.
  Previously, the function trusted `lit.bv_.size()` (the reported width)
  but emitted the raw `lit.bv_.words_[0]`, which `bitvector::operator=`
  does not mask (`include/bv/bv_assign.h:60-70`). Constructs like
  `ch_uint<8>(999)` therefore reported `width=8` while carrying 999, and
  the codegen emitted `8'h3e7` — a hard SystemVerilog syntax error
  (`Too many digits for N bit number`). Affected the three-way perf
  benchmark TC-07 (XOR chain) at `depth=1000`, where hundreds of
  intermediate `ch_uint<8>` literals produced 744 verilator errors and
  broke the Verilator compile. After the fix, TC-07 depth=1000 compiles
  cleanly and the Verilator row reports `34.54 ticks/sec` (was 0.00).
  `width >= 64` path is preserved (`ADR-035` wide-signal rejection
  caps port widths at 64; any wider literal would have been rejected
  upstream).

### Added
- **`perf_three_way` ctest entry** for CI-friendly three-way perf
  smoke tests. Runs TC-07 (XOR chain depth=10/100/1000) and TC-08
  (register chain) via `perf_tests --tc=07 --tc=08 --report=json` in a
  subprocess-isolated execution context (F2 perf subprocess isolation).
  TIMEOUT 360s, LABELS `perf;verilator`. The full `--all` perf suite
  remains available via the `perf_tests` ctest entry (TIMEOUT 1800).
  See `tests/benchmark/CMakeLists.txt:103-128`.

### Test coverage
- **`tests/test_verilator_three_way.cpp`** (391 lines, 12 test cases
  / 28 assertions, all PASS) covers:
  - Boundary values: `ch_uint<8>(1)` → `8'h1`, `ch_uint<8>(0)` → `8'h0`,
    `ch_uint<8>(255)` → `8'hff`, `ch_uint<8>(256)` → `8'h0` (mask drops
    high bit), `ch_uint<8>(999)` → `8'he7` (the regression case).
  - Multiple widths: 8, 16, 32, 64.
  - `print_concat` path (concatenation RHS literal).
  - `zext<32>` path (zero-extend output literal).
  - Random fuzz: 100 trials of random `(width, value)` pairs asserting
    no `'<width>'h<overflow-hex>` literal is emitted.
  - TC-07 end-to-end codegen for depth=10/100/1000.
  - Real `verilator --lint-only` integration (skipped if verilator not
    on PATH).

---

## 2026-09-21 — `adr-035-phase-3-complete`

See `openspec/specs/verilator-backend-e2e/spec.md` and
`openspec/specs/jit-llvm-compat-tests/spec.md` for the archived
specifications.

---

## 2026-09-22 — `axi4lite-verilator-e2e` (Gap F2 closure)

### Added
- **`tests/test_axi_lite_verilator_e2e.cpp`** (268 lines, 2 test cases /
  9 assertions) closes Gap F2 from the `verilator-perf-three-way`
  review: `examples/axi4/axi4_lite_example.cpp` runs only through the
  interpreter — no VerilatorBackend e2e coverage existed for any AXI
  class. Two test cases:
  - `AxiLiteTopPortAccessSnapshot`: VerilatorBackend compiles an
    `AxiLiteTop` fixture (wraps `AxiLiteSlave<32,32,4>` via
    `ch::ch_module`), dlopens `libVtop.so`, and verifies
    `port_access_` is populated with all 17 AXI4-Lite IO signals
    (10 inputs + 7 outputs + clock + reset).
  - `WriteReadReg0ThroughBackend`: dispatches through Simulator +
    VerilatorBackend, performs a write transaction (AW + W →
    `reg0 = 0xDEADBEEF`) and a read transaction (AR → R), and logs
    `RVALID` / `RDATA` for nightly inspection.

### Known limitations carried forward (issue #25)
- ~~**`RVALID` / `RDATA` readback via `data_map_` returns 0 even though
  the generated Verilog has pure `assign top_slave_unnamed_output =
  top_slave_mux_select_1;` (combinational).** Originally Oracle
  audit (C12) suggested this would propagate; in practice it does
  not, indicating issue #25 extends beyond `always_ff` reg updates to
  the **entire `sync_outputs_from_vtop` path**. Strong evidence that
  the VerilatorBackend eval-after-input-sync sequence does not
  refresh wire outputs. Resolution tracked under
  `verilator-issue-25-fix` (proposed next change, option A in the
  Oracle audit).~~ **Resolved 2026-10-15 by
  `verilator-hier-input-flattening-fix`**: the symptom was actually the
  hierarchical-input flattening bug (driven submodule `ch_in` ports
  became top-level input ports that nothing drove). Once the codegen
  emits `assign` for those ports, `RVALID` / `RDATA` propagate normally
  through the existing `sync_outputs_from_vtop` path. No changes to
  the sync machinery were needed.
- ~~Test case 2 uses `INFO(...)` only for the readback values; no
  `CHECK` is asserted on `RVALID` / `RDATA` until the underlying
  Verilator eval-eval-sync issue is fixed.~~ **Resolved 2026-10-15**:
  `test_axi_lite_verilator_e2e.cpp::WriteReadReg0ThroughBackend` now
  asserts `REQUIRE(rvalid == 1)` + `REQUIRE(rdata == 0xDEADBEEF)`.

### Test invocation
- Tagged `[verilator][e2e][axi4lite][m5]`. PR matrix (no verilator
  binary) SKIPs both cases via the standard `tool_available("verilator")`
  gate. Nightly Verilator CI (`.github/workflows/ci.yml`) runs them
  for real.

### Files
- `tests/test_axi_lite_verilator_e2e.cpp` (new)
- `tests/CMakeLists.txt` (+5)

---

## 2026-11-09 — 用户可见变更载体上线（`public-api-remove-visibility`）+ `chlib-dtor-composability-2026-11`（含 2026-10-15 以来的一次性回填）

> ⚠️ **本条是「载体机制」上线**（issue #28 的处置）：本仓的用户可见变更此前**只**
> 记录到 **2026-10-15** ⇒ ⇒ 而此后 **122** 个 change 已归档（截至 2026-11-09 实测 ⇒
> `ls openspec/changes/archive | awk 'substr($0,1,10)>="2026-10-16"' | wc -l`）⇒ ⇒ **破坏性变更对
> 下游不可见**（真实后果：下游 ChipForge 在 2026-11-08 才发现
> `ch_state_machine::is_built()` 已被移除 ⇒ ⇒ 编译失败 ⇒ ⇒ 上游 issue #28）。
> ⇒ ⇒ **机制**（R1）：**每个归档 change MUST 携带一行「用户可见变更」**
> ⇒ ⇒ 载体 = 归档件的 `## 用户可见变更` 段 + 本文件（变更级摘要）
> ⇒ ⇒ **符号级**明细 = `.rddf/checks/public-api-surface.md` 的
> 「已移除 / 已改名的公开符号」一节。

### Removed（**破坏性** · 跨仓下游 MUST 迁移）

- **`chlib::ch_state_machine` 的 15 个死 / 误导 API**（ADR-050 D2/D3 · `c7b0b28`）：
  `is_built()` · `on_exit` / `exit_action` · `on_entry` / `entry_action` ·
  `then()` / `next_state_on_exit()` / `has_exit_transition()` · `state_changed` ·
  `tick_state()` · `CH_STATE_MACHINE` / `CH_STATE` / `CH_STATE_MACHINE_BUILD` 宏 ·
  `state_def::parent`（+ `friend`）。
  ⇒ ⇒ **改名**：`current_state()` → **`entry_state()`**（旧名返回**编译期** entry
  ⇒ ⇒ 运行时状态用 `current_state_uint()` / `is_in()`）。
  ⇒ ⇒ **符号级迁移指引** = `.rddf/checks/public-api-surface.md` 的移除登记表。
- **`get_lnode` 的「无约束通用入口」②**（ADR-053 决策 C）：改用 bundle 自带的
  `impl()`（约束偏序下**恒胜** ⇒ ⇒ 爆炸半径 0）。
- **`include/cpu/cache/*_stub.h`**（pass-through stub 族 · ADR-037 · 2026-10-20）：
  改用 `i_cache_{sync,async}.h` / `d_cache_{sync,async}.h`。
- **`bundle_concat` / `bundle_slice_view`**（ADR-043 · 2026-10-22）：两者当时均为
  **静默失效**（前者字段不在位布局里、后者根本不切片）⇒ 改用 bundle **字段直连**
  （ADR-042 之后字段是一等节点）。迁移指引见 `.rddf/checks/public-api-surface.md`。
- **`conditional_reg_assign`** → **改名** `conditional_assign`（ADR-051 · 2026-10-30）：
  旧名暗示「只对寄存器」，而输出端口同样需要条件化。
- **cache wrapper 的死数据 / 死配置面**（ZERO-DEBT · phase-6 **P9** · 2026-11-09）：
  `include/cpu/cache/d_cache_write_buffer.h` 的 `WriteBufferEntry` / `WriteBufferConfig`，
  `include/cpu/cache/i_cache_prefetch.h` 的 `prefetch_data` / `prefetch_addr` /
  `prefetch_valid` / `PrefetchConfig` ⇒ ⇒ **零消费者**的死数据 ⇒ ⇒ 下游若引用 MUST **自建**；
  ⇒ ⇒ 两个 wrapper 的 `__io` 签名与**可观测行为不变** ⇒ ⇒ **真 FIFO 写缓冲 / 真预取 = 独立立项**。
  迁移指引见 `.rddf/checks/public-api-surface.md`。

### Added

- **`chlib::ch_state_machine` 的「可组合形态」**（ADR-067 · `chlib-dtor-composability-2026-11`）：
  新增第三模板实参 `chlib::dtor_detection`（默认 `automatic` ⇒ **既有用户零改动**）
  ⇒ ⇒ `dtor_detection::explicit_only` 的析构为 `noexcept` ⇒ ⇒ **该类型可作
  「有虚析构的类」的按值成员**（修掉下游的编译失败）⇒ ⇒ 检出改走
  **显式 `check_unresolved()`（可捕获）+ 析构期终止兜底（fail-closed）**。
- **`chlib::ch_state_machine::contrib()` / `resolve_hold()` / `resolve_or()`**
  （ADR-050 D4）：状态守卫贡献 API（`include/chlib/state_machine.h`）。

### Deprecated（**尚未移除**）

- `ch_bool` / `port<ch_bool>` 上的 `&&` / `||`（ADR-036）⇒ ⇒ 改用 `&` / `|`；
  ⚠️ **移除窗口 = 日历锁：最早 2027-04-17**。

### Changed（契约层）

- `chlib::conditional_block` 的析构 MUST 为 `noexcept(false)`
  （ADR-051 · `chlib-conditional-semantics`）⇒ ⇒ **「析构非 `noexcept` 的类型」
  MUST NOT 作为「有虚析构的类」的按值成员或基类**（ADR-067 决策 ④）。
- `chlib::ch_state_machine` 的析构期检出 MUST 可捕获（ADR-050 D5）⇒ ⇒ **默认形态
  逐字保留**；「可组合形态」的检出 MUST 是 **fail-closed** 的（ADR-067 决策 ②）。
- **stream 组合子的 ready 语义**（下游若依赖旧行为 MUST 复核）：`stream_demux` 的 ready
  方向改为**消费端 ⇒ 上游**（ADR-056 · 2026-11-01，与 `stream_mux` 对称）· `stream_fork`
  同步语义为 `input.ready = AND(各输出 ready)`，`synchronous=false` **响亮拒绝**
  （ADR-057 · 2026-11-01）· 组合子的 ready **所有权**形态 = ADR-054（2026-10-31 修订）。
- **`stream_width_adapter` 重写**（ADR-064 · 2026-11-03）：三族 adapter 的语义与
  命名对齐（详见 `.rddf/checks/chlib-naming-honesty-registry.md`）。
- **节点命名 / 归属策略**（ADR-059 · 2026-11-02）：生成的 Verilog 中**子模块端口名**
  与节点归属按「构造作用域」修正 ⇒ ⇒ 依赖**旧生成名**的下游脚本需要复核。

---

## Format

Entries are reverse-chronological. Each entry may have sections:
**Added**, **Changed**, **Deprecated**, **Removed**, **Fixed**,
**Security**. The opening line of each entry is the date and the
OpenSpec change name (or external release tag if no change is in
flight).
