## ADDED Requirements

### Requirement: 多 word 左移 MUST 与 Verilog 的 `<<` 逐位一致

当移位结果的位宽 **大于一个 machine word**（`ch_machine_word_bits`，当前实现为 64）
时，interpreter 的左移 MUST 与生成 Verilog 的 `<<` 给出**逐位相同**的结果。

⇒ 判据：结果 `R = S << d` 的每一位 MUST 等于「`S` 的第 `k` 位置位 ⇒ `R` 的第
`k + d` 位置位」（移位量的**定义式**），且 **MUST NOT** 在 `R` 的高位出现
`S` 与 `d` 之外的任何置位。

⇒ 该契约**不因**「结果被截断到窄端口」而放宽：截断发生在**消费侧**，
中间节点的槽位宽度 MUST 保持其声明宽度（与「interpreter 的赋值 MUST 保持
目标节点的声明宽度」同源）。

#### Scenario: 跨 word 进位 MUST 从低 word 挤入高 word

- **WHEN** 源 `S` 只在**最低** word 内有置位（如 `S = 0x8000000000000000`，64 位）
  且结果位宽为 191 位（3 个 word）、移位量 `d = 1`
- **THEN** `R` MUST 等于 `2^64`（⇒ 高 word 的 bit 0 为 1）
- **AND** `R` 的低 word MUST 全 0
- **NOTE**: 实现曾把这一项写成 `in[i-s-1] << (WORD_SIZE - d)`（**方向反了**）
         ⇒ ⇒ 跨 word 的位**一位都没进来**（实测得到 0），同时把**源**再左移一次
         塞进高 word（⇒ **虚假高位**）。⇒ ⇒ 与 Verilog `<<` 分歧。

#### Scenario: 源比目标短时 MUST NOT 越界读源

- **WHEN** 目标 word 数大于「源 word 数 + 移位量整除的 word 数」
- **THEN** 实现 MUST 把源的缺失 word 视为 `0`
- **AND** MUST NOT 读源数组之外的内存
- **NOTE**: 「只判下界、不判上界」会让 `out = 191 位 / in = 64 位 / d = 0` 触发
         越界读 ⇒ ⇒ 结果不确定（且可能被 sanitizer 抓到）。这是**内存安全**问题，
         不只是数值错误。

#### Scenario: 同址左移 MUST NOT 破坏输入

- **WHEN** 调用方对 `bitvector<N>::operator<<=`（`N` 跨多个 word）就地进行左移
- **THEN** 输入 MUST 在被写出之前先被完整读取（实现内部必须先拷贝）
- **AND** 结果 MUST 与「非就地 `operator<<`」一致
- **NOTE**: 多 word 路径的第一件事是 `std::fill_n(out, ...)` ⇒ ⇒ 同址调用会把
         **输入清成 0**（实测 128 位 `<<= 1` 得到 `0`）。⇒ ⇒ 该缺陷**只有**
         bv 层用例能覆盖（图层走 `bv_shl_truncate` 时 dst ≠ src）。

#### Scenario: 该契约 MUST 有双后端判据 + 独立参考模型

- **WHEN** 为该契约写判据
- **THEN** 它 MUST 是**双后端**（interpreter + verilator）用例
- **AND** 期望值 MUST 由**独立模型**给出（逐位置位的定义式），
  MUST NOT 取自任一后端的输出
- **AND** 激励 MUST 覆盖「源 1 word → 目标多 word」与「源多 word → 目标更多 word」
  两种形态，以及 `d = 0` / `d < word` / `d = word` / `d > word`
- **NOTE**: 该缺陷**只在 interpreter 侧**（Verilog 的 `<<` 天然正确）⇒ ⇒
         只断言数值会把它误判成「比较/赋值的语义问题」（立项时就是这样误判的）。
