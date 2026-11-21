# Testbench 编写指南

**文档编号**: USAGE-09  
**版本**: v2.0  
**最后更新**: 2026-04-09  
**目标读者**: CppHDL 用户（硬件设计者）  
**前置知识**: [Simulator API 使用指南](01-simulator-api.md)

---

## 🎯 本章目标

学完本章，你将能够：

1. ✅ 使用 CppHDL 测试框架
2. ✅ 编写 Component testbench
3. ✅ 使用 Bundle 进行测试
4. ✅ 运行和验证测试
5. ✅ 避免常见测试错误

---

## 📖 测试框架概述

CppHDL 使用 **Catch2 v3.7.0** 测试框架，集成到 CMake/CTest 构建系统。

### 测试组织

| 类别 | 说明 | 标签 |
|------|------|------|
| 基础测试 | CppHDL 核心功能测试 | `[base]` |
| chlib 测试 | chlib 库组件测试 | `[chlib]` |
| 示例测试 | 使用示例验证功能 | 按需 |

---

## 🔧 Testbench 基本结构

### 标准模板

```cpp
// tests/test_my_component.cpp
#include "catch_amalgamated.hpp"
#include "ch.hpp"
#include "simulator.h"
#include "my_component.h"  // 被测模块

using namespace ch::core;

TEST_CASE("MyComponent - Functionality Test", "[my_component][testbench]") {
    // 1. 创建 ch_device（自动调用 build()）
    ch::ch_device<MyComponent> dev;
    
    // 2. 创建 Simulator
    Simulator sim(dev.context());
    
    // 3. 获取 IO 引用
    auto& io = dev.instance().io();
    
    // 4. 设置输入值
    sim.set_input_value(io.data_in, 0x42);
    sim.set_input_value(io.valid, 1);
    
    // 5. 运行仿真
    sim.tick();
    
    // 6. 验证输出
    auto output = sim.get_value(io.data_out);   // ⚠️ 读 ch_out 用 get_value
    REQUIRE(static_cast<uint64_t>(output) == 0x42);
}
```

> ⚠️ **读输出端口用 `get_value`（不是 `get_input_value`）**：`get_input_value`
> 只有 `ch_in<T>` 与 `ch_uint<N>` 两个重载（`include/simulator.h:464` / `:469`）
> ⇒ ⇒ 给 `ch_out` 传它 ⇒ **编译不过**。见 `01-simulator-api.md` §2.1。

---

### 组成部分说明

**1. 头文件包含**:
```cpp
#include "catch_amalgamated.hpp"  // Catch2 框架
#include "ch.hpp"                 // CppHDL 核心
#include "simulator.h"            // Simulator
#include "my_component.h"         // 被测模块
```

**2. TEST_CASE 宏**:
```cpp
TEST_CASE("测试名称", "[标签 1][标签 2]")
```

**3. ch_device**:
```cpp
ch::ch_device<MyComponent> dev;  // 自动调用 build()
```

**4. Simulator**:
```cpp
Simulator sim(dev.context());    // 初始化仿真
```

**5. 断言**:
```cpp
REQUIRE(condition);              // 验证条件
CHECK(condition);                // 非致命验证
```

---

## 🎭 测试模式

### 模式 1: 独立信号测试

```cpp
TEST_CASE("Basic Signal Test", "[signal][basic]") {
    auto ctx = std::make_unique<ch::core::context>("test");
    ch::core::ctx_swap ctx_guard(ctx.get());
    
    ch_uint<8> signal(0_d);
    Simulator sim(ctx.get());
    
    // 设置和验证
    sim.set_input_value(signal, 0xA5);
    REQUIRE(sim.get_input_value(signal) == 0xA5);
}
```

**适用场景**:
- 基础 API 验证
- 独立信号操作
- 快速原型测试

---

### 模式 2: Component 功能测试

```cpp
TEST_CASE("Counter - Increment Function", "[counter][increment]") {
    ch::ch_device<Counter<8>> counter_dev;
    Simulator sim(counter_dev.context());
    
    auto& io = counter_dev.instance().io();
    
    // 设置初始值
    sim.set_input_value(io.reset, 1);
    sim.tick();
    
    sim.set_input_value(io.reset, 0);
    sim.set_input_value(io.enable, 1);
    
    // 运行多个周期
    for (int i = 0; i < 5; i++) {
        sim.tick();
    }
    
    // 验证计数值
    auto count = sim.get_input_value(io.count);
    REQUIRE(static_cast<uint64_t>(count) == 5);
}
```

