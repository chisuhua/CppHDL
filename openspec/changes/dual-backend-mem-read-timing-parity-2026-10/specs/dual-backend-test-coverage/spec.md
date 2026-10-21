# Capability: dual-backend-test-coverage

## MODIFIED Requirements

### Requirement: 适用范围排除清单 MUST 写明不可 dual 的测试类别

Capability MUST 明确划分"不可 dual"测试类别，避免迁移工作浪费与无意义的"dual 失败"误判。

#### Scenario: 不可 dual 测试类别

- **WHEN** reviewer 阅读本 capability spec 的排除清单
- **THEN** 下列测试类别 MUST 标注为 "interpreter-only"，不得 dual：
  1. **纯编译期 / 静态断言测试**（如 `test_reg.cpp` 的 `STATIC_REQUIRE` 块、concept 测试、`test_precise_width` / `test_source_info` / `test_bv_cmp` / `test_bitvector` 等无 Simulator 调用的）
  2. **裸 context 测试**（如 `tests/chlib/test_arithmetic.cpp:13-39` 直接在裸 context 构图，无 `ch_device` / `ch_module` / `ch_out` 端口）
  3. **>64-bit 端口设计**（VerilatorBackend 物理拒收）
  4. **Catch2 SECTION 重入测试**（macro 双 run 结构下 SECTION 会各跑一次，行为语义不一致）
  5. **STATIC_REQUIRE / 编译期元函数测试**（无 runtime 语义）

> ⚠️ **aread-based 设计已从排除清单移除**：该排除项的理由是「verilator 0-cycle 与
> simulator posedge 的设计 drift 导致必然 parity 失败」，而该 drift 已由
> `sim-mem-read-timing-align-2026-10` 消除（Simulator 的 `ch_mem::aread` 现为
> 0-cycle，与生成的 `assign proxy = mem[addr];` 一致）。
> ⇒ aread-based 设计**现在可以**走双后端 parity，且**应当**走 —— 见
> 「mem 读端口时序 MUST 有双后端 parity 覆盖」这条 Requirement。

#### Scenario: 排除清单由 spec 引用 grep 模式定义

- **WHEN** builder 阶段执行"分类扫描"以填充测试名单
- **THEN** MUST 用下列 grep 命令锁定可 dual 候选：
  ```
  grep -l "ch_device\|ch_module" tests/test_*.cpp tests/chlib/test_*.cpp | \
    xargs grep -L "STATIC_REQUIRE" | \
    xargs grep -L "ch_out<ch_uint<[0-9]\{3,\}>>"
  ```
- **AND** 输出名单 MUST **不再**用 `grep -L "aread"` 过滤候选
  （aread 的 drift 已消除，aread-based 设计现在是**合法的** dual 候选）
- **AND** 输出名单中**未排除** dmem stub（dmem stub 是 stub-wide 设计，按 cpu-cache-subsystem spec §R5 分类处理）

### Requirement: cache 双后端 MUST 按 Sync/Async 分类（Sync 走 parity / Async 走 golden-value）

cache 双后端集成时 MUST 区分 `ICacheSync` / `DCacheSync`（1-cycle sread，default）与 `ICacheAsync` / `DCacheAsync`（0-cycle aread）：
- **Sync** dual-run 走 parity（verilator 与 interpreter 逐周期一致）
- **Async** dual-run **可以**走 parity（aread 的 0-cycle 语义已与 Verilog 对齐，
  两后端 MUST 逐周期一致）；golden-value 路径保留为**可选**，用于刻意比较不同
  cycle 数的场景

理由：`sim-mem-read-timing-align-2026-10` 已消除 `mem-read-timing` spec 登记过的
「aread 0-cycle vs simulator posedge 1-cycle skew」drift —— Simulator 的
`ch_mem::aread` 现为 0-cycle，与生成的 `assign proxy = mem[addr];` 一致，
⇒ 「Async 强制 parity 会永远红」这一前提**已不成立**，禁令解除。

