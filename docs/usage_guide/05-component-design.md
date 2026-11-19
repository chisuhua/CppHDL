# Component 设计指南（多层模块 / 跨层连线 / 成员接口 / RTL 输出）

**文档编号**: USAGE-05
**版本**: v1.1
**最后更新**: 2026-11-06
**目标读者**: 使用 CppHDL 设计硬件的工程师
**前置知识**: [Simulator API 使用指南](04-simulator-api.md)、[Bundle 使用模式](06-bundle-patterns.md)

---

## 本章目标

学完本章，你将能够：

1. 用 `ch_device<T>` / `ch::ch_module<T>` 搭出任意深度的组件树
2. 在层与层之间正确连线（4 种端口方向 + 表达式 + 寄存器/存储 + bundle）
3. 判断什么时候该用 `io()` 端口、什么时候该用成员变量做接口
4. 知道生成的 Verilog 有**扁平 / 层次化**两种形态，各自的端口规则与**观测盲区**边界
5. 避开常见反模式（§6 列了 10 条「编译通过、运行不报错、结果静默错」的坑）

> ⚠️ 本文所有行为都经过实测（§5 于 2026-11-06 重新实测：`g++ 14 / -std=c++23`）。
> 与代码实现冲突时以代码为准，并请顺手更新本文。

---

## 1. 组件的基本形态

### 1.1 三件套

每个 `Component` 子类必须提供三样东西：

```cpp
class Adder : public ch::Component {
public:
    __io(                                    // ① 端口声明（宏，见下）
        ch_in<ch_uint<8>>  a;
        ch_in<ch_uint<8>>  b;
        ch_out<ch_uint<9>> y;
    )

    Adder(ch::Component* parent = nullptr, const std::string& name = "adder")
        : ch::Component(parent, name) {}      // ② 必须能接 (parent, name)

    void create_ports() override { new (io_storage_) io_type; }   // ③ 必须
    void describe() override { io().y <<= zext<9>(io().a) + zext<9>(io().b); }
};
```

### 1.2 `__io` 与 `create_ports()` 是配套的

`__io(...)`（`include/core/io.h:319`）只做两件事：

1. 生成内嵌 `struct io_type { ... };`
2. 生成 `io()` 访问器，返回 `io_storage_` 上的对象

**placement new 要你自己写**：`create_ports()` 里必须 `new (io_storage_) io_type;`。
漏了它，`io()` 里全是未构造对象 ⇒ 读到的是垃圾，而且**不会有任何报错**。

> 组件没有端口时，`create_ports()` 写成空函数体即可（成员变量做接口的场景，见 §4）。

**`io()` 有两个重载**（非 const / const）：

```cpp
[[nodiscard]] io_type &io();                  // 非 const 对象 → 可变引用（既有行为）
[[nodiscard]] const io_type &io() const;      // const 对象 → 只读引用
```

⇒ ⇒ **const 上下文可以读 IO**：在 `const` 成员函数 / `const` 引用上，
`io().port.impl()` / `.name()` / `.is_valid()` 全部可读
（⇒ 不必为了「在 const 接口里读 IO」而放弃 const 正确性）。

⚠️ 两个重载**不改变**非 const 代码的行为（重载决议仍选非 const 版本）⇒ 纯新增。

### 1.3 端口节点的名字

`__io` 里的字段是**默认构造**的 `ch_in/ch_out`，因此节点名恒为
`unnamed_input` / `unnamed_output`（`include/core/io.h:48-68, 141-158`）。
这也是为什么生成的 Verilog 端口名像 `top_unnamed_input_3` —— 见 §5。

⚠️ **同名节点默认是合法的**（解释器侧**不改名**，而 Verilog codegen 会**自动加 `_N` 后缀**
⇒ ⇒ 两后端看到的节点名**可能不同**）。要让「同名」在**创建点**就报错，**显式开启**策略：

```cpp
ctx.set_node_name_policy(ch::core::NodeNamePolicy::kRejectDuplicates);  // 默认 kAllowDuplicates
```

⇒ ⇒ 开启后同名节点在创建点**抛异常**（错误信息含冲突的节点名）；**默认宽松**
⇒ ⇒ 既有代码与产物**零变化**。⚠️ 策略是**每 ctx** 的，且 ctx MUST **不改名**
（冲突只由「放行」或「报错」两种方式处理）。

⚠️ **开启时机**：切到严格的那一刻，ctx MUST 把**既有**节点的名字一并纳入登记
⇒ ⇒ 「先宽松建完 DUT、再开严格」也**同样受保护**（不是只保护开启之后新建的）

⚠️ **适用边界**（严格模式判的是 **ctx 内全部节点**，含框架内部派生节点）：

- `_` 前缀是**框架保留**命名空间 —— `create_input` / `create_output` 会额外建
  `"_" + name` 的 proxy ⇒ 你自己的 `"a"` 与 `"_a"` **会被判重**；
- 内部默认名会让「**未经显式命名**」的普通设计必然报错（`ch_reg` 默认名 `reg`、
  mux 的默认名等）⇒ ⇒ 严格模式适用于**全部节点都显式命名**的设计与框架自测。

---

## 2. 装配：把组件搭成树

### 2.1 三个入口

| 入口 | 角色 | 能否无 parent | 是否自动 `build()` | 推荐度 |
|---|---|---|---|---|
| `ch::ch_device<T>` | 顶层 / 根 | ✅ 唯一允许 | ✅ | 生产与测试都用 |
| `ch::ch_module<T>` | 子模块（任意深度） | ❌ 必须有 `Component::current()` | ✅ | **层次化装配的唯一正路** |
| `Component::create_child<T>` | 只登记 child | — | ❌ | ⚠️ 仅结构/计数，不可用于装配 |
| 手工 `new T(parent, name)` + `build(ctx)` | 框架内部通道 | — | 手工 | 不建议用户层使用 |

入口由 `include/ch.hpp:8,10` 导出（`device.h` + `module.h`）。

### 2.2 `ch::ch_device<T>` —— 根组件

```cpp
ch::ch_device<MyTop> dev;              // 建 context("top_ctx") 并 build
ch::Simulator sim(dev.context());      // 仿真器吃这个 context
sim.set_input_value(dev.instance().io().a, 5u);
sim.tick();
ch::toVerilog("out.v", dev.context()); // 出 RTL（扁平，见 §5）
```

机制（`include/device.h:11-24`）：

1. 先建 `context("top_ctx")` 并 `ctx_swap` —— 保证顶层端口与 `describe()` 里产生的节点**同一 context**
2. `std::make_shared<T>(nullptr, "top", ...)` —— 所以 T 的构造函数签名必须是 `(Component*, const std::string&, ...)`
3. `top_->build(ctx)` —— 传外部 context，顶层不自己再建

访问面：`io()` / `instance()` / `context()`（`device.h:26-32`）。

### 2.3 `ch::ch_module<T>` —— 子模块

**必须写在 `Component::describe()` 里，且是局部变量**：

```cpp
void describe() override {
    ch::ch_module<Adder>   adder{"adder"};     // ✅ 局部变量
    ch::ch_module<RegFile> rf{"rf"};

    adder.io().a <<= rf.io().q;
    io().y       <<= adder.io().y;
}
```

构造函数内部六步（`include/module.h:41-74`）：

| 步 | 做什么 | 失败表现 |
|---|---|---|
| 1 | `Component::current()` 取父 | 为空 ⇒ `CHERROR` 后**静默 return** |
| 2 | 取 `parent->context()` | 为空 ⇒ 同上 |
| 3 | 拼层级名 `parent.child`（`module.h:13-32`） | — |
| 4 | `make_unique<T>(parent, 拼好的名字, ...)` | 子组件 ctor 也必须是 `(Component*, string, ...)` |
| 5 | `parent->add_child()` 移交所有权 | — |
| 6 | `child->build(parent_context)` —— **复用父 context** | — |

两个访问通道：

```cpp
x.io().port          // 端口
x.instance().member  // 子组件的非端口成员（bundle / stream / ch_reg / ch_mem）
```

### 2.4 为什么能无限嵌套

`Component::build_internal()`（`src/component.cpp:124-139`）在调用 `describe()` **之前**
先 `ctx_swap` 并置 `Component::current_ = this`；`describe()` 里的 `ch_module`
因此看到的就是当前这一层。第 N 层的 `describe()` 由第 N-1 层的 `ch_module` ctor 触发
⇒ 逐层递归。

> ⚠️ 前提是「`ch_module` 出现在 `describe()` 里」。只要挪到构造函数/成员声明区，
> 父指针就会出错 —— 见 §6 表第 1 条。

