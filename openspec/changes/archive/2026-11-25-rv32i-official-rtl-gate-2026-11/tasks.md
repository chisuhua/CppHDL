# tasks · 官方套件的 RTL 产物级门禁

> **Change**：`rv32i-official-rtl-gate-2026-11`
> **纪律**：TDD 5 步（**先见红**）· 每步小迭代 · 每步跑验证 · 提交**单独成段**
> ⇒ ⇒ **零端口面变更**（53 端口契约表 + C6 快照**不动**）
> ⇒ ⇒ **零公开 API 变更**
> ⇒ ⇒ **零 CTest 条目变更**（`CTEST_TOTAL` **282 不变**）
> ⇒ ⇒ **零既有护栏判据口径改动**

---

## T0 · 先见红（复现缺陷 + 建立判据骨架）

- [x] **T0.1** 复现：带固件产物（`--emit-rtl --firmware <镜像目录>`）的 lint
      ⇒ ⇒ `WIDTHEXPAND` × 2 ⇒ ⇒ `verilator --lint-only` **退出码 1**（仓内口径 ⇒ 红）
      ⇒ ⇒ 证据 = `top.v:2829` / `top.v:3243`（mem 初值清零块）
- [x] **T0.2** 复现：同产物按仓内口径编译（**无** `-Wno-fatal`）⇒ ⇒ **退出码 1**
      ⇒ ⇒ 即「带固件产物**当前不可用**」
- [x] **T0.3** 摸清**覆盖分析**：既有 3 个护栏 lint 的产物形态**都没有** TCM 初值块
      ⇒ ⇒ 缺口精确形状 = 「带固件初值」这条形态从未被 lint（design.md §2）
- [x] **T0.4** 写 scratch TB（临时 · 不入库）+ scratch 批量脚本
      ⇒ ⇒ 实测 **40/40 PASS**（`tohost` 全 `0x1`）⇒ ⇒ 技术路径成立
      ⇒ ⇒ 实测 **`tohost` 写入拍数 = 41 ~ 1552**（最坏 `ld_st`）⇒ ⇒ 拍数上限 4000 的
      余量 **≈ 61%**（design.md §3.3 · ⚠️ 口径 = `tohost` 写入拍数，**不是**
      `dbg_id_valid` 的拍数）

## T1 · 缺陷修复（`src/codegen_verilog.cpp`）

- [x] **T1.1** `print_mem_init` 的清零块改写**目标宽度**的零字面量
      （`node->data_width()` ⇒ `<W>'h0`）⇒ ⇒ 与 `:1890` 的既有 C6 卫生修法**同形**
- [x] **T1.2** 注释写明：根因 · 与 `:1890` 同族 · **不**做 `emit_hygiene` 分支的理由
- [x] **T1.3** **零行为变化**：`<W>'h0` 与 `1'b0` 语义等价
- [x] **T1.4** **影响面收窄**：该 for 循环只在 `init_data` 非空时发射
      ⇒ ⇒ **无固件产物逐字节不变**（AC5）

## T2 · 通用 TB（`examples/riscv-mini/tb_rv32i_official.sv` · 新）

- [x] **T2.1** 镜像无关（镜像在产物 `initial` 块 ⇒ TB 不装载）
- [x] **T2.2** 复位序列 = 既有 `tb_rv32i_soc.sv` 同序列（`rst=1` + `ext_rst=1` 保持 4 拍 ⇒ 释放）
- [x] **T2.3** 8 根 `axil_*` 输入 **tie 0**（核级无 MMIO 从设备 ⇒ 与 C++ 侧 `tie_axil_inputs` 逐项同值）
- [x] **T2.4** 判据 = **上游 env 的 `tohost` 协议**（`tohost == 1` ⇒ PASS）
      ⇒ ⇒ 观测面 = 既有根端口（`top_obs_data_write_en` / `_addr` / `_write_data`）⇒ **零新增端口**
