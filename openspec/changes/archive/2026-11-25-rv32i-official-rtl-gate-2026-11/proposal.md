# Change: 官方套件的 **RTL 产物级**门禁（交付物级判据）+ 带固件产物的 lint 缺陷修复

> **Change 名**：`rv32i-official-rtl-gate-2026-11`
> **母 improvement**：**无独立草案** —— 本项由**用户直接发起**（2026-11-25 ·
> 「请实施上面 3 件」⇒ 指上一轮审查列出的 ① 门禁缺失 ② 通用 TB 缺失
> ③ 产物 lint 不干净）
> ⇒ ⇒ `.rddf/improvements/` **不新增**文件（builder 不越权起草 improvement 草案）。
> **日期**：2026-11-25 · builder
> **性质**：
> - 🔴 **含一处产物级缺陷修复**（`codegen_verilog.cpp` 的 mem 初值块宽度 ⇒ Verilator 告警）
> - 🟢 **含交付物级门禁**（官方套件 × RTL 产物）
> ⇒ ⇒ **零端口面变更**（53 端口契约表与 C6 快照**不动**）
> ⇒ ⇒ **零公开 API 变更**
> ⇒ ⇒ **零 CTest 条目变更**（`CTEST_TOTAL` **282 不变** ⇒ 见 design.md §4 的取舍）

---

## Why

### 1. 官方套件只在**解释器**后端跑 ⇒ ⇒ **交付物本身**没有权威判据

上一轮 change（`riscv-tests-gate-2026-11`）建立的核级门禁
（`examples/riscv-mini/tests/test_rv32i_official_suite.cpp` · 4 个 CTest 条目 · 40/40）
跑在 `Simulator`（**解释器**）上 ⇒ ⇒ 它判定的是「**本仓那份 C++**」的行为，
**不是**交付给用户的 **Verilog 产物**的行为。

⚠️ 仓内 `AGENTS.md` §ANTI-PATTERNS 已登记这条纪律：
**「两后端一致」不能当作正确性证据** —— 解释器与 Verilog codegen 由**同一份 C++**生成
⇒ ⇒ **同构缺陷两后端一致地错** ⇒ ⇒ 双后端 parity 对该族缺陷**结构性失明**。

⇒ ⇒ **codegen 侧的缺陷（端口映射 / 初值块 / 时序形状 / 宽度）在解释器侧永远看不见**
⇒ ⇒ 「官方套件全过」这一结论**不能外推到 RTL 产物**。

### 2. 实测已在 RTL 侧挖出**一个真缺陷**（解释器侧不可见）

把官方镜像装进 RTL 产物（`--emit-rtl --firmware <镜像目录>`）后实测：

```
%Warning-WIDTHEXPAND: top.v:2829:39: Operator ASSIGN expects 32 bits on the Assign RHS,
                       but Assign RHS's CONST '1'h0' generates 1 bits.
   2829 |             top_dtcm_dtcm_memory[__i] = 1'b0;
%Warning-WIDTHEXPAND: top.v:3243:39: ... top_itcm_itcm_memory[__i] = 1'b0;
%Error: Exiting due to 2 warning(s)
```

⇒ ⇒ **40/40 个带固件产物的 lint 全部红** ⇒ ⇒ 按仓内口径（`verilator --lint-only`
⇒ 退出码 0 + 零 `%Error`）**编译直接失败**（exit 1）⇒ ⇒ 只有加 `-Wno-fatal` 才能编过。
⇒ ⇒ 根因 = `src/codegen_verilog.cpp:1752` 发射 mem 初值清零块时写 `1'b0`
（32-bit 数组元素 ← 1-bit 常量）⇒ ⇒ 与 `codegen_verilog.cpp:1890` **同族**（C6 产物卫生）
—— 那处**已修**（`<W>'h0`）⇒ ⇒ 本处是**漏网**。

### 3. 盲区的成因（为什么仓内护栏一直没抓到）

| 护栏 | 它 lint 的产物形态 | 该形态有没有 TCM 初值块 | 结果 |
|---|---|---|---|
| `scripts/rv32i_rtl_sim_env.sh` | `--emit-rtl --out <dir> --emit-hygiene`（**默认固件**） | **没有**（默认固件不落 `init_data` ⇒ 该 for 循环不发射） | 绿 |
| `scripts/verify_examples_rtl_sim.sh` | 登记表里的 `target` / `hier` / `soc` 三种 | **没有** | 绿 |
| **本 change 补的面** | `--emit-rtl --firmware <镜像目录>`（**带固件初值**） | **有** | 🔴 **红** |

