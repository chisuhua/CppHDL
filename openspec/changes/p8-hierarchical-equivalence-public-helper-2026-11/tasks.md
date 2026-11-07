# 任务清单：`p8-hierarchical-equivalence-public-helper-2026-11`

> 判据来源：proposal.md §Acceptance（摘要）· capability delta
> `hierarchical-equivalence-public-helper`（**ADDED ×4**）· improvement 草稿 v2
> `.rddf/improvements/p8-hierarchical-equivalence-public-helper-2026-11.md` §3
> ⚠️ **新增公开 API 面** ⇒ ⇒ 归档前须过 **critic**（2 轮预算）。

## T1 公共 API（`include/hierarchy_equivalence.h` · 新增）

- [x] 1.1 新增 `include/hierarchy_equivalence.h` · 命名空间 **`ch::hier_eq`**
      （★ 头名 ≠ 命名空间 ⇒ ⇒ 文档 MUST 两处都写）
- [x] 1.2 `struct DrivenInput`（`name` + `set` 回调）⇒ ⇒ 回调 MUST 走
      `set_input_value(port, <纯整数>)`（`<<=` ⇒ 假分歧 ⇒ ⇒ 写进头注释）
- [x] 1.3 `struct Observation`（`name` + `read` 回调 + `node_id`）
- [x] 1.4 `struct ProductSet`（`ok` / `error` / `hier_path` / `ports` / `signal_map`）
- [x] 1.5 `struct Result`（`ok` / `equal` / `error` / `first_mismatch` /
      `mismatch_count` / **`flat_trace`** / `flat_product` / `hier_product`）
      ⇒ ⇒ **`trace` 改名 `flat_trace`**（★ 实测：只 push 扁平侧 ⇒ ⇒ 名字语义不符）
      ⇒ ⇒ **取消 `Trace` 类型别名**（类型内联写）
- [x] 1.6 `ProductSet emit(Component&, work_dir, granularity = RespectDeclarations)`
- [x] 1.7 `Result compare(ctx, work_dir, hier_path, ports, inputs, observations, stimulus)`
- [x] 1.8 **模板工厂** `make_observation(name, port, read)` ⇒ ⇒ 由 helper 填 `node_id`
      ⇒ ⇒ **用户永不接触 `impl()`** ⇒ ⇒ ⇒ 公共文档里不出现 `impl()`
- [x] 1.9 公共头 **include 面** MUST **无** `#error` 风险
      ⇒ ⇒ **MUST NOT** 拖进 `core/verilator_backend.h`（★ `__cpp_lib_expected` 缺失时
      **显式 `#error`** ⇒ ⇒ 会破 macOS/Windows 的 `#include` 契约）
- [x] 1.10 公共头 **自包含**（`scripts/verify_header_self_contained.sh` PASS）
- [x] 1.11 公共头 **ODR 安全**（`scripts/verify_header_odr.sh` PASS）
- [x] 1.12 公共头 **不进** `ch.hpp` 聚合器 ⇒ ⇒ 文档 MUST 给**显式** include
      （★ 与 P6 的 `hierarchy_analysis.h` 同款既有惯例）
- [x] 1.13 **只 include 这一个头**就够用全部公共符号（★ MUST NOT 逼用户再手动 include
      `hierarchy_verilog.h` 才能写 `ProductSet`）

## T2 实现迁移（`src/`）

- [x] 2.1 新增 `src/hierarchy_equivalence_impl.h`（**内部头** ⇒ ⇒ **不进 `include/`**
      ⇒ ⇒ 仓内先例：`src/hierarchy_chain.h` 等 4 个）⇒ ⇒ 迁入现有
      `ch_test::hier::impl` 的全部实现（产物生成 · backend 装配 · 逐拍驱动 ·
      目录创建 · 文件读取）
- [x] 2.2 新增 `src/hierarchy_equivalence.cpp` ⇒ ⇒ **登记进** `CMakeLists.txt` 的
      `CH_SOURCES` ⇒ ⇒ ⇒ 公共符号定义在此
- [x] 2.3 `tests/hier_equivalence.h` 改为「include 公共头 + alias」⇒ ⇒ 既有
      `ch_test::hier::*` 调用点**零改动**
