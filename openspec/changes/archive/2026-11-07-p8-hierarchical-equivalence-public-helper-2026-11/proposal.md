# Proposal: 层次化等价性 harness 公共化（P8）

## Why

### 1. 用户的痛点：**自己的设计拿不到「层次化后行为不变」的证据**

台账 §7 残留 1（2026-11-06 由 oracle 从残留 1 拆出）：

> **用户面缺口**：层次化产物的等价性 harness 仍在 `tests/` 内部 ⇒ ⇒ **用户自己的设计
> 拿不到**「层次化后行为不变」的逐拍反馈（⇒ ⇒ 这才是 P8 的原始痛点）

现有等价性证据**全部**在仓内测试里（`tests/hier_equivalence.h` · T3-005 的 9 用例 / 414 断言）
⇒ ⇒ 用户的仓外代码 **include 不到** ⇒ ⇒ 用户**没有任何**可用的手段回答
「我把设计改成层之后，行为变了吗」。

⇒ ⇒ 而这个问题**恰好是用户决定成层粒度时必须回答的**（ADR-058 决策的落点是「值不值得成层」
⇒ ⇒ 而「值不值得」的判据之一就是「成层会不会改变行为」）。

### 2. 为什么「把 harness 挪进 `include/`」不是一行 commit

用户要自己复现这个证据，必须重写**四步**，而每一步都有**静默失效**的陷阱：

| 步 | 陷阱 | 后果 |
|---|---|---|
| ① 生成层次化产物 | `GranularityScope` 是**静态**开关 ⇒ 必须**只**包住发射 | 泄漏到别的阶段 ⇒ 行为漂移 |
| ② 派生端口集合 | 端口集合**必须**由 emitter 产物派生（`backend_port_set`）⇒ ⇒ **MUST NOT** 手写列举 | 手写 ⇒ 与 emitter 真相脱钩 ⇒ 假绿 / 假红 |
| ③ 装配两个 backend | 扁平侧**必须**用「backend 自己生成的那份 `top.v`」⇒ ⇒ 不得另生成一份 | 两份不一致 ⇒ 「等价」结论无意义 |
| ④ 施加激励 | 激励 MUST 走 `Simulator::set_input_value(port, <纯整数>)` | 用 `<<=` ⇒ 顶层端口变成「有内部驱动」⇒ 两侧观测面不一致 ⇒ **假分歧** |

⇒ ⇒ **门槛不在「写多少行」，在「知道每个陷阱在哪」** ⇒ ⇒ 这正是框架该提供的价值。

⇒ ⇒ ⚠️ **设计原则（planner v2 采纳 metis #1）**：这四步的陷阱**应当由 helper 在结构上消除**，
而不是**写进文档提醒用户自己避开** ⇒ ⇒ **凡「用户填错就静默失效」的字段，helper MUST NOT
让用户能填错**（⇒ 端口集合只由 `emit` 产出 · 观测信号只由工厂函数产出 ⇒ ⇒ 见 design.md §2）。

### 3. 边界：P8 **不是**「双后端 parity 的替代」

⚠️ 必须写进公共 API 的文档，否则用户会误用：

| | P8（等价性 helper） | 既有双后端 parity |
|---|---|---|
| 比较什么 | **两份 Verilog 产物**（扁平 vs 层次化） | **C++ 解释器 vs Verilog 编译产物** |
| 判别什么 | 「**层次化变换**是否引入行为分叉」 | 「**两套执行后端**是否行为一致」 |
| 对「共享发射底座算错」 | **结构性失明**（两产物同源 ⇒ 同构缺陷一起错） | **能抓**（解释器 vs 编译产物不同源） |
| 对「层次化变换算错」 | **能抓** | **结构性失明**（两层都由同一份 C++ 生成） |

⇒ ⇒ 两者**互补**，**不可互相替代** ⇒ ⇒ 公共 API 的文档 MUST 明说。

### 4. 与 P6 的关系（同一族「用户面缺口」的两半）

台账 §7 残留 11 同时登记 P6 + P8：

> **用户看不到自己的设计是否「层次友好」**（诊断 API）· **用户拿不到自己 DUT 的等价性反馈**（公共 helper）

⇒ ⇒ **P6 已收口**（2026-11-07）⇒ ⇒ 本项是那半边：**「能不能改」+「改了会不会变」**。

