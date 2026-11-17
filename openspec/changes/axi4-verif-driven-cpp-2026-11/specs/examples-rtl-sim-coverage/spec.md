# Capability: examples-rtl-sim-coverage

## ADDED Requirements

### Requirement: C++ 侧 MUST 与生成式 TB 共享同一验证源

**由 `.verif` 驱动**的产品，其 **C++ 侧（解释器路径）** 与 **Verilog 侧（渲染式 TB）** MUST 由
**同一份**验证源（`.verif`）驱动 ⇒ ⇒ 两侧断言**不可能**漂移。

⇒ ⇒ **作用域限定**：本条只约束**已迁移**（`.verif` 驱动）的产品 ⇒ ⇒ 仍在手写激励的产品
**不受**本条约束（迁移 = 独立 change）。

⇒ ⇒ **MUST NOT** 在 C++ 侧另写一份激励 / 断言 / 期望值（第二份期望载体）；
**MUST NOT** 存在「只打印不判定」的检查器（打印期望值但不影响退出码）。

⇒ ⇒ **MUST fail-closed**：验证源打不开 / 扫描器报错 / 端口清单有诊断 ⇒ **非零退出** +
stderr 说明，且 **MUST NOT** 打印结论行。

⇒ ⇒ 「断言条数 MUST 为三方等式」归本 capability 的既有 requirement ⇒ ⇒ 本条**不重述**。

#### Scenario: 期望载体只有一份

- **WHEN** 检视某产品的 C++ 侧实现
- **THEN** 它的激励与断言 MUST 来自 `.verif`（逐字同一文件）
- **AND** MUST NOT 存在第二份期望值载体（手写检查器 / 独立断言表）
- **AND** 该实现 MUST NOT 访问端口（无 `set_input_value` / `get_port_value` / `.io().`）
- **AND** 运行 MUST NOT 落任何文件（零 `.v` / 零 `.vcd`）

#### Scenario: 只打印不判定的检查器视为违规

- **WHEN** C++ 侧某项检查只输出文本、其结论不影响进程退出码
- **THEN** 该实现 MUST 判为**不满足**本条
- **AND** 该检查 MUST 改为断言（失败 ⇒ 非零退出）

#### Scenario: 验证源不可用必须响亮失败

- **WHEN** 验证源文件不存在 / 扫描器报错 / 端口清单有诊断
- **THEN** 退出码 MUST **非零**
- **AND** stderr MUST 有可行动说明
- **AND** stdout MUST NOT 含结论行（不得出现「0 条断言且退出 0」）

### Requirement: 由验证源驱动的 example 断言失败 MUST 以非零退出码结束

**由 `.verif` 驱动**的 example，其退出码 MUST 反映验证结论：断言失败或执行不完整 ⇒
**非零**；全部通过且条数自洽 ⇒ **零**。

⇒ ⇒ **作用域限定**：本条**只**约束 `.verif` 驱动的 example ⇒ ⇒ 已登记但**尚未迁移**的
example 产品**不受**本条约束（迁移 = 独立 change）。

⇒ ⇒ **MUST** 打印机器可读结论行（`RESULT: checks=<N> fails=<M>`）⇒ ⇒ **十进制**、
单行、无 ANSI 颜色 ⇒ ⇒ 与 Verilog 侧同协议，两后端结论可直接比对。

⇒ ⇒ 「条数自洽」= 运行期条数 MUST 等于扫描器静态展开的条数（防提前中止/少跑）。

#### Scenario: 断言失败 ⇒ 非零退出

- **WHEN** 验证源里存在一条**必然失败**的断言
- **THEN** 退出码 MUST **非零**
- **AND** 结论行 MUST 报出失败的 `check_id`

#### Scenario: 断言全过 ⇒ 零退出

- **WHEN** 验证源的全部断言通过
- **THEN** 退出码 MUST 为 **0**
- **AND** 结论行的 `fails` MUST 为 **0**
- **AND** 结论行的 `checks` MUST 等于扫描器静态展开的条数

#### Scenario: 退出码契约有判别力

- **WHEN** 把验证源的断言改成 1 条且全过
- **THEN** 以「结论行条数 == 登记条数」为判据的护栏 MUST **转红**
- **AND** 仅判退出码的护栏 MUST NOT 被视为充分