- [x] 2.4 **零行为回归**判据：既有 3 个层次化产物（`bubble.v` / `row1_implicit.v` /
      `three_layers.v`）**逐字节不变**（对照 HEAD 的 `cmp` = 0 · sha256 + 重建 + 复验）
      ⇒ ⇒ ⇒ `src/hierarchy_verilog.cpp` 的**行为**零改动

## T3 fail-closed（★ **新增能力** · 不是既有行为）

- [x] 3.1 `observations` 为空 ⇒ `ok=false` + 前缀 `empty-observations`
- [x] 3.2 `stimulus` 为空 ⇒ `ok=false` + 前缀 `empty-stimulus`
- [x] 3.3 观测集非空但**无任何信号随激励变化** ⇒ `ok=false` + 前缀 `no-varying-signal`
      ⇒ ⇒ **实现**：用**两组**激励跑 ⇒ ⇒ 判「某观测信号的取值集合在两组下不同」
      ⇒ ⇒ ⇒ ★ **代价**：跑两遍（已在 proposal 登记为新增工作量）
- [x] 3.4 产物生成失败 ⇒ `ok=false` + 前缀 `emit-failed`
- [x] 3.5 派生端口集为空 ⇒ `ok=false` + 前缀 `port-set-empty`
- [x] 3.6 **>64bit 端口** ⇒ `ok=false` + 前缀 `port-width-64-exceeded`
      ⇒ ⇒ **点名端口 + 位宽**（★ `VerilatorPortSpec::bitwidth` 已有 ⇒ ⇒ 可判）
- [x] 3.7 **DUT 无顶层 `ch_out`** ⇒ `ok=false` + 前缀 `no-top-level-output` + 点名成因
- [x] 3.8 扁平 / 层次化 backend 初始化失败 ⇒ `ok=false` + 前缀 `backend-init-failed`
- [x] 3.9 **verilator 不可用** ⇒ `ok=false` + 前缀 `verilator-unavailable`
      ⇒ ⇒ 正文 MUST 含**正确构建命令** `cmake --build build --target verilator`
      ⇒ ⇒ ⇒ **MUST NOT** 建议 `apt install verilator`（★ 仓内已知陷阱：那条路**是错的**）
- [x] 3.10 **被观测信号在层次化顶层不可见** ⇒ `ok=false` + 前缀 `observation-not-visible`
      + 点名信号 ⇒ ⇒ ⇒ 实现 = 「`node_id` ∈ `signal_map` ∧ `hier_top_port_name` **非空**」
      ⇒ ⇒ ⇒ ★ 「盲区侧」的对账**留在仓内测试**（★ 需要枚举 DUT 的**全部** `ch_out` ⇒ ⇒
      那是 DUT 的知识 ⇒ ⇒ helper 拿不到）
- [x] 3.11 所有错误串 MUST 以**稳定前缀**开头 ⇒ ⇒ 前缀清单 MUST 写进头注释 / 文档

## T4 测试

- [x] 4.1 既有 `tests/test_hierarchy_equivalence.cpp` 的**断言集合与期望值不变**
      ⇒ ⇒ 9 用例 / **414 断言**仍全绿 ⇒ ⇒ `git diff` MUST 只含 `trace`→`flat_trace`
      的**机械改名**
- [x] 4.2 **out-of-tree 真实消费方**（★ D1：`-fsyntax-only` 只证明语法 ⇒ ⇒ 不证明链接 /
      不证明默认参数可用 / 不证明能跑）⇒ ⇒ 新增 `tests/test_public_equivalence_api.cpp`
      ⇒ ⇒ 只 `#include "hierarchy_equivalence.h"` + 链接 `libcpphdl` ⇒ ⇒ **真编译 +
      真链接 + 真运行** ⇒ ⇒ ⇒ 它同时充当文档示例的**来源**（★ 防文档腐烂）
- [x] 4.3 4.2 的用例 MUST 覆盖**全部**公共符号 ⇒ ⇒ ⇒ **能力判据**（★ A2'：少一项 ⇒ 红）
- [x] 4.4 空 `observations` / 空 `stimulus` / 无随激励变化的信号 ⇒ ⇒ 三条 fail-closed
      各有独立断言