### 2.5 `create_child` / 手工 `build(ctx)`：不要拿来做装配

- `Component::create_child<T>(name, ...)`（`component.h:42-59`）：`make_shared` +
  塞进 `children_shared_`，**不调用 `build()`** ⇒ 子组件的 `create_ports()` /
  `describe()` **不会执行**。仓内唯一用途是 `tests/test_multithread.cpp:236-317`
  验证父子关系与 `child_count()`。
- `Component::build(ctx)`（`component.cpp:96-122`）有两个分支：
  - 传 `external_ctx` ⇒ 共享该 context（`ch_device` / `ch_module` 走这条）
  - 不传 ⇒ `new context(name_, parent_ctx)`，**自己再建一个 context**（含自己的
    `default_clock` / `default_reset`）

`Simulator`（`src/simulator.cpp:463`）与 `toVerilog`（`codegen_verilog.cpp:59-88`）
**只遍历传进去的那一个 context 的节点** ⇒ 跨 context 连线不受支持。

### 2.6 测试里的「承载组件」模式

`ch_module` 不能在 `TEST_CASE` 顶层使用（没有父组件 ⇒ ctor 早退 ⇒ 随后 `io()`
触发 `CHFATAL` ⇒ SIGSEGV）。测试多组件装配要写一个 wrapper：

```cpp
class PipelineTestTop : public ch::Component {
public:
    __io(ch_in<ch_bool> rst; ch_out<ch_uint<32>> pc;)
    PipelineTestTop(ch::Component* p = nullptr, const std::string& n = "top")
        : ch::Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override {
        ch::ch_module<Rv32iPipeline> pipeline{"pipeline"};
        pipeline.io().rst <<= io().rst;
        io().pc <<= pipeline.io().dbg_id_pc;
    }
};

TEST_CASE("...") {
    ch::ch_device<PipelineTestTop> top;    // ✅
    ch::Simulator sim(top.context());
}
```

参考实现：`examples/riscv-mini/tests/test_pipeline_debug.cpp:47-62`。

---

## 3. 跨层连线

### 3.1 只有 6 个入口

连线运算符全部定义在 `include/core/io.h`，共 6 个重载：

| # | 签名 | 行 | 用途 | 约束（`CHREQUIRE`） |
|---|---|---|---|---|
| 1 | `out <<= in` | 347-370 | 本层直通 | receiver 必须是**当前组件**的端口 |
| 2 | `in <<= in` | 372-395 | 父 in → 子 in | driver 必须是**当前组件**的端口 |
| 3 | `out <<= out` | 397-417 | 子 out → 父 out | receiver 必须是**当前组件**的端口 |
| 4 | `in <<= out` | 419-443 | 兄弟 out → in | receiver 必须是**子模块**的端口 |
| 5 | `out <<= U`（任意表达式） | 444-470 | 表达式驱动本层输出 | 无 |
| 6 | `in <<= U`（任意表达式） | 471-500 | 表达式/常量驱动子输入 | 无 |

这 4 个方向对**穷举了所有合法的端口↔端口连线**，没有第 5 种组合。

> ⚠️ 连线的时机：在**父组件的 `describe()` 内**。`lnodeimpl` 的 `parent_` 是
> **建节点时**用 `Component::current()` 记下的（`src/core/lnodeimpl.cpp:25-31`），
> 上面 4 条检查全靠它判归属。

### 3.2 四种端口方向

```cpp
void describe() override {
    ch::ch_module<Adder> add0{"add0"};
    ch::ch_module<Adder> add1{"add1"};

    add0.io().a <<= io().a;        // ② 父 in  → 子 in
    add0.io().b <<= io().b;
    add1.io().a <<= add0.io().y;   // ④ 兄弟 out → 兄弟 in
    io().y      <<= add1.io().y;   // ③ 子 out → 父 out
    io().mirror <<= io().a;        // ① 本层 out ← 本层 in（直通）
}
```

仓内实例：
- ② ③：`examples/riscv-mini/src/rv32i_pipeline_top.h:112-119`
- ④：`include/cpu/pipeline/rv32i_pipeline.h:238-262`、`examples/axi4/axi_soc_example_top.h:197-219`

### 3.3 表达式 / 常量 / 位切片驱动

走重载 5、6，子模块输入可以直接吃任何表达式：

```cpp
dtcm.io().valid <<= ch_bool(true);                                        // 常量
itcm.io().addr  <<= bits<kTcmAddrWidth + 1, 2>(pipeline.io().instr_addr); // 位切片
```
（`rv32i_pipeline_top.h:111,124`）

### 3.4 父层中间节点：扇出与 mux

`describe()` 里先建普通节点，再喂给多个子模块 —— 一个源驱动多个目的**是合法的**：

```cpp
ch_uint<8> shared = io().a + io().b;
m0.io().in <<= shared;                     // 扇出 ①
m1.io().in <<= shared;                     // 扇出 ②
io().y <<= select(io().sel, m0.io().out, m1.io().out);   // 跨层 mux
```

### 3.5 寄存器 / 存储器作为层间屏障

跨层的**时序边界**通常不是连线，而是父层持有 `ch_reg` / `ch_mem`，子层只经端口进出：

```cpp
ch_reg<ch_uint<32>> exmem_alu_result{0_d, "exmem_alu_result"};

exmem_alu_result <<= select(exmem_en, ex_stage.io().alu_result, exmem_alu_result); // 子 out → 父 reg
mem_stage.io().alu_result <<= exmem_alu_result;                                     // 父 reg → 子 in
```

- `ch_reg::operator<<=` 是**非阻塞赋 next**（`include/core/reg.h:52-57`），等价 `reg->next = expr`
- `ch_mem` 归父层持有，父层做 `aread` / `write`，把结果接给子层端口
  （`rv32i_pipeline.h:134, 189-196`）

### 3.6 bundle / stream 级连线

一组信号用 bundle 传，不要逐字段手接：

```cpp
connect(io_bundle, fifo_inst.instance().io);        // 方向感知（bundle_base.h:563-581）
result1 <<= consumer1_inst.instance().out_flow;     // bundle 级 <<=
consumer1_inst.instance().stream_in <<= producer_inst.instance().stream_out;  // ch_stream
```

- 方向由 `as_master()` / `as_slave()` 决定（`bundle_base.h:140-145`），
  `connect()` / `<<=` 会**按字段方向**决定谁驱动谁（`bundle_base.h:169-205`）
- `ch_stream` 的 `sink <<= source` 同时建立 `valid/payload` 前向边与
  `source.ready <<= sink.ready` 反向边（`include/chlib/stream_operators.h:32-40`）
- ⚠️ `bundle = bundle`（**赋值**）不是连接，是节点重绑定 ⇒ 原驱动被孤儿化
  （`bundle_base.h:207-224`）

### 3.7 穿透（跃层）接线

只能跟**直接子组件**和自己的端口连线。要越过一层，有两条路：

1. 中间层把需要的信号提升成 `__io` 端口（推荐）
2. 中间层暴露访问器 / 公开成员，父层用 `m0.instance().leaf.io()...` 摸下去

⚠️ 若靠「把 `ch_module` 声明成成员变量」来实现穿透，会踩 §6 表第 1 条的坑
（子模块会挂到**祖父**节点下，且同类型多实例**重名**）。

### 3.8 方向检查的真实强度

4 条 `CHREQUIRE` **只打日志、不中止**（`include/utils/logger.h:192-200`）：

```cpp
#define CHREQUIRE(condition, fmt, ...) \
    do { if (!(condition) && ...) log_message(error, ...); } while (0)
```

紧接着仍然会执行 `set_src()` / `set_driver()` ⇒ **方向写错能编译、能连上、能跑**，
只是 stderr 多一条 `Requirement failed`。所以「编译通过」不代表方向对。

### 3.9 多次驱动

| 类型 | 行为 |
|---|---|
| `ch_out` 被驱动多次 | `set_src(0, …)` ⇒ **last-wins**，前一个驱动变孤儿节点 |
| `ch_in` 被驱动多次 | `set_driver()` = `driver_ = drv; add_src(drv);`（`ast_nodes.h:156-159`）⇒ **替换指针但累积 src 边** ⇒ IR 里残留死边 |

仓内明确记录：`include/cpu/pipeline/rv32i_pipeline.h:250-256`（ADR-047 D8）。

### 3.10 禁止 / 无意义的组合

