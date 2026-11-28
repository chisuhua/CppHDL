## ADDED Requirements

### Requirement: I-L1 的 fill MUST 走 AXI4-full 的 4-beat burst

I-L1（取指侧 cache）的 **miss 填充** MUST 走 **AXI4-full 读通道**的 **4-beat burst**：

- 请求 MUST 是 `ARLEN=3` / `ARSIZE=2` / `ARBURST=INCR`（16 B line = 4 × 32-bit word）。
- MUST 有 `RLAST` 收尾，并以**当拍**的读数据拼出整行（用尚未装载的 beat 寄存器拼行 ⇒
  **最后一个 lane 恒 0** ⇒ 属功能错误）。
- 单拍读协议（一次一个 `RDATA`）MUST NOT 被当作 fill 的完成条件。

#### Scenario: 一行 4 个字经 burst 填充

- **WHEN** I-L1 对一个未命中的 line 发起填充
- **THEN** 读通道 MUST 以 `ARLEN=3` / `ARSIZE=2` / `ARBURST=INCR` 发出
- **AND** 4 个 beat MUST 各落进 line 的**对应 lane**（含**最后一个** lane）

#### Scenario: 单拍激励不得完成填充

- **WHEN** 外部只提供**一次**读数据与响应（无 burst 序列）
- **THEN** 填充 MUST NOT 完成 ⇒ line MUST 保持无效
