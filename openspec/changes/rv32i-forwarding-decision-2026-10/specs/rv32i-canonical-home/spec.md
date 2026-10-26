# Capability: rv32i-canonical-home

## MODIFIED Requirements

### Requirement: 前推数据 mux MUST 归属独立的 mux 组件

- **MODIFIED**: 该要求从「mux **MUST** 归属独立组件」演进为「mux **已**归属独立组件
  **且已接入**真实流水线装配」⇒ 补上**接入形态**的约束（数据源 MUST 取自流水线
  寄存器、档位选择 MUST 延迟一拍生效）与「已接入」后的**可执行验证**
  （含**负向对照**）。「收敛 MUST NOT 丢失前推覆盖」一条**不变**，另新增一条
  「前推接入后 MUST 有**判别性**证据」。

- **WHEN** 冒险检测（产生 `forward_a` / `forward_b`）与**数据选择**（按 `forward_a` /
  `forward_b` 选 EX / MEM / 寄存器堆数据）被放在**同一个**组件里
- **THEN** 该组件接入真实流水线装配时 MUST NOT 形成**组合环**
  （`hazard → ex_alu_result → ex_stage → id_stage → reg_file → hazard`）
- **AND** 数据选择 MUST 由**独立的 mux 组件**承担
- **AND** 该 mux 的**档位语义** MUST 与既有 mux 组件逐档等价
  （`0`→寄存器堆、`1`→EX、`2`→MEM）
- **AND** 该 mux 的**数据源** MUST 取自**流水线寄存器**（EX/MEM 级与 MEM/WB 级），
  MUST NOT 取自 EX/MEM 级的**组合输出**（那正是组合环的成因）
- **AND** 档位选择 MUST **延迟一拍**再生效
  （否则 MUST 把「生产者自己的结果」喂给当前正在执行的指令 ⇒ 静默错值）

#### Scenario: 数据 mux 接入流水线 MUST 不成环

- **WHEN** 前推数据 mux 被接入真实流水线装配
- **THEN** 拓扑排序 MUST NOT 报组合环
- **AND** 该约束 MUST 有**可执行的验证**（装配后构建 + 运行 ⇒ 无环错误）
- **AND** **负向对照** MUST 存在：数据源取自 EX/MEM 级**组合输出**的形态 MUST
  **仍然**触发环检测
  （否则「不成环」可能只是**环检测本身失效** ⇒ 该结论没有意义）

#### Scenario: 收敛 MUST NOT 丢失前推覆盖

- **WHEN** 收敛把数据 mux 从冒险检测组件里拆出
- **THEN** 原来断言「前推后的数据」的用例 MUST 继续断言**同样的值**
- **AND** MUST NOT 把该断言降级为「直通值」或删除

#### Scenario: 前推接入后 MUST 有**判别性**证据

- **WHEN** 前推被接入真实流水线装配
- **THEN** MUST 存在「生产者与消费者之间**无间隔**」的用例，
  且断言消费者拿到**新值**
- **AND** MUST NOT 只依赖「生产者与消费者之间留了间隔」的固件
  （那种固件在**未接前推**时同样通过 ⇒ 对前推是**零信号**）