| 写法 | 结果 |
|---|---|
| `child.io().out <<= io().in` | 能连上，但语义反了（CHREQUIRE 记一条 error） |
| `child_a.io().in <<= child_b.io().in` | driver 必须是当前组件的端口 ⇒ 违反约定，仍会连上 |
| `ch_out` 用 `=` 赋一个未驱动节点 | 编译过、什么都不发生 |
| 两个 `ch_device` 各建一个再互连 | 两个 context、两套 default clock ⇒ 不受支持 |

---

## 4. 用成员变量当接口（不经 `io()`）

很多场景不需要 `__io`：组件之间可以直接共享**成员节点**。能力矩阵（全部实测）：

| 成员类型 | 能否当跨层接口 | 机制 |
|---|---|---|
| bundle 派生（`ch_stream` / `ch_flow` / AXI / 自定义 `bundle_base<Derived>`） | ✅ **推荐** | 字段是一等 `proxyimpl` 节点，`as_master()/as_slave()` 时创建 |
| `ch_reg<T>` | ✅ 可读、可改 `next` | 构造即建节点 |
| `ch_mem<T,N>` | ✅（构造期注入引用） | 构造即建节点 |
| 裸 `ch_uint<N>` / `ch_bool` | ❌ | 默认 `node_impl_ == nullptr`；给初始值则变 **lit 节点** |
| `ch_in` / `ch_out` 当普通成员（不在 `__io` 里） | ❌ | 没有 `io()`，不参与端口/方向语义 |

### 4.1 bundle 成员（推荐路径）

子模块**不需要 `__io`、不需要 `create_ports()`**：

```cpp
class Child : public ch::Component {
public:
    ch_stream<ch_uint<8>> s;
    ch_reg<ch_uint<8>>    acc{0_d, "acc"};

    Child(ch::Component* p = nullptr, const std::string& n = "child")
        : ch::Component(p, n) { s.as_slave(); }   // ★ 必需

    void create_ports() override {}               // 空
    void describe() override { acc->next = acc + s.payload; }
};

class Top : public ch::Component {
public:
    __io(ch_in<ch_uint<8>> a; ch_in<ch_bool> en; ch_out<ch_uint<8>> y;)
    ch_stream<ch_uint<8>> s;
    void describe() override {
        ch::ch_module<Child> c{"c"};
        s.as_master();                            // ★ 必需
        s.payload <<= io().a;
        s.valid   <<= io().en;
        c.instance().s <<= s;                     // 成员级连接，不经 io()
        io().y <<= c.instance().acc;
    }
};
```

三个必知点：

1. **必须先 `as_master()` / `as_slave()`**。字段节点是在
   `set_role()` → `create_field_proxy_nodes()` 里才创建的
   （`include/core/bundle/bundle_base.h:409-437, 487-530`）。不设 role ⇒ 字段是空节点，
   子层 `describe()` 读它就是 `Invalid operands` + 结果恒 0。
2. **方向由 role 决定**，同一份代码 master 侧写 `payload/valid`，slave 侧写 `ready`。
3. **bundle 成员不能当顶层 IO**：它是普通节点，不是 `ch_in/ch_out`
   ⇒ Verilator / 外部 TB 看不到。顶层仍要 `io().y <<= b.field` 提出来。

### 4.2 `ch_reg` / `ch_mem` 成员

```cpp
auto v = c.instance().acc;                     // 读
c.instance().acc->next = ch_uint<8>(119_d);    // 改下一拍值（覆盖子层自己的 next）
```

`ch_mem` 作为跨层共享资源时，用构造期注入把引用传进子模块（见 §4.3）。

⚠️ 父层直接改子层 `next` 是 **last-wins**，会**覆盖**子层 `describe()` 里写的逻辑。
它适合「外部可控状态 / 调试注入」，不适合替代端口。

### 4.3 构造期注入（引用 / 指针）

`ch_module` / `ch_device` 支持转发构造参数，但 **`Args...` 是类模板参数，必须显式写**：

```cpp
using MemT = ch_mem<ch_uint<8>, 16>;

class MemChild : public ch::Component {
public:
    MemT& mem;                                  // 构造期注入的引用
    MemChild(ch::Component* p, const std::string& n, MemT& m)
        : ch::Component(p, n), mem(m) {}
    void describe() override {
        mem.write(ch_uint<4>(0_d), ch_uint<8>(0xAB_h), ch_bool(true));
    }
};

ch::ch_module<MemChild, MemT&> c{"c", mem}; // ★ 注意第二组模板实参
```

> ⚠️ 被注入的节点对象 MUST 是父组件的**成员变量**（如 `rv32i_pipeline.h:134`
> 的 `reg_file`）。写成 `describe()` 里的**局部变量**也不会报错——但 `describe()`
> 返回后该 C++ 对象就析构了，子模块手里那条引用随即**悬垂**（节点本身仍在
> context 里，所以仿真结果可能"看起来对"）。

```cpp
ch::ch_device<SomeComp, int> dev(42);      // ch_device 同理：Args 也要显式写
```

> ⚠️ **注入的节点必须在 context 内构造**。`ch_mem` / `ch_reg` 的构造函数需要
> 活跃 context（否则抛 `No active context for memory creation`）。
> `ch_device` 的构造实参在**它创建 context 之前**求值 ⇒ 只能注入普通值，
> **不能注入节点**。节点注入只适用于 `describe()` 里的 `ch_module`。

写成 `ch::ch_module<MemChild> c{"c", mem}` 会编译失败（类模板参数不从 ctor 推导）：

```
note: candidate: 'ch::ch_module<T, Args>::ch_module(const std::string&, Args&& ...)
      [with T = MemChild; Args = {}]'
note:   candidate expects 1 argument, 2 provided
```

### 4.4 不成立的写法：裸 `ch_uint` / `ch_bool` 成员

```cpp
class Bad {
    ch_uint<8> data_in;                                   // 默认 null
    void describe() override { data_out <<= data_in; }    // ← 此刻仍为 null
};
```

子模块 `describe()` 先于父层连线执行（`ch_module` ctor 里就 `build()`），所以：

```
[ERROR] [ch_uint::operator<<=] ... node_impl_ or src_lnode is null for ch_uint<8>!
[ERROR] [ch_bool::operator=]    ... node_impl_ or src_lnode is null for ch_bool!
[ERROR] Invalid port connection: driver node is null
```

更糟的是**用字面量初始化**：`ch_uint<8> din{0_d};` 会变成 lit 节点，
父层 `din <<= x` 对 lit 无效 ⇒ **完全静默**，结果恒 0，一条错误都不打。

> `ch_uint` / `ch_bool` 上确实有 `as_input()/as_output()/direction()`
> （`include/core/uint.h:130-171`、`include/core/bool.h:55-78`），但那是
> `bundle_base::make_input/make_output` 的**内部**通道，不是给用户当模块接口的。

### 4.5 什么时候用成员、什么时候用 `io()`

| 你要传什么 | 用什么 |
|---|---|
| 单根信号、要在 Verilog / TB 可见 | `__io` 的 `ch_in/ch_out` |
| 一组协议信号（AXI / Stream / Flow / 自定义束） | **bundle 成员** + `as_master/as_slave` |
| 共享存储 / 跨模块共享状态 | `ch_mem` / `ch_reg` 成员 + 构造期注入 |
| 调试期改子模块状态 | `child.instance().reg->next = …`（谨慎） |
| 裸 `ch_uint` 想当线 | ❌ 改用 `ch_reg`，或走 `io()` 端口 |

---

## 5. RTL 输出形态

框架有**三个** RTL 出口 ⇒ ⇒ ⇒ 同一份 ctx、同一套发射底座 ⇒ ⇒ ⇒ **默认参数下**前两条
**逐字节一致**（差别只在**端口面**）：

| 入口 | 产物 | 何时用 |
|---|---|---|
| `ch::toVerilog(filename, ctx)` | **单个** `module top` · 端口面 = **ctx 里每个 `ch_out`** + 无 driver 的 `ch_in` | **遗留 / 非产品**路径（既有 49 处调用点）⇒ ⇒ 产物**逐字节不变** |
| `ch::toVerilog(filename, root, options)` | **单个** `module top` · 端口面 = **根组件自有 IO** | **产品**路径（`cpphdl-emit-rtl` / 各 example 发射器） |
| `ch::toVerilogHierarchical(out_path, root, options)` | **多个** module（**单文件**） | 需要**可读的层级 RTL**（IP 单独交付 / 逐级看 RTL / 外部评审） |

### 5.1 扁平产物（默认）

