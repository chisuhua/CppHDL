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

- [x] **T2.1** `scripts/verify_product_lines.sh` 四条判据：
      **C1** 发现集 == 登记集 · **C2** marker 完备性（必填字段 / `layer` 合法 /
      `line` == 目录名 / `adrs` 指向的 ADR 文件存在）· **C3** 覆盖完备性（每条线 MUST 被
      **至少一个**护栏登记面引用）· **C4** `--self-test` 投毒自证
- [x] **T2.2** `--self-test` **三条投毒全部实测转红**（改 marker / 删 marker / 改 `layer`）
      ⇒ ⇒ 每条投毒**先确认能跑**（不是「文件不存在」式的假红）
- [x] **T2.3** 元护栏接入 `scripts/verify_guard_reachability.sh` 的守恒面 + CI

**T2 的实施期发现（2026-11-28）**：

- **T2.1 的多字段面比初稿多**：C2 从 6 条子判据（字段 / `layer` / `line` / `members` /
  `adrs` / `specs`）扩到 7 条（+ **归属完备性**）；C3 从 1 条判据扩到 2 条
  （每条线 ≥1 target · 每个 examples target 归线唯一）。
- **判据实现拆两件**：`scripts/lib/product_lines.py`（判据 · 可单测）+ `scripts/verify_product_lines.sh`
  （入口 + `--self-test`）⇒ ⇒ 与 `guard_reachability.sh` + `guard_reachability.py` 同型。
- **`specs` 字段 MUST 允许「active change 的 delta capability」** —— 实测：full 线的 marker
  引用 `rv32i-product-lines`（本 change 正在新增）⇒ 归档前不在 `openspec/specs/` ⇒ ⇒
  若只判「`openspec/specs/<name>` 存在」⇒ **变更进行中恒红**。
- **排除清单是必需件**（见 design §1.1 的 dated 订正）。
- **`--self-test` 的 trap 变量 MUST 是脚本级** —— 实测：`local` 变量在 EXIT trap 执行时
  已销毁 ⇒ ⇒ `set -u` 下报 `unbound variable`（功能仍对，但那是错误用法，会掩盖真的 trap 故障）。
- **C3 的 ctest target 提取 MUST 认两种形态** —— ① 字面 `add_test(NAME <t>`；
  ② **helper 形态**（`riscv_mini_catch_test(<t> <src.cpp>)`）⇒ ⇒ 只认 ① 会漏掉
  **整条 riscv-mini 线**（实测：mini 线被判「零 ctest target」）。

**T2.3 的实测**：`verify_guard_reachability.sh` **PASS**（**60 个被引用脚本** · 登记表双向 1:1
且排除类型逐字派生一致）；登记表由 `--render` 派生（禁手改）⇒ 本批按 render 输出替换表格段。

## P3 · B 类 8 脚本按产品线参数化

> ⚠️ 逐个的「S4c-0 动作 / 何时才需要按线分组」见 design §5 ⇒ **不可写成「参数化护栏」一句**。

- [x] **T3.1** `verify_no_duplicate_rv32i_headers.sh`：C1/C4/C5/C6 的**扫描面**改为按 marker 发现
      ⇒ C1 因 full 线 basename 独立而天然绿；**C6 快照仍只对 mini**（full 线尚无核）
- [x] **T3.2** `verify_riscv_soc_core_ports.sh`：加 `--line` 参数 ⇒ full 线**无对象**（不误红）
- [x] **T3.3** `rv32i_obs_vector.py`：核头 / 观测向量源按 marker 发现 ⇒ 找不到 ⇒ **跳过而非误红**
- [x] **T3.4** `dual_backend_parity.py`：测试源清单按 marker 发现 + 锚点改分组
- [x] **T3.5** `verify_example_assertion_quality.sh`：新线测试源 MUST 登记（漏登记 ⇒ 红）
- [x] **T3.6** `verify_example_test_wiring.sh`：归属守恒面按 marker 发现（full 线测试源 MUST
      在其 CMake 注册）
- [x] **T3.7** `verify_examples_rtl_sim.sh`：S4c-0 的 full 线**无 RTL 产物 ⇒ tsv 无行是合法的**
      ⇒ 「有产物但未登记」⇒ 红（判定逻辑须区分这两种情形）