⇒ ⇒ 「**带固件初值的产物**」这条形态**从未被任何护栏 lint / 编译过**
⇒ ⇒ 它是**交付物的真实形态**（用户拿到的就是这份 `top.v`）⇒ ⇒ 缺口是实质性的。

### 4. 技术路径已验证（不是「能不能」，是「有没有门禁」）

实测（scratch 工件，非仓内）：官方 40 个镜像装进 RTL 产物 ⇒ ⇒ **40/40 PASS**
（`tohost` 全部读为 `0x1`）⇒ ⇒ 扁平与**层次化**两种形态均可（顶层端口表同源 · 53）⇒ ⇒
负向对照有判别力（手写镜像写 3 ⇒ 读到 3 ⇒ 判红）⇒ ⇒ **缺的不是能力，是机械判据**。

---

## What Changes

### 1. 修产物级缺陷：`mem` 初值清零块的宽度（`src/codegen_verilog.cpp`）

```
-            out << "            " << mem_name << "[__i] = 1'b0;\n";
+            out << "            " << mem_name << "[__i] = "
+                << node->data_width() << "'h0;\n";
```

⇒ ⇒ 与 `codegen_verilog.cpp:1890` 的**既有 C6 卫生修法同形**（宽度正确的零字面量）
⇒ ⇒ 语义不变（都是零值）⇒ ⇒ **零行为变化**（仿真语义等价）。

⚠️ **影响面**：该 for 循环**只在 `init_data` 非空时发射** ⇒ ⇒ 只影响
「**带固件初值**」的产物 ⇒ ⇒ 无固件产物**逐字节不变** ⇒ ⇒ 既有护栏
（`verify_rv32i_fw_init_rtl.sh` 判**端口面 + 两次发射确定性** · `verify_examples_rtl_sim.sh`
的 E7 是**缓存 key**）**均不撞**。

### 2. 通用 TB（`examples/riscv-mini/tb_rv32i_official.sv` · 新）

⚠️ **与既有 `tb_rv32i_soc.sv` 的分工**：
`tb_rv32i_soc.sv` = **固定激励**（`load_word` 写死 7 个字 · 24 条手推判据）⇒ ⇒ 跑不了官方镜像；
本 TB = **镜像无关**（镜像在**产物**的 `initial` 块里 ⇒ TB 不装载）⇒ ⇒ 只负责
「跑 + 判 `tohost` + 输出 `RESULT: checks=<N> fails=0`」。

⇒ ⇒ 判据 = **上游 env 的 `tohost` 协议**（`tohost == 1` ⇒ PASS）⇒ ⇒ 与核级门禁**同口径**
⇒ ⇒ 观测面 = 核的**既有**根端口（`top_obs_data_write_en` / `top_obs_data_addr` /
`top_obs_data_write_data`）⇒ ⇒ **零新增端口**。

### 3. 交付物级门禁（`scripts/verify_rv32i_official_rtl.sh` · 新）

流程（**逐镜像独立**）：发射（`--emit-rtl --firmware` + `--emit-hygiene`）⇒ ⇒
**lint**（退出码 0 + 零 `%Error`）⇒ ⇒ **编译**（仓内口径 flags）⇒ ⇒ **运行** ⇒ ⇒ 判 `tohost`。
⇒ ⇒ **lint 是门禁的一环**（不是可选步骤）⇒ ⇒ 它就是缺陷 ① 的**机械判据**。

⇒ ⇒ **负向对照**：手写镜像写 `3` ⇒ ⇒ 门禁 MUST 如实读到 `3`
⇒ ⇒ 「读到 1」⇒ ⇒ 门禁**红**（判定被吞）⇒ ⇒ 「读到 3」⇒ ⇒ 门禁**绿**（判定如实）
⇒ ⇒ 与核级门禁的负向对照**同语义**（`REQUIRE(tohost_value == 3)`）。

⇒ ⇒ **两种形态**：扁平（**全量 40**）+ 层次化（**抽样**，登记在脚本输出与文档）。

### 4. 负向对照镜像入库

`examples/riscv-mini/tests/riscv-tests/negative/`（**手写** · 4 个字）⇒ ⇒
在 `PROVENANCE.md` **显式登记**「**非官方** · 手写 · 仅作负向对照」⇒ ⇒
不让它混进「官方镜像」的单一真相（官方镜像仍只在 `images/`）。

### 5. CI 落点

| job | 事件 | 跑什么 | 理由 |
|---|---|---|---|
| `pr-verilator-fast` | `pull_request` | **抽样**（快） | PR 反馈要快 |
| `nightly-verilator` | `schedule` / `workflow_dispatch` | **全量 40** | 冷跑 ≈ 10 min（verilator 编译主导） |

