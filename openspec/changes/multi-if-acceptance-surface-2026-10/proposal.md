# 提案：`multi_if` 的接受面 —— 五种形态逐一处置（实施 ADR-052）

- **架构依据**：**ADR-052**（`docs/adr/ADR-052-multi-if-acceptance-surface.md`，Accepted）
- **证据**：`docs/architecture/2026-10-30-n4-n6-evidence.md`（全部实测数据；本 proposal 只引不复制）
- **规格影响**：capability `chlib-conditional-semantics` ⇒ **MODIFIED ×1 + ADDED ×1**
  ⇒ 归档**禁** `--skip-specs`
- **规模 / 风险**：**小**（单文件 `include/chlib/if.h` + 一个新护栏脚本 + 测试）；
  但**含破坏性变更**（见 Impact）

## Why

`chlib::multi_if<T>` / `if_then` 是 ADR-051 确立的**唯一**表达式形式条件构造。
实测其接受面存在**五种**「框架放行了却无法正确执行」的形态：

| # | 形态 | 实测 | 严重度 |
|---|---|---|---|
| **A** | 首分支窄、后续宽 | 编译通过，**静默丢高位**（`assign top_bits = top__add_add[7:0]`） | 高（数值错） |
| **B** | **单分支**（无 `elif` / `else_`） | 编译通过，**条件被完全忽略**（结果恒 `=x`） | **最高（控制流不生效）** |
| **C** | `T` 推导成 **port 类型** | 编译失败（模板深渊） | 中（可用性） |
| **D** | `T` = **纯算术类型** | ctor 放行、转换处失败（**两次**用户定义转换） | 中 |
| **E** | 显式 `T` **过窄** | 编译通过，**静默截断**（`ch_uint<8>` → `ch_uint<1>`） | 高 |

⚠️ **形态 B 是「条件被忽略」缺陷家族的第三个实例**（一 = `_if` 双重失效 / ADR-051；
二 = `uart_tx` 的 FSM / ADR-050）⇒ 严重度高于数值丢位。

**为什么长期无人发现（假绿机制）**：截断在**进入 mux 之前**插入
（`add_add(9b) → bits(8b) → mux`）⇒ 两后端**同构** ⇒ 双后端 parity 对这一类缺陷
**结构性失明**；Verilog 侧 `WIDTHTRUNC` 又被**全局关闭**
（`src/codegen_verilog.cpp:299-300`，理由是 CH_MEM select-tree 的已知宽度差 ——
**正当不可撤**）⇒ 唯一能「变响」的位置是 **C++ 前端**。

⚠️ **规范层的假绿**：capability `chlib-conditional-semantics` 的
`#### Scenario: 表达式形式的分支值 MUST 同类型` 其 `THEN` 写
「`multi_if<T>` 的类型推导 MUST 失败（⇒ 无法编译）」—— **该陈述已被实测证伪**
（今天编译通过、静默丢位）⇒ 文档声称的保护并不存在。

## What Changes

### 范围内

1. **`include/chlib/if.h`**（ADR-052 D1–D6）：
   - `ctor` / `elif` / `else_` **模板化**（`<typename U>` + `if constexpr`）——
     ⚠️ **硬前置**：现形参是 `const T &` ⇒ 实参在进入函数体前**已完成到 `T` 的转换**
     ⇒ 原始值 / `W` 不可恢复 ⇒ 两套判据 MUST 在**转换之前**拦截；
   - **D1** 硬件分支 `ch_width_v<U> <= ch_width_v<T>`（更窄 ⇒ 零扩展；更宽 ⇒
     `static_assert`，消息含两个宽度 + `if_then<ch_uint<W>>` 指引）；
   - **D2** `ch_literal_impl<V,W>` 按 `W` 走**静态**判据；裸算术走**建图期值域**检查
     （`CHFATAL_EXCEPTION`）；负数算术 ⇒ 拒绝；
   - **D3** `operator T()` 在 `branches_.size() < 2` 时 ⇒ `CHFATAL_EXCEPTION`
     （单分支的条件结构上不可能被用到）；
   - **D4** 首分支为 port ⇒ `T` **归一化**为 `port::value_type`
     （`ch_bool` 端口经 `lnode<ch_bool>` 兜底）；`T` 为算术 ⇒ `static_assert` 拒绝；
   - **D5** 结果类型恒为 `T`（`select` 在 `result_width<=1` 时返回 `ch_bool` ⇒ 须收敛）；
   - **D6** 六种写法 MUST 保持合法（防过度实现）。
