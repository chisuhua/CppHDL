## ADDED Requirements

### Requirement: 仲裁器的选择与锁存 MUST 用硬件原语表达（建图期求值 MUST NOT 决定硬件行为）

`chlib` 的仲裁器 MUST NOT 用**普通 C++ 控制流**（`if` / `else` / 三元）来决定
**硬件量**（通道选择、锁存、指针推进）—— 此类条件在**建图期**求值 ⇒ ⇒
**恒取默认值** ⇒ ⇒ 表现为「恒选通道 0」/「指针永不推进」。

选择 MUST 用硬件原语表达（`select` 链 / 真 `&` `|`），寄存器写入 MUST 用
`->next`（**MUST NOT** 用对 `ch_reg` 的 C++ 赋值）。

#### Scenario: 选择 MUST NOT 由建图期求值的条件决定

- **WHEN** 只有一个输入通道有效（其余无效）
- **THEN** 被选中的通道 MUST 是该有效通道
- **AND** 输出 payload MUST 是**该通道**的 payload（MUST NOT 是其它通道的）
- **NOTE**: 「输出别的通道的 payload」比「选错通道」更严重 ⇒ ⇒ 属
      **静默数据损坏** ⇒ ⇒ 判据 MUST 同时断言「选中索引」与「payload」。

#### Scenario: 锁存式仲裁 MUST 保持选中直到事务完成

- **WHEN** 某通道被授予且事务尚未完成（下游未就绪）
- **THEN** 即使**其它**通道此后变为有效，选中 MUST **保持不变**
- **AND** 该通道的 payload MUST 持续输出（MUST NOT 换成别的通道）
- **NOTE**: 「保持到事务完成」= 释放条件是 `valid & ready`（**事务完成**），
      而**不是**「被锁通道是否仍然 valid」—— 后者会在通道中途撤销 valid 时
      换人（打断已送出的数据）。

#### Scenario: 输出 valid MUST 是**选中通道**的 valid

- **WHEN** 锁定的通道撤销了 valid，而其它通道有效
- **THEN** 输出 `valid` MUST 为 `0`
- **AND** MUST NOT 输出其它通道的数据
- **NOTE**: 「任一通道 valid」的实现会在此处给出 `valid=1` + **错通道**的
      payload ⇒ ⇒ 数据/valid 不匹配 ⇒ ⇒ 比「选错通道」更难排查。

#### Scenario: 自持状态的轮转仲裁器 MUST 每拍推进指针

- **WHEN** selector 层的轮转仲裁器被持续请求
- **THEN** 其优先级指针 MUST **每拍推进**（环绕）MUST NOT 停在固定位置
- **AND** 多个请求者 MUST 在有限拍内**全部**被授予
- **NOTE**: 「指针停在固定位置」的实现**逐位等同于固定优先级仲裁** ⇒ ⇒
      持续有效的低位通道**饿死**高位通道。

#### Scenario: 轮转判定 MUST NOT 依赖宽位宽的移位比较

- **WHEN** 实现「被授予位」的判定
- **THEN** MUST 用**取位**（`bit_select`）而非「与宽移位结果比较」
- **NOTE**: `1 << pos` 会生成 **N + (N-1) 位**的中间节点（N=64 ⇒ 127 位）；
      本仓 interpreter 的 `eq` 在「64 位 vs 127 位」且**移位量 ≥ 2** 时**判错**
      ⇒ ⇒ 表现为该宽度下轮转**不工作**（`next_ptr` 恒 0）。