`ch::toVerilog(filename, ctx)`（`include/codegen_verilog.h:67`）**只接收 context**，
固定输出一个 `module top (`（`src/codegen_verilog.cpp:313`）到 `endmodule`
（`:390`）。整个 writer 里**没有任何** `children()` / `child_count()` 遍历，
也不发射实例化语句。

三层设计（Top → Mid → Leaf，**均未**声明成层）实测输出：

```verilog
module top (
    input default_clock,
    input default_reset,
    input [7:0] top_unnamed_input,
    output [7:0] top_unnamed_output,
    output [7:0] top_m_unnamed_output,           // 子模块的 ch_out 也在端口列表里
    output [7:0] top_m_lf_unnamed_output         // 孙模块的 ch_out 也在
);

    logic [7:0] top__unnamed_input;
    logic [7:0] top_m_unnamed_input;
    logic [7:0] top_m_lf_unnamed_input;
    /* … 体内线网与 assign 省略 … */

    assign top_m_unnamed_input  = top_unnamed_input;
    assign top_m_lf_unnamed_input = top_m_unnamed_input;
    assign top_m_lf_unnamed_output = top_m_lf_unnamed_input;
    assign top_m_unnamed_output = top_m_lf_unnamed_output;
    assign top_unnamed_output  = top_m_unnamed_output;
endmodule
```

⚠️ **归属已修正**（ADR-059 · T3-①a/①b，已归档）⇒ ⇒ 节点归属 = **构造它的组件**
⇒ ⇒ 名字**不再**重复父前缀（早先的 `top_top_m_lf_*` 形态**已消解**）⇒ ⇒ 上面的
`top_m_lf_unnamed_output` 就是修正后的形态。

### 5.2 扁平产物的端口规则

⚠️ **两条入口的端口面不同** —— 选错入口 = 交付「不该出现的端口」。
**产品端口面的唯一权威 = manifest 的 `port_table[]`**（`rtl-emit-manifest-v2`）。

#### 5.2.1 ctx-only 入口：`toVerilog(filename, ctx)`

| 节点 | 在生成的 Verilog 里 |
|---|---|
| 顶层 `ch_in`（无驱动） | `input` 端口 |
| **任意层**被驱动的 `ch_in` | 内部 `logic` + `assign`（`ModuleEmitter::print_decl` · `src/codegen_verilog.cpp:396` + `print_logic` · `:525`） |
| **任意层**的 `ch_out` | `output` 端口（**只过滤 input，不过滤 output** ⇒ `ModuleEmitter::print_output` · `src/codegen_verilog.cpp:707`） |

推论：多层设计里「某个层的输入忘了驱动」在 Verilog 里表现为**顶层凭空多出一个
input 端口**；而子模块输出会全部暴露在顶层端口列表里（端口表随层次变长）。

⚠️ **这是遗留 / 非产品路径**（ADR-072）：产物**逐字节冻结**（既有 49 处调用点
不受影响），但它**不是**产品端口面 —— 下游 IP 交付 MUST 用带根入口。

#### 5.2.2 带根入口：`toVerilog(filename, root, options)`

端口面 = `default_clock` → `default_reset` → **根组件自有的、无 driver 的 `ch_in`**
→ **根组件自有的 `ch_out`**。**后代**（成层或 inline）的 `ch_in` / `ch_out` **一律**
落到体内 `logic`（不再被抬成顶层端口）。

| 节点 | 在生成的 Verilog 里 |
|---|---|
| 根自有 `ch_in`（无驱动） | `input` 端口 |
| **后代**的 `ch_in`（无论有没有 driver） | 体内 `logic`（有 driver 的发 `assign`；无 driver 的 ⇒ 见下） |
| 根自有 `ch_out` | `output` 端口 |
| **后代**的 `ch_out` | 体内 `logic` + `assign out = <落点网>` |

⚠️ **唯一例外：悬空的后代 `ch_in`**（既没有父层驱动、也没有 tie-off）⇒ ⇒
**响亮拒绝**（`ok = false` + 点名 + 不产生文件）—— 不再「冒泡成顶层 `input`」。
接法只有两条：给它接上驱动源，或由父层显式 tie-off 常量（tie-off 的输入**有** driver）。

```cpp
// 产品路径：端口面 = 根组件自有 IO
ch::toVerilog("top.v", dev.instance(), ch::EmitOptions{});
```

### 5.3 层次化产物

#### 5.3.0 拿到层次化产物：两种入口

| 入口 | 形态 | 适用 |
|---|---|---|
| **CLI**（推荐） | `cpphdl-emit-rtl --hierarchical --out <dir>`（或调用 `ch::rtl_emit::run_emit_cli` 时传 `--hierarchical`） | 用户在自己的可执行文件里登记设计 + 一个 flag ⇒ 同时拿到产物 + manifest |
| C++ API | `ch::toVerilogHierarchical(path, root, opts)` | 需要自定义选项 / 在测试内发射 |

⚠️ **CLI 的两种形态（扁平 / 层次化）产物顶层端口表同源**（都走 `make_flat_module_view`）
⇒ ⇒ **同一个独立 Verilog TB 可吃两种产物**（顶层 module 名同为 `top`）
⇒ ⇒ 仓内实例 = `examples/riscv-mini/emit_rtl_rv32i_soc_hier`（层次化）与
`emit_rtl_rv32i_soc`（扁平）共用 `tb_rv32i_soc.sv` ⇒ 两者 `checks=24 fails=0` 完全相同。

#### 5.3.1 怎么声明成层

**默认不成层**（inline）⇒ ⇒ 组件默认**不**产生独立 module ⇒ ⇒ 它的逻辑留在**最近的
成层祖先** module 体内 ⇒ ⇒ 谁都不声明时，层次化产物与扁平产物**逐字节一致**。

三种声明方式（**成层判定与装配点解耦** ⇒ 没有 active parent 的装配点也一样成立）：

```cpp
// ① 类级：组件作者声明「我是一层」—— MUST 给名字（宏体 = 声明 + 命名一体）
class Mid : public ch::Component {
public:
    CH_HIERARCHY_LAYER("my_mid");           // ← module 名 = "my_mid"
    ...
};

// ② 实例级 · 档 ②：T 已类级声明成层 ⇒ 继承 T 的类级名（单参形态）
ch::ch_layer<Mid>  m{"m"};

// ③ 实例级 · 档 ③：T **未**声明（helper 被提升）⇒ MUST 在实例处给名（双参形态）
ch::ch_layer<Helper> h{"h", "helper_layer"};

// ④ 实例级：显式 inline（覆盖类级声明）
ch::ch_inline<Mid> i{"i"};
```

⚠️ `CH_HIERARCHY_LAYER` **必须带名字**（括号里给一个**字符串字面量**）⇒ ⇒
**不带名字的写法编译不过** ⇒ ⇒ 「成层却忘了命名」这个组合**在编译期就不可表达** ⇒ ⇒
不存在「运行期才发现名字不对」。

#### 5.3.2 完整可编译示例

```cpp
#include "ch.hpp"
#include "hierarchy_verilog.h"

using namespace ch;
using namespace ch::core;

class Leaf : public ch::Component {
public:
    __io(ch_in<ch_uint<8>> a; ch_out<ch_uint<8>> y;)

    Leaf(ch::Component *parent = nullptr, const std::string &name = "leaf")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }
    void describe() override { io().y <<= io().a; }
};

class Mid : public ch::Component {
public:
    CH_HIERARCHY_LAYER("my_mid");          // ← 成层，并给出 module 名

    __io(ch_in<ch_uint<8>> in; ch_out<ch_uint<8>> out;)

    Mid(ch::Component *parent = nullptr, const std::string &name = "mid")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }
    void describe() override {
        ch::ch_module<Leaf> lf{"lf"};       // ← helper：未声明成层 ⇒ 默认 inline
        lf.io().a <<= io().in;
        io().out <<= lf.io().y;
    }
};

int main() {
    ch::ch_device<Mid> dev;
    ch::HierarchyEmitResult r = ch::toVerilogHierarchical("mid.v", dev.instance());
    if (!r.ok) {
        return 1;                            // r.error 点名失败原因
    }
    return 0;
}
```

⚠️ 组件**必须**提供 `(parent, name)` 构造器（`ch_device<T>` 与 `ch_module<T>` 都按
`T(parent, name)` 构造）⇒ ⇒ 见 §1.1「三件套」。

#### 5.3.3 产物长什么样

同一棵设计，把 `Mid` 声明成层后，实测产物（**单文件多 module**）：

