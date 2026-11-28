# rv32i-i-l1-timing Specification

## Purpose
定义 **I-L1**（取指侧 L1 cache）的**填充协议**与**行内寻址**契约。

I-L1 是 RV32I 的**取指侧**缓存：它决定「一条取指请求在命中时几拍拿到指令、在未命中时
怎样从下级取回整条指令并填入」。这两个问题任一错位都是**静默的功能错误**：

- **行内寻址**若恒取 line 的首字 ⇒ 同一 line 内的第 2/3/4 条指令**全部取成第一条**
  ⇒ 核会跑飞（而**不是**报错）—— 这类缺陷**只有**在「同一 line 内多个不同地址」
  的判据下才可见；
- **填充协议**若与下级（AXI4-full 从设备）的语义不符 ⇒ 整行填不进去 ⇒ 未命中反复发生
  ⇒ 表现为「取指停死」，同样**零报错**。

因此本 capability 把 I-L1 的**填充协议**（4-beat burst）与**行内寻址**
（按 `addr[3:2]` 选 lane）写成可验证的 Requirement。

适用对象：

- `include/cpu/cache/i_cache_sync.h`（`ICacheSync` · 命中延迟 1 拍 `sread` · AXI4-full 读通道）

与既有 capability 的分工：

- `cpu-cache-subsystem` 管辖 cache **子系统**的通用契约（容量 / 相联度 / 变体选择）；
- `rv32i-dcache-timing` 管辖 **D-L1** 的握手与时序；本 capability 管辖 **I-L1** 的对应面
  （二者是**同构但独立**的两侧：D 侧有写路径，I 侧**只读** ⇒ 故 I 侧的 AXI 面只需
  **读通道**）；
- `rv32i-data-forwarding` / `rv32i-memory-timing` 管辖**流水线**侧的前推与访存时序，
  **不**管辖 cache 内部的数据选择 ⇒ 故「行内寻址」归本 capability。

### 本 capability 的由来

`rv32i-dcache-timing` 曾声明「取指侧的 I-L1 **各有独立 capability**」，但该 capability
在 specs 下**从未存在** ⇒ ⇒ 本 capability 补上这个**悬空声明**。

## Requirements

### Requirement: I-L1 MUST 按 `addr[3:2]` 选择行内 lane

I-L1 的读出 MUST 按请求地址的 **lane 位**（`addr[3:2]`，16 B line = 4 × 32-bit word）
从 line 中选择对应的 32-bit word：

- MUST NOT 恒取 line 的**低位**（那会让同一 line 的 4 个地址读到**同一个字**）。
- 选中的 lane MUST 与写入该 lane 的数据**一致** ⇒ ⇒ 填充与读出 MUST 用**同一套 lane 编号**。

#### Scenario: 同一 line 的四个地址各返回各自的字

- **WHEN** 一条 line 被填入 **4 个不同的字**，随后按 offset `0` / `4` / `8` / `12` 依次读
- **THEN** 每次读 MUST 返回**该 offset 对应**的那个字
- **AND** MUST NOT 四次都返回第一个字

### Requirement: I-L1 的命中延迟 MUST 保持 1 拍

I-L1 的**命中**路径 MUST 保持 **1 拍**（寄存器读）的延迟语义：

- 「请求被接受」与「数据可用」**不是**同一拍 ⇒ ⇒ 消费方 MUST 按 1 拍延迟取数。
- 命中延迟是 **cache 变体维度**（同步 1 拍 / 异步 0 拍）⇒ ⇒ 本 capability 只约束
  **同步变体** ⇒ ⇒ 异步变体 MUST NOT 被当作同步变体使用。

#### Scenario: 命中后一拍数据可用

- **WHEN** 请求命中
- **THEN** 数据 MUST 在**下一拍**可用（与下级存储的同步读延迟对齐）

### Requirement: I-L1 的「就绪」语义 MUST 与命中延迟一致

I-L1 的就绪输出 MUST 表达「**本请求已被接受**」而非「数据此刻可用」：

- 命中 ⇒ 就绪为高，数据在下一拍可用。
- 未命中 ⇒ 就绪为低，**填充完成后**方为高。
- ⇒ ⇒ 消费方 MUST NOT 把就绪当「数据此刻可用」⇒ ⇒ 该语义 MUST 与流水线侧的取指契约一致。

#### Scenario: 命中后就绪为高、数据在下一拍

- **WHEN** 请求命中
- **THEN** 就绪 MUST 在**本拍**为高
- **AND** 数据 MUST 在**下一拍**可用

#### Scenario: 未命中时就绪为低

- **WHEN** 请求未命中
- **THEN** 就绪 MUST 为低
- **AND** MUST 保持为低直到**填充完成**
