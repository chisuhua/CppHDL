# rv32i-canonical-home Specification

## Purpose

规定 RV32I 微架构的**唯一权威归属地**：ALU / 寄存器堆 / PC / decoder /
流水线寄存器 / stage 组件 / hazard / forwarding / branch 预测 / 数据前推 mux /
TCM / cache 包装等组件的头文件 MUST 位于 `include/cpu/` 之下，
`examples/riscv-mini/src/` 只保留 SoC 顶层与 example 专属件。

该契约的存在理由是「**消歧不能靠 `-I` 顺序**」：收敛前 10 组同名头文件靠
`-I` 顺序决定实现（调序即静默换实现），且库测试通过反向 `-I` / `#include`
依赖 example 源码。契约把归属地、SoC 顶层端口冻结、死头删除、零消费者豁免登记
与**自动化护栏**（6 条检查 + 自身回归测试）写成可执行条款，使「提升回
`include/cpu/`」不再是保留死头的理由。

护栏的判据边界如实声明：**只漏报、绝不把活头判死**（两个独立证据源都判
无 includer 且源② 可信才判死）；已知误报向量（`#if 0` 非激活区内的字面
`#include` 仍计入证据）与其可达性限制一并记录。

## Requirements

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
- **AND** SoC 顶层的**端口集合** MUST 被冻结 —— 冻结范围含**端口名 + 方向
  （`ch_in` / `ch_out`）+ 位宽**（供下游双后端 parity 作为 DUT；方向翻转对 DUT 的
  破坏力等同改名）
- **AND** 任何改变该端口集合的修改 MUST 单独裁决

#### Scenario: SoC 顶层端口 MUST 被冻结

- **WHEN** 收敛工作修改 SoC 顶层的内部装配
- **THEN** 它的**端口集合** MUST NOT 改变（**名 + 方向 + 位宽**三者皆冻结）
- **AND** 该约束 MUST 有**可执行的验证**（护栏的「SoC 顶层端口快照」检查：
  改动端口名 / 方向 / 位宽任一 ⇒ 护栏 MUST 报违规）

### Requirement: 零消费者的头文件 MUST 被删除

- **WHEN** 一个头文件**零消费者**（**两个独立证据源都**判定没有任何 TU 的
  `#include` 闭包包含它）
- **THEN** 它 MUST 被删除，MUST NOT 以「提升回 `include/cpu/`」的方式保留
- **AND** 删除 MUST 有留痕
- **AND** 例外 MUST 被登记，且 MUST 有**失效条件**（条件达成后 MUST 移除登记）

#### Scenario: 提升不是保留的理由

- **WHEN** 一个头文件曾因「未被使用」而被移出 `include/cpu/`
- **THEN** 「提升回 `include/cpu/`」MUST NOT 作为保留它的理由
- **AND** 判定 MUST **保守**：任一证据源说「有 includer」⇒ MUST NOT 判死
  （绝不把**活头**误判成死头）
- **AND** 该约束 MUST 有**可执行的验证**（零消费者头出现在 `include/cpu/` ⇒ 护栏 MUST 报违规）

#### Scenario: 零消费者头 MUST 登记

- **WHEN** 一个头文件因「已被下游 change 具名预留」而暂时零消费者
- **THEN** 它 MUST 出现在护栏的允许清单里，**带理由 + 失效条件**
- **AND** 允许清单 MUST 记**期望条数**（防止豁免项静默增加）
- **AND** 清单项一旦不再成立（该头重新有了消费者）⇒ 护栏 MUST 失败（stale 豁免不得静默残留）

#### Scenario: 「stale 豁免」的「已有消费者」MUST 是**真 include 证据**且必须是**当前的**

- **WHEN** 护栏检查允许清单里的某个头是否已不再零消费者
- **THEN** 「已有消费者」的判据 MUST 是**真 include 证据**（编译器的 `.d` 记录）
- **AND** 该证据 MUST 是**当前的**：① `.d` 所引用的源文件 MUST 仍存在于磁盘；
  ② 该源文件 MUST **仍 `#include` 该 basename**（⚠️ 比对对象是 `#include`
  的**操作数**（引号 / 尖括号内的文本，取最后一段 == basename）——
  若只查「文本提及」，则**注释**里的提及也会被当成证据；若只比对
  「行首是 `#include` 的整行」，则行尾注释（`#include "other.h" // foo.h`）
  的提及仍会被当成证据 ⇒ 收窄到操作数消除了该形态）；
  ③ 该 `.d` MUST **不早于该源文件**（⚠️ 阈值 = 0）：`.d` 由预处理阶段写出，
  与源文件 mtime 的间隔常常 < 1s（实测 `g++ -MD` 产出同秒的 `.d`）⇒
  硬阈值「≥ 1 秒」会**必然**漏判（假阴性）⇒ 用**显式时间差 ≥ 0** 而非裸 `-nt`
  （macOS 自带 bash 3.2 的 `-nt` 只比秒级 `st_mtime`）
  （`.d` 是**累积**构建产物 ⇒ TU 被删 / target 被移除后它不会被清掉 ⇒
  「`.d` 里出现过」**不等于**「现在有消费者」）
