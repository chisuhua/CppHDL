## ADDED Requirements

### Requirement: AXI4-Lite 互联的端口面 MUST 是 19 根 Lite 通道

`AxiLiteCrossbar<Map, ADDR_WIDTH = 32, DATA_WIDTH = 32>` 的**每个** master / slave 面
MUST 是 AXI4-Lite 的 **19 根**通道：

- MUST 无 ID、无 burst（`awlen` / `arlen`）、无 `last`。
- 地址 MUST 是**绝对地址**（MUST NOT 重定基）。
- 端口面固定 **2 个 master × 6 个 slave**，且每个端口 MUST **具名**（标识符 == 名字串）。

#### Scenario: 端口面不含 Lite 之外的信号

- **WHEN** 例化 `AxiLiteCrossbar<Map>`
- **THEN** 每个 master 面与 slave 面 MUST 各为 19 根
- **AND** MUST NOT 出现 `awid` / `awlen` / `wlast` / `rlast`
- **AND** 端口命名棘轮 MUST 报 0 条新增未具名端口

### Requirement: 路由 MUST 在握手当拍锁存且写读独立

- AW / AR 握手当拍 MUST 锁存目标 slave。
- 写路由与读路由 MUST **各自独立**（MUST NOT 用写地址驱动读路由）。
- W / B 通道 MUST 使用**写路由锁存**；R 通道 MUST 使用**读路由锁存**。
- 每次多路选择 MUST **单次赋值**（MUST NOT 先赋 m0 再读回 `io().s0_*`）。

#### Scenario: 握手后改地址不影响在途事务

- **WHEN** master 0 的 AW 已在 slave 1 完成握手
- **AND** master 0 随后把 `awaddr` 改指向 slave 0
- **THEN** 该笔事务的 W / B MUST 仍转发到 slave 1
- **AND** slave 0 MUST NOT 看到该笔事务的任何通道信号

#### Scenario: 读响应跟随读路由而不是写授权

- **WHEN** master 1 的 AR 在 slave 0 完成握手（同一拍 master 0 正在写 slave 2）
- **THEN** 该笔 R MUST 回到 master 1、来自 slave 0
- **AND** MUST NOT 因写方向的授权 / 路由而改道

### Requirement: 仲裁 MUST 每方向独立轮转且未获授权的请求 MUST 保持

- 仲裁 MUST 是**每方向独立**的 round-robin（AW / AR 各一套指针）。
- 指针 MUST **只在握手后**推进（MUST NOT 每拍自转）。
- 未获授权的请求 MUST **保持**（level 信号，MUST NOT 被丢弃或吞掉）。
- 状态（在途标志 / 路由 / 响应）MUST **只在握手当拍**更新。

#### Scenario: 两 master 同时请求同一 slave

- **WHEN** master 0 与 master 1 同拍向同一 slave 发出 AW（或 AR）
- **THEN** 只有被授权者看到 `awready=1`（或 `arready=1`）
- **AND** 连续多次请求的授予顺序 MUST 轮转（MUST NOT 固定优先 m0）
- **AND** 未获授权者的 `valid` MUST 保持到其被授予

### Requirement: 同一 slave MUST 串行（写与读各自一套）

- 某个 slave 已有在途写时，同方向的**另一笔**请求 MUST 不被接受：
  master 侧 `ready` MUST 为 0，**且**该 slave MUST NOT 看到第二次 `valid`。
- 读方向同型，且读写各自独立（一读一写到同一 slave 可并存）。
- 该限制 MUST 只影响**同一 slave**；不同 slave 的并发事务 MUST NOT 被阻塞。

#### Scenario: 两 master 同时写同一 slave

- **WHEN** master 0 与 master 1 同拍向同一 slave 发出 AW
- **THEN** 第一笔被接受后，第二笔的 `awready` MUST 为 0
- **AND** 该 slave MUST NOT 看到第二笔的 `awvalid`（`valid ∧ ready` 就是握手）
- **AND** 第一笔的 B 完成后，第二笔 MUST 被接受

### Requirement: 未映射地址 MUST 由互连自应答 DECERR 且不挂死

- 未命中任何窗口（或命中 `kErr` 窗口）的访问 MUST 在互连层完成。
- 写侧 `bresp` 与读侧 `rresp` MUST 为 `DECERR (0b11)`；读侧 `rdata` MUST 为 0。
- master MUST NOT 因等待响应而永久挂死。

#### Scenario: 未映射写与读

- **WHEN** master 访问未映射地址（或 `kErr` 窗口）
- **THEN** AW / AR MUST 被接受（`ready=1`）
- **AND** 随后 MUST 收到 `bresp = 0b11`（写）或 `rresp = 0b11` + `rdata = 0`（读）
- **AND** MUST NOT 有任何真实 slave 看到该笔访问

### Requirement: SoC 的 Lite 窗口表 MUST 派生自库层地址表

SoC 侧的 `Map::windows[]` MUST **派生**自 `include/cpu/riscv/rv32i_mem_map.h`，
MUST NOT 手写基地址字面量。

- 派生关系 MUST 由**编译期断言**逐段钉住（`base` / `size` 相等）。
- 设备尚未接入的段 MUST 映射到 `kErr`（互连自应答 DECERR）⇒ MUST NOT 留死从口。

#### Scenario: 地址表改动 MUST 编译失败而不是静默漂移

- **WHEN** 有人只改了 `rv32i_mem_map.h` 的某段 `base` / `size`
- **AND** 没有同步 SoC 窗口表
- **THEN** 编译 MUST 因 `static_assert` 失败
- **AND** MUST NOT 出现「编译通过但地址路由错位」的静默漂移