- [x] 4.5 >64bit 端口 ⇒ ⇒ `ok=false` + 错误串**点名端口与位宽**
- [x] 4.6 无顶层 `ch_out` 的 DUT ⇒ ⇒ `ok=false` + 点名成因
- [x] 4.7 verilator 不可用 ⇒ ⇒ `ok=false` + 前缀 `verilator-unavailable`
      ⇒ ⇒ ⚠️ 判据 = **错误串含正确构建命令** ⇒ ⇒ **MUST NOT** 含 `apt install verilator`
- [x] 4.8 观测一个**层次化顶层不可见**的信号 ⇒ ⇒ `ok=false` + 前缀
      `observation-not-visible`

## T5 护栏（`scripts/verify_hierarchical_p8.sh` 改口径）

- [x] 5.1 **R1 改口径**：「`include/` 下**文件名**匹配 `*equivalence*` 或 `*hier_eq*` 的文件
      ⇒ MUST **逐字等于** `include/hierarchy_equivalence.h`」（★ **全称否定 + 单一字面豁免**
      ⇒ ⇒ 新判据 ⊇ 旧判据 ⇒ ⇒ **不是**换成存在性白名单）
- [x] 5.2 **R2 双向核对**：内部实现符号 deny-list **+**「每个 deny 符号 MUST **仍存在于
      代码库某处**」（★ 否则改名后「grep 无命中 ⇒ 绿」⇒ ⇒ 判据静默失效）
- [x] 5.3 **R2' 新增**：「**声明** `namespace ch::hier_eq` 的文件 ⇒ MUST **恰好是**那一个」
      （★ 堵住「改名绕过」⇒ ⇒ 补 R1 的能力缺口）
- [x] 5.4 **R3 明确化**：公共头 MUST **确实存在**（★ **断言存在** ⇒ ⇒ MUST NOT 沿用旧脚本
      「不存在 ⇒ 说明没有 ⇒ 绿」的逻辑）
- [x] 5.5 **R4 拆分**：① **历史窗口**（原 R4，base/head **钉死** ⇒ ⇒ **逐字保留**）
      ② **新增活账**：P8 之后新增的公开头 MUST 登记在 `.rddf/checks/public-api-surface.md`
      ⇒ ⇒ **双向核对**
- [x] 5.6 `--self-test` **5 条投毒**，**名字写死** ⇒ ⇒ 逐条**非零退出**：
      ① 内部实现符号泄漏 ② 公共头**改名** ③ 公共头**缺失**
      ④ **多出第 2 个公共头且名字不命中** `*equivalence*`/`*hier_eq*`
      （★ 如 `include/hierarchy_compare_public.h` ⇒ ⇒ 否则红的是**旧**判据）
      ⑤ **公共头 `#include` 了内部实现头 / `src/` 里的头**（★ **R2 的盲区**）
- [x] 5.7 护栏正文含「**全称否定 + 单一豁免 ⇒ 新判据 ⊇ 旧判据**」的显式说明
- [x] 5.8 **CI 接线**：★ **实测** `verify_hierarchical_p8.sh` **当前不在**
      `.github/workflows/ci.yml` ⇒ ⇒ MUST 接线（⇒ ⇒ 否则「强度不减」只是一次性本地证据）

## T6 判别力自证（变异 · 每条先确认仍能编译）

- [x] 6.1 变异 ① 公共面**多**一个符号 ⇒ ⇒ 对应用例**转红**
- [x] 6.2 变异 ② `emit` 的端口集合改成**手写** ⇒ ⇒ ⚠️ **判据归属**：断言 MUST 落在
      「`ports` 与 `backend_port_set(emitted,"top")` **逐元素相等**」⇒ ⇒ **MUST NOT** 挂在
      「篡改产物转红」上（★ 对固定 fixture 手写集合**极易碰巧相等**⇒ ⇒ 挂在那边 ⇒ 该变异
      **不改变可观测行为**⇒ ⇒ 无效变异）