- [x] **T3.8** `verify_hierarchical_rtl.sh`：S4c-0 无需改（`RV32I_HIER_EMITTER` 已可覆盖）
- [x] **T3.9** **B 类 8 个的「full 线存在 / 不存在」两态退出码相同**（⇒ AC5）
      ⇒ **实测 4 个**（把 `examples/riscv-full*` 临时移走后逐个跑）：
      `no_duplicate_headers` **0==0** · `riscv_soc_core_ports` **0==0** ·
      `dual_backend_parity` **0==0** · `assertion_quality` **1==1**
      （present 的 1 = 既有红；absent 的 1 = 既有红 + 1 条**真** STALE〔锚点文件消失〕）
      ⇒ ⚠️ **另 4 个以逻辑论证 + 单态实测代替**（`test_wiring` / `examples_rtl_sim`
      单次 ~15 min；`hierarchical_rtl` 无对象；`obs_vector` 随 `dual_backend_parity`）
      ⇒ ⇒ 已如实登记（见 AC5 的措辞）

**T3 的实施期发现（2026-11-28）**：

1. **5 个 B 类脚本实测「零改动」即可满足 AC5** —— AC5 的判据是「**两态退出码相同**」，
   不是「必须参数化」。实测：`no_duplicate_headers` / `riscv_soc_core_ports` /
   `obs_vector` / `dual_backend_parity` 两态**同为 0**（它们只看 mini 线路径或已全扫）
   ⇒ ⇒ **为此做无谓改动会违反「改动最小化」** ⇒ ⇒ 处置表 8 行已逐条改为「实测形态」。
2. **T3.7 实际已由 `verify_examples_rtl_sim_coverage.sh` 的守恒判据承担**
   （`tracked == 覆盖 ∪ 落选` 且三者互斥，实测 `30 == 17 ∪ 13`）⇒ ⇒ `verify_examples_rtl_sim`
   本批**零改动**。
3. ⚠️ **`set -e` + 临时移走仓库目录 = 危险组合（差点丢目录）** —— 实测：批量跑护栏时
   一条失败 ⇒ `set -e` 中止 ⇒ **恢复命令没执行** ⇒ `examples/riscv-full*` 留在 `/tmp`。
   ⇒ ⇒ **纪律**：① 移走/恢复类操作 **MUST NOT 用 `set -e`**（或用 `trap ... EXIT` 保证恢复）；
   ② 更根本的替代 = **在 `git worktree` 的副本里做两态实验**（不碰主工作树）。
   ⇒ ⇒ 已在 AC5 的实测里用「逐个 `timeout` + 显式恢复」而非 `set -e`。

## P4 · A 类登记 + 锚点分组 + 逐线 scope + 散文表治理

- [x] **T4.1** A 类 6 个脚本（`verify_rv32i_official_rtl` / `verify_dtcm_read_timing` /
      `verify_rv32i_fw_init_rtl` / `verify_rv32i_elf_e2e` / `rv32i_rtl_sim_env` /
      `probe_r21_hier_granularity`）**只登记不改** ⇒ 处置表 14 行齐（AC4）
- [x] **T4.2** 根 `AGENTS.md` 的 **5 个 CI 整数锚点**改**按产品线分组** ⇒ 且
      `verify_test_counts.sh` 改**分组比对**（分组子集 = 由 marker 派生的「该线的 ctest target
      清单」⇒ 见 design §6）⇒ **每个 ctest target MUST 归属唯一一条产品线**（未归属 ⇒ 元护栏 C3 红）
      ⇒ **实测（2026-11-28）**：格式 = `<组名>=<N>`（组名 = **产品线全名** + `lib` 余项组）
      ⇒ `CTEST_TOTAL: lib=283, riscv-mini=2, riscv-full=1`（和 286）
      ⇒ `VERILATOR_E2E_TOTAL: lib=81, riscv-mini=1, riscv-full=0`（和 82）
      ⇒ `DUAL_BACKEND_PARITY_TOTAL: riscv-mini=68, riscv-full=0` · `PAIRS: riscv-mini=307, riscv-full=0`
      ⇒ `EXAMPLES_TEST_SOURCES: riscv-mini=26, riscv-full=1`（和 27）
      ⇒ ⇒ **三个读取锚点的护栏全部改分组解析**（`verify_test_counts` / `verify_dual_backend_parity`
      / `verify_example_test_wiring`）⇒ 三个护栏实测绿
      ⇒ ⚠️ **组名 MUST 用产品线全名**（实测：用缩写 `mini` ⇒ 与 `--counts` 输出不匹配 ⇒ 恒 0）