**适用场景**:
- 模块功能验证
- 时序行为测试
- 状态机测试

---

### 模式 3: Bundle 测试

```cpp
TEST_CASE("Stream - Data Transfer", "[stream][bundle]") {
    ch::ch_device<StreamProducer<ch_uint<32>>> producer_dev;
    ch::ch_device<StreamConsumer<ch_uint<32>>> consumer_dev;
    
    Simulator sim_prod(producer_dev.context());
    Simulator sim_cons(consumer_dev.context());
    
    auto& prod_io = producer_dev.instance().io();
    auto& cons_io = consumer_dev.instance().io();
    
    // 设置主从方向
    prod_io.stream_out.as_master();
    cons_io.stream_in.as_slave();
    
    // 连接 Bundle（需要在同一个 context 中）
    // 注：实际测试中通常将 producer 和 consumer 放在顶层模块中
    
    // 设置测试数据
    sim_prod.set_input_value(prod_io.stream_out.payload, 0x12345678);
    sim_prod.set_input_value(prod_io.stream_out.valid, 1);
    
    sim_prod.tick();
    
    // 验证消费者接收
    auto payload = sim_cons.get_input_value(cons_io.stream_in.payload);
    REQUIRE(static_cast<uint64_t>(payload) == 0x12345678);
}
```

**适用场景**:
- Stream/Flow 测试
- AXI 总线测试
- 接口协议验证

---

### 模式 4: AXI 总线测试

```cpp
TEST_CASE("AXI Lite - Register Write/Read", "[axi][register]") {
    ch::ch_device<AxiLiteSlave> slave_dev;
    Simulator sim(slave_dev.context());
    
    auto& io = slave_dev.instance().io();
    
    // AXI 写事务
    sim.set_input_value(io.awaddr, 0x10);
    sim.set_input_value(io.awprot, 0);
    sim.set_input_value(io.awvalid, 1);
    
    // ⚠️ **等握手 MUST 有界**（2026-11-19 订正）：仓内一律写成
    //    `for (i < max_cycles)` + 超时判失败（见 `tests/axi_lite_slave_fixture.h:39-45`）
    //    ⇒ ⇒ 无界 `while` 在 DUT 不握手时会**永久挂死** ⇒ CI 卡住 ⇒ 查不出是谁的问题
    // ⚠️ 读 `ch_out`（awready 是从端的**输出**）⇒ 用 `get_value`，不是 `get_input_value`
    bool aw_done = false;
    for (int i = 0; i < 16; ++i) {
        if (static_cast<uint64_t>(sim.get_value(io.awready)) == 1) { aw_done = true; break; }
        sim.tick();
    }
    REQUIRE(aw_done);                 // 握手超时 ⇒ 直接判红（不挂死）
    
    sim.tick();
    sim.set_input_value(io.awvalid, 0);
    
    // 写数据
    sim.set_input_value(io.wdata, 0xDEADBEEF);
    sim.set_input_value(io.wstrb, 0xF);
    sim.set_input_value(io.wvalid, 1);
    
    bool w_done = false;
    for (int i = 0; i < 16; ++i) {
        if (static_cast<uint64_t>(sim.get_value(io.wready)) == 1) { w_done = true; break; }
        sim.tick();
    }
    REQUIRE(w_done);
    
    sim.tick();
    sim.set_input_value(io.wvalid, 0);
    
    // 验证写入（通过内部状态或后续读操作）
    // ...
}
```

**适用场景**:
- AXI 协议验证
- 寄存器访问测试
- 总线互连测试

---

### 模式 5: 验证源（`.verif`）驱动 —— **产品级 RTL 仿真的推荐路径**

> 📌 **2026-11-19 新增**。前 4 个模式都是「C++ 里手写激励」⇒ 适合**单元级**验证。
> 当你需要一个**产品级 RTL 仿真**（同一份语义要同时喂给「解释器跑」与
> 「Verilog 独立 TB 跑」），用手写激励就必然出现**同一语义的多个物理载体**
> ⇒ ⇒ `.verif` 机制就是为消掉它而生的（ADR-074）。