```verilog
// ---- module: top.m (my_mid) ----
module my_mid (
    input  [7:0] top_m_unnamed_input,
    output [7:0] top_m_unnamed_output
);
    logic [7:0] top_m_lf_unnamed_input;
    logic [7:0] top_m_lf_unnamed_output;
    /* … 体内线网与 assign 省略 … */
endmodule

// ---- module: top (top) ----
module top (
    input default_clock,
    input default_reset,
    input  [7:0] top_unnamed_input,
    output [7:0] top_unnamed_output
);
    my_mid m (
        .top_m_unnamed_input(top_unnamed_input),
        .top_m_unnamed_output(top_unnamed_output)
    );
endmodule
```

⇒ ⇒ 三条**看得见**的事实：

1. **module 名** = `CH_HIERARCHY_LAYER("...")` 给的名字；**实例名** = `ch_module` 的
   第一个参数（局部名）⇒ ⇒ 两者都会出现在外部评审看到的 RTL 里；
2. **端口名仍是设计级全局名**（`top_m_unnamed_input` 带全路径前缀）⇒ ⇒ 这是**故意**的
   —— 顶层端口名 MUST 与扁平产物**逐字相同**（下游按名解析不受影响）；
3. ⚠️ **顶层端口比 ctx-only 扁平**少了 `top_m_unnamed_output` / `top_m_lf_unnamed_output`
   ⇒ ⇒ 这不是「丢了」，是**契约**（后代 `ch_out` 一律落在体内 `logic`）⇒ ⇒
   **观测盲区**见 §5.5。

#### 5.3.4 层次化产物的端口规则

| module | 端口集合 |
|---|---|
| **根** module | **只含根组件自有 IO**（clock → reset → 无 driver 的**根自有** `input` → **根自有** `ch_out`）⇒ ⇒ 与 `toVerilog(path, root)` 的产品端口表 / manifest 的 `port_table[]` **逐字逐序相同** |
| **非根**成层 module | 自身 `ch_in` ∪ 自身 `ch_out` ∪ **隐式端口** ∪ 时序基础设施（clock / reset，**分开判**） |

- **隐式端口**：跨层**非端口边**按方向合成（父→子 = 子侧 `input`；子→父 = 子侧 `output`）
  ⇒ ⇒ 端口名带框架前缀 `__if_` / `__x_`（`__up_` = 向上 pass-through）⇒ ⇒
  **这正是 §1 的 D1 / D5 要避免的写法**；
- **悬空的后代 `ch_in`**（无驱动源）⇒ ⇒ **响亮拒绝**（`ok = false` + 点名 + 不产生文件）
  ⇒ ⇒ 不再「向上冒泡成顶层 `input`」（ADR-072 D1-f）⇒ ⇒ 接上驱动源或由父层 tie-off 常量；
- **跨模块 `ch_mem`**（父层直接读 `child.mem.aread()`）⇒ ⇒ **响亮拒绝**（`ok = false`
  + `error` 点名 + **不产生任何文件**）⇒ ⇒ 改用「mem 与使用者同层」；
- **成层组件自驱动自己的 `ch_in`** ⇒ ⇒ 同样**响亮拒绝**；
- ⚠️ 失败一律走 `HierarchyEmitResult`：`ok = false` + `error` 点名 + `files` 为空
  ⇒ ⇒ **发射器内不抛异常**（异常会落进 `print()` 的 `catch(...)` ⇒ 变成「静默失败 +
  看起来成功」）⇒ ⇒ **调用方 MUST 检查 `ok`**。

#### 5.3.5 选项

```cpp
ch::HierarchyEmitOptions opts;
opts.port_policy    = ch::PortPolicy::SynthesizeWarn;   // 默认：隐式端口 + 告警
opts.undriven_policy = ch::UndrivenPolicy::Reject;      // 默认（且唯一）：悬空后代输入被拒
opts.top_module_name = "top";                            // 根 module 名
ch::HierarchyEmitResult r = ch::toVerilogHierarchical("out.v", dev.instance(), opts);
```

⚠️ `port_policy` / `undriven_policy` 的**唯一权威**是 `ch::PortPolicy` /
`ch::UndrivenPolicy`（`include/component.h`）⇒ ⇒ 不要自己再定义一份枚举。
⚠️ 根 module 名**只**由 `top_module_name` 决定 ⇒ ⇒ 根组件自己写
`CH_HIERARCHY_LAYER("x")` 是 **no-op**（不告警）。
⚠️ 层次化产物**文件名 MUST NOT** 与扁平产物同名（同名 ⇒ 「同名不同物」）⇒ ⇒
仓内护栏 `scripts/verify_hierarchical_rtl.sh` 的 **R7** 静态扫描调用点。

### 5.4 层级友好度：三类「贵」的写法

见 [层次化 RTL 写作指南](../developer_guide/patterns/HIERARCHICAL-RTL-AUTHORING-GUIDE.md)
§1 的 **D1 / D4 / D5**（bundle 成员跨层 · 悬空子输入 · 父层直接改子层 `reg->next`）
⇒ ⇒ 这三类写法会在产物里留下 `__if_` / `__x_` 端口或凭空多出的顶层 `input`。

### 5.4-bis 层次友好度**诊断 API**（`ch::diagnose_hierarchy`）

> 权威 = capability `hierarchy-friendliness-diagnosis`。
> ⚠️ **头文件不在 `ch.hpp` 聚合器里** ⇒ ⇒ 必须**显式** include：

**为什么需要**：层次化分析的默认入口 `ch::analyze_hierarchy` 是 **fail-closed**
（`ImplicitPortHandling::Reject`）⇒ ⇒ 遇到第一条「不友好」就**抛** ⇒ ⇒ 用户只看到
「被拒」，看不到「**几处**不友好、要改哪里」。诊断 API 把这些**一次报全**。

```cpp
#include "ch.hpp"
#include "hierarchy_analysis.h"    // ★ 不是 ch.hpp（该头不在聚合器里）
#include <cstdio>

using namespace ch::core;         // ★ ch_in / ch_out / ch_uint 在这里

class MyTop : public ch::Component {
public:
    __io(ch_in<ch::core::ch_uint<8>> din; ch_out<ch::core::ch_uint<8>> dout;)
    MyTop(ch::Component *p = nullptr, const std::string &n = "top")
        : ch::Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override { io().dout <<= io().din; }
};

int main() {
    ch::ch_device<MyTop> dev;
    const ch::HierarchyDiagnosis d = ch::diagnose_hierarchy(dev.instance());

if (d.verdict == ch::HierarchyFriendliness::kNeedsWork) {
    for (const auto &det : d.details) {
        // det.category ∈ {implicit_edge, dangling_input,
        //                 unconsumed_child_output, cross_module_mem}
        // det.node = **Verilog 名**（⇒ 可直接在产物里搜到）
        printf("%s | %s | %s\n", det.category.c_str(), det.module.c_str(),
               det.node.c_str());
    }
}
    std::printf("%s", d.to_text().c_str());  // ★ 确定性快照（两次调用逐字节相同）
}
```

**报告的 5 项**（全部是 `std::optional` ⇒ 「**未测得**」与「**实测为 0**」
**结构上可区分** ⇒ ⇒ 不可分析时不会交出一份「全是 0」的误导报告）：

| 项 | 含义 | 怎么读 |
|---|---|---|
| `implicit_edge_count` | 隐式**边**数 | 单位 = **边**（⇒ ≠ 端口数） |
| `synthesized_port_count` | 需合成的**端口**数 | ≈ **代价**（同类隐式边可能是 1 个端口，也可能是 N 跳端口链）⇒ ⚠️ **本数只覆盖「直接父子」隐式边**（`__if_` / `__x_`）⇒ ⇒ **不含**向上 pass-through（`__up_`）与第三轴链 ⇒ ⇒ **链长不可由本报告推导** ⇒ 别把「端口数小」读成「成本低」 |
| `dangling_input_count` | **跨层**悬空输入数 | 根自己的 / inline 折叠进根的**不计**（与发射器同款口径） |
| `unconsumed_child_output_count` | 未被子层消费的输出数 | 按**实例**判（多实例里只有一个消费 ⇒ 仍算未消费） |
| `cross_module_mem_count` | 跨模块 `ch_mem` 访问数 | 正常路径必为 **0**（分析层对它**永远抛**）⇒ ⇒ 不可分析时由**独立预扫描**报出计数 ⇒ ⚠️ **该数是「尽力而为」的**（归属不明的边会被跳过 ⇒ 可能偏小）⇒ ⇒ MUST NOT 当成精确值 |

