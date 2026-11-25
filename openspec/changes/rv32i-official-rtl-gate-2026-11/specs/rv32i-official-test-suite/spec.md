# delta · rv32i-official-test-suite

## ADDED Requirements

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

## MODIFIED Requirements

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
