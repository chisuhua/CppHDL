## ADDED Requirements

### Requirement: AXI4-Lite N×M 互联的端口面 MUST 是 19 根 Lite 通道

`AxiLiteCrossbar<N, M, Map>` 的每个 master / slave 面 MUST 是 AXI4-Lite 的 **19 根**：

- MUST 无 ID、无 burst（`awlen` / `arlen`）、无 `last`。
- 地址 MUST 是**绝对地址**（MUST NOT 重定基）。
- 所有方向 MUST 符合既有端口约定（`ch_in` = 父模块驱动）。

#### Scenario: 端口面不含 Lite 之外的信号

- **WHEN** 例化 `AxiLiteCrossbar<2, 3, Map>`
- **THEN** 每个 master 面与 slave 面 MUST 各为 19 根
- **AND** MUST NOT 出现 `awid` / `awlen` / `wlast` / `rlast`

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

### Requirement: 仲裁 MUST 轮转且授权保持到握手

- 仲裁 MUST 是**每方向独立**的 round-robin（AW / AR 各一套）。
- 授权 MUST **保持到握手**（中途 MUST NOT 改选）。
- 在途期间该 master 的同向 `ready` MUST 为 0，且 MUST NOT 静默吞掉未被接受的请求。

#### Scenario: 两 master 同时请求同一 slave

- **WHEN** master 0 与 master 1 同拍向同一 slave 发出 AW
- **THEN** 只有被授权者看到 `awready=1`
- **AND** 连续 4 轮请求的授予顺序 MUST 轮转（MUST NOT 固定优先 m0）
- **AND** 未获授权者的 `awvalid` MUST 保持到其被授权

### Requirement: 未映射地址 MUST 由互连自应答 DECERR 且不挂死

- 未命中任何窗口（或命中 `ERR` 窗口）的访问 MUST 在互连层完成。
- 写侧 `bresp` 与读侧 `rresp` MUST 为 `DECERR (0b11)`；读侧 `rdata` MUST 为 0。
- master MUST NOT 因等待响应而永久挂死。

#### Scenario: 未映射写与读

- **WHEN** master 访问未映射地址
- **THEN** AW / AR MUST 被接受（`ready=1`）
- **AND** 随后 MUST 收到 `bresp = 0b11`（写）或 `rresp = 0b11` + `rdata = 0`（读）

### Requirement: SoC 的 Lite 窗口表 MUST 派生自库层地址表

SoC 侧的 `Map::windows[]` MUST **派生**自 `include/cpu/riscv/rv32i_mem_map.h`，
MUST NOT 手写基地址字面量。

- 派生关系 MUST 由**编译期断言**逐段钉住（`base` / `size` 相等）。
- 设备尚未接入的段 MUST 映射到 `ERR`（互连自应答 DECERR）⇒ MUST NOT 留死从口。

#### Scenario: 地址表改动 MUST 编译失败而不是静默漂移

- **WHEN** 有人只改了 `rv32i_mem_map.h` 的某段 `base` / `size`
- **AND** 没有同步 SoC 窗口表
- **THEN** 编译 MUST 因 `static_assert` 失败
- **AND** MUST NOT 出现「编译通过但地址路由错位」的静默漂移
