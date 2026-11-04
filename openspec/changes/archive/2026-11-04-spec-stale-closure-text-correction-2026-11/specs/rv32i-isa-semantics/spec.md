# Capability: rv32i-isa-semantics

## MODIFIED Requirements

### Requirement: `DataTCM.strbe` MUST 被其实例化点显式驱动

- **WHEN** 一个 `DataTCM` 实例被构造
- **THEN** 它的 `strbe` 输入端口 MUST 被**显式驱动**
- **AND** MUST NOT 依赖「未驱动端口的默认读值」

#### Scenario: 未驱动端口的读值是未定义的

- **WHEN** 一个输入端口**没有任何驱动**
- **THEN** 它的读值 MUST NOT 被当作「全 0」或「全 1」的契约
- **AND** 依赖该默认值的实现 MUST 被视为**隐患**（实测：未驱动的输入端口
  读到的是**全 1** ⇒ 恰好等价于「全车道写」⇒ 整字写用例会**碰巧**通过）
- **AND** interpreter 与 verilator 两个后端的未驱动初值 MUST NOT 被假定相同

#### Scenario: 整字写与字节写走不同的车道来源

- **WHEN** 一个 fixture 同时存在「测试台直接写」与「流水线写」两条写路径
- **THEN** `strbe` MUST **随写路径切换**：直接写 ⇒ 全车道（`0xF`）；
      流水线写 ⇒ 流水线算出的车道掩码
- **AND** 恒驱动全车道 MUST NOT 被当作「整字写」的实现方式
      （那会让流水线写的 `sb` / `sh` 变成「**lane 对齐数据的整字覆盖**」）

> ⚠️ **范围限定（2026-10-26 critic SHOULD_FIX #3 · **2026-11-04 状态订正**）**：
> 本 Requirement 只约束**本 capability 引入的 `DataTCM.strbe`**
> （全仓 **4** 个实例化点 —— 3 处 `ch_module` / `ch_device` **构造**
> + 1 处 `using` **类型别名**（`tests/test_rv32i_timing_dual.cpp`；该别名
> 被当 DUT 构造 ⇒ ⇒ 计入）⇒ ⇒ ⚠️ **裸 grep `DataTCM<` 会命中 12 处**
> （含注释与类型声明）⇒ ⇒ ⇒ ⇒ **不可作判据** ⇒ ⇒ ⇒ ⇒ 判据 MUST 按
> 「**构造形态** + **类型别名形态**」两式匹配 ⇒ ⇒ ⇒ ⇒ 命中恰为 **4** 处）。
> 「**任意**新增输入端口 ⇒ 全实例化点 MUST 显式驱动」是**框架层约定**
> （`AGENTS.md §ANTI-PATTERNS`）⇒ ⇒ 其核验所需的「必需端口注册表」
> **已于 2026-11-03 落地**（`.rddf/checks/input-port-required-registry.md`
> ⇒ ⇒ 双向校验 `scripts/verify_input_port_required_registry.sh`（R1–R6）
> ⇒ ⇒ ⇒ 该表登记的 `DataTCM.strbe` 一行即由它承载；另配**事件驱动**的
> 「新增端口增量检查」`scripts/verify_input_port_additions.sh`）
> ⇒ ⇒ ⇒ ⇒ **仍不在本 capability 内**（本 capability 只约束 `DataTCM.strbe`
> 这一条）⇒ ⇒ ⇒ ⇒ ⇒ ⚠️ **登记表不是全仓普查**（只覆盖两类**已实测**风险：
> 永久复位 `rst_n` / 写入放大 `strbe`）⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ 未登记的端口类别
> **仍无护栏** ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **义务本体仍归「人工核对 +
> code review」**（根 `AGENTS.md` 已如此登记）。
