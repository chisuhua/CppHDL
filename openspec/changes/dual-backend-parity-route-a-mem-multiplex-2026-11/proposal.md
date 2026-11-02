# 提案：路线 A（D 批）第 3 个 —— `ch_mem` 的两个**新 codegen 形态**进双后端 parity

- **依据**：`.rddf/plans/dual-backend-full-coverage-2026-11.md` §4 **D 批** + §4 的「可转性 4 问」前置条件
- **规格影响**：**无 spec 变更**（`.openspec.yaml` 声明 `skip_specs: true`）
- **规模**：1 个新测试文件（2 个 `DUAL_BACKEND_TEST_PAIR`）+ 1 条 CMake 注册 + 计数锚点
- **关联**：`objective-test-coverage-100pct` · capability `dual-backend-test-coverage`

## Why

`tests/test_mem_timing.cpp`（7 个用例）是本批唯一还没做的 A 类目标。逐个过 4 问后，
**只有 2 个形态**带来新覆盖 —— 其余 5 个已被既有 dual 文件覆盖：

| 原用例 | 4 问结论 |
|---|---|
| `:535` sread clears its value while enable low | ❌ **重复** —— 禁读拍清零已被 `tests/test_mem_sread_disable_dual.cpp` 覆盖 |
| `:389` / `:435` 写后读 / 顺序写读 | ❌ **重复** —— 已被 `tests/test_mem_comb_read_dual.cpp`（跨拍/同拍写后读 + `we=0` 不写）覆盖 |
| `:306` Initialized Memory（aread + init） | ❌ **重复** —— aread → `ch_out` 数据通路已被 `test_mem_connection_dual.cpp` pair 2 覆盖 |
| `:231` Dual Port（读写地址分离） | ⚠️ 与下面的「两个读端口」部分重叠 ⇒ 不重复做 |
| `:182` Single Port | ✅ **新形态（部分）** —— 它的 `select()` 里**嵌读端口**是**任何既有 dual 文件都没有**的 codegen 路径 |
| `:358` Multi-Read Port | ✅ **新形态** —— 既有 dual 文件里每个 DUT **只有一个**读端口 |

## 为什么这两个形态值得钉（具体理由）

- **mux 的操作数是读端口的 `data proxy`，不是那个「虚构」的 `read_port` 节点**：
  `read_port::impl()` 返回 `port_impl_->data_output()`（`include/core/mem.h:45`）⇒
  codegen 必须引用**已声明**的那个 proxy 信号；一旦它遍历过头引用 `read_port`，
  生成的是**未声明信号** ⇒ Verilator 编译失败 ⇒ 而 interpreter 走 `data_map`
  **不经过这些分支** ⇒ ⇒ **单后端测试结构性看不见**。
- **同一 mem 上两个读端口**：两个 proxy 同时驱动同一 mem，codegen 的
  `print_mem_read_port` 会被调用两次 ⇒ ⇒ 任何「只处理第一个端口」类的缺陷
  （串线 / 复用同一个 proxy 名 / 只声明一个 proxy）在单端口 DUT 上**不可见**。

## ⚠️ 判据边界（如实登记）

- 本文件**对「0/1-cycle 采样语义」不判别**（T2）：地址来自 TB 驱动的输入端口 ⇒
  `tick()` 前已写入 `data_map_` ⇒ 沿上采样到的已是新地址 ⇒ 两种时序观测值相同。
  时序维度由 `tests/test_mem_read_timing_dual.cpp`（**寄存器驱动**形态）负责 ⇒ 不重复。
- ⚠️ **但这条边界不能推广成「零判别力」** —— 见下 N4。

## 🔴 N4 挖出的**框架层不变量**（本 change 最重要的产出，不是代码）

我按上述 T2 边界**预测** N4（把 `aread` 也发射成 `always_ff`）是**无效变异（恒绿）**，
**实测转红**。三步实证把机制查清：

1. 把该变异下生成的 `top.v` 用**独立简易 harness** 驱动（同 rst 脉冲 + 同 clk 0→1→0 序列）
   ⇒ **读到正确值**（`0xc3` / `0x90` / `0xa1` / `0x3a`）⇒ ⇒ **生成的 Verilog 语义本身没错**
   （含 dangling-else：`if (!rst) if (1'b1) proxy <= mem[a]; else proxy <= 0;` 的
   `else` 按 SystemVerilog 规则绑**内层** `if`）。