- [x] 6.3 变异 ③ 激励改用 `<<=` ⇒ ⇒ ⚠️ **判据归属**：属**初始化侧 fail-closed**
      （`ok=false`）⇒ ⇒ **不是** `equal=false`（★ `<<=` 让端口变内部驱动 ⇒ 初始化失败）
- [x] 6.4 变异 ④ 人为让两侧激励不同 ⇒ ⇒ `equal=false` + `first_mismatch` 非空
      ⇒ ⇒ ⇒ 证明**比较侧**的判别力（③ 只证明初始化侧）
- [x] 6.5 每条变异**还原后** MUST **逐字节比对 + 重建 + 复绿**（★ 还原源码 ≠ 复原 ⇒
      二进制也是状态）
- [x] 6.6 ⚠️ **变异脚本 MUST 检查 python 退出码**（★ 否则断言失败会**静默跳过**变异 ⇒ ⇒
      误判「无判别力」⇒ ⇒ 本仓 P6 已踩过）

## T7 文档

- [x] 7.1 `docs/usage_guide/05-component-design.md` 新增用户面小节 ⇒ ⇒ **显式** include
      ⇒ ⇒ **头名 ≠ 命名空间**（两处都写）⇒ ⇒ 端口口径表（根 `ch_in`/`ch_out` 恒在顶层 ·
      子组件 `ch_out` **不在**顶层 ⇒ ⇒ **MUST NOT** 观测）⇒ ⇒ 观测面 MUST 手写 +
      至少一个随激励变化 ⇒ ⇒ 激励 MUST 走 `set_input_value` ⇒ ⇒ 错误码前缀清单
- [x] 7.2 文档 MUST 明说：**与双后端 parity 互补不可替代**（对照表）
- [x] 7.3 文档 MUST 明说：只判「**变换**引入的分叉」；**两份产物同源 ⇒ 同构缺陷一起错**
- [x] 7.4 文档 MUST 明说：**Linux-only**（Verilator backend 的既成限制）
- [x] 7.5 文档 MUST 明说：**A 侧固定是扁平产物** ⇒ ⇒ 「A vs B」走**传递性**
- [x] 7.6 文档 MUST 明说：`ProductSet` **与其 ctx 绑定**（`node_id` 是 ctx 私有编号 ⇒
      不可跨 ctx 传递）
- [x] 7.7 文档 MUST 明说：**不提供 verilator 编译超时控制**（调用方自管）· **不承诺可重入**
- [x] 7.8 示例 **MUST 由真实测试源文件承载**（4.2 的那个）⇒ ⇒ 文档只**引用**它
      ⇒ ⇒ ⇒ 防文档腐烂

## T8 零回归 · 锚点 · 文档同步

- [x] 8.1 `ctest -LE perf` 全绿 · samples 全绿 · examples 全绿
- [x] 8.2 计数锚点：`CTEST_TOTAL` +1 · non-perf +1 · `VERILATOR_E2E_TOTAL` +1
      （★ 新用例跑真 verilator ⇒ ⇒ MUST 带 `verilator_e2e` 标签）
      ⇒ ⇒ ⇒ ⚠️ parity 两锚点**不变**（新用例**不是** `DUAL_BACKEND_TEST_PAIR`）
- [x] 8.3 根 `AGENTS.md` 的「WHERE TO LOOK」补一行（公共 helper 的入口）
- [x] 8.4 `openspec validate p8-... --type change --strict` PASS

## 审查（critic · **1 轮** ⇒ `APPROVE_WITH_CONDITIONS`）

- **轮次**：**1 / 2**（预算未用尽 · **0 BLOCKER** ⇒ ⇒ **不触发返工与复审**）
- **结论**：`mergeable: yes` · `scope_exceeded: false` · **2 SHOULD_FIX + 4 NIT**
- **记录**：`.rddf/reviews/p8-hierarchical-equivalence-public-helper-2026-11-critic.md`

