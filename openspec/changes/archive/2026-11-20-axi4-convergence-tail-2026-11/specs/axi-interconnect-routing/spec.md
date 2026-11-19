## MODIFIED Requirements

### Requirement: AXI 互连层 MUST 提供低有效复位

`AxiInterconnect4x4` 与 `AxiLiteMatrix2x2` MUST 暴露 `ch_in<ch_bool> rst_n`
（低有效，**在时钟沿生效** —— `select(!io().rst_n, <init_val>, ...)` 落在 `ch_reg` 的
`next` 表达式上，由时钟沿采样），其**全部**状态寄存器
（路由锁存、`active` 标志、AW/AR 仲裁指针、error responder 状态）MUST 使用该复位 pattern，
`<init_val>` MUST 带该寄存器的**显式位宽**（同 `axi-slave-conformance` 的
「复位初值必须带显式位宽」）。

`rst_n` 是**必需端口**，实例化点 MUST 显式接线；
未接线读出为 0 = **永久复位**（同 `axi-slave-conformance` 的
「未接线的 rst_n 等价于永久复位」）。

**与 `axi-slave-conformance` 的分工**：互连层是 rst_n 链路的**中间节点**，
MUST 与下游 slave 在**同一复位窗口内**复位 —— 否则 slave 的在途标志被清零
而互连的路由锁存残留，该 master 永久等不到响应（**静默死锁**）。

**为什么 MUST 而非豁免**（裁决记录，2026-10-21）：

1. 改造完成后互连层**不再是纯路由**（路由锁存 + `active` 标志 + 双仲裁指针 +
   error responder 状态），「纯路由故由 INIT 保证复位」的前提不成立。
2. 「backend reset pulse 保证复位」**只在 t=0 成立**：
   `Simulator::reset()` 把 `data_map_` 全清 0 且**不委派给 backend**
   （`src/simulator.cpp:422-441`），
   interpreter 清 0 而 verilator 加载 INIT → **两后端中途复位语义分歧**。
   唯一两后端一致、testbench 可控的中途复位机制 = 组件级 `rst_n`。
3. 互连在 rst_n 链路上是**唯一断点**：
   `examples/axi4/phase3a_top.h`（设计定义）的顶层 `rst_n` 已透传到 converter 与
   AXI4-Lite slave，唯独互连没有该端口；`examples/axi4/tb_axi_soc_example.cpp`
   同理（顶层 `rst_n` → gpio/uart，唯独 matrix 断开）。
   ⚠️ **证据按符号定位、不按行号**（2026-11-20）：原文引用的
   `phase3a_complete.cpp` 已拆为 `phase3a_top.h` + `tb_phase3a_example.cpp`
   （`axi4-convergence-tail`），`axi_soc_example.cpp` 亦已改名 —— 行号随重构漂移
   ⇒ ⇒ 引用 MUST 落在**文件 + 符号**上，否则会随重构悬空。
4. 「可选端口」在本框架**不可行**：未接线的 `ch_in<ch_bool>` 读出为 0，
   配标准 pattern 即永久复位；「未接线 = 不复位」需要运行期「是否被驱动」查询，
   而 `num_srcs()` / `driver()` 是**构图期**属性（子组件 `describe()` 执行时
   父的 `<<=` 接线尚未发生）→ 不可靠。

#### Scenario: 复位清零全部互连状态

- **WHEN** `rst_n` 为 0 并经过至少 1 个时钟沿
- **THEN** 全部路由锁存 MUST 为 0，全部 `active` 标志 MUST 为 0
- **AND** 仲裁指针 MUST 回到初值
- **AND** error responder 状态 MUST 为空闲
- **AND** 复位 MUST NOT 依赖外部 testbench 预先清零状态

#### Scenario: 中途复位提供逃生路径

- **WHEN** 一笔事务在途（AW 已握手、W 未完成）时拉低 `rst_n` 至少 1 拍
- **THEN** 全部 `active` 标志 MUST 被清零
- **AND** 复位后互连层 MUST 能接受一笔**全新**的事务
- **AND** 该行为 MUST 在 interpreter 与 verilator 两后端下一致
  （两后端的中途复位语义只由组件级 `rst_n` 保证一致）

#### Scenario: 实例化点不得 tie 常量

- **WHEN** 审查互连层的实例化点
- **THEN** `rst_n` MUST 透传自父级的复位源
- **AND** MUST NOT 出现 `interconnect.io().rst_n <<= ch_bool(true)` 之类的 tie 常量
  （那会永久掩盖复位路径，使中途复位无法验证）