#### 为什么需要它

```
.verif 验证源 ──扫描器──> IR（唯一语义载体）─┬─> IR 解释器 ──> Simulator
                                             └─> Verilog 渲染器 ──> .sv 独立 TB
```

⚠️ **值在生成期全部求值成字面量**（`REPEAT` 在扫描期展开、`_i` 在扫描期绑定）
⇒ ⇒ IR 里**没有表达式** ⇒ ⇒ 两个执行后端**不必各自解释表达式**
⇒ ⇒ 「语义真的只有一份」⇒ ⇒ **两侧断言不可能漂移**。

#### `.verif` 的语法（原语**封闭集**）

| 头部指令 | 含义 |
|---|---|
| `@expect-src <path>#<anchor>` | **期望值的推导**在哪个登记文档（**必须是人写的推导**，不是抄生成器输出） |
| `@tb-module <name>` | 生成的 TB 的 module 名 |
| `@scenario <name>` | 一个场景（= 一组断言） |
| `@anchor <name>` | 该场景在期望源文档里的锚点 ⇒ 护栏按 **scenario 级 1:1** 核对两边 |

| 原语 | 含义 |
|---|---|
| `SET <port> <value>` | 驱动一个端口（**不消耗拍**） |
| `TICK <n>` | 走 n 个时钟沿 |
| `SETTLE` | 等待采样窗结算（**`SET` 之后 `EXPECT` 之前 MUST 有它**） |
| `WAIT <port> [<high\|low>] <max>` | 等某端口达到目标电平（`high` ≡ **非零** ≡ **缺省**；`low` ≡ 零）⇒ 上限 max 拍 ⇒ 产生一条断言 |
| `EXPECT <port> <value>` | 断言某端口等于某值 ⇒ 产生一条断言 |
| `EXPECT_STABLE <port> <n>` | 断言某端口连续 n 拍不变 |
| `OBSERVE <port>` | 观测（只记录，不判定） |
| `RESET <port> <high\|low> [cycles]` | 把某个端口拉成该电平、保持 n 拍、再拉回**相反**电平（⚠️ **不是**框架复位 ⇒ 见下方边界） |
| `RECORD <port>` | 把端口**当前采样点**的值追加到该端口的**历史序列**（**不**产生断言） |
| `EXPECT_SEQ_SELF <port> <n> <lag>` | 断言该端口历史的**最近 n 项**与**再往前 lag 项**逐项恒等 ⇒ 产生一条断言 |
| `EXPECT_SEQ_STEP <portV> <portE> <n>` | 断言「`portV` 的相邻差」与「`portE` 的同位项」逐项恒等 ⇒ 产生一条断言 |
| `EXPECT_SEQ_COUNT <port> <value> <n> <count>` | 断言该端口历史**最近 n 项**中等于 `<value>` 的项数 == `<count>` ⇒ 产生一条断言 |

**历史/序列原语的已知边界**（ADR-076 · 钉死语义，避免两端各理解一次）：

- **历史按「端口标识符」分桶**，**跨 scenario 累积不清空**（与 DUT 状态持续性同构）⇒
  ⇒ **一个物理端口 MUST 只用一个标识符 `RECORD`**（两个标识符 ⇒ 两条**互不相干**的历史）。
- **深度上限 1024**（每端口累计 `RECORD` 次数）⇒ **超限在扫描期就报错** ⇒ 运行期不可能越界。
- **历史不足在扫描期就报错**（消息点名端口与所需/已有）⇒ ⇒ 「运行期历史不够」这种
  **两端分叉**被消灭在扫描期。
- ⚠️ **`EXPECT_SEQ_STEP` 的对齐**：V 侧取**最后 n+1 项**的 n 个相邻差
  （下标 `LV-n+i` 与 `LV-n-1+i`）· E 侧取**同窗口的同位项**（下标 `LE-n-1+i`）⇒
  ⇒ 「造成 `V[t] → V[t+1]` 这次跃迁的 E 值」正是**采样在 t 时刻**的那个值 ⇒
  ⇒ ⚠️ **不是** `hV[LV-n+i+1]`（那会越界读 `hV[LV]`）· **不是** `hE[LE-n+i]`（那是**晚一拍**的 E）。
