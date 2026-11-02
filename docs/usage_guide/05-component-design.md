# Component 设计指南（多层模块 / 跨层连线 / 成员接口 / RTL 输出）

**文档编号**: USAGE-05
**版本**: v1.0
**最后更新**: 2026-11-01
**目标读者**: 使用 CppHDL 设计硬件的工程师
**前置知识**: [Simulator API 使用指南](04-simulator-api.md)、[Bundle 使用模式](06-bundle-patterns.md)

---

## 本章目标

学完本章，你将能够：

1. 用 `ch_device<T>` / `ch::ch_module<T>` 搭出任意深度的组件树
2. 在层与层之间正确连线（4 种端口方向 + 表达式 + 寄存器/存储 + bundle）
3. 判断什么时候该用 `io()` 端口、什么时候该用成员变量做接口
4. 知道生成的 Verilog 是**扁平**的，以及要怎么应对
5. 避开常见反模式（§6 列了 10 条「编译通过、运行不报错、结果静默错」的坑）

> ⚠️ 本文所有行为都经过实测（2026-11-01，`g++ 14 / -std=c++23`，interpreter 后端）。
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

`__io(...)`（`include/core/io.h:312`）只做两件事：

1. 生成内嵌 `struct io_type { ... };`
2. 生成 `io()` 访问器，返回 `io_storage_` 上的对象

**placement new 要你自己写**：`create_ports()` 里必须 `new (io_storage_) io_type;`。
漏了它，`io()` 里全是未构造对象 ⇒ 读到的是垃圾，而且**不会有任何报错**。

> 组件没有端口时，`create_ports()` 写成空函数体即可（成员变量做接口的场景，见 §4）。

### 1.3 端口节点的名字

`__io` 里的字段是**默认构造**的 `ch_in/ch_out`，因此节点名恒为
`unnamed_input` / `unnamed_output`（`include/core/io.h:48-68, 141-158`）。
这也是为什么生成的 Verilog 端口名像 `top_unnamed_input_3` —— 见 §5。

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
- ④：`include/cpu/pipeline/rv32i_pipeline.h:238-262`、`examples/axi4/axi_soc_example.cpp:204-215`

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

### 5.1 当前：单一扁平 module

`ch::toVerilog(filename, ctx)`（`include/codegen_verilog.h:36`）**只接收 context**，
固定输出一个 `module top (`（`src/codegen_verilog.cpp:301`）到 `endmodule`
（`:409`）。整个 writer 里**没有任何** `children()` / `child_count()` 遍历，
也不发射实例化语句。

三层设计实测输出（Top → Mid → Leaf，节选）：

```verilog
module top (
    input default_clock, input default_reset,
    input  [7:0] top_unnamed_input,
    output [7:0] top_unnamed_output,
    output [7:0] top_m_unnamed_output,           // 子模块的 ch_out 也在端口列表里
    output [7:0] top_top_m_lf_unnamed_output     // 孙模块的 ch_out 也在
);
    logic [7:0] top_m_unnamed_input;             // 被驱动的子 ch_in → 内部 wire
    assign top_m_unnamed_input = top_unnamed_input;
    assign top_m_unnamed_output = top_top_m_lf_unnamed_output;
    assign top_unnamed_output  = top_m_unnamed_output;
endmodule
```

### 5.2 层级只体现在信号名里

- 节点名 = `组件路径 + "." + 局部名`（`src/core/lnodeimpl.cpp:25-31`），
  生成 Verilog 时 `.` 被替换成 `_`（`sanitize_name`，`codegen_verilog.cpp:59-88`）
- 端口节点默认名 `unnamed_input` / `unnamed_output`，同名再补 `_N`
- ⚠️ **子模块里以「成员变量」形式声明的节点（`ch_reg` / `ch_mem` / bundle 字段）
  会挂在父组件前缀下**（它们在子组件构造期创建，那时 `Component::current()`
  还是父组件）⇒ 出现 `top_m_r` 这种「名字像父层、实际属于子层」的信号
- ⚠️ **深度 ≥ 3 的组件名会重复前缀**：`build_hierarchical_name()`
  （`include/module.h:21`）用的是 `parent->hierarchical_name()`，而后者又会把
  `name_`（**已经是全路径**）再拼一次父路径（`include/component.h:61-73`）
  ⇒ 实测出现 `top_top_m_lf_*`