⇒ ⇒ 门禁**只在这两个 job 里可达** ⇒ ⇒ 护栏可达性类型 = `gate-event-only`
⇒ ⇒ `.rddf/checks/guard-reachability.md` 由 `verify_guard_reachability.sh --render` **自动渲染**
（⇒ **禁手改**）。

---

## 验收标准

| # | 判据 | 验证命令 | 期望 |
|---|---|---|---|
| **AC1** | 产物 lint **干净**（带固件形态） | `bash scripts/verify_rv32i_official_rtl.sh --subset 2` | lint 步骤退出码 0 + 零 `%Error` |
| **AC2** | **官方 40 个在 RTL 上全过** | `bash scripts/verify_rv32i_official_rtl.sh --full` | 40/40 `tohost == 1` |
| **AC3** | **负向对照有判别力** | 同上（脚本内建负镜像） | 读到 `3` ⇒ 绿；读到 `1` ⇒ **红** |
| **AC4** | **判别力自证**（变异） | 把 TB 里的 `tohost` 地址改错 ⇒ ⇒ 门禁**必红** | 还原后复绿 |
| **AC5** | **无固件产物逐字节不变** | `git stash` 前后对比 `--emit-rtl --out` 产物 | 逐字节一致（修法只碰 `init_data` 非空的分支） |
| **AC6** | 既有护栏全绿 | `scripts/verify_*.sh` 全跑 | 全绿（**零**既有判据被撞） |
| **AC7** | 全量回归 | `ctest --test-dir build --output-on-failure -j$(nproc)` | **282/282**（`CTEST_TOTAL` 不变） |
| **AC8** | 护栏可达性一致 | `bash scripts/verify_guard_reachability.sh --check` | 绿（表由 `--render` 生成） |

---

## Out of Scope

- ❌ **综合（synthesis）** —— 本机**无 yosys** ⇒ ⇒ 综合面**未验证**、也无综合门禁
  ⇒ ⇒ 「可综合」**不是**本 change 的结论（登记为后续项）；
- ❌ **时序约束（SDC / 时钟定义）** —— 产物只有 RTL，无约束文件 ⇒ ⇒ 属「产品化」的另一层；
- ❌ **官方套件进 CTest** —— 40 个 × verilator 编译 ≈ 15 s/个 ⇒ ⇒ 单条目可落在 300 s 内，
  但 `verify_example_test_wiring.sh` 的 **C3 会实跑每个 examples 的 ctest 条目**
  ⇒ ⇒ 40 个条目会让该护栏从 ≈10 min 涨到 ≈20 min ⇒ ⇒ 故取**脚本形态**（详见 design.md §4）；
- ❌ **层次化形态的全量 40** —— 门禁跑**抽样**（登记在输出里）⇒ ⇒ 层次化全量是后续项；
- ❌ **改既有 RTL 护栏的判据口径** —— `verify_examples_rtl_sim.sh` 的 `kind` 是**封闭集合**
  （`{target, dut, soc, hier}` + 三个独立计数锚点）⇒ ⇒ 加 `kind=official` 会改守恒判据
  ⇒ ⇒ 故**另立脚本**，不动既有口径；
- ❌ **改上游 riscv-tests**（测试体 / env）—— 只用**已入库**的镜像。

## 用户可见变更

- **产物修复（用户可观测）**：带固件初值（`--emit-rtl --firmware <dir>` /
  `--emit-rtl --firmware-init`）的 Verilog 产物里，TCM 的初值清零块由 `1'b0` 改为
  **元素宽度**的零字面量（`32'h0`）⇒ ⇒ 这些产物**按仓内口径可编译通过**
  （此前 40/40 个产物 `verilator --lint-only` exit 1 ⇒ ⇒ 只有 `-Wno-fatal` 才能编过）。
  ⇒ ⇒ **语义零变化**（零值仍是零值）⇒ ⇒ **无固件产物逐字节不变**。
- **新增门禁**：`scripts/verify_rv32i_official_rtl.sh`（交付物级 · 官方套件 × RTL 产物）
  ⇒ ⇒ PR 跑抽样（`pr-verilator-fast`）· nightly 跑全量（`nightly-verilator`）。
- **新增 TB**：`examples/riscv-mini/tb_rv32i_official.sv`（镜像无关 · 判 `tohost`）。
- **无公开 API 变更** · **无端口面变更** · **无 CTest 条目变更**（`CTEST_TOTAL` 282 不变）。