**3 档**（`kClean` / `kNeedsWork` / `kNotAnalyzable`）⇒ ⇒ **不是 5 档**：
5 大项里有 4 项是**正交事实**（隐式边 / 悬空输入 / 未消费输出 / 跨模块 mem），
谁更糟**没有客观依据**（默认策略下隐式边与悬空输入**都是可发射的告警**）⇒ ⇒
5 档是**伪全序** ⇒ ⇒ 严重度信息放在**明细**里。

⚠️ **四个必须知道的边界**：

| # | 边界 | 后果 |
|---|---|---|
| 1 | **结果依赖当前 `GranularityScope`** | inline 折叠会改变 module 归属 ⇒ ⇒ 同一设计在不同粒度下**结果可能不同**（如跨模块 `ch_mem` 在 `ForceInline` 下折进同层 ⇒ 0） |
| 2 | 诊断路径**必然**在 stderr 出现 `FATAL:` | 分析层的拒绝宏是「**先 log 再 throw**」⇒ ⇒ 用户设计有问题时会先看到 FATAL 日志 |
| 3 | **静态析构期**该宏**一律不抛** | ⇒ ⇒ 该期可能得到**部分结果** |
| 4 | **不覆盖**「子 `ch_out` 有端口但体内无驱动」 | 该形态**不在** 5 项里 ⇒ ⇒ 别把报告当成「全部问题清单」 |

⚠️ **边界**：诊断**不**解决「前置校验首错即抛」⇒ ⇒ 15 个拒绝点里**命中第一条就抛**
⇒ ⇒ 迭代次数仍与 precheck 缺陷数成正比 ⇒ ⇒ 本 API 解决的是**「隐式边家族」的完整性**。

### 5.4-ter 层次化**等价性 helper**（公共 API · `ch::hier_eq`）

> 权威 = capability `hierarchical-equivalence-public-helper`。
> ⚠️ **头名 ≠ 命名空间**：头是 `hierarchy_equivalence.h`，命名空间是 `ch::hier_eq`
> ⇒ ⇒ 两处都要写（用户 `grep` 头名时找不到符号名）。

**它回答什么**：用户自己的设计**改成层之后，行为变了吗**。
⇒ ⇒ 这是「值不值得成层」的判据之一（ADR-058 决策的落点）。

#### 怎么用

```cpp
#include "ch.hpp"
#include "hierarchy_equivalence.h"    // ★ 显式 include（**不在** ch.hpp 聚合器里）
// ⚠️ 命名空间：ch::hier_eq（★ **不是** ch::hierarchy_equivalence）

#include <cstdio>
#include <string>
#include <sys/stat.h>
#include <vector>

using namespace ch::core;   // ★ ch_in / ch_out / ch_uint 在这里

class MyTop : public ch::Component {
public:
    __io(ch_in<ch_uint<8>> din; ch_out<ch_uint<8>> dout;)
    MyTop(ch::Component *p = nullptr, const std::string &n = "top")
        : ch::Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override { io().dout <<= io().din; }
};

int main() {
    ch::ch_device<MyTop> dev;
    const std::string work_dir = "/tmp/my_hier_eq";   // ★ 调用方给的临时目录
    // ⚠️ **work_dir MUST 由调用方创建** ⇒ ⇒ `emit` 只在它**之下**建 `flat/` `hier/`
    //    ⇒ ⇒ 不建它本身（⇒ 不建 ⇒ `emit` 返回 `emit-failed`）。
    ::mkdir(work_dir.c_str(), 0755);                  // 已存在 ⇒ EEXIST（无害）

    // ① 生成层次化产物 + **由产物派生**端口集合 + 映射表
    auto products = ch::hier_eq::emit(dev.instance(), work_dir,
                                       ch::Granularity::RespectDeclarations);
    if (!products.ok) {
        std::printf("emit failed: %s\n", products.error.c_str());
        return 1;
    }

    // ② 声明受驱动的输入（★ MUST 用 set_input_value，**MUST NOT** 用 <<=）
    std::vector<ch::hier_eq::DrivenInput> inputs;
    inputs.push_back(ch::hier_eq::DrivenInput{
        "top.din", [&dev](ch::Simulator &s, std::uint64_t v) {
            s.set_input_value(dev.instance().io().din, v);
        }});

    // ③ 声明被观测的输出（★ 用工厂 ⇒ node_id 由 helper 填 ⇒ **不**接触 impl()）
    std::vector<ch::hier_eq::Observation> observations;
    observations.push_back(ch::hier_eq::make_observation(
        "top.dout", dev.instance().io().dout,
        [&dev](ch::Simulator &s) -> std::uint64_t {
            return static_cast<std::uint64_t>(s.get_port_value(dev.instance().io().dout));
        }));

    // ④ 逐拍比对「扁平产物」与「层次化产物」
    auto result = ch::hier_eq::compare(dev.context(), work_dir, products.hier_path,
                                        products.ports, inputs, observations,
                                        {{0x11}, {0x22}, {0x33}});
    if (!result.ok) {
        std::printf("compare failed: %s\n", result.error.c_str());
        return 1;
    }
    std::printf("equal = %s\n", result.equal ? "true" : "false");
    return result.equal ? 0 : 1;
}
```

> ⚠️ **示例的权威来源**：`tests/test_public_equivalence_api.cpp`（★ **仓外视角**的
> 真实消费方：它**只** include 公共头 ⇒ ⇒ 真编译 + 真链接 + 真运行 ⇒ ⇒ 证明公共面
> 自足）⇒ ⇒ 文档只**引用**它 ⇒ ⇒ 杜绝文档腐烂。

#### 端口口径（★ 选错端口 = 假绿）

| 端口 | 扁平侧 | 层次化侧 | 规则 |
|---|---|---|---|
| 根 `io()` 的 `ch_in` / `ch_out` | 顶层端口 | **恒为**顶层端口 | ⇒ `DrivenInput` / `Observation` **只能**取**根**的 |
| **子组件**的 `ch_out` | **多出**为顶层端口 | **不在**顶层（ADR-058 §4 风险 6 的盲区） | ⇒ **MUST NOT** 观测 ⇒ ⇒ 观测了 ⇒ ⇒ helper **机械拒绝**（前缀 `observation-not-visible`） |

#### fail-closed：错误前缀是**稳定契约**（可机械分类）

| 前缀 | 触发 | 用户下一步 |
|---|---|---|
| `empty-observations` | 观测集合为空 | 声明至少一个被观测信号 |
| `empty-stimulus` | 激励为空 | 给至少一拍激励 |
| `no-varying-signal` | 没有任何被观测信号随激励变化 | 换一个**随激励变化**的信号来观测 |
| `emit-failed` | 层次化发射失败 | 看 `error` 里的发射原因 |
| `port-set-empty` | 派生端口集为空 | 检查 DUT 是否有顶层端口 |
| `port-width-64-exceeded` | 端口 >64 bit | 缩小该端口位宽（**点名了端口与位宽**） |
| `no-top-level-output` | DUT 没有顶层 `ch_out` | 给 DUT 加一个顶层 `ch_out` |
| `backend-init-failed` | backend 初始化失败 | 看 backend 的原文 |
| `verilator-unavailable` | verilator 不可用 | ⚠️ **正确**的路是 `cmake --build build --target verilator`（★ **不是** `apt install verilator`） |
| `observation-not-visible` | 观测了层次化顶层**不可见**的信号 | 只观测根 `ch_out` |

#### 判别力边界（★ **必读**，否则会误用）

| | 本 helper | 既有双后端 parity |
|---|---|---|
| 比较什么 | **两份 Verilog 产物**（扁平 vs 层次化） | **C++ 解释器 vs Verilog 编译产物** |
| 判别什么 | 「**产物变换**是否引入行为分叉」 | 「**两套执行后端**是否行为一致」 |
| 对「共享发射底座算错」 | **结构性失明**（两产物同源 ⇒ 同构缺陷一起错） | **能抓** |
| 对「层次化变换算错」 | **能抓** | **结构性失明** |

⇒ ⇒ 两者**互补**，**MUST NOT** 互相替代 ⇒ ⇒ 本 helper **不**证明产物语义正确。

#### 其余边界

- ⚠️ **A 侧固定是「扁平产物」** ⇒ ⇒ 「两个层次化粒度互比」**不**直接支持 ⇒ ⇒ 走
  **传递性**（A ≡ 扁平 ∧ B ≡ 扁平 ⇒ A ≡ B）
- ⚠️ **`ProductSet` 与其 `ctx` 绑定**（`node_id` 是 **ctx 私有**编号 ⇒ ⇒ 不可跨 ctx 传递）
- ⚠️ **Linux-only**（Verilator backend 的既成限制）
- ⚠️ **不**提供 verilator 编译的**超时控制**（调用方自管）· **不**承诺在**已仿真过**的 ctx 上可重入

