## ADDED Requirements

### Requirement: sread 的禁读拍 MUST 清零输出，且两条后端一致

`ch_mem::sread(addr, enable)` 在 `enable == 0`（**禁读拍**）时的输出 MUST 为
**全零**，且 Simulator 与生成的 Verilog MUST 逐位相同。

⚠️ `ch_mem::aread` **不在本条管辖内**：它不提供 `enable`（`create_mem_read_port`
收到的是 `nullptr`）⇒ 恒为「读」，不存在禁读拍。

#### Scenario: 禁读拍的输出为 0 且两后端相等

- **WHEN** 一个设计使用带 `enable` 的 `ch_mem::sread`，并在某一拍把 `enable` 拉低
- **THEN** 该拍之后读到的输出 MUST 为 **0**，且在 Simulator 与生成的 Verilog 上
  MUST 逐位相同
- **AND** 该判据 MUST 是可执行的（双后端 parity 用例 · 期望值手推），
  MUST NOT 只靠代码审阅或注释
- **AND** 判据 MUST 能区分「清零」与「保持」：至少一个用例的前一拍输出**非零**

#### Scenario: 裁决理由可追溯

- **WHEN** 审阅本条契约的由来
- **THEN** 以下裁决理由 MUST 可追溯：修复前 Simulator 是「保持上一拍值」
  （`src/ast/instr_mem.cpp` 的 `if (!enabled) return;`）、生成的 Verilog 是
  「清零」（`src/codegen_verilog.cpp` 的 `else proxy <= 1'b0;`，按 Verilog
  **dangling-else** 与内层 `if (en)` 配对 —— **不是** reset 分支）
- **AND** 裁决 MUST 选择「对齐 Verilog」，理由三条：① 只改 Simulator ⇒ 既有用户
  生成的 Verilog 零变化；② 「不读就没有数据」比「读到上一拍的陈旧值」更不易被
  下游误用；③ 生成代码的 `else` 语义本就是「`enable == 0` 清零」，Simulator 是
  偏离的一侧

#### Scenario: 生产组件 `chlib::memory` 的 RAM 族被覆盖

- **WHEN** 校验本条的判据覆盖面
- **THEN** `include/chlib/memory.h` 中三个使用 `mem.sread(addr, !we)` 的组件
  （`single_port_ram` · `dual_port_ram` 的两个端口）MUST 各自有双后端 parity 覆盖
- **AND** 覆盖 MUST 包含「写的那一拍」（`we == 1` ⇒ `enable == 0`）的输出断言，
  且该断言 MUST 是 **0** —— 那正是唯一会踩中禁读拍的路径