> ℹ️ 本次**不**要求把既有的 Async cache 双后端测试改造成 parity（cache 子系统
> 的独立工作）；此处只**解除禁令**，使后续改造不被 spec 挡住。

#### Scenario: Sync cache test 走 parity

- **WHEN** fixture 用 `ICacheSync`（同步 L1 imem）
- **AND** TEST_CASE 跑 `DUAL_BACKEND_TEST`
- **THEN** macro MUST 逐周期比对 `data` / `miss` / `ready` 等顶层端口
- **AND** 填充完成后 hit 路径两后端 MUST 在同 cycle 反映缓存数据

#### Scenario: Async cache test 走 parity 或 golden-value

- **WHEN** fixture 用 `ICacheAsync`（TCM-style 异步）
- **AND** TEST_CASE 跑 `DUAL_BACKEND_TEST`
- **THEN** MUST 逐周期比对顶层端口（aread 的 0-cycle 语义两后端一致）
- **AND** MUST NOT 依赖「两后端存在 1-cycle skew」这一**已消除**的前提
- **AND** 若采用 golden-value 路径（`DUAL_BACKEND_GOLDEN_TEST` 或
  `DUAL_BACKEND_TEST` 的 `[backend-shift-N]` tag），MUST 跑 verilator N+1 cycle
  后断言终值、interpreter M cycle 后断言终值（M != N）

## ADDED Requirements

### Requirement: mem 读端口时序 MUST 有双后端 parity 覆盖

`ch_mem::aread`（0-cycle）与 `ch_mem::sread`（1-cycle）的时序契约 MUST 有**双后端
parity** 测试覆盖：interpreter 与 verilator 在**同一 cycle** MUST 观测到**同一个值**。

理由：`mem-read-timing` capability 的核心主张是「Simulator 与生成的 Verilog 完全
对齐」。单后端测试只能证明「interpreter 自己一致」，**证明不了**「interpreter ==
Verilog」——而后者才是该 capability 的承诺。

#### Scenario: aread 的 0-cycle 语义有双后端 parity 测试

- **WHEN** 一个 DUT 用 `ch_mem::aread` 读内存，并把读数据接到**顶层 `ch_out` 端口**
- **AND** 该 TEST_CASE 走 `DUAL_BACKEND_TEST_PAIR`（interpreter + verilator）
- **THEN** 读地址 MUST 由**寄存器**驱动（`ch_reg` → `mem.aread(reg)`）
- **AND** 该测试 MUST 断言：地址寄存器提交新地址的**同一 tick**，两后端的
  `ch_out` 读数据 MUST 都是**新地址**对应的值
- **AND** 该测试 MUST 断言：`ch_mem::sread` 在同一条件下仍是**旧地址**的值（1-cycle）

> ⚠️ **MUST 用寄存器驱动的地址**：地址直接来自顶层输入端口时，两后端在修复前
> 就都是同拍可见 → 这样的断言**区分不了 0-cycle 与 1-cycle** → 锁不住任何东西。

#### Scenario: 该 parity 测试 MUST 对 posedge 回归敏感（非空转）

- **WHEN** interpreter 侧的 async read port 被退回 posedge 求值
  （即 `sequential_instr_list_`）
- **THEN** 该 parity 测试的 interpreter 半边 MUST 失败
  （interpreter 滞后 1 拍 vs verilator 0-cycle）
- **AND** 该测试 MUST NOT 是「恒绿」的空转断言

#### Scenario: 读数据 MUST 走顶层 ch_out 端口观测

- **WHEN** 该 parity 测试观测读数据
- **THEN** MUST 通过**顶层 `ch_out` 端口**观测
  （VerilatorBackend 无法通过 dlopen accessor table 观测深层的 mem / reg-file 状态）
- **AND** 顶层 `ch_out` 端口位宽 MUST ≤ 64（VerilatorBackend 的硬上限）
