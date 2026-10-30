# 提案：`Component` 生态的三处硬化（IO const 可达 / ctx 节点名唯一性 / 公共头 ODR 安全）

- **提出方**：★ **下游消费者**（★ ChipForge 的 `ip/cpu/core/` 开放核工作）★ 实测暴露
- **架构依据**：★ ★ **尚无上游 ADR**（★ 上游惯例是先立 ADR —— 先例：`get-lnode-entry-set-2026-10`
  引用 ADR-053）⇒ ★ **本 proposal 即决策输入** ⇒ ★ 若评审倾向先立 ADR ⇒ ★ 需补
- **规格影响**：★ **新建** 3 个 capability（★ 见 §Capabilities）⇒ 归档**禁** `--skip-specs`；
  ⚠️ archive **不合并** `## Purpose` ⇒ 归档后 MUST 另行补写主 spec 的 `## Purpose`
  （先例：`axi-interconnect-routing`）
- **规模 / 风险**：★ **极小**（★ U-1 ~50 行 + 1 个开关 · ★ U-2 1 行 · ★ U-4 1 行）
- **下游受益**：★ ChipForge 的 ADR-091 v1.15（★ **G-1o** + Verification **V-25**）★
  G-1b（★ `<ch.hpp>` 的 ODR 禁令）

## Why

三条缺口的共同形态：★ **下游正常使用上游公开 API 时会踩** ⇒ ★ 上游内部看不出来
（★ 上游的 test target 都是单 TU / 内部用例都不触发）。

### 1. ★★ `include/module.h` 的 ODR 缺陷（★ **实测证明是上游自己的链接 bug**）

★ `include/module.h:13` 定义了一个**非 inline 的自由函数**：

```cpp
namespace ch {
std::string build_hierarchical_name(Component* parent, const std::string& local_name) { ... }
```

⇒ ★ `#pragma once` 只保护**同一 TU 内**不重复 include ⇒ ★ **跨 TU 仍是两份定义**
⇒ ★ 而该符号是**强全局符号**（`nm` 显示 `T`）⇒ ★ ★ **链接期 multiple definition**。

★ ★★ **决定性对照（★ 已实测，★ 可复现）**：