2. 所以错在后端驱动路径：`Simulator::tick()` 里的 `eval()`（⇒ `eval_sequential()` ⇒
   Vtop 时钟 0→1→0）**只在 `default_clock_instr_` 存在时**才被调用。
3. 而 `src/simulator_init.cpp:153` 有一行
   `if (sequential_instr_list_.empty()) default_clock_instr_ = nullptr;`
   ⇒ ⇒ **无时序节点的设计**（pair 2 的 DUT 只有 `aread` + `ch_out`）⇒ 时序表为空
   ⇒ 时钟**从不翻转** ⇒ `always_ff` **永不触发** ⇒ 读出 **0**（且**无任何报错**）。
   这也解释了为什么 pair 1（用 `sread` ⇒ 时序表非空）在同一变异下**完好**。

⇒ ⇒ **不变量**：「Verilog codegen 为该设计发射了任一 `always_ff`」 ⟺
「Simulator 的 `sequential_instr_list_` 非空（⇒ 时钟会 toggle）」。
两者一旦漂移 ⇒ 生成的时序逻辑**静默不工作**（读 0，无报错）。

⚠️ **本 change 不修也不追**：是否有**其它**构造也在漂移（某个组件发射 `always_ff`
但整个设计无时序节点）**未审计** ⇒ 需**独立立项**（那是一次全仓 codegen × instr 分类的
交叉审计，不是本批的范围）。本文件只是把该不变量**钉成可回归的判据**（N4 常驻）。

## 判别力（变异证据；每条先确认仍可编译；还原用 `cp` 后重建）

| 变异 | 内容 | 结果 |
|---|---|---|
| N1 | sync 读端口**恒读地址 0** | 转红（pair 1 `:180`） |
| N2 | 写口**忽略 `we`** | 转红（pair 1 `:180`） |
| N3 | 写口**忽略地址** | 转红（pair 1 `:180`） |
| N4 | `aread` 也发射成 `always_ff` | 转红（pair 2 `:197`，机制见上） |

⇒ **4/4 全抓**。⚠️ **N4 的结论与我自己的预测相反**，这是本批「预测必须被实测检验」的
又一实例 —— 若只做「预期会红的变异」，就会漏掉这条框架层不变量。

## What Changes

| 文件 | 内容 |
|---|---|
| `tests/test_mem_multiplex_dual.cpp`（**新**） | 2 pair：`MuxOverReadPortDut`（`select()` 里嵌读端口 + 写使能，6 步序列 = 6 断言/后端）· `MultiReadPortDut`（同一 mem 两个读端口，4 向量 × 2 端口 = 8 断言/后端）⇒ **14 断言/后端** |
| `tests/CMakeLists.txt` | 1 条 `add_dual_backend_test` |
| `AGENTS.md` | 4 个计数锚点 + NOTES 计数 + 本批 NOTES 条目 |
| 计数锚点 | `CTEST_TOTAL` 229 → **230** · `VERILATOR_E2E_TOTAL` 61 → **62** · `DUAL_BACKEND_PARITY_TOTAL` 52 → **53** · `PAIRS` 249 → **251** · non-perf 218 → **219** · `tests/test_*.cpp` 129 → **130** |

## Verification（前瞻 · 可勾选）

- [x] **N2′**：**28 == 2 × 14**（verilator 半边真的跑了）
- [x] 变异 **4/4 全抓**（每条先确认仍可编译；还原用 `cp` 后重建）
- [x] 全部输入端口**逐个显式驱动**
- [x] 期望值**手推**（手推一次通过；公式与向量表写进文件头）
- [x] **原文件 `tests/test_mem_timing.cpp` MUST NOT 修改**（已遵守）
- [x] N4 的机制**已查清并写进文件头**（不是「记为未定」）
- [x] 全量重建后 `ctest -LE perf` 零回归
- [x] 计数锚点同步且两个计数护栏双绿
- [x] 归档：`--skip-specs`（`.openspec.yaml` 已显式声明）
