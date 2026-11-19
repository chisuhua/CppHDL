# hierarchical-port-naming-guard Specification

## Purpose
定义「端口命名契约**必须**有静态护栏看守」这条**流程契约**：★ 护栏的存在性 ·
★ 判据的两面性（既禁「根有合成端口」，也要求「语料里确有被判对象」）· ★ 根的**结构**
判定 · ★ fail-closed 与判据面（★ 只判端口声明）。

⚠️ **命名规则本身**归 `rtl-hierarchical-output`（★ 规范所有者）⇒ ⇒ 本 capability
**MUST NOT** 复述该规则 ⇒ ⇒ 两份规范并存必然漂移。

## Requirements

### Requirement: 端口命名契约 MUST 有静态护栏核验

层次化产物的端口命名契约 MUST 由**静态护栏**核验 ⇒ ★ 护栏 MUST 存在于仓内脚本
（★ `scripts/` 下）⇒ ★ 且 MUST 可**独立运行**（★ 不依赖人工触发）。

⚠️ 护栏 MUST 只承担**机制义务**（★ 存在性 / 判据面 / 失败语义）⇒ ⇒ 命名规则本身
MUST 由 `rtl-hierarchical-output` 规范所有 ⇒ ⇒ 本 capability MUST NOT 复述该规则。

#### Scenario: 护栏缺失即违反

- **WHEN** 仓内不存在核验端口命名契约的静态护栏
- **THEN** 本契约 MUST 判定为**未满足** ⇒ ⇒ 「命名规则已写进 spec」**不构成**豁免

### Requirement: 护栏 MUST 同时核验「禁止」与「确有被判对象」

护栏 MUST 同时具备**两面**判据 ⇒ ★ ① 根 module 的端口 MUST NOT 是**框架合成端口**
（★ 形如 `__if_` / `__x_` / `__up_` 加序号）⇒ ★ ② 语料 MUST **确实**包含可被判的
合成端口（★ 三种前缀各至少一个真实产物覆盖）。

⇒ ⇒ 缺任一面都可能是**恒绿** ⇒ ⇒ ② MUST 是**对语料的存在量词**（★ 而非「每个
module 至少一处」）⇒ ⇒ 否则不含合成端口的合法产物会被误报。

#### Scenario: 只实现禁止面 ⇒ 护栏恒绿

- **WHEN** 护栏只实现①，且语料里没有任何合成端口
- **THEN** ① MUST 恒为「通过」⇒ ⇒ 该实现 MUST 被判定为**不满足**本契约

### Requirement: 护栏 MUST 由结构判定根 module

护栏 MUST 由**结构**判定哪个 module 是根 ⇒ ★ 根 = **声明集减去被实例化集**⇒ ★
MUST NOT 硬编码任何 module 名或**文件名**。

⇒ ⇒ 「零个 CELL 的单 module 产物」MUST 被判为**合法绿**⇒ ⇒ 「多棵独立树」与
「根被实例化」MUST 与其它失败**文本可区分**⇒ ⇒ 否则护栏退化成「不是某个名字就红」。

⚠️ 结构判定使「配置根名」**不必要** ⇒ ⇒ 本契约 **MUST NOT** 要求提供根名配置项。

#### Scenario: 根名可配置

- **WHEN** 产物的根 module 名被配置成非默认名
- **THEN** 护栏 MUST 仍正确识别该根 ⇒ ⇒ 硬编码根名的实现 MUST 在此转红

### Requirement: 护栏 MUST fail-closed 且 MUST 只判定端口声明

护栏 MUST 在**工具或产物缺失**时**明确报错并以非零退出** ⇒ ★ MUST NOT 报成「判据
通过」⇒ ★ 且各缺失类 MUST **文本可区分**。

⇒ ⇒ 护栏 MUST 只判定 module 的**端口声明**（★ 真端口）⇒ ⇒ MUST NOT 扫描**体内
线网**或**实例端口映射**（★ 二者合法地含合成名）⇒ ⇒ MUST NOT 自研文本解析器
（★ MUST 复用结构化提取）。

#### Scenario: 提取器缺失

- **WHEN** 结构化提取器不可用
- **THEN** 护栏 MUST 以**非零**退出并明确报告「缺提取器」⇒ ⇒ MUST NOT 报告为通过

### Requirement: 端口名的 C++ 成员可解析性 MUST 由 1:1 join 静态核验

护栏 MUST 以**静态**方式核验「产品端口的 `cpp_member` 可解析」⇒ ⇒ 判据 =
manifest 的 `port_table[].name` 与 `ports[].cpp_member` 之间建立 **1:1 唯一映射**
（join 键 = `name`）。

⇒ ⇒ 判定域 MUST 限于 `role ∉ {clock, reset}` 且 `is_top_io == true` 的端口
（A 类基础设施端口 MUST NOT 被要求提供 `cpp_member` ⇒ ⇒ 与
`rtl-hierarchical-output` 的三类划分一致 ⇒ ⇒ 本条 **MUST NOT** 复述三类定义）。

⇒ ⇒ 下列任一情形 MUST 以**非零退出**报错，且 MUST **点名**违规端口：
① `port_table[]` 的元素在 `ports[]` 中**无匹配** ② 匹配**多于一个**（`name` 不唯一）
③ 匹配到的 `cpp_member` 是占位值（`unnamed_input` / `unnamed_output`）。

⇒ ⇒ ⚠️ 护栏 MUST NOT 用字符串拼接推导端口名（形如 `"top_" + member`）⇒ ⇒ 根 module
名是可配置的 ⇒ ⇒ 端口名 MUST 从 `port_table[].name` **读取**。

#### Scenario: 缺映射 / 多义 / 占位值各自转红且文本可区分

- **WHEN** manifest 里某产品端口的 `name` 在 `ports[]` 中无匹配
- **THEN** 护栏 MUST 以非零退出并报告「无匹配」并点名该端口
- **AND** 当匹配多于一个时 MUST 报告「多义」（与「无匹配」文本可区分）
- **AND** 当 `cpp_member` 为占位值时 MUST 报告「未具名」（与上两者文本可区分）

#### Scenario: 用拼接实现即违反

- **WHEN** 护栏以 `"top_" + cpp_member` 生成期望端口名并与产物比对
- **THEN** 该实现 MUST 被判定为**不满足**本条 ⇒ ⇒ 当根 module 名配置为非 `top` 时它会静默错判
