# Change: dual-backend-parity-batch-b-2026-11

> ⚠️ **本 change = A5 的【批次 B1】** —— 详见
> `.rddf/plans/dual-backend-full-coverage-2026-11.md`（批次清单 + 墙钟账本）。
>
> ⚠️⚠️ **零 spec 变更声明**（归档时**允许** `--skip-specs` 的依据）：
> 新增的 `_behavior` 文件**不违反** capability `dual-backend-test-coverage`
> 的任何条款 ⇒ ⇒ ⇒ **无 Requirement 需新增 / 修改 / 退役** ⇒ ⇒ ⇒
> ⇒ ⇒ `.openspec.yaml` 已声明 `skip_specs: true`（+ `schema: spec-driven`，
> CLI 1.14.3 的 change metadata 要求该字段）⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 计数锚点与
> objective 的 Go 判据**都不是 spec 面** ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 不受本声明影响**。

## Why

🔴 **A5 的批次 B 有 8 个候选文件，但「全做」不现实**（⇒ ⇒ 本 change 只做
**一个可交付子集**，把「筛选纪律」显式化）⇒ ⇒ ⇒ **⇒ 而筛选本身产出了
一条比批次更重要的结论**。

### ⚠️ 筛选纪律（**三条硬门槛**，缺一不转）

| 门槛 | 理由 | 淘汰者 |
|---|---|---|
| ① **零局部 DUT 类** | ⚠️ **机理订正（critic NIT #1 · 2026-11-01）**：`DUAL_BACKEND_TEST_PAIR` 的 `DUT_TYPE` 实际在 **TEST_CASE 函数体内**使用（`tests/dual_backend_fixture.h:152,173`）⇒ ⇒ **不是**「宏在文件作用域展开」⇒ ⇒ ⇒ **真正的障碍**是：候选文件的 DUT 类定义在**原文件的函数作用域**内 ⇒ ⇒ **新文件够不着** ⇒ ⇒ ⇒ **⇒ 必须提升到共享头** ⇒ ⇒ ⇒ **⇒ 「原文件 MUST NOT 修改」纪律与它冲突** ⇒ ⇒ ⇒ 提升会引入「同一逻辑两处维护」⇒ ⇒ **本次不做**（共享 fixture + cache key 守恒 = 独立议题） |
| ② **零平凡断言**（`REQUIRE(true)`） | 平凡断言 ⇒ ⇒ dual 化**零增量价值**（两半边都恒真） | `test_axi_pwm_simple`（Simulation 用例**只有** `REQUIRE(true)`）⇒ ⇒ 且它**自带 main**（`CATCH_CONFIG_MAIN`）⇒ ⇒ **不可用 macro** |
| ③ **无 `std::cout` 重定向** | 那断言的是**C++ 侧打印**（不是硬件信号）⇒ ⇒ **双后端对它毫无意义**；⇒ ⇒ 且 stdout 是**全局状态**⇒ ⇒ 双跑结构下会互相干扰 | `chlib/test_assert` 的 **2 个宏用例**（`CH_ASSERT` / `ch_assert`）⇒ ⇒ **MUST NOT 转** |

⇒ ⇒⇒ **⇒ 唯一通过全部门槛的文件 = `tests/chlib/test_assert.cpp`** ⇒ ⇒
DUT = `chlib::AssertChecker`（**库头类型**）⇒ ⇒ ⇒ **零原文件修改**。

## 🔴🔴🔴 本 change 最重要的产出：**成本模型的单位错了**（⇒ ⇒ 批次预算全错）

⚠️ **逐 0.01 s 打点的实测**（假 HOME 全冷 · 4 pair）：

```
[  0.02s] 写出 top.v / sim_main.cpp
[  0.23s] Verilator: Walltime 0.012 s (elab=0.000, cvt=0.004, bld=0.000)
[ 10.89s] VerilatorBackend: cached artifact at .../.cache/cpphdl/verilator/<key>
[ 10.89s] VerilatorBackend: dlopen'd .../libVtop.so, created Vtop
```