| # | 类型 | 内容 | 处置 |
|---|---|---|---|
| **SF1** | SHOULD_FIX | `kOutDir` 的修复**不彻底**：注释声称「自足」，但实现是**单层** `mkdir` ⇒ ⇒ 全新 runner 上 `/tmp/opencode` 不存在时**仍失败** | ✅ **已修**：逐级建（`/tmp` → `/tmp/opencode` → 目标）⇒ 隔离探针实测深路径创建成功 |
| **SF2** | SHOULD_FIX | 文档 §5.4-ter 示例照抄运行会在 ① 步失败（`work_dir` 从未创建，而 `emit` **不建**它） | ✅ **已修**：示例补 `::mkdir(work_dir)` + `<sys/stat.h>` + 一行说明 ⇒ 重编译 0 错误 |
| NIT1 | NIT | deny 条目 `ch_test::hier::impl` 已是「历史名」 | ✅ 脚本注明「防复辟 tripwire · **不是**活符号」 |
| NIT2 | NIT | 符号枚举器**形态覆盖有边界**（成员函数 / `using` / `typedef` / 宏 / `constexpr` 变量不可见） | ✅ 写进脚本头注释：**tripwire ≠ 完备门** |
| NIT3 | NIT | R4② 的失败措辞大于实际扫描口径 | ✅ 改为「**扫描范围内的**公开 API 面静默扩张」 |
| NIT4 | NIT | `no-varying-signal` 的单拍豁免只写在 impl 注释 | ✅ 公共头已补该边界 + 「单拍判别力天然很弱」 |

⇒ ⇒ **SHOULD_FIX / NIT 全部处置后重新跑验证门**（★ 清扫是改动 ⇒ ⇒ MUST 重跑）。

---

## 实施期实测（证据）

### A. 公共面（**闭合集合** ⇒ 7 项 ⇒ 护栏 R2'' 双向核对）

`emit` · `compare` · `ProductSet` · `Result` · `DrivenInput` · `Observation` ·
`make_observation` ⇒ ⇒ 护栏 `scripts/verify_hierarchical_p8.sh` 的 **R2''**
（python3 枚举公共头的声明符号 ⇒ 与白名单**双向**比对）⇒ ⇒ 多一个 / 少一个 ⇒ ⇒ **红**。

⚠️ **`Trace` 类型别名已取消** ⇒ ⇒ `Result::flat_trace` 的类型**内联**写
⇒ ⇒ 公共符号集合里**没有** `Trace` ⇒ ⇒ 「恰好 6 项」的旧口径**已作废**（⇒ 7 项）。

### B. 护栏的 7 条投毒（**逐条必红** + 还原复绿）

| # | 投毒 | 抓它的判据 |
|---|---|---|
| ① | 内部实现符号泄漏到公共面 | R2 |
| ② | 公共头**改名** | R2'（内容维度）+ R3（存在性） |
| ③ | 公共头**缺失** | R3 |
| ④ | **多出第 2 个公共头**（名字**不**命中 `*equivalence*`/`*hier_eq*`） | R4②（活账） |
| ⑤ | 公共头 `#include` 了**内部实现头** | R1'（include 图） |
| ⑥ | 公共面**多**一个符号 | R2''（闭合集合） |
| ⑦ | 活账登记了一个**不存在**的头 | R4②（另一方向） |

⚠️ **投毒打的是 `include/` 的副本** ⇒ ⇒ **绝不改仓库** ⇒ ⇒ 中断也不污染工作区。

⚠️ ⚠️ **实施期发现的一个真缺口（已修）**：自检最初让**每条投毒只跑单条检查**
⇒ ⇒ ⇒ 投毒 ②（公共头改名）**恒绿** ⇒ ⇒ 因为 R1 的**全称否定**在「找不到匹配文件」
时**真空通过** ⇒ ⇒ ⇒ **必须由聚合 `judge()` 判**（⇒ ⇒ 全部检查一起跑）
⇒ ⇒ ⇒ 这是「**单条判据 ≠ 整体判据**」的实例 ⇒ ⇒ 已在脚本里落成 `judge()`。

### C. 变异（5 条 · 全部转红 + 逐字节还原 + 重建 + 复绿）

