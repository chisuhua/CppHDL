## ADDED Requirements

### Requirement: sread 的禁读拍输出语义 MUST 在两条后端一致

`ch_mem::sread(addr, enable)` 在 `enable == 0`（**禁读拍**）时的输出 MUST 在
Simulator 与生成的 Verilog 上**逐位相同**。

任一侧被选定为权威语义后，另一侧 MUST 与之对齐，且**选定的语义 MUST 写进本
capability**（「两侧一致但没人知道是哪一侧」等于没有契约）。

⚠️ `ch_mem::aread` 不在本条管辖内：它不提供 `enable`（`create_mem_read_port`
收到的是 `nullptr`）⇒ 恒为「读」，不存在禁读拍。

#### Scenario: 禁读拍的输出在两条后端相等

- **WHEN** 一个设计使用带 `enable` 的 `ch_mem::sread`，并在某一拍把 `enable` 拉低
- **THEN** 该拍之后读到的输出值，在 Simulator 与生成的 Verilog 上 MUST 逐位相同
- **AND** 该判据 MUST 是可执行的（双后端 parity 用例 · 期望值手推），
  MUST NOT 只靠代码审阅或注释
- **AND** 已知的当前分歧（Simulator 保持上一拍值 / Verilog 清零）MUST 被消除

#### Scenario: 生产组件 `chlib::memory` 的 RAM 族被覆盖

- **WHEN** 校验本条的判据覆盖面
- **THEN** `include/chlib/memory.h` 中三个使用 `mem.sread(addr, !we)` 的组件
  （`single_port_ram` · `dual_port_ram` 的两个端口）MUST 各自有双后端 parity 覆盖
- **AND** 覆盖 MUST 包含「写的那一拍」（`we == 1` ⇒ `enable == 0`）的输出断言
  —— 那正是唯一会踩中禁读拍的路径