## What Changes

1. **新增 1 个公共头** `include/hierarchy_equivalence.h` · 命名空间 **`ch::hier_eq`**
   （★ **头名 ≠ 命名空间** ⇒ ⇒ 文档 MUST 两处都写）。
   - **公共面 = 一个闭合集合**（★ **MUST NOT** 用「符号总数」当质量指标 ⇒ ⇒ 逐符号登记
     + 双向核对 ⇒ ⇒ 另设**能力判据**独立于符号数）：
     | # | 符号 | 种类 |
     |---|---|---|
     | 1 | `ch::hier_eq::emit(...)` | 入口（生成层次化产物 + **由产物派生**端口集合 + 映射表） |
     | 2 | `ch::hier_eq::compare(...)` | 入口（逐拍比对「扁平产物」与「层次化产物」） |
     | 3 | `ch::hier_eq::ProductSet` | 结果结构（`ok` / `error` / `hier_path` / `ports` / `signal_map`） |
     | 4 | `ch::hier_eq::Result` | 结果结构（`ok` / `equal` / `error` / `first_mismatch` / `mismatch_count` / **`flat_trace`** / `flat_product` / `hier_product`） |
     | 5 | `ch::hier_eq::DrivenInput` | 输入类型（`name` + `set` 回调） |
     | 6 | `ch::hier_eq::Observation` | 输入类型（`name` + `read` 回调 + `node_id`） |
   - ⚠️ **「最小承诺」的精确化**：入口的**参数类型**也是公共面 ⇒ ⇒ **不暴露它们就无法调用**
     ⇒ ⇒ 「最小承诺」的**可执行形态** = **2 入口 + 2 结果结构 + 2 输入类型** ⇒ ⇒ 第 5/6 项
     **不是「新增承诺」**，而是「调用所必需」⇒ ⇒ 文档 MUST 分开列。
   - ⚠️ **`Result::trace` 改名为 `Result::flat_trace`**：★ **实测**：现有实现**只 push 扁平侧**
     （`tests/hier_equivalence.h:190` 的 `row.push_back(vf)` ⇒ 层次化侧 `vh` 只参与比较、
     **不入轨迹**）⇒ ⇒ `trace` 这个名字**语义不符** ⇒ ⇒ 改名 + **取消 `Trace` 类型别名**
     （类型**内联**写）⇒ ⇒ 公共符号集合里**没有** `Trace`。
   - ⚠️ **观测信号的生产方式**：用户 **MUST NOT** 手填 `node_id` ⇒ ⇒ 提供**工厂函数**
     `make_observation(...)` ⇒ ⇒ 由 helper 自己取 ⇒ ⇒ **公共文档里不出现 `impl()`**。

2. **实现从 `tests/` 迁到 `src/`**（★ **公开 API 面** ⇒ ⇒ 实现必须是库的一部分 ⇒ ⇒
   用户链接 `libcpphdl` 即得）
   - `src/hierarchy_equivalence_impl.h`（**内部头** ⇒ ⇒ **不进 `include/`** ⇒ ⇒ 仓内先例：
     `src/hierarchy_chain.h` · `src/hierarchy_port_check.h` 等 4 个）
   - `src/hierarchy_equivalence.cpp`（实现 ⇒ ⇒ 登记进 `CMakeLists.txt` 的 `CH_SOURCES`）

3. **`tests/hier_equivalence.h` 改为「include 公共头 + alias」** ⇒ ⇒ 既有
   `tests/test_hierarchy_equivalence.cpp`（9 用例 / 414 断言）**断言集合与期望值不变**
   ⇒ ⇒ **零行为回归**。

