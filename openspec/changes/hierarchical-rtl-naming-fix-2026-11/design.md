# 设计：纯命名（`hierarchical-rtl-naming-fix-2026-11`）

> 决策在 ADR-059（D3 / D4 / §2.2.1）；本文件承载**推导与证据**。
> 上游：架构主题文档 `docs/architecture/2026-11-02-hierarchical-rtl-output.md` §10。

---

## 1. 缺陷的精确机制（实测）

### 1.1 三个函数的当前关系

```
ch_module::ctor(instance_name)
  └─ child_path_name = build_hierarchical_name(parent, instance_name)   // module.h:19
        └─ parent->hierarchical_name()                                   // component.h:61
              └─ parent_path + "." + parent->name_                       // name_ 已是全路径！
  └─ make_unique<T>(parent, child_path_name, ...)   // ⇒ 子组件 name_ = 全路径
```

⇒ ⇒ **父路径被拼两次**：`child_path_name` 已经是 `top.m.lf`，
而 `child->hierarchical_name()` 之后再拼一次父路径 ⇒ `top.top.m.lf`（深度 3）。

### 1.2 为什么「改 `hierarchical_name()` 就行」是错的

`hierarchical_name()` 的**唯一**消费者是 `module.h:27`（`build_hierarchical_name` 内部）
⇒ ⇒ 改它 ⇒ 它变成**死代码** ⇒ ⇒ 而死代码 = 下一个人会重新用它 ⇒ ⇒ 前缀重复复发。

⇒ ⇒ **裁决**：**删除** `hierarchical_name()`，把「全路径」的唯一真源定为 `name()`。

### 1.3 关键事实：`Component::name()` 没有功能消费者

实测（`grep -rn '\->name()' src/ include/`）：`Component::name()` 的消费者**只有日志**
（`src/component.cpp:27,32,167`）⇒ ⇒ Verilog 侧用的是 **lnode** 的 `node->name()`
（`src/codegen_verilog.cpp:70`），与组件名**无关**。

⇒ ⇒ ⇒ 「统一 `name()` 的语义」的风险 = **零功能风险**（只影响日志文本）。

---

## 2. 命名模型（**单一真源**）

| 维度 | 访问器 | 含义 | 谁提供 |
|---|---|---|---|
| **全路径** | `name()` | 「父路径 + `.` + 局部名」⇒ 深度 ≥3 **无重复前缀** | `name_`（**既有**，语义不变） |
| **局部名** | `local_name()` | 实例在父下的**局部**名（不含父路径） | **新增** `local_name_` |
| **模块类型名** | `module_type_name()` | module 定义的**类型**名（**同类型 ⇒ 同值**） | **新增** `module_type_name_` |

⇒ ⇒ **不变量**：`name() == 父的 name() + "." + local_name()`（根除外）
⇒ ⇒ ⇒ **MUST NOT** 存在第二个「拼全路径」的地方（⇒ `hierarchical_name()` 删除）。

### 2.1 `local_name_` 怎么来（**实施期定稿**）

⚠️ **初稿的方案（`ch_module` 构造后调 `set_local_name`）在实施期被推翻** ——
理由见 §2.1.1。

| 来源 | 传入的 `name` 形态 | `local_name_` | `name_`（全路径） |
|---|---|---|---|
| `ch_module<T> s{"s"}` | **局部名** | = 传入值 | 由**构造函数**计算 |
| `create_child<T>("c")` | **局部名** | = 传入值 | 由**构造函数**计算 |
| 根组件（`ch_device<T>`） | 局部名 | = 传入值 | = 传入值（无父） |

⇒ ⇒ **裁决（定稿）**：**构造函数的 `name` 参数恒为「局部名」**，
**全路径由构造函数自己算**（`build_hierarchical_name(parent, local_name_)`）
⇒ ⇒ ⇒ 由此：
① `local_name_` **天然**正确（不需要任何 setter、不需要字符串切分）；
② `name()` **恒为全路径**（R3 的要求，对**所有**组件成立）；
③ **派生组件的 ctor 签名零变更**（⇒ 不触发 ADR-061 决策① 的「tag 参数会要求每个组件
   的 ctor 都接受它 ⇒ 破坏现有组件」的同型问题）。

```cpp
// include/component.h
class Component {
public:
    Component(Component* parent = nullptr, const std::string& name = "");
    const std::string& name() const { return name_; }              // 全路径
    const std::string& local_name() const { return local_name_; }   // 局部名（新增）
    std::string module_type_name() const;                           // 模块类型名（新增）
private:
    std::string local_name_;   // 局部名（**先于** name_ 声明 ⇒ 初始化顺序正确）
    std::string name_;         // 全路径
};
```