⇒ ⇒ **Verilator 的 elaboration 只花 0.012 s** ⇒ ⇒ **那 10.66 s 是【C++ 编译
+ 链接 `libVtop.so`】** ⇒ ⇒ ⇒ **且第 2/3/4 个 pair 各自又花 ~10.6 s**
⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 【根因】：`DUAL_BACKEND_TEST_PAIR` 为【每个 pair】建独立的
`ch_device<DutType>`** ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ ⇒ 上下文的 verilog 源码不同
⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ ⇒ 缓存 key 不同 ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ 【每个 pair 一次编译】**

⇒ ⇒⇒ **⇒ ⇒ 成本单位 = 【pair 数】，不是 DUT 数、不是文件数**
⇒ ⇒⇒ ⇒ ⇒ **⇒ ⇒ 批次预算由 Σ 56 min 修正为 ~30–35 min**（详见 plan §3.0）

⚠️⚠️⚠️ **但该方案在【负向验证】时被实测证伪**（2026-11-01 · critic 建议的
方案 (b)）⇒ ⇒ XML 的 `<OverallResults successes="N"/>` 是 **per-run /
per-Section** 汇总 ⇒ ⇒ **不给** 无 Section 的 TEST_CASE 的断言数 ⇒ ⇒
⇒ ⇒ **⇒ 方案作废** ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 最终判据 = 「总断言数 == 2 × pair 数」
+ 约定「每 pair 恰 1 条 `REQUIRE`」** ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ ④ 号用例因此
【拆成 2 个 pair】**（断言**不删**，只是各自独立）⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **⇒
代价 = 多一次 verilator 编译（~10.66 s/pair）** ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **⇒
⇒ 「宁可多编译一次，也不删断言」** ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ ⇒ 【断言强度 MUST NOT
被判据绑架】的正解**。

⇒ ⇒⇒ **判别力的【双向】实测证明**（批次 B1）：

| 方向 | 命令 | 实测 |
|---|---|---|
| **正向** | `./build/tests/test_assert_behavior` | **10 assertions / 10 test cases** ⇒ **10 == 2 × 5 pair** ✔ |
| **负向** | `env PATH=/usr/bin:/bin ./build/tests/test_assert_behavior` | **5 assertions / 10 test cases** ⇒ **5 ≠ 10** ⇒ ⇒ **【能识别 skip】** ✔ |

⚠️ **用例数【不能】作判据**：负向时用例数**不变**（10）⇒ ⇒ 因为 macro 的
verilator 半边在缺席时**仍然注册**（只 WARN + return）⇒ ⇒ ⇒ **⇒ 判据必须
用【断言数】而非【用例数】**。

## What Changes

### ① 新增 `tests/chlib/test_assert_behavior.cpp`（**4 个 dual pair**）

- DUT = `chlib::AssertChecker`（**纯组合**：`failed = enable & !condition`）
- ⚠️ **原 `tests/chlib/test_assert.cpp` MUST NOT 修改** ⇒ ⇒ **零回归**
- ⚠️ **期望值全部手推**（`failed = enable & !condition` 的真值表逐条推出）
  ⇒ ⇒ **不是**抄仿真输出
- ⚠️ ④ 号用例**有 2 条 `REQUIRE`**（拍 1 = 1 · 拍 2 = 0）⇒ ⇒ ⇒ **这正是
  旧判据会失效的场景** ⇒ ⇒ ⇒ **⇒ 新判据（XML per-TEST_CASE）覆盖它** ✔

### ② CMake 注册 + **4 个计数锚点**同步

