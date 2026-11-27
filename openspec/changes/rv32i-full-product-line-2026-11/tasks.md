# Tasks: S4c-0 —— 双产品线骨架

> **change**：`rv32i-full-product-line-2026-11` · **范围** = ADR-078 的 D8 的 **S4c-0**
> （产品线抽象 + 护栏参数化 + full 线空壳）⇒ S4c-1 / S4c-2 / S4c-3 **各自另立**。
> **决策依据**：`docs/adr/ADR-078-dual-product-lines-and-full-line-scope.md`（Accepted · 双审全闭）。

---

## P0 · 决策门（已闭）

- [x] **T0.1** ADR-078 **Accepted**（oracle 1 轮 + critic 2 轮全闭 · 审稿记录
      `.rddf/reviews/ADR-078-dual-product-lines-and-full-line-scope.md`）⇒ ⇒ 本 change 的
      **范围** = ADR-078 的 D8 的 S4c-0（逐字）
- [x] **T0.2** 用户 2026-11-27 裁决：**平台形态 = 双产品线** · 命名 =
      `examples/riscv-full/` + `examples/riscv-full-soc/` · **旧名不改**（D1/D2/D9）

## P1 · per-line marker + 登记面

- [x] **T1.1** **每条产品线恰好一个** `.rddf-line.yaml`（放在该线的 **IP 级目录** · 产品线的锚点）：
      `examples/riscv-mini/.rddf-line.yaml`（mini 线 · `members` = 迷你 + `examples/riscv-soc`）·
      `examples/riscv-full/.rddf-line.yaml`（full 线 · `members` = full + `examples/riscv-full-soc`）
      ⇒ 字段 = `line` / `layer`（恒 `ip`）/ `members` / `adrs` / `specs`
      ⇒ 且 `line` == 所在目录名（design §2）
      ⚠️ **实施期订正**（2026-11-27）：初稿把 marker 放「每个成员目录一个」⇒ 但 C2 的
      「`line` == 目录名」对 SoC 级目录**不成立**（`examples/riscv-soc` 的 `line` 是
      `riscv-mini`）⇒ ⇒ 改为「每线一个 + `members` 列出全部成员目录」+ 新增「归属完备性」
      子判据（`examples/` 下每个产品线成员目录 MUST 有归属）
- [x] **T1.2** `scripts/gen_product_lines.py`（生成器 + `--check` 模式）⇒ 输出
      `.rddf/checks/product-lines.md`（登记面：产品线表 · 锚点分组表 · 脚本处置表指针）
      ⇒ **生成物不含任何手写内容**
- [x] **T1.3** `scripts/product_line_scope.tsv`（处置表 · **14 行** = 12 sh + 2 py）
      ⇒ 每行含：脚本 · 写死内容 · 类（A|B）· 处置 · S4c-0 的动作
      ⇒ ⚠️ **B 类 8 个的动作各不相同**（design §5）⇒ MUST 逐条落

## P2 · 元护栏

- [ ] **T2.1** `scripts/verify_product_lines.sh` 四条判据：
      **C1** 发现集 == 登记集 · **C2** marker 完备性（必填字段 / `layer` 合法 /
      `line` == 目录名 / `adrs` 指向的 ADR 文件存在）· **C3** 覆盖完备性（每条线 MUST 被
      **至少一个**护栏登记面引用）· **C4** `--self-test` 投毒自证
- [ ] **T2.2** `--self-test` **三条投毒全部实测转红**（改 marker / 删 marker / 改 `layer`）
      ⇒ ⇒ 每条投毒**先确认能跑**（不是「文件不存在」式的假红）
- [ ] **T2.3** 元护栏接入 `scripts/verify_guard_reachability.sh` 的守恒面 + CI

## P3 · B 类 8 脚本按产品线参数化

> ⚠️ 逐个的「S4c-0 动作 / 何时才需要按线分组」见 design §5 ⇒ **不可写成「参数化护栏」一句**。