- ⚠️ **使用前提（规范义务）**：两端口 MUST 在**同一采样点集合**上 `RECORD`
  （写法范式 = 每拍 `TICK 1` → `RECORD v` → `RECORD en`，且两者次数相等）
  ⇒ ⇒ `LV ≠ LE` 时断言仍可执行，但比较的是**两条不同尾部的窗口** ⇒ ⇒ 不是「同拍关系」。
- **`SET` / `RESET` 之后紧邻 `RECORD`** ⇒ 扫描器报错（与 `EXPECT` 同纪律：它读的是**当前**值）。
  ⇒ ⇒ 但 **`EXPECT_SEQ_*` 不受**该纪律约束（它们只读**历史** ⇒ 与当前结算状态无关）。
- **>64 位端口**：被 `RECORD` / 被序列断言引用的端口 ⇒ **fail-closed**（两端同源拒绝）
  ⇒ ⇒ 不做「截成低 64 位」的静默降级。
- **X 态两端不对称**（与 `EXPECT` 同型）：渲染侧用 `!==` / `===` ⇒ X 判不等 ⇒ FAIL；
  `EXPECT_SEQ_COUNT` 的 X 项**不计数** ⇒ 计数偏少 ⇒ FAIL。解释器侧无 X。
- ~~场景首条 `RECORD` 继承渲染式 TB 复位尾缝（比解释器多一个 `@(posedge clk)`）~~
  ⇒ ⇒ **2026-11-21 已对齐**：解释器的框架复位末尾补了同一拍（`apply_frame_reset`）
  ⇒ ⇒ 两侧相位一致 ⇒ 夹具**仍建议**用 `SET` + `TICK` 前缀（显式起点更好读，
  但不依赖它是为了对齐）。

**`EXPECT` / `EXPECT_STABLE` 的已知边界**（2026-11-21 迁移实测）：

- ⚠️ **`EXPECT <port> <value>` 的字面量 MUST 落在该端口位宽内**：渲染侧按位宽截断
  （`8'd0`），解释器按 **64 位原值**比较 ⇒ ⇒ `EXPECT value 256`（8 位端口）会让两侧
  **结论相反** ⇒ ⇒ 需要「回绕点」时写成循环到 `255` + 单列一条 `EXPECT value 0`。
- ⚠️ **`EXPECT_STABLE <port> <n>` 的 n 条结论共用同一个 `check_id`**（原语级 id，
  不是逐拍 id）⇒ ⇒ 与 `EXPECT` 的逐条命名不同（历史行为，未改）。

**`RESET` 的已知边界**（2026-11-21 实测 · 钉死语义，避免两端各理解一次）：

- ⚠️ **`RESET` = 「拉电平 + 保持 n 拍 + 拉回相反电平」**，**不是**「让 DUT 复位」
  ⇒ ⇒ 对**普通数据输入**（如 `rst_n`）它是**正确的用法**（电平型信号）。
- ⛔ **`RESET` 指向**框架复位端口**（`default_reset` · manifest `role == "reset"`）⇒ ⇒
  **fail-closed 拒绝**（两端同源：解释器与渲染器**逐字相同**的错误消息）。
  **为什么**（实测，gpio 真实 DUT + 真 verilator）：
  · **解释器**：`build_register` 给寄存器的 `rst_val` 是 **nullptr** ⇒ 复位期间
    `instr_reg::eval` **只 return、不赋值** ⇒ 寄存器**保持原值**；
  · **渲染 TB**：释放动作与**最后一个**时钟沿**同拍** ⇒ 该沿走 `else` 分支
    ⇒ 刚写下的复位值被**同拍覆盖**。
  ⇒ ⇒ **两端都不复位** ⇒ 「看起来像复位、实际不复位」⇒ ⇒ 与「两端结论相反」不同，
  但同样危险（作者会以为复位发生了 ⇒ 可能把期望值改成「不复位的值」⇒ **假绿**）。
- ⇒ ⇒ **运行期中框架复位当前不可表达** ⇒ ⇒ 需要框架侧新增能力（如 `FRAME_RESET`）
  ⇒ ⇒ 决策归 architect（见 `.rddf/checks/examples-rtl-expectations.md#phase3a-example`
  的「⛔ 为什么该 TB MUST 保持手写」节）。
