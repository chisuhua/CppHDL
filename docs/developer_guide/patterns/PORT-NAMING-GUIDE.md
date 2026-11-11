# 端口命名指南（PORT NAMING GUIDE）

> **一句话**：`__io` 里的端口成员**用「名字串」显式命名** ⇒ ⇒ 产物端口名**可自解释**、
> **可解析回 C++ 成员名**（manifest 的 `cpp_member`）⇒ ⇒ 用户写 TB 不必再查 port map。
>
> **契约载体**：命名规则归 capability `rtl-hierarchical-output`（规范所有者）
> ⇒ ⇒ **本文只讲「怎么写」，不复述条款**。
> **判据载体**：`tests/test_port_name_named_ctor.cpp`（端到端探针）·
> `scripts/verify_port_naming.sh`（静态护栏 P1：标识符 MUST == 名字串）。
> **来源**：`port-naming-ratchet-2026-11` 的 **N1**（用法约定 + 最小可运行探针）。

---

## 1. 用法约定（三条，缺一不可）

### ① 端口成员**带 NSDMI**，形如 `ch_in<ch_bool> prog_we{"prog_we"};`

```cpp
class NamedPorts : public ch::Component {
public:
    __io(
        ch_in<ch_bool>      prog_we{"prog_we"};      // ← 名字串
        ch_out<ch_uint<32>> prog_addr{"prog_addr"};  // ← 名字串
        ch_in<ch_uint<8>>   prog_len{"prog_len"};    // ← 名字串
    )
    // ...
};
```

⚠️ **必须是「直接初始化」形态**（`p{"name"}` 或 NSDMI）⇒ ⇒
**拷贝初始化形态 `ch_in<ch_bool> p = "prog_we";` 编译不过** ——
`port`'s 具名构造是 `explicit`（`include/core/io.h`）。

### ② 组件**必须** override `create_ports()` 并 `new (io_storage_) io_type;`

```cpp
void create_ports() override { new (io_storage_) io_type; }
```

⚠️ **不 override ⇒ 端口根本不会被创建** ⇒ ⇒ `io().xxx` 读到的是**未初始化的 `io_storage_`**
⇒ ⇒ `impl()` 是未定义行为（实测：未 override 的 `ch_device<T>` ⇒ 端口不存在）。

### ③ **名字串 MUST 与标识符逐字相同**

`ch_in<ch_bool> prog_we{"prog_we"};` ✅ · `ch_in<ch_bool> prog_we{"port_addr"};` ❌

⚠️ **框架不禁止二者不一致**，且**不一致时「名字串」胜出**（实测）⇒ ⇒ 若标识符与名字串不同，
端口名会是**字符串**那个 ⇒ ⇒ manifest 的 `cpp_member` 与实际端口名**对不上**
⇒ ⇒ 「端口名可解析到 C++ 成员名」这条 MUST **不可验证**
⇒ ⇒ **静态护栏 P1 MUST 判「标识符 == 名字串」**（`scripts/verify_port_naming.sh`）。

---

## 2. 名字怎么变成产物（实测链路）

| 层 | 形态 | 例（组件名 `top`） |
|---|---|---|
| **图节点名** | `<组件路径>.<显式名>` | `top.prog_we` |
| **扁平产物端口名** | `<顶层前缀>_<显式名>` | `top_prog_we` |
| **manifest `name`** | 同扁平产物端口名 | `top_prog_we` |
| **manifest `cpp_member`** | C++ 成员路径 | `prog_we` |

⚠️ **扁平产物是「单 module 全局命名空间」** ⇒ ⇒ **前缀是唯一的消歧手段**
⇒ ⇒ 所以「顶层端口名 == C++ 成员名」对多数端口**结构上不可满足**
⇒ ⇒ **契约是「名字可被解析到成员名」**（`cpp_member` 字段），**不是**「字面同名」。
⇒ ⇒ 这也是 `rtl-hierarchical-output` 那条 Requirement 的口径（**MUST 可解析**，不是 MUST 字面相同）。

---

## 3. 已知限制（如实登记）

| # | 限制 | 后果 / 缓解 |
|---|---|---|
| **L1** | **这条路径此前从未被端到端跑过**（具名构造的**调用点实测 = 0**）⇒ ⇒ 它是**新验证**的，不是久经考验的 | ⇒ ⇒ 本指南 + 探针 ctest 是它的**第一道**保障 ⇒ ⇒ 新增具名端口时 MUST 跑该 ctest |
| **L2** | **拷贝初始化形态编译不过**（`explicit` ctor） | ⇒ ⇒ MUST 用 `p{"name"}` / NSDMI 形态 |
| **L3** | **不 override `create_ports()` ⇒ 端口不存在** | ⇒ ⇒ 用 undefined behavior 读 `impl()` |
| **L4** | **标识符 ≠ 名字串时，名字串胜出**（框架允许） | ⇒ ⇒ 护栏 P1 MUST 拦（静态） |
| **L5** | **顶层端口名 ≠ C++ 成员名**（`top_prog_we` vs `prog_we`） | ⇒ ⇒ 这是**契约**不是缺陷 ⇒ ⇒ 想拿到成员名请读 manifest 的 `cpp_member` |
| **L6** | **显式名目前只对 `__io` 内的端口成员有效**；`describe()` 里临时造的节点不走这条路 | ⇒ ⇒ 那些节点仍是 `unnamed_*` ⇒ ⇒ 需要可读名的场合 SHOULD 用端口成员表达 |

---

## 4. 怎么验证我写对了

```bash
# 端到端探针（含产物端口名断言）
./build/tests/test_port_name_named_ctor
# ⇒ All tests passed (18 assertions in 4 test cases)

# 静态护栏（标识符 == 名字串 · 逐端口 · 双向）
bash scripts/verify_port_naming.sh
# ⇒ 护栏自检
bash scripts/verify_port_naming.sh --self-test
```

⚠️ **判别力自证**（本仓纪律：还原缺陷 ⇒ 判据 MUST 转红）：
把 `include/core/io.h` 的具名构造里**创建节点**那行改成用默认名
（`ctx->create_input(ch_width_v<T>, "unnamed_input", sloc)`）⇒ ⇒
**3 个用例会转红**（节点名 / 产物端口名 / 名字串胜出）⇒ ⇒ 还原后复绿。

---

## 5. 相关

- 契约：capability `rtl-hierarchical-output`（端口命名规则的**唯一**规范所有者）
- manifest schema：capability `rtl-emit-portability`（`rtl-emit-manifest-v1`）
- 组件生命周期（`create_ports()` / `describe()` 的调用时机）：`COMPONENT-LIFECYCLE-GUIDE.md`
- 层次化 RTL 写法：`HIERARCHICAL-RTL-AUTHORING-GUIDE.md`