4. **护栏改口径**（`scripts/verify_hierarchical_p8.sh`）：⚠️ 现有 R1–R4 是**为「T3-005
   MUST NOT 暴露公共 API」写的** ⇒ ⇒ 公共化**必然**与 R1/R2/R3 冲突** ⇒ ⇒ MUST 改
   ⇒ ⇒ 且改法 MUST 是**「全称否定 + 单一显式豁免」**（⇒ 新旧判据是**超集**关系 ⇒ ⇒
   「强度不减」才有载体 ⇒ ⇒ **不是**换成存在性白名单）：
   - **R1**：`include/` 下**文件名**匹配 `*equivalence*` 或 `*hier_eq*` 的文件 ⇒ MUST
     **逐字等于** `include/hierarchy_equivalence.h`
   - **R2**：内部实现符号 deny-list ⇒ ⇒ **+ 双向核对**（★ 每个 deny 符号 MUST **仍存在于
     代码库某处** ⇒ ⇒ 否则改名后「grep 无命中 ⇒ 绿」⇒ ⇒ 判据**静默失效**）
   - **R2'**（新增）：**声明** `namespace ch::hier_eq` 的文件 ⇒ MUST **恰好是**那一个
     （⇒ 堵住「改名绕过」⇒ ⇒ 补 R1 的能力缺口）
   - **R3**：公共头 MUST **确实存在**（⇒ **断言存在** ⇒ ⇒ MUST NOT 沿用旧脚本
     「不存在 ⇒ 说明没有 ⇒ 绿」的逻辑）
   - **R4**：**拆成两条** ⇒ ⇒ ① **历史窗口**（原 R4，base/head **钉死**在 T3-005 的两个
     commit ⇒ ⇒ **逐字保留**）② **新增活账**：P8 之后新增的公开头 MUST 登记在
     `.rddf/checks/public-api-surface.md` ⇒ ⇒ **双向核对**
     （★ 原 R4 只扫 `git diff BASE..HEAD_REF` ⇒ ⇒ P8 的文件**不在窗口内** ⇒ ⇒ 「白名单
     4→5 扩容」是**空操作**）
   - ⚠️ **CI 接线**：★ **实测**：`verify_hierarchical_p8.sh` **当前不在
     `.github/workflows/ci.yml`**（零命中 ⇒ 现有 P8 守卫**零执行**）⇒ ⇒ MUST 接线。

5. **文档**：`docs/usage_guide/` 新增用户面小节（★ 示例 **MUST 由真实测试源文件承载**
   ⇒ ⇒ 文档只**引用**它 ⇒ ⇒ 防文档腐烂）。

## 不做（Non-Goals）

- ✗ **不**自动推断观测面（保留「调用方手写」⇒ 猜 = 空洞判据 + 踩 ADR-058 §4 风险 6 盲区）
- ✗ **不**做「C++ 解释器 vs Verilog」的对比（那是既有 parity 的职责）
- ✗ **不**承诺「层次化产物语义正确」（只承诺「变换不改行为」）
- ✗ **不**改发射器 / 分析层 / backend（**零** `src/hierarchy_*.cpp` 行为改动）
- ✗ **不**承诺观测盲区里的信号（成层子树的 `ch_out` 在层次化顶层**不可观测**）
- ✗ **不**为「公开 API 面登记」机制立新 ADR（记 backlog，与 P6 同源）
- ✗ **不**支持「A vs B（两个层次化产物直接互比）」⇒ ⇒ 走**传递性**（A ≡ 扁平 ∧ B ≡ 扁平 ⇒ A ≡ B）
- ✗ **不**提供 verilator 编译的**超时控制**（`invoke_verilator` 无此参数 ⇒ ⇒ 调用方自管）
- ✗ **不**承诺在**已仿真过**的 ctx 上可重入
- ✗ **不**承诺 `ProductSet` 可**跨 ctx** 传递（`ports` 里的 `node_id` 是 **ctx 私有**编号）

## Acceptance

> 完整判据清单（25 条）见 `tasks.md` §3；此处是**摘要**。

- [ ] **公共面** = 闭合集合（6 项）⇒ ⇒ **MUST NOT** 按「符号总数」核 ⇒ ⇒ 另设**能力判据**
      （逐项有可运行的调用点 ⇒ ⇒ 少任一项 ⇒ **红**）
- [ ] **零回归**：`tests/test_hierarchy_equivalence.cpp` 的**断言集合与期望值不变**
      （允许 `trace` → `flat_trace` 的机械改名）⇒ ⇒ 9 用例 / 414 断言仍全绿
- [ ] **护栏**：R1 全称化 + R2 双向 + **R2'** + R3 明确化 + **活账** ⇒ ⇒ `--self-test`
      **5 条投毒**逐条必红 ⇒ ⇒ 且**接进 CI**（★ 现状零执行）
- [ ] **可用性**：一个**独立 out-of-tree 子工程**（只 `-I<repo>/include` + 链接
      `libcpphdl`）能用公共 API **真编译 + 真链接 + 真运行** ⇒ ⇒ 证明公共面**真的够用**
