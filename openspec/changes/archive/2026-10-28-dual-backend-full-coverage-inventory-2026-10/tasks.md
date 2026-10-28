# Tasks: dual-backend-full-coverage-inventory-2026-10

## 0. 准备
- [x] 0.1 读 `dual-backend-test-coverage` 的 R6 排除清单（**5 类**）⇒ 判定口径
      —— 5 类 = ① 纯编译期/静态断言 ② 裸 context ③ >64-bit 端口
      ④ Catch2 SECTION 重入 ⑤ STATIC_REQUIRE/编译期元函数；
      ⚠️ 且该 capability **明文规定**了候选 grep（`ch_device|ch_module` ∧
      ¬`STATIC_REQUIRE` ∧ ¬`ch_out<ch_uint<[0-9]\{3,\}>>`）
      ⇒ 本盘点按该口径
      ⚠️ **另加一条判定源**：该 capability 的「**现有 9 个 verilator 测试 MUST 保留原状**」
      Requirement（`spec.md:145-147`）⇒ 那 9 个文件**不得**算作「可转 dual」
- [x] 0.2 枚举**测试条目**（ctest target）⇒ ⚠️ 口径 = **条目**，不是文件
      —— 实测 `ctest --show-only=json-v1` ⇒ **202** 条
      （标签分布：base 165 / verilator_e2e **38** / perf 11 / spinalhdl-ported 25 /
      chlib 1 / verilator 1，多标签重叠）
- [x] 0.3 枚举**测试源文件**并建**文件 → 条目**映射
      —— 实测：**175** 个注册调用（`add_catch_test` 135 + `add_dual_backend_test` 22
      + `riscv_mini_catch_test` / `riscv_mini_unit_test` 18）
      ⇒ 其中 **172** 个进 CTest、**3** 个（`rv32i_phase{1,2,3}_test`）
      **只构建、不进 CTest** ⇒ **不是 ctest 条目** ⇒ 不在判据的分子里
      ⇒ 涉及 **172** 个源文件 ⇒ **平均每文件 1 个条目**；**多文件一条目** = 0；
      ⚠️ **1 文件 2 条目**的形态存在：`test_codegen_literal_mask.cpp` 同时注册为
      `test_codegen_literal_mask` 与 alias 条目 `test_verilator_three_way`
      （`tests/CMakeLists.txt:88`）

## 1. 盘点（**只读**，不改代码）
- [x] 1.1 逐个**条目**判定 ⇒ 产出「可 dual 清单」+「排除理由表」
      —— 实测：**172** 行盘点表（每行给 `条目` + `源文件` + `注册处:行号` + `判定`
      + `类别` + `理由`）
      ⇒ 分布：**已 dual（macro 调用）28** / **已 dual（hand-crafted）1** /
      **不可 dual（契约锁定）9** / **可 dual 33** /
      **可 dual 但无 runtime 语义 3** / **不可 dual（R6 各类）98**
- [x] 1.2 行数守恒：可 dual + 排除 = **总条目数**（⚠️ 不是 166）
      —— 实测：172 = 28 + 1 + 9 + 33 + 3 + 98 ✓；且 202 = 172（范围内）+ 30（范围外）✓
      ⇒ 范围外 30 = perf 基准 4 + `spinalhdl-ported` 演示程序 25 + alias 条目 1
      ⇒ ⚠️ **另有 3 个 target 只构建、不进 CTest ⇒ 不计入 202**
- [x] 1.3 **双侧上限**（乐观 / 保守）⇒ 结论以**区间**表述 ⇒ 批次数 + 估时
      —— 实测（**两种判据口径**各给双侧）：
      口径 A（标签条目数，**现状判据**）∈ [**71**, **96**]；
      口径 B（真 dual pair 数，语义正确）∈ [**62**, **87**]
      ⇒ ⚠️ **两种口径的乐观上限都 < 100** ⇒ 即便把 25 个 demo 全改写成 dual 形式
      （口径 A = 96）**仍差 4** ⇒ **判据在两种口径下都不可达**
      ⇒ 批次：**33** 个可直接转 dual ⇒ 每批 ≤ 10 ⇒ **至少 4 批**（墙钟须实测）；
      另 **3** 个「无 runtime 语义」的**不计入**批次数（dual 化前还得先补断言）
- [x] 1.4 判据结论：**不可达** ⇒ 产出「**判据修订请求**」（建议值 + 依据）
      ⇒ **交 planner**（build **不得**自行改 objective 的 Go 判据）
      —— 候选：B（推荐，真 dual pair 数）阈值 **≥ 60**（保守上限 62 的 ~97%）；
      A（现状口径）阈值 **≥ 70**（保守上限 71 的 ~99%）
      ⇒ ⚠️ 两者都 **必须** ≤ 对应口径的保守上限 ⇒ 否则就是「硬凑」的判据
      ⇒ ⚠️ 且「保留 ≥ 100」在当前仓内**没有可行路径**（乐观上限只有 96）

## 2. 验收
- [x] 2.1 盘点表逐行可核（不得给「大概 XX 个」）
      —— 实测：**172** 行，每行带 `条目` + `源文件` + `注册处:行号` + `判定`
      + `类别` + `理由`
- [x] 2.2 附「文件 → 条目」映射（1 文件多条目 / 多文件一条目的形态已标出）
      —— 实测：**多文件一条目 = 0**；**1 文件 2 条目 = 1 例**（alias
      `test_verilator_three_way` ⇒ `test_codegen_literal_mask`）