- [x] **T4.3** **逐线 scope 标注**：根 `AGENTS.md` + `include/cpu/AGENTS.md` 的
      「imem MUST `sread`（1 拍）」类红线 + capability `rv32i-data-forwarding` 的
      距离矩阵 ⇒ 三处 MUST 各含「mini 线」标记（grep 可判 ⇒ AC7）
- [x] **T4.4** **散文表治理**：`examples/riscv-soc/AGENTS.md` 的 module 集表 ⇒ 替换为**指针**
      （指向 `.rddf/checks/soc-rtl-expectations.md#riscv-soc`）⇒ 且该指针目标的 module 集
      == 磁盘实际（17）（AC8）
      ⇒ ⚠️ **实测该表曾长期停在 12**（磁盘已是 `AxiLiteCrossbar` + 6 外设 = 17）⇒ ⇒ **同周内漂移**
      ⇒ ⇒ 散文表**不复制**数值，只留指针（单一真相源唯一）

## P5 · full 线空壳（**零功能**）

- [x] **T5.1** `examples/riscv-full/`：marker + README（说明 **S4c-1 才建核**）
- [x] **T5.2** `examples/riscv-full-soc/`：marker + **最小 SoC 顶层**（框架 2 + 2 个 `ch_out` 计数
      · 端口**具名**且**不与** mini 线重名）+ CMake 目标
- [x] **T5.3** **1 个 Catch2 测试源**（最小：构建 + 产物形态）⇒ ⇒ 使 full 线立即进入
      test-wiring / assertion-quality 的扫描面（AC11 的登记对象）
- [x] **T5.4** full 线的**组件端口面**登记（根端口面 = 框架 2 + 2 = **4** · 端口**具名**）⇒
      且被 Catch2 判据断言（**9 断言 / 4 用例**）+ `tb_riscv_full_soc` **3/3** ⇒ ⇒
      ⚠️ **实施期订正**：初稿写「`--check-products` PASS」⇒ 但 S4c-0 的 full 线**不发 RTL**
      （只有 2 根恒定输出 ⇒ 层次化产物**无判别力**）⇒ ⇒ `--check-products` 从 **S4c-1** 起
      ⇒ ⇒ 本批改为「组件端口面登记 + 可构建 + 可运行」

**P4 的实施期发现（2026-11-28）**：

- **分组名 MUST 用产品线全名** —— 锚点分组最初写成 `mini=` / `full=` ⇒ 与
  `product_lines.py --counts` 输出的 `riscv-mini` 不匹配 ⇒ 每线校验恒 0 ⇒ ⇒ 改用全名。
- **`lib` 余项组是必需的** —— 根 `tests/**` 的 280+ 个 ctest **不属任何产品线** ⇒ ⇒
  分组必须含 `lib`（= 总数 − 各产品线）⇒ 否则「和」对不上。
- **`bc` 在精简容器里不存在** ⇒ 分组求和 MUST 用 `awk`。
- **`awk -F'\t'` 在 mawk 下不把 `\t` 解释为 TAB**（实测：整行不分割 ⇒ 恒 0）⇒ ⇒
  MUST 用 `-F"$(printf '\t')"`。
- **三个读取锚点的护栏都要改** ⇒ ⇒ 只改 AGENTS.md 会打断它们 ⇒ ⇒ **锚点格式变更
  MUST 与读取方的解析同批**。
- **逐线 scope 标注的第三处（capability）在 delta 里** —— 本 change 的
  `specs/rv32i-data-forwarding/spec.md` **ADDED** 了适用范围声明 ⇒ ⇒ 归档时自动进主 spec。

## P6 · 回归与收口