> 上面两条**已立项修复**（ADR-059：节点归属与组件命名）⇒ 它们是层次化输出的**硬前置**，
> 修完之前任何「按 `get_parent()` 分组」的工具都会把子模块状态划进父模块。

### 5.3 端口规则

> ⚠️ 本表描述的是**当前扁平产物**的规则。**层次化产物**的端口策略（隐式端口按方向合成 /
> 悬空输入冒泡 / 跨模块 `ch_mem` 拒绝）见 **ADR-060**；本节的「子输出全部进顶层端口表」
> 那条在层次化产物里**不再成立**（无人消费的子输出留在实例端口上，不上顶层）。

| 节点 | 在生成的 Verilog 里 |
|---|---|
| 顶层 `ch_in`（无驱动） | `input` 端口 |
| **任意层**被驱动的 `ch_in` | 内部 `logic` + `assign`（`codegen_verilog.cpp:333-360`） |
| **任意层**的 `ch_out` | `output` 端口（**只过滤 input，不过滤 output**，`:363-370`） |

推论：多层设计里「某个层的输入忘了驱动」在 Verilog 里表现为**顶层凭空多出一个
input 端口**；而子模块输出会全部暴露在顶层端口列表里（端口表随层次变长）。

### 5.4 需要层次化 Verilog 时：今天怎么做

框架**今天仍只输出扁平 RTL**（目标形态已立项，见 §5.5）。在那之前只有手工折中：

| 方案 | 做法 | 代价 |
|---|---|---|
| **A · 逐层单独生成 + 手写顶层例化** | 为每个组件写「只含它自己」的 wrapper，`ch_device` 生成它的扁平文件 ⇒ `sed` 改 `module top` 的名字 ⇒ 手写顶层 module 例化各子模块 | 子模块会带一堆多余 `output`（孙辈 `ch_out`）；端口名全是 `top_unnamed_input_N`，要人工对齐；`ch_device<Sub>` **不能**用于「把 `ch_module` 当成员变量」的组件 |
| **B · 自己写层次化 writer** | 遍历 `Component::children()` 分组发射 | ⚠️ **今天做不了**：节点归属对成员节点是**错的**、深度 ≥3 名字**重复前缀**（§5.2）⇒ 分组边界本身就错 |
| **C · 接受扁平输出，只加注释分节** | 用 `children()` 导出「实例名 → 信号前缀」清单，后处理扁平文件插 `// ---- top.m (Mid) ----` | 无层次语义；只解决可读性 |

> 综合工具本来就会展平层次，功能上无损；层次化的价值在**可读性**与后端布局控制粒度。

### 5.5 目标形态（**已立项，未实现**）

决策正本：**ADR-058**（边界与等价性）· **ADR-059**（归属与命名）·
**ADR-060**（端口策略与复用）· **ADR-061**（层级粒度由用户显式控制）。
实施载体：`openspec/changes/hierarchical-rtl-output-2026-11/`。

对**写代码的你**只有三条要记住：

1. **默认不成层**：组件默认不产生 module（逻辑留在父 module 里）⇒ 小组件不需要任何声明；
2. **成层要声明**：`CH_HIERARCHY_LAYER`（类级）或 `ch_layer<T>` / `ch_inline<T>`（实例级覆盖）；
3. **默认路径零变化**：谁都不声明时，产物与今天的扁平产物**逐字节一致** ⇒
   可以**先写代码、零风险**，将来要交付某个 IP 单元时再加一行声明。

⚠️ 该能力依赖**归属修正**（ADR-059）先行 —— 归属错会让 emitter 把子模块状态划进父模块，
产出「看起来对、实际错」的 RTL ⇒ 故 §5.2 的两个缺陷是**硬前置**。

写法指南（含「什么时候该成层」的判据）：
`docs/developer_guide/patterns/HIERARCHICAL-RTL-AUTHORING-GUIDE.md`。

> 架构主题文档：`docs/architecture/2026-11-02-hierarchical-rtl-output.md`
> （取代 `docs/developer_guide/architecture/verilog-codegen-architecture.md` §7.1）。

---

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
| 5+ 子模块 + 互连 | `examples/axi4/axi_soc_example.cpp:192-201` |
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