### 5.5 ⚠️ 观测盲区（等价性只覆盖**顶层可见行为**）

> **契约边界**（ADR-058 §4 风险 6）· 权威 = capability `rtl-hierarchical-output`。

层次化的**顶层输出集** = 扁平顶层输出集 **−** {`ch_out` 的最近成层祖先 ≠ 根 的全部节点}
⇒ ⇒ 被移出顶层的那批 `ch_out` 在层次化产物里**降级为子 module 端口**（或空连接
`.port()`）⇒ ⇒ **顶层观测不到** ⇒ ⇒ 「子树内部算错、但顶层输出恰好相同」这一类缺陷
**结构性测不出**。

⚠️ **这不是缺陷，是契约边界** ⇒ ⇒ MUST 如实登记，MUST NOT 假装全覆盖：

- **输出方向有盲区**：成层子树的 `ch_out` 顶层读不到 ⇒ ⇒ 等价性判据只覆盖
  **顶层可见行为**；
- **输入方向无盲区**：顶层 `input` 集合**两侧相等** ⇒ ⇒ 激励可完整施加。

⇒ ⇒ 等价性怎么判（可机械判定）：层次化产物 MUST 与扁平产物**逐拍等价** ——
判据由 capability `rtl-hierarchical-output` 的 Requirement
「层次化产物 MUST 与扁平产物等价（可机械判定）」给出（映射表 + 输入端口集合相等 +
输出端口集合等于精确期望集 + 显式观测集合上的逐拍全等 + 盲区以**对偶断言**登记），
仓内实现见 `tests/test_hierarchy_equivalence.cpp`。

⇒ ⇒ 想让**自己**的设计也被这样判一遍 ⇒ ⇒ 用 `HierarchyEmitResult::signal_map`
（= 四元组：`node_id` → 设计级信号名 → 扁平位置 → 层次化位置集合）定位信号 ⇒ ⇒
`signal_map` MUST 由 emitter **同次产出**（MUST NOT 自建 ⇒ 否则表错则两错相抵）。

### 5.6 写代码要记住的三条

1. **默认不成层**：组件默认不产生 module（逻辑留在父 module 里）⇒ 小组件不需要任何声明；
2. **成层要声明**：`CH_HIERARCHY_LAYER("name")`（类级，**必须带名**）或
   `ch_layer<T>` / `ch_inline<T>`（实例级覆盖）；
3. **默认路径零变化**：谁都不声明时，层次化产物与扁平产物**逐字节一致** ⇒
   可以**先写代码、零风险**，将来要交付某个 IP 单元时再加一行声明。

### 5.7 护栏与范例

| 用途 | 位置 |
|---|---|
| 产物级护栏（lint 门 / pin 名 / 宽度 / 确定性 / 拒绝 / 树同构 / 路径分离） | `scripts/verify_hierarchical_rtl.sh`（R1–R8 + `--self-test`） |
| 公共 API 面守卫（harness MUST NOT 进 `include/`） | `scripts/verify_hierarchical_p8.sh` |
| 完整可编译范例 | `tests/test_hierarchy_equivalence.cpp`（等价性）/ `tests/test_hierarchy_verilog.cpp`（发射） |
| 写法指南（**什么时候该成层**的判据） | `docs/developer_guide/patterns/HIERARCHICAL-RTL-AUTHORING-GUIDE.md` |

> 架构主题文档：`docs/architecture/2026-11-02-hierarchical-rtl-output.md`
> （取代 `docs/developer_guide/architecture/verilog-codegen-architecture.md` §7.1）。

---

### 5.8 SoC 装配（**下游怎么用**：产物怎么接线 · 端口怎么找 · TB 怎么搭）

> 设计定义 = `examples/axi4/axi_soc_example_top.h`（SoC = Test Master + Matrix 2x2 + GPIO + UART）
> ⇒ ⇒ 激励与发射入口 = `examples/axi4/tb_axi_soc_example.cpp`（**三模式**：默认激励 · `--emit-rtl` · `--check-products`）。

#### 产物怎么来

```bash
cmake --build build --target tb_axi_soc_example -j$(nproc)
build/examples/axi4/tb_axi_soc_example --emit-rtl --out build/rtl_products/soc
```

⇒ 产出 **5 个 module**（`AxiLiteTestMaster` / `AxiLiteMatrix2x2_32_32_` / `AxiLiteGpio_8_` /
`AxiLiteUart_16_` / `top`）—— IP 边界**在产物里可见**（这是「SoC 层产物」与「单 IP 产物」的
结构区别）。

#### 端口怎么找

顶层端口 = **框架 2**（`default_clock` / `default_reset`）+ **根自有 12**（8 in + 4 out）
= **14**；层次化顶层**不含**任何后代 IP 的端口。

✅ **产品面已具名**（2026-11-18 · `example-file-boundary-2026-11` B2 · ADR-075 的
「首批 32 个顶层端口具名」落地）⇒ ⇒ **产物端口名 = `top_<C++ 成员名>`**
（`top_start` / `top_rdata` …）⇒ ⇒ **端口 ↔ C++ 成员直接同名**，不再是「按编号猜」。

⚠️ **通用未具名机制仍在**（**未具名**的顶层组件 ⇒ 产物端口名退化为
`top_unnamed_input_N` 那种编号形态）⇒ ⇒ 讲**机制**见 §1.3 / §5.1 / §5.3.3；
**产品面**（本节这张表）**已不是**那种形态。
⇒ ⇒ 权威的端口 ↔ C++ 成员对照表在
`.rddf/checks/soc-rtl-expectations.md#soc-example` §1。

| 产物端口 | 方向/位宽 | C++ 成员 | 备注 |
|---|---|---|---|
| `top_start` | in / 1 | `start` | 启动一笔事务 |
| `top_write` | in / 1 | `write` | 1=写 · 0=读 |
| `top_addr` | in / 32 | `addr` | `addr[31]=0`⇒GPIO · `=1`⇒UART |
| `top_wdata` | in / 32 | `wdata` | 写数据 |
| `top_rst_n` | in / 1 | `rst_n` | **低有效**复位 |
| `top_gpio_in` | in / 8 | `gpio_in` | |
| `top_uart_rx` | in / 1 | `uart_rx` | **空闲高**（低电平会被当起始位） |
| `top_timer_irq` | in / 1 | `timer_irq` | **零消费死端口** ⇒ 接 `0` |
| `top_rdata` | out / 32 | `rdata` | 读数据 |
| `top_done` | out / 1 | `done` | ⚠️ **粘滞**（首笔后恒 1）⇒ **禁**用它判完成 |
| `top_gpio_out` | out / 8 | `gpio_out` | |
| `top_uart_tx` | out / 1 | `uart_tx` | **空闲高** |

#### 怎么接线

⚠️ **14 个端口 MUST 全部连接**（否则严格 flags 下 `%Warning-PINMISSING` **致命**）。

⚠️ **AXI payload MUST 在事务期间保持稳定**：master 的 `awaddr` / `wdata_axi` 是**组合**跟随顶层
`addr` / `wdata` 的 ⇒ ⇒ 事务没握完就清零 ⇒ **写进去的是 0 而不是你的数据**，且**不报任何错**。

#### TB 怎么搭

范例 = `examples/axi4/tb_axi_soc_example.sv`（`RESULT: checks=14 fails=0`）：

- 复位释放 MUST 在 **negedge**（在 `posedge` 同刻拉低 ⇒ 复位拍被吃掉）；
- 采样时机 `@(posedge clk); #1;`；
- **按已知延迟采样**（事务 ≤ 4 拍 ⇒ 取 16 拍余量）⇒ **禁**等 `done` 上升沿（它粘滞）；
- 期望值出处 MUST 指向登记文档（`EXPECT-SRC:`）⇒ ⇒ **禁**把被实现 `.cpp` 写成出处。

#### 护栏

`scripts/verify_examples_rtl_sim.sh` 的 **④″**（`kind=soc` 专用）会跑
`tb_axi_soc_example --check-products --out <dir>` ⇒ ⇒ 一次判掉：**module 集 · 顶层端口面 · lint ·
双形态逐拍等价 + 变异自证**（篡改层次化产物 ⇒ 必红 ⇒ ⇒ 判据非恒绿）。

### 5.9 产品面声明（**交付前必读**）

> 权威 = ADR-072 的 **D2 / D3 / D4** · 可执行载体 = `scripts/verify_examples_rtl_sim.sh`
> 的**判据⑧**（四件套）与 ④″（SoC 产物级）。

