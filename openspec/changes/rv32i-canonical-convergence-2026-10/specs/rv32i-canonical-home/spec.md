# Capability: rv32i-canonical-home

## ADDED Requirements

### Requirement: RV32I 微架构 MUST 有唯一权威归属地

- **WHEN** 一个组件属于 RV32I 微架构（ALU / 寄存器堆 / PC / decoder / 流水线寄存器 /
  stage 组件 / hazard / forwarding / branch 预测 / 数据前推 mux / TCM / cache 包装）
- **THEN** 它的头文件 MUST 位于 `include/cpu/` 之下
- **AND** MUST NOT 在 `examples/` 下存在**同 basename** 的副本
- **AND** 消歧 MUST NOT 依赖 `-I` 顺序

#### Scenario: 同 basename 双副本 MUST 被消除

- **WHEN** `include/cpu/**` 与 `examples/riscv-mini/src/**` 存在**同 basename** 的头文件
- **THEN** 护栏 MUST 报违规
- **AND** 该约束 MUST 有**可执行的验证**（构造一个同名副本 ⇒ 护栏 MUST 报违规）

#### Scenario: 消歧 MUST NOT 依赖 -I 顺序

- **WHEN** 某个头文件在两个目录下同名
- **THEN** 编译结果 MUST NOT 取决于 `-I` 顺序
- **AND** 该约束 MUST 由「**同名双副本 MUST NOT 存在**」保证
  （同名副本一旦消失，顺序就不再决定实现）
- **AND** 该约束 MUST 有**可执行的验证**（护栏的「同名双副本」检查：
  构造一个同名副本 ⇒ 护栏 MUST 报违规）

### Requirement: example 侧 MUST 只保留 SoC 顶层与 example 专属件

- **WHEN** `examples/riscv-mini/src/` 收敛完成
- **THEN** 其内容 MUST 只包含：
  - **SoC 顶层装配**（把流水线 / TCM / cache 组装成 example 的 SoC 顶层）
  - **example 专属件**（仅供该 example 的测试/演示使用、且**不构成可复用微架构组件**的文件）
- **AND** SoC 顶层的**端口列表** MUST 被冻结（供下游双后端 parity 作为 DUT）
- **AND** 任何改变该端口列表的修改 MUST 单独裁决

#### Scenario: SoC 顶层端口 MUST 被冻结

- **WHEN** 收敛工作修改 SoC 顶层的内部装配
- **THEN** 它的**端口列表** MUST NOT 改变
- **AND** 该约束 MUST 有**可执行的验证**（端口集合的快照比对）

### Requirement: 零消费者的头文件 MUST 被删除

- **WHEN** 一个头文件**零消费者**（没有任何 TU 的 `#include` 闭包包含它）
- **THEN** 它 MUST 被删除，MUST NOT 以「提升回 `include/cpu/`」的方式保留
- **AND** 删除 MUST 有留痕
- **AND** 例外 MUST 被登记，且 MUST 有**失效条件**（条件达成后 MUST 移除登记）

#### Scenario: 提升不是保留的理由

- **WHEN** 一个头文件曾因「未被使用」而被移出 `include/cpu/`
- **THEN** 「提升回 `include/cpu/`」MUST NOT 作为保留它的理由
- **AND** 该约束 MUST 有**可执行的验证**（零消费者头出现在 `include/cpu/` ⇒ 护栏 MUST 报违规）

#### Scenario: 零消费者头 MUST 登记

- **WHEN** 一个头文件因「已被下游 change 具名预留」而暂时零消费者
- **THEN** 它 MUST 出现在护栏的允许清单里，**带理由 + 失效条件**
- **AND** 允许清单 MUST 记**期望条数**（防止豁免项静默增加）
- **AND** 清单项一旦不再成立（该头重新有了消费者）⇒ 护栏 MUST 失败（stale 豁免不得静默残留）

### Requirement: 库 MUST NOT 依赖 example 源码

- **WHEN** 根 `tests/**` 的编译单元需要 RV32I 微架构组件
- **THEN** 它们 MUST 从 `include/cpu/` 获取
- **AND** 根 `tests/CMakeLists.txt` MUST NOT 出现指向 `examples/**/src` 的 `-I`
- **AND** 根 `tests/**` 的任何 TU MUST NOT `#include` `examples/**` 路径

#### Scenario: 反向 -I MUST 被消除

- **WHEN** 根 `tests/CMakeLists.txt` 出现 `examples/riscv-mini/src` 的 `-I`
- **THEN** 护栏 MUST 报违规

#### Scenario: 反向 include MUST 被消除

- **WHEN** 根 `tests/**` 的某个 TU `#include` 了 `examples/**` 路径
- **THEN** 护栏 MUST 报违规

### Requirement: 前推数据 mux MUST 归属独立的 mux 组件

- **WHEN** 冒险检测（产生 `forward_a` / `forward_b`）与**数据选择**（按 `forward_a` /
  `forward_b` 选 EX / MEM / 寄存器堆数据）被放在**同一个**组件里
- **THEN** 该组件接入真实流水线装配时 MUST NOT 形成**组合环**
  （`hazard → ex_alu_result → ex_stage → id_stage → reg_file → hazard`）
- **AND** 数据选择 MUST 由**独立的 mux 组件**承担
- **AND** 该 mux 的**档位语义** MUST 与既有 mux 组件逐档等价
  （`0`→寄存器堆、`1`→EX、`2`→MEM）

#### Scenario: 数据 mux 接入流水线 MUST 不成环

- **WHEN** 前推数据 mux 被接入真实流水线装配
- **THEN** 拓扑排序 MUST NOT 报组合环
- **AND** 该约束 MUST 有**可执行的验证**（装配后构建 + 运行 ⇒ 无环错误）

#### Scenario: 收敛 MUST NOT 丢失前推覆盖

- **WHEN** 收敛把数据 mux 从冒险检测组件里拆出
- **THEN** 原来断言「前推后的数据」的用例 MUST 继续断言**同样的值**
- **AND** MUST NOT 把该断言降级为「直通值」或删除

### Requirement: 组件文档 MUST 与磁盘一致

- **WHEN** `include/cpu/` 的目录结构或头文件集合发生变化
- **THEN** `include/cpu/AGENTS.md` 的 STRUCTURE / KEY FILES 表 MUST 与磁盘一致
- **AND** 表中列出的每个头文件 MUST 在磁盘上存在
- **AND** 自述的文件数 / 行数 MUST 与实测一致

#### Scenario: 幽灵条目 MUST 被消除

- **WHEN** 文档表列出一个磁盘上不存在的头文件
- **THEN** 护栏 MUST 报违规

#### Scenario: 自述计数 MUST 与实测一致

- **WHEN** 文档自述的头文件数与磁盘实测不一致
- **THEN** 护栏 MUST 报违规