### 2.1.1 为什么初稿方案被推翻（**实施期记录**）

初稿让 `ch_module` 传**全路径**给构造函数、再用 `set_local_name` 补局部名。
实测推演发现它**必然留下一个不一致**：`create_child` 的子组件走的是**同一个构造函数**
⇒ ⇒ 构造函数收到的是**局部名** ⇒ ⇒ 「`name_` 有时是全路径、有时是局部名」
⇒ ⇒ `name()` 的语义**仍然不一致** ⇒ ⇒ 而 R3 要求 `name()` **恒为全路径**
⇒ ⇒ 且「深度 ≥3 无重复前缀」只有在**全路径恒定成立**时才成立。

⇒ ⇒ ⇒ **一条更一般的教训**：当「同一个构造函数」被**两种语义**的调用方使用时，
**必须**在**构造函数内部**统一语义 ⇒ ⇒ 而不是靠调用方约定 + setter 补洞。

### 2.2 `module_type_name_` 怎么来（**四档的第 ① 档 + 回落**）

ADR-059 §2.2.1 的四档里，本 change 只落地**第 ① 档**与**回落**：

| 档 | 来源 | 本 change 的处置 |
|---|---|---|
| ① | **类级** `CH_HIERARCHY_LAYER("name")` | ✅ **本 change 落地**（宏 + 存储 + 访问器） |
| ② | `ch_layer<T>` 继承 `T` 的类级名 | ⏳ **T3-①c**（`ch_layer` 尚不存在） |
| ③ | `ch_layer<Helper>` 实例处给名 | ⏳ **T3-①c** |
| ④ | `Granularity::ForceLayer` ⇒ 回落 `sanitize(typeid)` + `CHWARN` | ⏳ **T3-①c**（`ForceLayer` 尚不存在） |

⇒ ⇒ **回落**（未声明成层者）：`sanitize(typeid(*this).name())`
⇒ ⇒ **同类型 ⇒ 同值** ⇒ ⇒ 满足 R3 的「同一类型多实例返回相同值」。

⚠️ **回落 MUST NOT 违反「同 ABI」限定**（ADR-059 §2.2.1 ④）：`typeid` 拼写是实现相关的
⇒ ⇒ 跨编译器/跨平台**不可复算** ⇒ ⇒ 契约 MUST 把跨平台 golden 的比较范围限定为
「**同编译器 / 同 ABI 内**」⇒ ⇒ 已在 capability delta 的 R4 正文写明。

### 2.3 合并宏 `CH_HIERARCHY_LAYER("name")`

```cpp
// include/component.h
#define CH_HIERARCHY_LAYER(name_literal)                                    \
    static constexpr const char* kChHierarchyLayerName = name_literal;      \
    static constexpr bool kChIsLayer = true;
```

⇒ ⇒ **两个要求**：
① **编译期可见**的名字（`static constexpr`）⇒ ⇒ `ch_layer<T>` 才能据此在
「② 继承」与「③ 必须另给」之间**编译期**分派（ADR-059 §2.2.1 末段）；
② **成层 + 命名一体** ⇒ ⇒ 「成层却忘了命名」**编译期即不可表示**。

⚠️ **本 change 的宏只落地「①」**：它把名字记下来 + 标记 `kChIsLayer`；
`is_layer()` 的**运行期**读取与 `ch_layer` / `ch_inline` 的分派归 **T3-①c**
（⇒ ⇒ 本 change **不新增** `is_layer()` 公开 API ⇒ ⇒ 保持「零语义变更」）。

⚠️ **宏体 MUST NOT 触碰 `this`** ⇒ ⇒ 它是**类级**声明 ⇒ ⇒ 实例相关的名字
（`ch_layer<Helper>` 的档 ③）**不能**用它 ⇒ ⇒ 那正是 ①c 的「实例处给名」形态。

---

## 3. 命名权威（推导 · 从 umbrella design.md §4 承接）

下游按 **ctx 级节点名**解析观测点（`resolve_node_id(ctx, name)` 精确匹配 `node->name()`）
⇒ ⇒ 若层次化用 per-module 命名 ⇒ ⇒ 下游**解析不到** ⇒ ⇒ ⇒ 破下游的 parity 门禁。