#### 5.9.1 产品单元 = **四件套**

一个「产品单元」= 下列四件**齐备**才算产品化完成（只有前两件 = **工具**，不是产品）：

| # | 件 | 判据 |
|---|---|---|
| ① | **manifest**（机器可读契约） | `manifest.json` 存在且 schema = `rtl-emit-manifest-v2` |
| ② | **Verilog 产物** | `top.v` 存在且 lint 干净（卫生产物） |
| ③ | **独立可跑的 TB** | 独立编译 + 跑通 + `RESULT: checks=<N> fails=0` + 失败**非零退出码** |
| ④ | **覆盖登记** | `.rddf/checks/examples-rtl-sim-targets.tsv` 有该行（`status=verified`） |

🔴 **最硬的耦合**（= 判据⑧）：manifest 的 `port_table[]` MUST == 该单元产物端口表
（**逐字逐序**）。首批 **10** 个产品单元逐个受检；负向对照（篡改一个端口名 / 产物多插
一个下沉端口）各投一次 ⇒ MUST 转红（`--self-test` 的投毒 6/7）。

#### 5.9.2 ISA 边界（RV32I **用户态**）

- **含**：`RV32I` 基础整数指令 + `FENCE` / `FENCE.I`（本版 = **NOP**：单核无 cache ⇒ 无排序语义）
  + **非法指令 4 态可观测**（`0` 合法 / `1` 已知未实现 / `2` 未知 opcode / `3` 保留组合）。
- 🔴 **不含特权级** —— 没有 CSR（`csrrw/csrrs/csrrc`）、没有 `ECALL` / `EBREAK` / `MRET`、
  没有 trap / 中断 / 特权态切换。特权级是**独立立项**（`rv32i-privileged-csr-2026-11`），
  本产品面 MUST NOT 顺手塞进来。
- 🔴 用户态下遇到非法指令时的 **冻结是终态**：PC 停在原地、该指令**永不退休**
  （`dbg_illegal_reason` 可观测）⇒ ⇒ 它不是「可恢复异常」，也**不会**被跳过 ⇒ ⇒
  用它做「跑飞检测」是**契约内**的用法。

#### 5.9.3 独立 TB 的判据汇总骨架

TB 的**机械部分**（端口连线 · clk/rst · VCD · 收尾骨架）由
`cpphdl-emit-rtl --emit-tb-stub` 生成；**激励与期望值 MUST 人工手推**
（禁抄仿真输出）。收尾骨架 = 每个 TB 的固定约定：

```systemverilog
$display("RESULT: checks=%0d fails=%0d", checks, fails);
if (fails != 0) $fatal(1, "tb_xxx failed");   // ⇒ 失败 MUST 非零退出码
```

⇒ ⇒ 护栏按 `RESULT:` 行判红绿（Verilator 的 `$finish` **恒** exit 0 ⇒ 不能靠退出码），
且 `checks` MUST ≥ 登记的 `min_checks`（否则「删断言」不会被发现）。

## 6. 反模式与静默失效清单

| # | 反模式 | 后果 | 证据 |
|---|---|---|---|
| 1 | 把 `ch_module<T>` 声明成**成员变量** | 成员构造早于 `build()` ⇒ 子模块挂到**祖父**节点下（父子关系错、同类型多实例重名）；顶层直接用会 SIGSEGV | `include/cpu/pipeline/rv32i_pipeline.h:114-123`；`examples/riscv-mini/tests/test_pipeline_debug.cpp:26-31`；`examples/removed_examples.txt:26` |
| 2 | `ch_module` 出现在 `TEST_CASE` / `main()` 顶层 | ctor `CHERROR` 后静默 return ⇒ `io()` 时 `CHFATAL` ⇒ SIGSEGV | `module.h:41-46, 78-84` |
| 3 | 两个 `ch_device` 分别实例化再互连 | 各自 context/时钟；`Simulator` 与 `toVerilog` 只看一个 | `device.h:20`、`simulator.cpp:463` |
| 4 | 用 `create_child` 当装配手段 | 子组件从不 `build()` ⇒ 端口/`describe()` 全不执行 | `component.h:42-59` |
| 5 | bundle 成员没 `as_master()/as_slave()` 就使用字段 | 字段节点不存在 ⇒ `Invalid operands`，结果恒 0 | `bundle_base.h:409-437` |
| 6 | 裸 `ch_uint`/`ch_bool` 成员当接口 | 子层读到 null；用字面量初始化则**静默恒 0** | §4.4 实测 |
| 7 | 子模块输入端口漏驱动 | 读到**未定义值**（不是 0）⇒ 跨后端可能分叉 | `AGENTS.md` ANTI-PATTERNS |
| 8 | 单个端口 > 64 bit | Verilator 后端 `initialize()` 返回 false | `AGENTS.md` VERILATOR BACKEND RULES |
| 9 | bundle 当顶层 IO | 不是端口 ⇒ 外部 TB / Verilator 驱动不到 | `AGENTS.md` CONVENTIONS |
| 10 | 用 `=` 代替 `<<=` 连 bundle 字段 | 节点重绑定 ⇒ 原驱动被孤儿化，连接静默失效 | `bundle_base.h:207-224` |

---

## 7. 速查

### 7.1 连线矩阵（源 → 目的）

| 目的 \ 源 | 本层 `ch_in` | 本层表达式 / `ch_out` / reg | 子模块 `ch_out` | 子模块 `ch_in` |
|---|---|---|---|---|
| **本层 `ch_out`** | ✅ 重载 1 | ✅ 重载 5 | ✅ 重载 3 | ❌ 连得上但语义错 |
| **子模块 `ch_in`** | ✅ 重载 2 | ✅ 重载 6 | ✅ 重载 4 | ❌ 违反 driver 约定 |

（"本层" = 正在执行 `describe()` 的组件；bundle 字段按「表达式」列处理。）

### 7.2 选型

```
要实例化组件
├─ 在 main() / TEST_CASE，且是根        → ch_device<T>
├─ 在某个 Component 内部                → describe() 里的 ch_module<T>（局部变量）
├─ 测试里要多组件互连                   → 写 wrapper Component，再 ch_device<Wrapper>
└─ 只想枚举/统计子组件                  → create_child<T>（不 build，不能装配）
```

---

## 8. 仓内范例索引

| 场景 | 位置 |
|---|---|
| 两层单子模块（最简） | `samples/counter.cpp:44-52` |
| 两层多实例 + `instance()` 取成员 | `samples/bundle_top_example.cpp:94-108` |
| 三层（SoC → pipeline → stages） | `examples/riscv-mini/src/rv32i_pipeline_top.h:94-96` |
| 5+ 子模块 + 互连（`ch::ch_layer<T>` 四处） | `examples/axi4/axi_soc_example_top.h:185-194` |
| 流水线级间寄存器/存储 | `include/cpu/pipeline/rv32i_pipeline.h:134, 189-196, 359-442` |
| bundle 成员 `connect()` | `tests/test_bundle_master_slave.cpp:110-115` |
| 测试承载 wrapper | `examples/riscv-mini/tests/test_pipeline_debug.cpp:47-62` |
| 双实例共享同一 DUT（双后端 parity） | `examples/riscv-mini/tests/test_rv32i_soc_dual_parity.cpp:679-712` |

---

## 9. 相关文档

- [Component Lifecycle Guide](../developer_guide/patterns/COMPONENT-LIFECYCLE-GUIDE.md) —— `ch_device` vs `ch_module` 的生命周期
- [Bundle 使用模式](06-bundle-patterns.md) —— bundle 定义、方向、序列化
- [层次化 RTL 写法指南](../developer_guide/patterns/HIERARCHICAL-RTL-AUTHORING-GUIDE.md) —— 今天就能用的 7 条纪律 + 什么时候该成层
- [层次化 RTL 输出 · 目标架构](../architecture/2026-11-02-hierarchical-rtl-output.md) —— 需求 / 结构事实 / 已知残留（**取代** `verilog-codegen-architecture.md` §7.1）
- `docs/adr/ADR-058-hierarchical-rtl-output.md` · `ADR-059`（归属与命名）· `ADR-060`（端口策略与复用）· `ADR-061`（层级粒度由用户显式控制）—— 决策正本
- [Verilator 后端](10-verilator-backend.md) —— 只认顶层端口、64 bit 上限
- `docs/adr/ADR-042-bundle-field-first-class-nodes.md` —— bundle 字段作为一等节点
- `docs/adr/ADR-008-single-clock-domain-constraint.md` —— 单时钟域约束