- [x] **T2.5** 输出 `RESULT: checks=<N> fails=<M>`（与仓内 RTL 护栏的协议行一致）
- [x] **T2.6** **不写 `timescale`** ⇒ ⇒ 不触发 `TIMESCALEMOD`（免污染「产物 lint 是否干净」的判定）

## T3 · 门禁脚本（`scripts/verify_rv32i_official_rtl.sh` · 新）

- [x] **T3.1** **fail-closed** 前置：verilator（仓内 `build/verilator-install/bin/verilator` 优先）
      · 发射器 · TB · 镜像目录 ⇒ ⇒ 缺任一项 ⇒ exit 2（**不**静默跳过）
- [x] **T3.2** 逐镜像：发射（`--emit-rtl --firmware` + `--emit-hygiene`）⇒ ⇒ **lint**
      ⇒ ⇒ **编译**（仓内口径 flags）⇒ ⇒ **运行** ⇒ ⇒ 判 `tohost`
- [x] **T3.3** **lint 是门禁的一环**（不是可选步骤）⇒ ⇒ 它是 T1 的机械判据
- [x] **T3.4** **负向对照**：手写镜像写 `3` ⇒ MUST 如实读到 `3`
      ⇒ ⇒ 读到 `1` ⇒ **红**（判定被吞）⇒ ⇒ 读到 `3` ⇒ **绿**
- [x] **T3.5** **层次化抽样**（与扁平同判据）⇒ ⇒ 抽样名单与命中数打印出来
- [x] **T3.6** `--full` / `--subset N` ⇒ ⇒ PR 跑抽样 · nightly 跑全量
- [x] **T3.7** 每镜像记录实际拍数 ⇒ ⇒ 余量被吃掉时**显式红**（不是静默截断）

## T4 · 资产与登记

- [x] **T4.1** 负向对照镜像入库 `examples/riscv-mini/tests/riscv-tests/negative/`
      （**手写** · 4 个字 · 与核级负向对照**逐字节同源**）
- [x] **T4.2** `PROVENANCE.md` 登记该镜像「**非官方** · 手写 · 仅作负向对照」
      ⇒ ⇒ 不混入「官方镜像」的单一真相
- [x] **T4.3** `examples/riscv-mini/AGENTS.md` 同步：TB 清单 · RUN 段 · 「完成」定义的**覆盖面**
      （核级 + 交付物级**两层**）
- [x] **T4.4** `examples/riscv-mini/RUNNING-ELF.md` §8.6 更新 R2/R3 落地状态

## T5 · CI 落点

- [x] **T5.1** `ci.yml`：`pr-verilator-fast` 加**抽样** step
- [x] **T5.2** `ci.yml`：`nightly-verilator` 加**全量** step
- [x] **T5.3** 新脚本 **可执行位** `100755`（护栏可达性判据 ②）
- [x] **T5.4** `.rddf/checks/guard-reachability.md` 由
      `verify_guard_reachability.sh --render` **自动渲染**（⇒ **禁手改**）

## T6 · 验证

- [x] **T6.1** **AC1** 产物 lint 干净（带固件形态）
- [x] **T6.2** **AC2** 官方 40 个在 RTL 上全过（`--full`）
- [x] **T6.3** **AC3** 负向对照有判别力
- [x] **T6.4** **AC4** 判别力自证：变异（`tohost` 地址改错）⇒ ⇒ 门禁**必红** ⇒ ⇒ 还原复绿
- [x] **T6.5** **AC5** 无固件产物逐字节不变
- [x] **T6.6** **AC6** 既有护栏全绿（**零**判据被撞）
- [x] **T6.7** **AC7** 全量回归 `ctest` **282/282**
- [x] **T6.8** **AC8** 护栏可达性一致（`--check` 绿）
- [x] **T6.9** `openspec validate --strict` 绿 · `validate --specs` 全绿