- [x] **T6.1** **mini 线零行为变化**：全量重建 0 error · `ctest` **285/285**（+full 线的 1 条）
      ⇒ **实测（2026-11-28）**：全量重建 **0 error** · `ctest` **285/286 绿**（286 条含 full 线的 1 条）
      ⇒ ⚠️ **唯一失败 = `perf_regression`（#125）· verilator 后端微基准的环境漂移** ⇒ ⇒
      **不是本批引入**（证据链见 proposal 的「已知既有红」表：baseline 采集于 2026-10-28 ·
      interpreter/jit 后端正常 · verilator 缓存命中 · 本批零计算路径改动 · S4b 阶段同机 285/285 绿）
      ⇒ ⇒ 5 个既有护栏（rtl_sim / rtl_sim_coverage / hierarchical_rtl / no_duplicate_headers /
      riscv_soc_core_ports）实测**全绿** ⇒ ⇒ 「重采 baseline」属**需用户裁决**的独立决策
      · 5 个既有护栏全绿 · mini 线产物端口表**逐字不变**
- [x] **T6.2** 元护栏 + 生成器 `--check` + 锚点分组校验**全绿**；且各自的 `--self-test` 投毒**必红**
- [x] **T6.3** `openspec validate rv32i-full-product-line-2026-11 --strict` exit 0 ·
      `bash scripts/verify_openspec_delta.sh` 4/4
- [x] **T6.4** **AC 映射回填**：ADR-078 元数据的「关联 change」行指向本 change + 列出 AC 映射
- [x] **T6.5** `verifier` 独立验收（AC1–AC12 逐条工具证据 + 遗留清扫）
      ⇒ ✅ **PASS —— AC1–AC12 全部满足**（2026-11-28 · verifier 角色 · 第 1 轮 · **0 BLOCKER**）
      ⇒ 报告 = `.rddf/reviews/rv32i-full-product-line-2026-11-verify.md`
      ⇒ **6 处投毒实测**证明判据有判别力：AC2 手工改生成物 / AC3 五连投毒 /
      AC6 改分组 + **换组且和不变**（两层判据各自可抓）/ AC11 未登记测试源 / AC5 两态复核
      ⇒ 遗留清扫（verifier 独立跑）：TODO / 调试输出 / skip 测试 / 注释代码 **全 0 命中** ·
      工作区干净 · 被改的 3 个既有护栏只改锚点解析、判据逻辑未动
      ⇒ ⚠️ **AC10 如实判定**：ctest **285/286**（唯一红 = `perf_regression` 的 verilator 微基准
      **环境漂移**：baseline 采集于 2026-10-28 · interpreter/jit 未报 REGRESSION ⇒ 非代码退化 ·
      verilator 缓存命中 · 本批零计算路径改动 · S4b 阶段同机 285/285 绿）⇒ ⇒ **未宣称全绿**
      ⇒ 2 条 SHOULD_FIX：① 处置表空行（**本批已修**）② `perf_regression` 的 baseline
      重采属**改基线数据** ⇒ **需用户裁决**（独立决策，不阻塞归档）
- [x] **T6.6** 母 improvement `rv32i-soc-platform-2026-11` 的 `进度:` 追加 S4c-0 完成记录
      ⇒ ✅ 已回填（提交 `36e5411`）：交付 5 项 + verifier PASS + 下一批 = S4c-1 或 S5

## P7 · 归档
**P6 的实施期发现（2026-11-28）**：