- ⚠️ **另一条仍开放的已知差异**（**对所有** `RESET` 用法，含数据端口）：渲染侧的
  **去断言**与**最后一个**时钟沿**同拍**（`repeat(n) @(posedge clk); name = <deassert>;`
  **无 `#1`**）⇒ ⇒ 该沿可能已采样到**去断言后的值** ⇒ 与解释器（先 `tick` 完再 `set`）差 **1 拍**。
  登记 = `openspec/changes/archive/2026-11-20-verif-wait-level-2026-11/tasks.md` 的 **B0 ②**
  （**待 architect 裁决** ⇒ ⇒ 修任一侧都会影响另一侧的同结论 ⇒ ⇒ 不是本地可定的小改）。
  当前 `.verif` 零处使用 `RESET` ⇒ ⇒ **潜在**差异（未触发）。

**`WAIT` 的已知边界**（钉死语义，避免两端各理解一次）：

- **`high` ≡ 非零**（**不是**「N 位全 1」，二者仅 N = 1 时等价）⇒ N>1 端口上 `WAIT p high <max>`
  **不是**「等全 1」。判定是**零值测试** ⇒ 不需要端口位宽。
- **X 态两端不对称**（既有，与 `EXPECT` 同型）：渲染侧 `high` 用 `!== 0`（X 判真 ⇒ 当高）·
  `low` 用 `== 0`（X ⇒ `if` 判假 ⇒ 当未命中 ⇒ fail-closed）。
- **>64 位端口**：解释器的零值测试走 64 位视图 ⇒ 只覆盖**低 64 位**；渲染侧的 `EXPECT` 是全宽
  比较 ⇒ 该不对称早已存在（非本原语引入）⇒ 宽端口上的 `WAIT` 结论以低 64 位为准。
- **`max` = 0** ⇒ 不推进任何拍 ⇒ 恒 FAIL（不产生「零拍命中」）。


#### 一个最小完整例子

```
# examples/axi4/tb_axi4_lite_example.verif（节选）
@expect-src .rddf/checks/examples-rtl-expectations.md#axi4-lite-example
@tb-module tb_axi4_lite_example

@scenario bringup_and_low_active_reset
@anchor axi4-lite-example

SET rst_n 0
TICK 2
SET rst_n 1
TICK 2

SET awaddr 0x00000000
SET awvalid 1
SETTLE              # ⚠️ SET 之后 EXPECT 之前 MUST 先 SETTLE
EXPECT awready 1    # ⇒ 一条断言（check_id = bringup_and_low_active_reset#0）
TICK 1
```

#### 怎么跑（**两个模式，同一份 `.verif`**）

```bash
# ① 解释器模式（默认）：IR 解释器驱动 Simulator ⇒ 打印结论行
build/examples/axi4/tb_axi4_lite_example
# ⇒ RESULT: checks=113 fails=0

# ② 发射模式：发射 Verilog 产物 + 由同一份 .verif 渲染独立 TB
build/examples/axi4/tb_axi4_lite_example --emit-rtl --out <dir> \
    --emit-tb-verif examples/axi4/tb_axi4_lite_example.verif --tb-verif-out <dir>/tb.sv
# ⇒ 打印 VERIF-CHECKS: 113（静态展开数）
```

⚠️ **改完 `.verif` MUST 重新生成 TB**：仓内 TB 由 `.verif` 渲染而来（**禁手改**）⇒
护栏 `scripts/verify_examples_rtl_sim.sh` 的 ⑥′ 判据会**逐字节**比对 ⇒ 不重新生成
⇒ 护栏红（`生成物与验证源不一致`）。

#### 四条纪律