- [ ] **T3.1** `verify_no_duplicate_rv32i_headers.sh`：C1/C4/C5/C6 的**扫描面**改为按 marker 发现
      ⇒ C1 因 full 线 basename 独立而天然绿；**C6 快照仍只对 mini**（full 线尚无核）
- [ ] **T3.2** `verify_riscv_soc_core_ports.sh`：加 `--line` 参数 ⇒ full 线**无对象**（不误红）
- [ ] **T3.3** `rv32i_obs_vector.py`：核头 / 观测向量源按 marker 发现 ⇒ 找不到 ⇒ **跳过而非误红**
- [ ] **T3.4** `dual_backend_parity.py`：测试源清单按 marker 发现 + 锚点改分组
- [ ] **T3.5** `verify_example_assertion_quality.sh`：新线测试源 MUST 登记（漏登记 ⇒ 红）
- [ ] **T3.6** `verify_example_test_wiring.sh`：归属守恒面按 marker 发现（full 线测试源 MUST
      在其 CMake 注册）
- [ ] **T3.7** `verify_examples_rtl_sim.sh`：S4c-0 的 full 线**无 RTL 产物 ⇒ tsv 无行是合法的**
      ⇒ 「有产物但未登记」⇒ 红（判定逻辑须区分这两种情形）
- [ ] **T3.8** `verify_hierarchical_rtl.sh`：S4c-0 无需改（`RV32I_HIER_EMITTER` 已可覆盖）
- [ ] **T3.9** **B 类 8 个的「full 线存在 / 不存在」两态退出码相同**（逐个实测 ⇒ AC5）

## P4 · A 类登记 + 锚点分组 + 逐线 scope + 散文表治理

- [ ] **T4.1** A 类 6 个脚本（`verify_rv32i_official_rtl` / `verify_dtcm_read_timing` /
      `verify_rv32i_fw_init_rtl` / `verify_rv32i_elf_e2e` / `rv32i_rtl_sim_env` /
      `probe_r21_hier_granularity`）**只登记不改** ⇒ 处置表 14 行齐（AC4）
- [ ] **T4.2** 根 `AGENTS.md` 的 **5 个 CI 整数锚点**改**按产品线分组** ⇒ 且
      `verify_test_counts.sh` 改**分组比对**（分组子集 = 由 marker 派生的「该线的 ctest target
      清单」⇒ 见 design §6）⇒ **每个 ctest target MUST 归属唯一一条产品线**（未归属 ⇒ 元护栏 C3 红）
- [ ] **T4.3** **逐线 scope 标注**：根 `AGENTS.md` + `include/cpu/AGENTS.md` 的
      「imem MUST `sread`（1 拍）」类红线 + capability `rv32i-data-forwarding` 的
      距离矩阵 ⇒ 三处 MUST 各含「mini 线」标记（grep 可判 ⇒ AC7）
- [ ] **T4.4** **散文表治理**：`examples/riscv-soc/AGENTS.md` 的 module 集表 ⇒ 替换为**指针**
      （指向 `.rddf/checks/soc-rtl-expectations.md#riscv-soc`）⇒ 且该指针目标的 module 集
      == 磁盘实际（17）（AC8）

## P5 · full 线空壳（**零功能**）

- [ ] **T5.1** `examples/riscv-full/`：marker + README（说明 **S4c-1 才建核**）
- [ ] **T5.2** `examples/riscv-full-soc/`：marker + **最小 SoC 顶层**（框架 2 + 2 个 `ch_out` 计数
      · 端口**具名**且**不与** mini 线重名）+ CMake 目标
- [ ] **T5.3** **1 个 Catch2 测试源**（最小：构建 + 产物形态）⇒ ⇒ 使 full 线立即进入
      test-wiring / assertion-quality 的扫描面（AC11 的登记对象）
- [ ] **T5.4** full 线的产物面（module 集 / 顶层端口表）**登记** ⇒ `--check-products` **PASS**

## P6 · 回归与收口

- [ ] **T6.1** **mini 线零行为变化**：全量重建 0 error · `ctest` **285/285**（+full 线的 1 条）
      · 5 个既有护栏全绿 · mini 线产物端口表**逐字不变**