2. **新增护栏** `scripts/verify_multi_if_width.sh`（9 条判据的机械核验 +
   **中性化对照** + fail-closed），挂 CI 两处（主路径 + `--self-test`）。
3. **测试**：编译期 fixture（编译失败 + 报错文本 grep）× 正向 fixture × 运行期金标。
4. **契约**（见 Capabilities）。

### 范围外

见 ADR-052 §6 Non-Goals（`ch_uint` 全局窄化 / 全局 `WIDTHTRUNC` 移除 /
`operator>` 不可用 / 顶层 `ch_in` null 节点 / `if_demo.cpp` 归位 / E1 / E3）。

## Acceptance

- [ ] ADR-052 的 **D1–D6** 逐条落地，且**五种形态**各自被处置；
- [ ] `scripts/verify_multi_if_width.sh` 存在、挂 CI 两处、**每个「拒绝」fixture
      配中性化对照**（注释对应 `static_assert` ⇒ fixture 通过）；
- [ ] **V7 positive 清单全绿**（六种写法 ⇒ 防过度实现）；
- [ ] **V4-a / V4-c 的 fixture 在 GCC 与 Clang 下各编一次**（requires 行为可能分叉）；
- [ ] 变异验证（每条先确认编译成功）：zext→sext ⇒ 红 · 去值域检查 ⇒ 不抛 ⇒ 红 ·
      删 `size()==1` 检查 ⇒ 不抛 ⇒ 红 · 去归一化 ⇒ 退回模板深渊 ⇒ 红；
      ⚠️ **如实登记无判别力的变异**：「zext 换恒等存储」**不红**（窄值存宽 `T` 天然无损）；
- [ ] `ctest -LE perf` 零回归 · `run_all_samples.sh`（含 `conditional_demo` 自检）·
      `run_all_ported_tests.sh` 全绿 · 全部护栏（含 `--self-test`）exit 0；
- [ ] 计数锚点核对并同步（新测试文件 ⇒ ctest 条目数可能 +N）；
- [ ] 归档（**禁** `--skip-specs`）。

## Capabilities

- `chlib-conditional-semantics` ⇒ **MODIFIED ×1 + ADDED ×1**：
  - **MODIFIED**「条件构造的**权威表面** MUST 唯一」：删除已被证伪的
    `#### Scenario: 表达式形式的分支值 MUST 同类型`（其 `THEN` 是假陈述），
    E1 / E3 两条 Scenario 保留；
  - **ADDED ×1**「`multi_if` 的接受面 MUST 完备（要么语义正确、要么编译期拒绝）」
    + 6 个 Scenario（A / B / C / D / E / 防过度实现）。

## Impact

- **改动文件**：`include/chlib/if.h` · **新** `scripts/verify_multi_if_width.sh` ·
  测试（编译期 fixture + 运行期用例）· `openspec/specs/**`（归档时合并）· `.rddf/**`；
- **爆炸半径**：**小**（`multi_if` 的活消费者只有 `samples/conditional_demo.cpp`，
  它用的是**推导 `T`** + 每分支显式 `ch_uint<8>(...)` 收敛 ⇒ 在 D6 positive 清单内保活）；
- ⚠️ **破坏性变更（外部使用者）**：
  - **B**：`if_then(c, x)`（无 `else_`）今天能编译 ⇒ 之后拒绝；
  - **D**：`if_then(c, 1)` 今天能构造 ⇒ 之后 ctor 拒绝（须改 `if_then<ch_uint<N>>(c, 1)`）；
  - **E**：`if_then<ch_uint<1>>(c, a)` 今天能编译（静默截断）⇒ 之后拒绝；
  - **C**：`if_then(c, io().a)` 从「编译失败」变「**可用**」（正面一侧，但仍改了接受面）；
- **风险**：**中** —— 主风险是「过度实现」（把「更宽 ⇒ 拒绝」误写成「不等 ⇒ 拒绝」）
  ⇒ 已用 D6 + V7 的 positive 清单钉住。

## Non-Goals

与 ADR-052 §6 一致（6 条），不在本 change 重复。