- **AND** ⚠️ **已知误报向量（如实声明，MUST NOT 记成「漏判」）**：
  条件编译**非激活区**（`#if 0` / `#ifdef`）里的**字面** `#include`
  仍被计入证据 —— 预处理器语义不在判据 (b) 内
  （真修需完整预处理器语义：
  嵌套 `#if` / `#else` ⇒ 当时被否决；实际行为由自测 fixture 钉住 ⇒ 行为变化可见）
- **AND** 不满足「当前性」时 MUST **降级为警告**（与源① 一致的安全方向），
  MUST NOT 报违规（破坏性结论 ⇒ 假违规会诱导删掉被**冻结**的 DUT）
- **AND** MUST NOT 使用「源码里**原文提及**过该 basename」作为「已有消费者」的判据
  —— 源① 的语义是「任何原文提及 ⇒ 可能 include 过」，它在「判死」方向是保守的
  （漏报，安全）；若用**同一谓词**证明「已有真实消费者」，方向相反 ⇒
  一个**纯注释**里的提及即产生假违规，且错误信息会指示「MUST 移除该行」
  ⇒ 移除后护栏报「硬死头 MUST 删除」⇒ 会诱导删掉被**冻结**的 DUT
- **AND** 只有「原文提及」而无 `.d` 证据时 ⇒ MUST **只警告**（提请人工确认），
  且警告措辞 MUST NOT 指示删除允许清单行

#### Scenario: 「零消费者」判定 MUST 只漏报、且其边界由 fail-closed 探针守着

- **WHEN** 护栏判定某头文件零消费者
- **THEN** 判定 MUST **只漏报**：任何**原文提及**该 basename 的文件（不论
  `#include` 写成引号 / 尖括号 / 宏 / 续行 / 仓库根相对路径）都 MUST 算作
  includer ⇒ 代价是注释或文档里的提及也让它判活（**有意**，方向安全）
- **AND** 该头**自身**的提及（`/** @file foo.h */`）MUST NOT 算作 includer
  （否则每个头恒活 ⇒ 判定失去判别力）
- **AND** 源码级证据源的**扫描面** MUST 覆盖仓库自有的 C/C++ 扩展名；
  出现扫描面**之外**的 C/C++ 扩展名 ⇒ 护栏 MUST 报违规
  （不得在注释里默默承认覆盖不全）
- **AND** 扫描面的 **prune 列表 MUST 只包含**（a）第三方 / VCS 元数据目录
  **与**（b）**构建输出目录**；构建输出目录 MUST 用**纯内容**判定（实测判据：
  含 `CMakeCache.txt`，**与目录名无关**）⇒ MUST NOT 只按**目录名前缀** prune
  （`build*` 这类 glob 会连带 prune 掉 `buildtools/`、`builder/` 以及
  `openspec/specs/build-cpp-standard/` 这类**非构建目录** ⇒ 那里面的
  消费者既不被扫到、也不被上面的探针报出 ⇒ 活头会被误判成死头）
- **AND** 允许清单的每条豁免 MUST 是 **3 个 TAB 分隔字段**
  （路径 / 理由 / 失效条件）⇒ 字段数不符 MUST 报违规
  （人工编辑破坏格式 ⇒ 理由与失效条件丢失 ⇒ 豁免变成「无期限」，而护栏看不出来）
- **AND** 该证据源实际处理的 TU 数 MUST 等于扫描面文件数（崩溃 / 少读 ⇒ 违规）

### Requirement: 库测试 MUST NOT 通过 include 路径依赖 example 源码

- **WHEN** 根 `tests/**` 的编译单元需要 RV32I 微架构组件
- **THEN** 它们 MUST 从 `include/cpu/` 获取
- **AND** 根 `tests/CMakeLists.txt` MUST NOT 出现指向 `examples/**/src` 的 `-I`
- **AND** 根 `tests/**` 的任何 TU MUST NOT `#include` `examples/**` 路径

#### Scenario: 护栏覆盖面 MUST 被如实声明

- **WHEN** 本 Requirement 的护栏只覆盖「反向 `-I`」与「反向 `#include`」两种形态
- **THEN** 覆盖面边界 MUST 被如实记录（不得让护栏 PASS 掩盖未覆盖面）
- **AND** **第三种**形态 —— 根 `tests/CMakeLists.txt` 把 `examples/**` 下的**源文件本身**
  编进库测试 target（当前 1 处：编译 `examples/riscv-mini/tests/test_csr_unit.cpp`）
  —— 是**已知债务**，MUST NOT 被本护栏的 PASS 掩盖

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
