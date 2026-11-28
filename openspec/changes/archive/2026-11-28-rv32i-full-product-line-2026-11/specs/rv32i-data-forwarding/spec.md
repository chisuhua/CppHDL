## ADDED Requirements

### Requirement: 本 capability 的距离矩阵与停顿拍数 MUST 标注其适用产品线

本 capability 的**前推档位 / 距离矩阵 / load-use 停顿拍数**是**在「取指侧 1 拍 `sread`」这一
取指形态下**推导出来的 ⇒ ⇒ 它**只适用于具备该取指形态的产品线**。

- 本 capability MUST 显式声明其**适用产品线**，不得以「线无关权威」的措辞存在。
- 取指形态**不同**的产品线 MUST 有**独立**的 capability 承载其停顿契约 ⇒ ⇒
  不同产品线**不得**共用本矩阵，也**不得**由本矩阵推断另一条线的停顿拍数。

#### Scenario: 另一条产品线的取指形态不同

- **WHEN** 某产品线的取指侧为**总线读**（多拍 + 可能 miss）而非 1 拍 `sread`
- **THEN** 该产品线的 load-use 停顿拍数 MUST NOT 取自本 capability 的矩阵
- **AND** 本 capability MUST 已声明其适用产品线，使该取用可被机械判定为越界