⇒ ⇒ **命名权威 MUST 是 ctx 级全局表**（`verilogwriter::node_names_`）
⇒ ⇒ ⇒ 三个推论：
① 顶层端口名 MUST 与扁平产物**逐字相同**；
② **默认（全 inline）产物 MUST 与扁平逐字节一致**（最强的零成本判据）；
③ 下游 ADR-091 §G-1o「顶层端口名不得与内部节点同名」**天然满足**
（全局唯一化保证端口与内部节点不同名）⇒ ⇒ 层次化 MUST NOT 引入新的改名。

⇒ ⇒ **代价**：模块内信号名带**完整路径前缀**（`top_p_c_r`）⇒ ⇒ 名字偏长
⇒ ⇒ ⇒ 但换来「与扁平逐字一致」+「保留来源线索」⇒ ⇒ ⇒ **取此舍彼**。

---

## 4. 风险与对策

| # | 风险 | 对策 |
|---|---|---|
| 1 | `Component::name()` 语义被误改 ⇒ 破坏既有消费者 | 实测 `name()` **只有日志消费者**（§1.3）⇒ ⇒ 零功能风险；⇒ **仍** MUST 逐条核对 `grep -rn '\->name()'` |
| 2 | 「统一命名」顺手改了**归属** ⇒ 回归面失控 | 本 change **MUST NOT** 碰作用域栈 / `node_owner()`（那是 ①b） |
| 3 | 局部名靠字符串切分猜 ⇒ 名字含 `.` 时静默错名 | ⇒ **禁止**切分：局部名由**知道它的那一方**显式给出（§2.1） |
| 4 | 合并宏引入「有声明无消费者」的半截状态 | ⇒ 宏的运行期读取（`is_layer()`）归 ①c ⇒ ⇒ 本 change 只提供**存储 + 编译期可见名** |
| 5 | 跨平台 golden 因 `typeid` 拼写不可复算 | ⇒ 契约限定「**同编译器 / 同 ABI 内**」（§2.2） |

---

## 5. 实施期偏离记录（**文档 MUST 与代码一致**）

| # | 初稿 | 定稿 | 原因 |
|---|---|---|---|
| 1 | `build_hierarchical_name` 留在 `include/module.h` | **移到 `include/component.h`** | 全路径拼接的**唯一**真源 MUST 与「计算它的那一处」同文件 ⇒ ⇒ 构造函数（`component.h`）能直接用 ⇒ ⇒ `module.h` 经 `#include "component.h"` 继续**对外可见**（签名与语义不变）⇒ ⇒ ADR-059 D3 的判据（用 `parent->name()`）**仍然成立** |
| 2 | `ch_module` 传全路径 + `set_local_name` 补局部名 | **构造函数收局部名并自算全路径** | 见 §2.1.1（「同一构造函数被两种语义调用 ⇒ 必须在内部统一」） |
| 3 | `module_type_name()` 返回 `sanitize(typeid(...))` | 返回**未 sanitize** 的 `typeid(...)` 字面量 | `sanitize_name` 是 **writer 的成员函数**（`codegen_verilog.h:52` / `codegen_dag.h:55`）⇒ ⇒ `component.h` **用不了** ⇒ ⇒ 且在头里**复制**一份实现 = 第二个实现 = 会漂移 ⇒ ⇒ **sanitize 归发射侧**（T3-④）⇒ ⇒ 本 change 返回**原始**字面量，语义等价（同一个字面量 ⇒ 同一个字符串） |
| 4 | `module_type_name()` 存成员、ctor 里填 | **运行期由 `layer_name_literal()` 虚函数**解析 | 构造函数里调虚函数会取到**基类**版本（vtable 尚未切换到派生类）⇒ ⇒ 派生类的类级名字**取不到** ⇒ ⇒ 改为**运行期**解析（此时 vtable 已正确）⇒ ⇒ 类级宏仍提供**编译期可见**的 `kChHierarchyLayerName`（供 T3-①c 的 ②/③ 编译期分派） |

⚠️ **偏离 3 的契约影响**：`module_type_name()` 返回**未 sanitize** 的字面量
⇒ ⇒ 「模块类型名」在 Verilog 产物里的形态由**发射侧**决定（sanitize 后）
⇒ ⇒ 契约的「同类型 ⇒ 同值」**不受影响**（同一个字面量 ⇒ 同一个字符串）✔
⇒ ⇒ 「跨平台不可复算」也不受影响（`typeid` 拼写实现相关）✔
