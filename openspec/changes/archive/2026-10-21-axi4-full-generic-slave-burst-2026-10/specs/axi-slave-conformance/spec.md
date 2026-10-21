# Capability: axi-slave-conformance

## MODIFIED Requirements

### Requirement: 通用 AXI4-Lite slave MUST 提供低有效复位

`AxiLiteSlave` MUST 暴露 `ch_in<ch_bool> rst_n` 端口（低有效，**在时钟沿生效**），
且其全部状态寄存器 MUST 使用 `select(!io().rst_n, <init_val>, ...)` 复位 pattern。

**适用范围与措辞说明（契约的一部分）**：

1. 本 Requirement 管辖 `AxiLiteSlave`（`include/axi4/axi4_lite_slave.h`）。
   `Axi4Slave`（`include/axi4/axi4_full.h`，AXI4 **Full**）的规范形态与 burst 语义
   由独立 capability `axi4-full-slave-burst` 承载
   （`openspec/specs/axi4-full-slave-burst/spec.md`），
   **不在本 Requirement 范围**。
   ⚠️ MUST NOT 用「未实现的 change 名」做交叉引用 ——
   change 归档后目录会移入 `openspec/changes/archive/`，
   指向 change 名的引用会成为 dangling 指针；MUST 指向 **capability**。
2. 本 Requirement 与 `dual-backend-test-coverage` 的「AXI4-Lite slave peripheral MUST 收敛到
   相位分离 + 响应保持的规范形态」Requirement **同源**（同一套四件套形态），
   区别在**适用对象**：那条管辖 6 个 `include/axi4/peripherals/*.h` 的 AXI4-Lite peripheral，
   本条管辖通用 slave。**两者 MUST NOT 出现形态分歧。**
3. 复位措辞：本 Requirement 用「**低有效，在时钟沿生效**」。
   `dual-backend-test-coverage` 与 peripheral 头注释中的「低有效**异步**复位」措辞不准确 ——
   `select(!io().rst_n, <init_val>, ...)` 落在 `ch_reg` 的 `next` 表达式上，由时钟沿采样
   （`include/core/node_builder.h:180-182` 的 `ch_reg` 原生复位接的是 context 的
   `default_reset`，组件级 `rst_n` 只能走 `next` 表达式），实为**沿上生效**。
   措辞统一由后续 change `axi4-eval-order-doc-correction` 收口。
4. **字节道（WSTRB）语义以 `dual-backend-test-coverage` 的「AXI4 peripheral MUST 支持字节写」
   Requirement 为准**（该 Requirement 的适用范围已包含 `AxiLiteSlave`）。
   本 capability 只在「空写（`wstrb == 0`）的响应」这一点上引用它（MUST 仍为 `OKAY`），
   MUST NOT 另立一套字节道语义。

#### Scenario: 复位清零全部状态寄存器

- **WHEN** `rst_n` 为 0 并经过至少 1 个时钟沿
- **THEN** `busy` / `aw_done` / `b_valid` / `r_valid` / `addr_reg` / `ar_addr_reg`
  MUST 全部读回 0
- **AND** 4 个数据寄存器 MUST 读回各自的初值
- **AND** 复位 MUST NOT 依赖外部 testbench 预先清零数据

#### Scenario: 复位提供逃生路径

- **WHEN** 事务进行中被 `rst_n` 打断
- **THEN** slave MUST 在下一个时钟沿回到空闲态（`busy == 0`）
- **AND** MUST 能接受下一笔事务

#### Scenario: 未接线的 rst_n 等价于永久复位

- **WHEN** 实例化点未驱动 `rst_n`（读出为 0）
- **THEN** slave MUST 保持在复位态（全部状态寄存器为初值）
- **AND** 接线要求见 `include/axi4/AGENTS.md` CONVENTIONS → 复位
  （`rst_n` 是**必需端口**，实例化点 MUST 显式接线）
