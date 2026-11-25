# rv32i-official-test-suite Specification

## Purpose
本 capability 管辖 **RV32I 核的「官方 ISA 判据」**—— 即
「本核 MUST 通过上游 riscv-tests 的 `rv32ui-p-*` 物理内存测试集」这一层契约的
**唯一权威**：测试来源、测试体是否被改动、判定协议、**排除项的登记义务**，
以及「镜像 MUST 在仓内」这条可重复性要求。

它**不**管辖 ISA 语义本身（跳转 / 字节道 / 算术的正确性）—— 那是
`rv32i-isa-semantics` 的职责；也**不**管辖数据前推 / 反压期操作数完整性 ——
那是 `rv32i-data-forwarding` 的职责；也**不**管辖退休数 / 有效链 ——
那是 `rv32i-commit-path` 的职责。

## Requirements

### Requirement: 官方 riscv-tests 套件 MUST 全过

- **WHEN** 本仓的 RV32I 核被评估「是否符合 RV32I」
- **THEN** 上游 riscv-tests 的 `rv32ui-p-*`（物理内存）测试集 MUST **全部通过**
  （⇒ ⇒ **依下方「排除项」Requirement 登记者除外**）
  ⇒ ⇒ 该测试集 MUST 是「riscv-mini 完成」的**定义**
- **AND** 判定 MUST 用**上游 env 的判定协议**
  （`tohost == 1` ⇒ PASS；`tohost == TESTNUM | 1` ⇒ FAIL）
  ⇒ ⇒ MUST NOT 自拟一套判定口径
- **AND** 测试 MUST 跑在**真正的核**上
  （取指走 I-TCM · 数据走 D-TCM ⇒ ⇒ 即 `ch_device<Rv32iCoreTop, itcm, dtcm>` 形态）
- **AND** **核级**门禁 MUST 是**可重复**的仓内工件 ⇒ ⇒ MUST 进 CTest
  ⇒ ⇒ **交付物级**门禁（见「官方套件 MUST 也在 RTL 产物上被判定」Requirement）
  MUST 有 **CI 入口** ⇒ ⇒ 形态**可为脚本**（非 CTest）
  ⇒ ⇒ 理由：产物级门禁单次 ≈16 s（verilator 编译主导）⇒ ⇒ 40 个条目会让
  「按条目名实跑每个 ctest 条目」的护栏从 ≈10 min 涨到 ≈20 min
  ⇒ ⇒ 而脚本形态可**抽样**（PR）+ **全量**（nightly）⇒ ⇒ 两类入口**同口径**

#### Scenario: 门禁条目 MUST 落在仓内的单测耗时上限之内

- **WHEN** 一个门禁条目的耗时超过仓内的**单测上限**
- **THEN** MUST **拆分**成多个条目
- **AND** 每个条目 MUST 是**独立可执行文件**
  （⇒ ⇒ 否则「按条目名找二进制」的保护会跑到**默认集合** ⇒ ⇒ 与条目不等价）

#### Scenario: 官方套件全过 = 「riscv-mini 完成」

- **WHEN** 全部 `rv32ui-p-*` 门禁条目为绿
- **THEN** 「riscv-mini 完成」的定义 MUST 被满足
- **AND** MUST NOT 以「本仓自写用例全过」替代该定义
  （⇒ ⇒ 自写用例与官方套件是**不同**的判据面）

#### Scenario: 判定 MUST 落在官方 env 的 `tohost` 协议上

- **WHEN** 官方 env 写 `tohost`
- **THEN** 门禁 MUST 观测到该次写并取其值作为判定依据
- **AND** MUST NOT 用「测试是否跑完」或「退休条数」代替 `tohost` 的值

### Requirement: 门禁的判定 MUST 有**负向对照**（防恒绿）

- **WHEN** 门禁判定「`tohost == 1` ⇒ PASS」
- **THEN** MUST 有一个**负向对照**证明判定**不是恒绿**
  ⇒ ⇒ 即：喂一个**已知 FAIL 值**的镜像（`tohost = TESTNUM | 1`）时
  门禁 MUST **如实读到该 FAIL 值**（而不是 1、也不是读不到）
- **AND** 该对照 MUST **手推**其期望值（禁抄仿真输出）
- **AND** 镜像缺失 / 装载失败 MUST 表现为**判据失败**（fail-closed）
  ⇒ ⇒ MUST NOT 表现为 skip 或「空洞通过」

#### Scenario: 恒绿判定的两种形态 MUST 被堵住

- **WHEN** 运行器把「观测到的值」写死成 1，或把「看不到 `tohost` 写」当成通过
- **THEN** 负向对照 MUST 转红
  ⇒ ⇒ 前者：FAIL 镜像读到 1 ⇒ 期望的 FAIL 值对不上
  ⇒ ⇒ 后者：`saw_tohost == false` ⇒ 判据失败

### Requirement: 官方测试的**测试体** MUST NOT 被改动

- **WHEN** 官方镜像被构建
- **THEN** 测试体（上游 `isa/rv32ui/*.S`）MUST **一字未改**
- **AND** 若测试体需要**不同的环境**（如无 CSR / 无 trap 的机器），
  则 MUST 只替换 **env 的 harness 宏** ⇒ ⇒ MUST NOT 改测试体