| # | 纪律 | 为什么 |
|---|---|---|
| 1 | **`SET` / `RESET` 之后必须先 `SETTLE`**（或 `TICK` / `WAIT`）才能 `EXPECT` / `RECORD` | 「沿后采样窗」内驱动、「最近一次结算之后」观测 ⇒ 不结算就观测 ⇒ **观测点未定义** ⇒ 扫描器**静态强制**（违例直接报错） |
| 2 | **期望值的推导 MUST 写在 `@expect-src` 指向的文档里**（人写） | 否则就是「抄生成器输出」⇒ ⇒ 两侧同时变 ⇒ **恒绿** ⇒ 护栏要求三方等式（登记 == 静态展开 == 运行期） |
| 3 | **fail-closed**：端口名拼错 / 多义 / 集合外构造 ⇒ 一律报错 + 点名 | `gpio_out` 写成 `gpio_outt` ⇒ 静默读 0 ⇒ 断言永远「通过」⇒ 必须显式拒绝 |
| 4 | **两侧结论逐条可比** | 解释器与渲染式 TB 都输出 `CHECK <id> PASS\|FAIL` ⇒ ⇒ 可直接比对集合（护栏 ⑧⁗ / 单元测试） |
| 5 | ⚠️ **两端一致 ≠ 正确**（同源缺陷会一致地错） | ⇒ ⇒ 每条判据 MUST 另有**手推期望**（禁抄仿真输出）⇒ ⇒ 判别力由**手推值**承担，不由「两端一致」承担（ADR-076 实施期实测到一处**两端一致地错**的差一错位） |

> 📚 **权威载体**：capability `examples-rtl-sim-coverage`（`openspec/specs/`）
> · 设计 = `docs/adr/ADR-074-rtl-verification-single-source.md`
> · 护栏 = `scripts/verify_examples_rtl_sim.sh`（+ `--self-test`）

---

## ⚡ 测试技巧

### 技巧 1: 使用 SECTION 组织测试

```cpp
TEST_CASE("FIFO - Multiple Operations", "[fifo]") {
    ch::ch_device<FiFo<ch_uint<8>, 16>> fifo_dev;
    Simulator sim(fifo_dev.context());
    auto& io = fifo_dev.instance().io();
    
    SECTION("Empty FIFO") {
        REQUIRE(sim.get_input_value(io.empty) == 1);
        REQUIRE(sim.get_input_value(io.full) == 0);
    }
    
    SECTION("Write Operation") {
        sim.set_input_value(io.push, 1);
        sim.set_input_value(io.din, 0x42);
        sim.tick();
        
        REQUIRE(sim.get_input_value(io.empty) == 0);
    }
    
    SECTION("Read Operation") {
        // 先写入
        sim.set_input_value(io.push, 1);
        sim.set_input_value(io.din, 0x55);
        sim.tick();
        
        // 再读出
        sim.set_input_value(io.pop, 1);
        sim.tick();
        
        auto dout = sim.get_input_value(io.dout);
        REQUIRE(static_cast<uint64_t>(dout) == 0x55);
    }
}
```

**优点**:
- 逻辑清晰的测试分组
- 共享 setup 代码
- 独立验证各个功能点

---

### 技巧 2: 参数化测试

```cpp
TEST_CASE("ALU - Arithmetic Operations", "[alu]") {
    ch::ch_device<ALU<32>> alu_dev;
    Simulator sim(alu_dev.context());
    auto& io = alu_dev.instance().io();
    
    struct TestCase {
        uint32_t a, b;
        int op;
        uint32_t expected_result;
    };
    
    std::vector<TestCase> test_cases = {
        {10, 20, 0, 30},   // ADD
        {100, 50, 1, 50},  // SUB
        {7, 8, 2, 56},     // MUL
        {100, 3, 3, 33},   // DIV
    };
    
    for (const auto& tc : test_cases) {
        SECTION(std::to_string(tc.op)) {
            sim.set_input_value(io.a, tc.a);
            sim.set_input_value(io.b, tc.b);
            sim.set_input_value(io.op, tc.op);
            
            sim.tick();
            
            auto result = sim.get_input_value(io.result);
            REQUIRE(static_cast<uint32_t>(result) == tc.expected_result);
        }
    }
}
```

**优点**:
- 减少代码重复
- 易于添加新测试用例
- 清晰的测试覆盖

---

### 技巧 3: 使用 tick() 控制时序

```cpp
TEST_CASE("State Machine - Sequence Test", "[fsm]") {
    ch::ch_device<TestFSM> fsm_dev;
    Simulator sim(fsm_dev.context());
    auto& io = fsm_dev.instance().io();
    
    // 初始状态
    sim.set_input_value(io.start, 0);
    sim.set_input_value(io.reset, 1);
    sim.tick();  // 复位
    
    // 开始状态机
    sim.set_input_value(io.reset, 0);
    sim.set_input_value(io.start, 1);
    sim.tick();  // 启动
    
    // 等待完成
    int max_ticks = 100;
    for (int i = 0; i < max_ticks; i++) {
        sim.tick();
        auto done = sim.get_input_value(io.done);
        if (static_cast<uint64_t>(done) == 1) {
            break;
        }
    }
    
    // 验证完成状态
    REQUIRE(sim.get_input_value(io.done) == 1);
}
```