| 锚点 | 前 | 后 |
|---|---|---|
| `CTEST_TOTAL` | 206 | **207** |
| `VERILATOR_E2E_TOTAL` | 39 | **40** |
| `DUAL_BACKEND_PARITY_TOTAL` | 30 | **31** |
| `DUAL_BACKEND_PARITY_PAIRS` | 185 | **189** |
| `tests/chlib/test_*.cpp` | 31 | **32** |

### ③ plan 的**成本模型二次订正** + **验收模板判据改写** + **墙钟账本回填**

## 为什么不做什么（Non-Goals）

- ❌ **不**转 `test_assert` 的 2 个**宏用例** ⇒ ⇒ 理由见筛选门槛 ③
  （断言 C++ 侧打印 ⇒ 双后端无意义 + stdout 全局状态会互相干扰）；
- ❌ **不**动其余 7 个候选文件 ⇒ ⇒ 门槛 ①（局部 DUT 需提升 ⇒ ⇒ 与
  「原文件 MUST NOT 修改」冲突）⇒ ⇒ ⇒ **共享 fixture 方案（oracle 建议）
  是独立议题** ⇒ ⇒ **本 change 不顺手做**；
- ❌ **不**改 objective 的 Go 判据（**planner 职责**）⇒ ⇒ ⇒ 数字已交
  （plan §6.1）；
- ❌ **不**改 capability `dual-backend-test-coverage` 的排除清单
  ⇒ ⇒⇒ 其中「Catch2 SECTION 重入」措辞待 planner 复核（**不在本 change
  擅自改契约**）。

## Impact

- **改动文件**：`tests/chlib/test_assert_behavior.cpp`（新）·
  `tests/CMakeLists.txt`（+1 个 `add_dual_backend_test`）· `AGENTS.md`
  （4 锚点 + 4 叙述值）· plan（成本模型 + 判据 + 账本）· 本 change 工件
- **爆炸半径**：**极小** —— 1 个新测试文件 + 1 行 CMake
- **行为变更**：**零**（原文件 MUST NOT 修改 ⇒ ⇒ 既有语义逐位不变）
- **风险**：**低** —— ① 「verilator 半边被 skip ⇒ ⇒ 假绿」⇒ ⇒ ⇒ **已用
  新判据（XML per-TEST_CASE 对比）核验**；② 纯组合 DUT ⇒ ⇒ **无 clk / 无
  rst_n 风险** ⇒ ⇒ ⇒ **⇒ 批次 C 的 AXI DUT 才有 `rst_n` 必需端口的风险**
  （oracle 提醒 ⇒ ⇒ 已在 plan 的批次 C 备注里）
- **计数锚点**：**+1** ctest 条目 · **+1** dual 源文件 · **+4** pair

## 验收实测（2026-11-01）

```
$ ./build/tests/test_assert_behavior          （真实 HOME，冷缓存）
All tests passed (10 assertions in 10 test cases)
  ⇒ ⇒ 10 = 2 × 5 pair ✔   ⇒ ⇒ **verilator 半边真跑了**
  ⇒ ⇒ ⚠️ **「TEST_CASE 数」本身【不能】证明这一点**（缺席时用例数不变）

$ env PATH=/usr/bin:/bin ./build/tests/test_assert_behavior   （负向）
All tests passed (5 assertions in 10 test cases)
  ⇒ ⇒ **5 ≠ 10** ⇒ ⇒ **【判据能识别 skip】** ✔

$ 假 HOME 全冷（逐 0.01 s 打点）
  0.02s 写源码 → 0.23s elaboration → 10.89s 编译+链接完成（10.66 s/pair）
$ 假 HOME 第二次（全热）⇒ ⇒ **0.6 s** ⇒ ⇒ 5 pair 全命中缓存
```

⚠️ **墙钟**：批次 B1 = **44.6 s**（真实 HOME 冷缓存）⇒ ⇒ 已回填 plan §5 账本
⇒ ⇒ 累计 **58.1 s** ⇒ ⇒ **CI 45 min 预算余量 44.0 min**