- [ ] **fail-closed**：空 `stimulus` / 空 `observations` / 无随激励变化的信号 / >64bit 端口 /
      无顶层输出 ⇒ ⇒ 全部 `ok=false` + **稳定错误码前缀** + **可行动的下一步**
- [ ] **判别力**：≥4 条变异 ⇒ ⇒ 归属写对（★ 「端口集合改手写」 MUST 落在「逐元素相等」
      上 ⇒ ⇒ **MUST NOT** 挂在「篡改产物转红」上 ⇒ ⇒ 否则该变异**不改变可观测行为**）
- [ ] **文档**：与双后端 parity **互补不可替代** · 只判「变换引入的分叉」· 观测面 MUST 手写 ·
      Linux-only · 头名 ≠ 命名空间 · 显式 include · A 侧固定是扁平产物
- [ ] **零回归数字**：`ctest -LE perf` 全绿 · samples · examples · 计数锚点同步

## Capabilities

| capability | delta |
|---|---|
| `hierarchical-equivalence-public-helper` | **ADDED ×4**（① 公共面 MUST 是闭合集合 ② MUST fail-closed + 错误可行动 ③ MUST 声明判别力边界 ④ MUST NOT 替代双后端 parity） |

⇒ 归档**禁** `--skip-specs`。

---

## 附录 A：实测证据

### A.1 metis 的 4 条事实断言已**逐条实测复核**

| 断言 | 实测 | 结论 |
|---|---|---|
| `verify_hierarchical_p8.sh` **不在 CI** | `grep -rn "hierarchical_p8" .github/workflows/` ⇒ **零命中** | ✅ 成立 ⇒ ⇒ C4 + 护栏接线 |
| `Result::trace` **只存扁平侧** | `tests/hier_equivalence.h:190` = `row.push_back(vf)` | ✅ 成立 ⇒ ⇒ 改名 `flat_trace` |
| `compare_products` **不校验观测集** | `tests/hier_equivalence.h:132-209` 无校验；`any_varies` 只在 `tests/test_hierarchy_equivalence.cpp:486-500`（**测试体**内） | ✅ 成立 ⇒ ⇒ 「至少一个随激励变化」是**新增能力** |
| R4 的 base/head **钉死** ⇒ 「4→5 扩容」是**空操作** | `scripts/verify_hierarchical_p8.sh:39-53` + R4 只扫 `git diff BASE..HEAD_REF` | ✅ 成立 ⇒ ⇒ R4 拆分 |

### A.2 根 `io()` 的端口在**层次化侧恒为顶层端口**（⇒ ⇒ 机制可保留「同文件新增用例」形态）

- `include/hierarchy_verilog.h:136-139`：`backend_port_set` **只取根 module 端口** 且 MUST 含
  clock/reset
- 既有实证：`tests/test_hierarchy_equivalence.cpp:1030-1033`（`EqDeepTop` ForceLayer）·
  `:1058-1060`（bundle）· `:1162-1165`（粒度矩阵）都用 `root.io().din` 驱动、
  `root.io().dout` 观测并通过
- ⇒ ⇒ **但**：**子组件**的 `ch_out` 在层次化侧**不在**顶层 ⇒ ⇒ `Observation` **只能**取根 `ch_out`

### A.3 `include/` 公共头的可移植性约束

- `include/core/verilator_backend.h` 在 `__cpp_lib_expected` 缺失时**显式 `#error`**
  ⇒ ⇒ 公共头**MUST NOT** 拖进它 ⇒ ⇒ ⇒ **`src/` 实现里用它，公共头不用**
- `include/core/verilator_port_set.h` **只**含 `<cstdint>` + `<string>` ⇒ ⇒ 可安全进公共头
- ⇒ ⇒ **公共头的 include 面**：`component.h`（`Component`/`Granularity`）·
  `core/verilator_port_set.h`（`VerilatorPortSpec`）· `hierarchy_verilog.h`（`SignalMapEntry`）·
  `simulator.h`（`Simulator`）· `core/io.h`（`ch_out` · 工厂函数用）
- ⚠️ `include/core/verilator_port_set.h:10` 的注释提到 `verilator_backend.h` ⇒ ⇒ **实测确认它
  并未 include** ⇒ ⇒ 公共头可安全包含它