- **AND** 该替换 MUST 有**上游依据**（维护者给出的做法）
- **AND** MUST NOT 改上游 Makefrag 的**测试集合**来回避失败

#### Scenario: 无特权机器上的 env 替换 MUST 只动 harness 宏

- **WHEN** 目标机器不实现 CSR / trap
- **THEN** env MUST 只替换 `RVTEST_CODE_BEGIN` / `RVTEST_PASS` / `RVTEST_FAIL` /
  `RVTEST_DATA_BEGIN` 这类 **harness 宏**
- **AND** 其余 env 内容 MUST 与上游同构

### Requirement: 链接布局 MUST 与目标的**地址映射**一致

- **WHEN** 官方镜像被链接
- **THEN** 链接脚本 MUST 与本核的**地址映射单一真相**一致
  ⇒ ⇒ 多 TCM 的核 MUST 把**数据段**放到**数据 TCM**里
- **AND** 判定锚点地址（如 `tohost`）MUST 固定且被门禁引用
- **NOTE**: 上游链接脚本假设**单一平坦内存** ⇒ ⇒ 直接沿用会把 `.data` 放进
  **指令 TCM** ⇒ ⇒ 数据侧读到 **0** ⇒ ⇒ **多个测试失败**（而根因是**地址映射**，
  **不是**指令语义）

#### Scenario: 多 TCM 的核 MUST NOT 沿用单平坦内存的链接脚本

- **WHEN** 目标核有**两个**地址空间（指令侧 / 数据侧各一个）
- **THEN** 链接脚本 MUST 把**指令**放进指令侧、把**数据**（含 `tohost`）放进数据侧
- **AND** 若沿用上游的单一平坦布局 ⇒ ⇒ 数据段的测试 MUST 因为读到 **0** 而失败
  ⇒ ⇒ 该失败 MUST 归因到**地址映射**（MUST NOT 归因到指令语义）

### Requirement: 排除项 MUST **逐条登记**且 MUST NOT 静默缩小

- **WHEN** 某个官方测试被排除
- **THEN** MUST 在仓内**登记**：测试名 + 排除理由
- **AND** 理由 MUST 是**能力对齐**（该能力本核不具备）或**场景不适用**
  ⇒ ⇒ MUST NOT 是「跑不过」
- **AND** 登记 MUST 可被查阅（⇒ ⇒ 门禁的载体文档内）

#### Scenario: 排除项 MUST 可被第三方核对

- **WHEN** 有人核对「本仓跑通了多少官方测试」
- **THEN** MUST 能查到**总数**与**排除清单**（名 + 理由）
- **AND** MUST NOT 存在「未登记的排除」

### Requirement: 官方套件 MUST 也在 **RTL 产物**上被判定（交付物级判据）

- **WHEN** 本仓对外交付 RV32I 核的 **RTL 产物**（Verilog）
- **THEN** 官方 `rv32ui-p-*` 套件 MUST 也在该**产物**上被判定
  ⇒ ⇒ 该判定与**核级**（解释器）判定**并存** ⇒ ⇒ 两者都 MUST 为绿
- **AND** 该判定 MUST 以**产物本身**为被测对象
  ⇒ ⇒ 即「**发射 → lint → 编译 → 运行**」的完整链路
  ⇒ ⇒ MUST NOT 以「同一份 C++ 的另一个执行后端」代替
- **AND** 产物级门禁 MUST **可重复**且 MUST 有 **CI 入口**
  ⇒ ⇒ 形态可为 **CI 脚本**（非 CTest）⇒ ⇒ 但 MUST fail-closed 且 MUST 零静默跳过
- **AND** 产物级门禁 MUST 覆盖**带固件初值**的产物形态
  ⇒ ⇒ 即 `--emit-rtl --firmware <镜像>` 这一形态
  ⇒ ⇒ 该形态 MUST **通过 lint**（零告警零错误）

#### Scenario: codegen 侧缺陷 MUST NOT 只由核级门禁承担

- **WHEN** 一个缺陷只存在于 **codegen / 产物**侧（如初值块宽度、端口映射、时序形状）
- **THEN** 核级（解释器）门禁 MUST NOT 具备发现它的能力
  ⇒ ⇒ 产物级门禁 MUST 在该形态上执行 ⇒ ⇒ 该缺陷 MUST 表现为门禁红

#### Scenario: 产物级门禁 MUST 有负向对照

- **WHEN** 产物级门禁判定「`tohost == 1` ⇒ PASS」
- **THEN** MUST 有**负向对照**（已知 FAIL 值的镜像）⇒ ⇒ MUST 如实读到该 FAIL 值
- **AND** 若读到的值是 `1` ⇒ ⇒ 门禁 MUST **红**（判定被吞 ⇒ 恒绿）

#### Scenario: 拍数上限 MUST NOT 静默截断

- **WHEN** 某测试在产物级门禁的拍数上限内**没有**写 `tohost`
- **THEN** 门禁 MUST **红** ⇒ ⇒ MUST NOT 表现为 skip 或「空洞通过」
- **AND** 门禁 MUST 记录每个测试的**实际拍数** ⇒ ⇒ 余量变化 MUST 可见