| # | 变异 | 判据归属 | 实测 |
|---|---|---|---|
| M1 | `emit` 的端口集合改成**手写** | 「端口集合必须是**派生**的」 | 红（`10 assertions / 5 failed`） |
| M2 | **用户侧**：输入**未被驱动**（`<<=` 的等价形态） | 「观测恒定 ⇒ 被 `no-varying-signal` 拒绝」 | 红（`33 / 1 failed`） |
| M3 | `compare` 恒返回 `equal=true` | 「篡改产物 MUST 转红」（等价性用例的判别力） | 红（`411 / 1 failed`） |
| M4 | 去掉 `no-varying-signal` 判据 | 「退化观测面 MUST 被拒绝」 | 红（`41 / 1 failed`） |
| M5 | 去掉 `observation-not-visible` 判据 | 「观测不可见信号 MUST 被拒绝」 | 红（`42 / 1 failed`） |

⇒ ⇒ 还原后 4 个源文件 **sha256 逐字节相同** + **重建成功** + 两套用例**复绿**
（43 assertions / 5 cases + 414 assertions / 9 cases）。

### C-bis. ⚠️ 实施期发现的**既有缺陷**（已修 · **不是** P8 的功能范围）

**`tests/test_hierarchy_verilog.cpp` 的输出目录从不创建**：

```cpp
const char *kOutDir = "/tmp/opencode/hieremit";   // ← 硬编码，且**从不** mkdir
```

⇒ ⇒ 干净机器上 `toVerilogHierarchical` 打不开 `<path>.tmp` ⇒ ⇒ **26 条断言全红**
（`34 test cases / 914 assertions` 只剩 `8 passed / 26 failed`）。

**实证它**不是**本 change 引入的**：`git stash` 到 HEAD（干净树）后**同样复现**
⇒ ⇒ 且 `ci.yml` / `scripts/` 里**零命中**「创建该目录」⇒ ⇒ ⇒ **全新 runner 上本 target
必然红**。

**修复**：`kOutDir` 改为函数（**首次调用时 `mkdir`** ⇒ 自足）⇒ ⇒ 实测从**空的**
`/tmp/opencode` 起跑 ⇒ **34 cases / 914 assertions 全绿**。

⚠️ **如实登记**：这是**范围外的历史遗留** ⇒ ⇒ 修它的理由是「它让本 change 的
**零回归门**（`ctest -LE perf` 全绿）不成立」+「它会让 CI 在干净 runner 上红」
⇒ ⇒ **不是**顺手重构 ⇒ ⇒ 改动面**只**限于「建目录」这一件事（`kOutDir` → `kOutDir()`
+ `<sys/stat.h>`）。

### D. 零回归

- 既有 **9 用例 / 414 断言**（`test_hierarchy_equivalence`）**断言集合与期望值不变**
  ⇒ ⇒ 仅 `trace` → `flat_trace` 的**机械改名**（7 处）
- 既有 **3 个层次化产物**（`bubble.v` / `row1_implicit.v` / `three_layers.v`）**逐字节不变**
  ⇒ ⇒ 对照 HEAD 的 `cmp` = **0**
- 计数锚点：`CTEST_TOTAL` **252 → 253** · non-perf **241 → 242** ·
  `VERILATOR_E2E_TOTAL` **76 → 77**（新用例跑真 verilator ⇒ ⇒ **必须**带
  `verilator_e2e` 标签 ⇒ ⇒ 已从 `add_catch_test` 改为 `add_dual_backend_test`）
  ⇒ ⇒ parity 两锚点（**66 / 289**）**不变**

---

## 验证命令

```bash
cd /workspace/project/CppHDL
set -euo pipefail
cmake --build build -j"$(nproc)"
./build/tests/test_hierarchy_equivalence
./build/tests/test_public_equivalence_api
( cd build && ctest -LE perf --output-on-failure )
bash scripts/run_all_samples.sh
./run_all_ported_tests.sh
bash scripts/verify_hierarchical_p8.sh
bash scripts/verify_hierarchical_p8.sh --self-test
bash scripts/verify_hierarchical_rtl.sh
bash scripts/verify_header_self_contained.sh
bash scripts/verify_header_odr.sh
bash scripts/verify_test_counts.sh
openspec validate p8-hierarchical-equivalence-public-helper-2026-11 --type change --strict
bash scripts/verify_openspec_delta.sh
```