```bash
# 两个 TU 各 include <ch.hpp> ⇒ 链接失败
cat > tu_a.cpp <<'EOF'
#include "ch.hpp"
int a() { return 1; }
EOF
cat > tu_b.cpp <<'EOF'
#include "ch.hpp"
int b() { return 2; }
EOF
g++ -std=c++23 -I include -I include/core -I include/ast -c tu_a.cpp -o a.o
g++ -std=c++23 -I include -I include/core -I include/ast -c tu_b.cpp -o b.o
g++ a.o b.o -o repro
# ⇒ tu_b.cpp:(.text+0x0): multiple definition of `ch::build_hierarchical_name(...)'
```

| 变体 | 结果（★ 实测） |
|---|---|
| ★ **原版** | ★ ★ **1 处 `multiple definition`** ⇒ 链接失败 |
| ★ **加 `inline`**（★ 1 个词） | ★ ★ **0 处** ⇒ ★ **链接成功**（★ 可执行文件生成） |

⇒ ★ ★ **上游目前只是「侥幸」**：★ 所有 test target 都是**单 .cpp**（★ `tests/CMakeLists.txt`
的 `add_catch_test` 只追加 `catch_amalgamated.cpp`，★ 后者不 include `ch.hpp`）⇒ ★ **不炸**。
⇒ ★ ★ 但 `include/ch.hpp:10` 与 `include/cpu/cache/i_cache_prefetch.h:47` **都** include `module.h`
⇒ ★ 一旦出现**任何 multi-source target** ⇒ ★ ★ **上游自己就炸**。

⇒ ★ ★ **下游已被它咬过**：★ ChipForge 的 ADR-091 **G-1b**（★ 「本头 MUST NOT include
`<ch.hpp>` —— ★ 它 include 了 `module.h`，★ 而 `module.h` 的 `build_hierarchical_name`
**缺 `inline`** ⇒ ★ **任何 2+ TU include `<ch.hpp>` 即链接失败**」）★ ★ **该约束是本缺陷的
下游 workaround** ⇒ ★ ★ 修上游 = **消掉下游一条硬约束**。

### 2. ★★ ctx 内的节点名**不唯一**，★ 且**只有 Verilog 后端**会改名

★ `src/core/context.cpp:200-237` 的 `create_input` / `create_output` **不做**名称唯一化
⇒ ★ **同名节点可以并存**（★ ctx 里有两个叫 `reg_1` 的节点是完全合法的）。

★ ★ 唯一化只发生在 **Verilog codegen**（★ `src/codegen_verilog.cpp:64-76` 建 `node_names_` map
⇒ ★ 同名者加 `_N` 后缀）⇒ ★ ★ **解释器后端（Simulator）不改名**。

⇒ ★ ★ **后果（★ 两后端看到不同的节点名）**：

| 后端 | 同名时的表现 |
|---|---|
| ★ Verilator | ★ 端口被改名（★ `reg_1` → `reg_1_1`）⇒ **按契约名匹配失败** |
| ★ 解释器 | ★ **不改名** ⇒ ★ 按契约名匹配**会命中** ⇒ ★ **静默采到错的那个** |

⇒ ★ ★ 下游若按名字解析观测点（★ ChipForge 的 parity runner 就是这么做的）⇒ ★
**解释器侧会静默采到内部节点** ⇒ ★ **读到貌似合理的错值** ⇒ ★ ★ **假绿**
（★ 比「读不到」更坏：★ 「读不到」至少会报错）。

⇒ ★ **上游自己的先例就是踩坑现场**：★ `include/cpu/pipeline/rv32i_regfile.h`
★ `create_ports()` 写 `new (io_storage_) io_type;`（★ 默认构造 ⇒ 端口名 `unnamed_input`）
★ 且 `describe()` 里用 **32 个局部 `ch_reg<ch_uint<32>> x0(0_d)`…`x31`**（★ `ch_reg` 默认名
`"reg"` ⇒ ★ **32 个同名节点**）⇒ ★ ⇒ ★ **若默认开启唯一性检测，上游自己的这个组件会立刻炸**
⇒ ★ ⇒ ★ **检测 MUST 是 opt-in**。

### 3. ★ `__io` 宏**无 const 版 `io()`**

★ `include/core/io.h:312-319` 的 `__io` 宏只生成**非 const** 的 `io()`：

```cpp
[[nodiscard]] io_type &io() { return *reinterpret_cast<io_type *>(io_storage_); }
```

⇒ ★ ★ **后果**：★ **const 上下文无法读 Component 的 IO** ⇒ ★ 下游若想在
`const` 方法 / `const` 引用里观测组件输出 ⇒ ★ **只能放弃 const 或加 mutable**
⇒ ★ ★ 且 `io_storage_` 是 `char[]` ⇒ ★ 加 const 重载是**零破坏**的纯新增：

```cpp
[[nodiscard]] const io_type &io() const { return *reinterpret_cast<const io_type *>(io_storage_); }
```

## What Changes

### 1. ★★ `include/module.h`：自由函数加 `inline`（★ **1 个词**）

```diff
-std::string build_hierarchical_name(Component* parent, const std::string& local_name) {
+inline std::string build_hierarchical_name(Component* parent, const std::string& local_name) {
```

⇒ ★ **零行为变更**（★ 语义完全相同）⇒ ★ **消除链接期 ODR 冲突**
⇒ ★ ★ **并解除下游的 G-1b 硬约束**（★ 下游可以自由 include `<ch.hpp>`）。

★ **护栏（★ MUST 有）**：★ 一个 **multi-source target 的编译 fixture**
（★ 两个 .cpp 各 include `<ch.hpp>` ⇒ ★ **链接必须成功**）⇒ ★ 破坏判据：★ 去掉 `inline`
⇒ ★ **链接失败** ⇒ ★ fixture 编译不过 ⇒ 红。

### 2. ★★ `include/core/io.h`：`__io` 宏加 const 重载（★ **1 行**）

⇒ ★ **零破坏**（★ 纯新增 overload）⇒ ★ 所有 `ch::Component` 用户立即受益。

★ **护栏**：★ 编译期断言（★ `static_assert`）验证 const 上下文可读 `io()`
⇒ ★ 破坏判据：★ 删掉 const 重载 ⇒ ★ **编译失败** ⇒ 红。

### 3. ★★ `src/core/context.cpp` + `include/core/context.h`：**opt-in** 的节点名唯一性检测

★ **提议形态**（★ API 细节可由评审调整）：

```cpp
// include/core/context.h
enum class node_name_policy { kAllowDuplicates, kRejectDuplicates };   // ★ 默认 kAllowDuplicates
class context {
  public:
    /// ★ opt-in：★ 开启后同名节点在**创建点**即 fail-fast
    void set_node_name_policy(node_name_policy p) { node_name_policy_ = p; }
    ...
};
```

⇒ ★ **开启后**：`create_input` / `create_output`（★ 以及 `create_node` 家族）
在**创建点**检测到**同名** ⇒ ★ ★ **立即报错并中止**（★ 用 `CHFATAL_EXCEPTION`
—— ★ `CHERROR` / `CHFATAL` / `CHREQUIRE` **只打日志、不抛异常** ⇒ ★ 不能用它们做安全网）。

⇒ ★ ★ **默认 MUST 保持 `kAllowDuplicates`**（★ 理由见 §Why.2：★ 上游自己的
`rv32i_regfile.h` 有 32 个同名 `ch_reg` + 7 个 `unnamed_*` ⇒ ★ 默认开会炸上游自己）。

★ **护栏（★ MUST 有两层）**：
- ★ ① **行为层**：★ 一个开启该策略的 fixture ⇒ ★ 制造同名 ⇒ ★ **MUST 抛异常**
  （★ 破坏判据：★ 把检测摘掉 ⇒ ★ **不抛** ⇒ 红）；
- ★ ② **默认层**：★ 一个**默认策略**的 fixture ⇒ ★ 同名 MUST **不抛**
  （★ ★ 这条护住「默认不破坏上游既有代码」）。

## 范围外

- ★ ★ **MUST NOT** 把唯一性检测**默认开启**（★ 会炸上游自己的 `rv32i_regfile.h`：
  32 × `"reg"` + 7 × `unnamed_*`）⇒ ★ 默认保持宽松，★ 由下游显式 opt-in；
- ★ ★ **MUST NOT** 在 ctx 层做**自动改名**（★ 「静默改名」本身是下游踩坑的机制 ——
  ★ 解释器侧不改名 / Verilator 侧改名 ⇒ ★ 两后端不一致）⇒ ★ 检测要么**放行**要么**报错**；
- ★ ★ **MUST NOT** 顺带改 `ch_logic_in` / `ch_logic_out` 的拷贝赋值语义
  （★ 裸 `ch_logic_out` 的拷贝赋值是**浅拷贝 impl_ 指针**、而模板 `operator=(const U&)`
  是 `set_src`（★ 驱动）⇒ ★ 同名 overload 语义分裂）⇒ ★ **另一条独立提案**（★ 下游已记录）；
- ★ ★ **MUST NOT** 顺带改 `ch_bool` / `ch_uint` 的运算符 / `select` 语义；
- ★ **MUST NOT** 引入新的依赖或改变既有 API 的**行为**（★ 三条改动全是**新增**或
  **ODR 修复**）。

## Acceptance

- [ ] **V1（U-4 · ODR）**：★ **multi-source fixture 链接成功**（★ 两个 .cpp 各 include
      `<ch.hpp>`）⇒ ★ **判别性对照**：★ 去掉 `inline` ⇒ ★ **链接失败** ⇒ 红；
- [ ] **V2（U-2 · const io）**：★ **编译期断言**验证 const 上下文可读 `io()`
      ⇒ ★ **判别性对照**：★ 删掉 const 重载 ⇒ ★ **编译失败** ⇒ 红；
- [ ] **V3（U-1 · 唯一性 · 行为层）**：★ 开启策略后制造同名 ⇒ ★ **MUST 抛异常**
      ⇒ ★ **判别性对照**：★ 把检测摘掉 ⇒ ★ **不抛** ⇒ 红；
- [ ] **V4（U-1 · 唯一性 · 默认层）**：★ **默认策略**下同名 MUST **不抛**
      ⇒ ★ ★ 这条护住「默认不破坏上游既有代码」（★ 尤其 `rv32i_regfile.h`）；
- [ ] **V5（U-1 · 两后端一致性）**：★ 开启策略后 ⇒ ★ **同名不再可能发生** ⇒ ★
      「解释器侧按契约名命中 / Verilator 侧被改名」这个**两后端分歧**的**前提被消除**；
- [ ] **V6（零回归）**：★ 全量构建 · `ctest -LE perf` · `run_all_samples.sh` ·
      `run_all_ported_tests.sh` 全绿；★ 计数锚点（★ `CTEST_TOTAL` / `VERILATOR_E2E_TOTAL` /
      `EXAMPLES_TEST_SOURCES` / `DUAL_BACKEND_PARITY_*`）若变 MUST 显式更新；
- [ ] **V7（文档同步）**：★ `include/core/io.h` / `include/module.h` 的头注释 MUST 说明
      新契约；★ `include/AGENTS.md` 若涉及其「ANTI-PATTERNS」 MUST 同步；
- [ ] 归档（★ **禁** `--skip-specs`；★ 归档后 MUST 补写 3 个新 capability 的 `## Purpose`）。

## Capabilities

- **新建** `core-header-odr-safety` ⇒ **ADDED ×1**
  「公共头 MUST 可被 2+ TU 同时 include ⇒ 头内自由函数 MUST NOT 定义非 inline 的
  外部链接符号」+ 2 个 Scenario（多 TU 可 include / 破坏判据）。
- **新建** `component-io-const-access` ⇒ **ADDED ×1**
  「`__io` 生成的 IO 访问器 MUST 提供 const 版本 ⇒ const 上下文可读 IO」
  + 2 个 Scenario（const 可读 / 破坏判据）。
- **新建** `core-ctx-node-name-uniqueness` ⇒ **ADDED ×1**
  「ctx MUST 提供 **opt-in** 的节点名唯一性策略 ⇒ 开启后同名 MUST 在**创建点** fail-fast
  ⇒ 消除两后端节点名分歧的前提」+ 4 个 Scenario（开启后报错 / 默认放行 /
  不得自动改名 / 破坏判据）。

## Impact

- **改动文件**：`include/module.h`（+1 词）· `include/core/io.h`（+1 行）·
  `include/core/context.h` + `src/core/context.cpp`（新策略 + 检测）·
  ★ 新增 fixture / 护栏 · `openspec/specs/{core-header-odr-safety,component-io-const-access,core-ctx-node-name-uniqueness}/`
- **爆炸半径**：★ **0**（★ 三条都不改变既有行为：★ U-4 是 ODR 修复 · U-2 是纯新增 overload
  · U-1 默认关闭）
- **行为变更**：★ **无**（★ 唯一「变化」是**原本会链接失败的组合现在能链接**）
- **下游收益**：★ ★ ChipForge 的 **G-1b** 硬约束（★ 「MUST NOT include `<ch.hpp>`」）
  **可以删除** ⇒ ★ 下游不再需要为上游的 ODR 缺陷绕路
- **风险**：★ **低** —— ★ 主风险是「U-1 的检测误伤」⇒ 已用 V4（默认放行）钉住

## Non-Goals

- ★ **不做** `ch_logic_in` / `ch_logic_out` 的拷贝赋值语义统一（★ 另一条独立提案）；
- ★ **不做** ctx 层的**自动改名**（★ 理由见 §范围外）；
- ★ **不做** 让上游 `rv32i_regfile.h` 的 32 个同名 `ch_reg` 改名（★ 那是**另一个**问题，
  ★ 且会改上游既有测试的节点名 ⇒ 爆炸半径大 ⇒ 独立立项）；
- ★ **不做** `ch_stream` / `Payload` / bundle 相关的任何改动。