- [x] 2.3 双侧上限都给出，结论以区间表述（见 1.3）
- [x] 2.4 **无代码改动**
      —— 实测：本 change 只动 `openspec/changes/dual-backend-full-coverage-inventory-2026-10/`
      下的文档 ⇒ `git diff --stat` 无 `src/**` / `include/**` / `tests/**`

## 3. 归档
- [x] 3.1 结论写入本 change 的 `design.md`（盘点表即交付物）
      —— 已写入 `design.md`（423 行）：§0 一句话结论 / §1 方法 / §2–8 盘点表与映射 /
      §9 判据修订请求
      ⇒ ⚠️ 盘点表与全部数字由**脚本生成**（`inventory.py` → `inventory.md` →
      `assemble_design.py` → `design.md`）⇒ 人只写 §0 / §1 / §9
      ⇒ ⇒ **杜绝手抄漂移**（本次手写版曾把 `test_bundle_ecosystem` 判成 R6 #4，
      实际是 R6 #1/#5 ⇒ 已改为程序化生成）
      ⇒ **逐行可核性实测**：172 行全部满足「源文件存在」+「注册处行号含该 target 名」
      +「序号连续 1..172」⇒ **0 行不可核**
- [x] 3.2 若需修订 capability ⇒ 补 delta；否则 `--skip-specs`
      —— 实测：**无 capability 变更**（只读盘点）⇒ `--skip-specs` 成立 ✓
      ⇒ ⚠️ 但 §9 交付了 **2 项契约缺口**给 planner（R6 grep 的 65–99 位盲区、
      判据的度量单位）⇒ 那属于**另一个** change（改契约）⇒ 不在本 change 内合并
- [x] 3.3 ⚠️ **请求 planner** 更新 `objective-test-coverage-100pct` 里的 `37`
      —— 实测 `grep -n "37"` = **6 处** ⇒ 分类：**4 处需处理** + **2 处是历史记录**（不改）：
      L37（单位陈述，**被本盘点证伪**）/ L45（done-when 当前值）/ L72（Go 判据当前值 +
      单位陈述）/ L93（历史表 `revise` 行 ⇒ 建议**加注**）；L25/L26 是状态快照链（历史事实）
      —— ⚠️ build **不得**自行改 objective 文件 ⇒ 本 change 只**请求**
      —— ⚠️ 且 §9.2 的**阈值本身**（≥ 60 或 ≥ 70）也属 planner 裁决
- [x] 3.4 ⚠️ **请求 planner** 注意**第三个被证伪的前提**（design.md §9.4）：
      objective §1 写「在既有文件里加 dual pair **不会**推进本指标 ⇒ Phase 5 MUST 以
      **新文件**为主」⇒ **被本盘点证伪**：本次的 **33** 个「可 dual」条目**全部**是
      **既有文件** ⇒ 只需把注册从 `add_catch_test` 改成 `add_dual_backend_test`
      ⇒ 指标 **+1** ⇒ ⇒ Phase 5 的推进方式与工作量估算都要改
      ⇒ ⇒ 且 `CTEST_TOTAL` / `EXAMPLES_TEST_SOURCES` 锚点**均不变**

## 4. 对抗审稿（**1 轮**，预算 2 轮内）
- [x] 4.1 第 1 轮 = **APPROVE_WITH_CONDITIONS**（`mergeable: yes` / `scope_exceeded: false`，
      **无 BLOCKER**；2 SHOULD_FIX + 2 NIT）⇒ 按「只有 BLOCKER 触发复审」的纪律
      ⇒ **不发起第 2 轮** ⇒ 2 SF + 2 NIT 在**同批**闭合
- [x] 4.2 **SHOULD_FIX #1**（「保留 ≥ 100 没有可行路径」表述不严谨）：
      → §9.2.1 厘清「上限」的**边界** —— 口径 A 的**技术上限其实无界**
      （加标签即可 +1 ⇒ 可到 202）⇒ 但那是**不诚实**路径 ⇒ 71/96 是
      「**语义诚实**的上限」；→ §0 与 §7 同步加「**判据在现有语料内不可达**」
      的限定语 + 交叉引用
- [x] 4.3 **SHOULD_FIX #2**（候选阈值的 margin 理由缺失 + 取舍集漏轴）：
      → §9.2.3 补 margin 理由（B ≥ 60 留 **2** 条余量 / A ≥ 70 留 **1** 条；
      A 的余量更紧是因为口径 A **本身**把非 parity 的标签计入 ⇒ 可达标空间被高估）；
      → §9.2.2 补齐**取舍集 4 个轴**：① 改阈值 ② 改度量单位
      ③ **新增有意义的 dual 测试文件**（本次盘点**未覆盖** ⇒ 工作量另计）
      ④ **放松 R6 #2**（67 个裸 context 条目改写为 component 包装形式）
- [x] 4.4 **NIT #1**（「人只写 §0/§1/§9」不精确）⇒ §1 改为准确列出人写区域
      （§0 / §1 / §7 末尾的「推翻前次审查」表 / §8 的批次数 / §9）
- [x] 4.5 **NIT #2**（proposal 写「`37` 出现在 3 处」，实测 6 处）⇒ proposal 改为
      「实测 6 处（4 处需处理 + 2 处历史记录）」+ 指向 §9.3
- [x] 4.6 ⚠️ 审稿指出的**「必要性 vs 可行性」区分**已落进 §9.4：
      本盘点证伪的是「MUST 以新文件为主」的**必要性**（前提「加 pair 不推进指标」
      已被证伪），**不是**其**可行性**（新增 dual 文件仍是可行路线 ⇒ 轴 ③）