- ⚠️ **`perf_regression` 依赖前置生成的 `perf_results.json`** ⇒ ⇒ **单独 `ctest -R perf_regression`
  是无效的**（会报 `cannot open perf_results.json` ⇒ 「红」不代表退化）⇒ ⇒ 判读 perf 红 MUST 在
  **完整 perf 组上下文**里做（且 `perf_results_clean`(#286) 在序列里的位置会提前清结果 ⇒ ⇒
  即便 `-R perf` 也不可靠 ⇒ ⇒ 最稳 = 跑**全量**）。
- **`perf_regression` 的 verilator 退化 = 环境漂移**（详见 proposal 的「已知既有红」表）⇒ ⇒
  T6.1 的「ctest 全绿」因此**如实写成 285/286 + 1 条环境红** ⇒ ⇒ 不谎报全绿。


- [x] **T7.1** `openspec archive rv32i-full-product-line-2026-11 --yes`
      ⇒ ✅ 归档为 `openspec/changes/archive/2026-11-28-rv32i-full-product-line-2026-11/`
      ⇒ spec 合并：新 capability **`rv32i-product-lines`（+5）** · `rv32i-data-forwarding`（+1）
      ⇒ ⇒ 归档后 `examples/riscv-full/.rddf-line.yaml` 的 `specs: rv32i-product-lines`
      ⇒ ⇒ 指向**真实存在**的主 spec（此前是 active change 的 delta）

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
4. **两个护栏的实测形态比处置表原写的更好**（⇒ 处置表已订正）——
   · `verify_example_test_wiring.sh` 的源集合**已是全扫**（`find examples -path '*/tests/*'`）
     ⇒ ⇒ **T3.6 = 零改动**（新线自动覆盖）
   · `verify_example_assertion_quality.sh` 的 `SRC_LIST` **自动收集**（含 `examples/**/tests/`）
     ⇒ ⇒ 新线测试源自动进扫描面；但护栏**主动要求**「新增测试源 MUST 登记一行（含真断言数下限）」
     ⇒ ⇒ **T3.5 = 登记 1 行** ⇒ ⇒ **「加线漏登记 = 当场红」天然成立**（ADR-078 AC11 的实现）
5. ⚠️ **护栏对 CMake 写法的约束（真实坑 · 未来每条产品线都会踩）** ——
   `verify_example_test_wiring.sh` 的 MAP 解析是**逐行**的：取「`fn(` 的**首个实参** =
   target」+「**最后一个** `.cpp` 实参 = 源文件」⇒ ⇒
   ① `add_executable(<target>` 与源文件**分行**⇒ ⇒ 该行无 `.cpp` ⇒ ⇒ **整行被跳过**
   ⇒ ⇒ 源文件与 target 的关联丢失 ⇒ ⇒ 护栏报「未被任何已覆盖的 target 编译」；
   ② 若把 `catch_amalgamated.cpp` 与测试源写在**同一行**⇒ ⇒ 关联落到 amalgamated 身上
   ⇒ ⇒ 同样丢关联。
   ⇒ ⇒ **正确写法**：测试源**独占一行**（唯一的 `.cpp`）· amalgamated 走 `target_sources`。
   ⇒ ⇒ ⚠️ 这**不违反** ADR-078 的 AC3（「0 处护栏**脚本**改动」）—— 它是**写法约束**，不是脚本改动；
   ⇒ ⇒ 但它是「加线成本」里的**隐性项** ⇒ ⇒ 已在 **AC5** 的载体里登记。
6. **慢护栏 MUST 独占 CPU** —— `verify_example_test_wiring.sh` 内部会跑
   `test_rv32i_soc_dual_parity`（**245 秒**）⇒ ⇒ 并行跑其它重活 ⇒ ⇒ 该测试超时
   （exit 124）⇒ ⇒ 护栏误报「CTest 条目退出码 ≠ 0」⇒ ⇒ 排查耗时的**假红**。
   ⇒ ⇒ 实测：单独复跑 `ctest -R test_rv32i_soc_dual_parity` ⇒ **PASS**（245 s）。
7. ⚠️ **`verify_example_test_wiring.sh` 的 C3 有既有脆弱性**（**非本批引入**）——
   它以 `timeout 300` 直接跑二进制，而 `test_rv32i_soc_dual_parity` 实测 **245 s**（ctest）/
   **>400 s**（直跑）⇒ ⇒ 稍慢即 exit 124。**失败原因是 `SIGTERM` 不是断言失败**
   （Catch2 打印 `{Unknown expression after the reported line} due to SIGTERM`）·
   `ctest -R` 同二进制 **PASS** ⇒ ⇒ 该红**不代表断言失败**，也不属本批。
   ⇒ ⚠️ T3.6 的**实质**（新线被自动覆盖）已由护栏输出证明：
   `OK examples/riscv-full-soc/tests/test_rv32i_full_soc.cpp → test_rv32i_full_soc` ✅
8. **根 `AGENTS.md` 的 5 个 CI 锚点当前仍是单值** ⇒ 生成器登记面 §2 标「**未归组**（T4.2）」
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