- [ ] **T6.2** 元护栏 + 生成器 `--check` + 锚点分组校验**全绿**；且各自的 `--self-test` 投毒**必红**
- [ ] **T6.3** `openspec validate rv32i-full-product-line-2026-11 --strict` exit 0 ·
      `bash scripts/verify_openspec_delta.sh` 4/4
- [ ] **T6.4** **AC 映射回填**：ADR-078 元数据的「关联 change」行指向本 change + 列出 AC 映射
- [ ] **T6.5** `verifier` 独立验收（AC1–AC12 逐条工具证据 + 遗留清扫）
- [ ] **T6.6** 母 improvement `rv32i-soc-platform-2026-11` 的 `进度:` 追加 S4c-0 完成记录

## P7 · 归档

- [ ] **T7.1** `openspec archive rv32i-full-product-line-2026-11 --yes`
      ⇒ 归档到 `openspec/changes/archive/2026-11-27-rv32i-full-product-line-2026-11/`

---

## 实施期发现（2026-11-27 · P1 期间）

⚠️ **实施顺序 MUST 调整**（P1 期间实测得出）：

1. **P5 的 full 线骨架 MUST 提前到 P2 之前** —— 元护栏的 **C3（每条产品线 MUST 有至少一个
   可执行的验证载体）**的载体 = 「该线的 ctest target 归属」⇒ ⇒ full 线 MUST 已有**至少一个**
   ctest target ⇒ ⇒ 否则 C3 恒红 ⇒ ⇒ 顺序改为 **P1 → P5（骨架）→ P3（B 类参数化）→ P2（元护栏）**。
2. **P3 的 B 类参数化 MUST 在 full 线骨架之后** —— 新 example 目录一旦存在，`verify_example_test_wiring`
   / `verify_example_assertion_quality` 会因**未登记**而红 ⇒ ⇒ 参数化与骨架**同批**才自洽。
3. **仓内存在 1 处既有护栏红**（`verify_example_assertion_quality.sh` 的
   `test_rv32i_soc_dual_parity.cpp` 计数不符 · 引入于 `1190a6f`/`ec5db47` · **非本批引入**）
   ⇒ 已用 `git worktree` 在 **S4c-0 立项前**的提交点复现 ⇒ 确认为既有 ⇒ **不修**（范围外债）·
   ⇒ ⚠️ **不在 AC10 的 5 个护栏清单内** ⇒ AC10 与 ADR-078 的 AC6 **不受影响**
4. **根 `AGENTS.md` 的 5 个 CI 锚点当前仍是单值** ⇒ 生成器登记面 §2 标「**未归组**（T4.2）」
   ⇒ ⇒ 生成器**同时容忍**单值 / 分组两种形态 ⇒ ⇒ T4.2 之前生成器不会误红。

## 硬约束

1. **S4c-0 MUST NOT 改 mini 线的任何行为 / 产物 / 端口面** ⇒ T6.1 是**零变化**判据（AC10）。
2. **S4c-0 MUST NOT 实现任何功能**（取指总线化 / I-L1 / DMA / 互连 / L2）⇒ 它们分属
   S4c-1 / S4c-2 / S4c-3 ⇒ ⇒ **越界即 review 阻塞**。
3. **旧线 MUST NOT 改名**（D9）⇒ 任何 `examples/riscv-mini` → 其它名的改动 ⇒ **review 阻塞**。
4. **B 类 8 个脚本 MUST 逐个处理**（design §5 的动作各不相同）⇒ 合并成「参数化护栏」一句
   ⇒ ⇒ **AC5 不可判 ⇒ review 阻塞**。
5. **A 类 6 个 MUST 只登记不改** ⇒ 顺手改它们 ⇒ ⇒ mini 线回归面膨胀 ⇒ **review 阻塞**。
6. **每个 ctest target MUST 归属唯一一条产品线** ⇒ 未归属 ⇒ T4.2 的分组校验红 ⇒ ⇒
   「新 target 忘了归线」当场红（**AC11 的核心**）。