---

## ⚠️ 注意事项

### 1. 初始化问题

```cpp
// ✅ 推荐：使用 ch_device（自动 build）
ch::ch_device<MyComponent> dev;
Simulator sim(dev.context());

// ⚠️ 谨慎：手动创建 Component
MyComponent comp(nullptr, "comp");
comp.build();  // 必须显式调用
Simulator sim2(comp.context());
```

---

### 2. Context 管理

```cpp
// ✅ 推荐：ch_device 自动管理 context
ch::ch_device<MyComponent> dev;  // 自动创建 top_ctx

// ✅ 推荐：显式 context（需要 ctx_swap）
auto ctx = std::make_unique<ch::core::context>("test");
ch::core::ctx_swap ctx_guard(ctx.get());
```

---

### 3. 断言选择

| 断言 | 用途 | 失败行为 |
|------|------|---------|
| `REQUIRE(condition)` | 关键验证 | 立即终止测试 |
| `CHECK(condition)` | 次要验证 | 继续执行测试 |
| `STATIC_REQUIRE(condition)` | 编译时验证 | 编译失败 |

**最佳实践**:
```cpp
REQUIRE(output == expected);  // 功能正确性
CHECK(warning_count == 0);    // 代码质量
```

---

## 🔍 故障排除

### 问题 1: 测试崩溃（Segmentation Fault）

**症状**: 测试运行时崩溃

**常见原因**:
- 未调用 `comp.build()`
- Context 未正确切换
- Simulator 未初始化

**解决**:
```cpp
// ✅ 使用 ch_device
ch::ch_device<MyComponent> dev;  // 自动 build()
Simulator sim(dev.context());    // 正确初始化
```

---

### 问题 2: 断言失败，值始终为 0

**症状**: `REQUIRE(value != 0)` 失败

**常见原因**:
- 未调用 `sim.tick()`
- 输入值未正确设置
- 信号未连接

**解决**:
```cpp
sim.set_input_value(signal, value);
sim.tick();  // ✅ 确保仿真运行
auto val = sim.get_input_value(signal);
REQUIRE(val != 0);
```

---

### 问题 3: Bundle 字段访问失败

**症状**: `sim.get_input_value(bundle.field)` 失败

**解决**: 参考 [Simulator API 使用指南](01-simulator-api.md) 的注意事项章节

---

## 📊 测试运行

### 运行所有测试

```bash
cd build
ctest  # 运行所有测试
```

### 按标签运行

```bash
# 只运行基础测试
ctest -L base

# 只运行 chlib 测试
ctest -L chlib

# 运行特定标签
ctest -L fifo
ctest -L axi
```

### 运行单个测试

```bash
# 运行特定测试文件
./tests/test_fifo

# 运行特定 SECTION
./tests/test_fifo "[fifo][write]"
```

### 详细输出

```bash
# 输出失败的测试详情
ctest --output-on-failure

# 详细模式
ctest -V

# 只运行失败的测试
ctest --rerun-failed
```

---

## 📖 参考资源

### 相关文档

| 文档 | 说明 |
|------|------|
| [01-simulator-api.md](01-simulator-api.md) | Simulator API 使用 |
| [03-bundle-patterns.md](03-bundle-patterns.md) | Bundle 使用模式 |

### 示例代码

| 示例 | 位置 |
|------|------|
| FIFO 测试 | `tests/test_fifo.cpp` |
| Stream 测试 | `tests/test_stream*.cpp` |
| 状态机测试 | `tests/test_state_machine.cpp` |
| AXI 测试 | `tests/test_axi*.cpp` |

### 外部资源

- [Catch2 文档](https://github.com/catchorg/Catch2/blob/devel/docs/tutorial.md)
- [CTest 文档](https://cmake.org/cmake/help/latest/manual/ctest.1.html)

---

**维护**: AI Agent  
**版本**: v2.0  
**下次审查**: 2026-05-09
