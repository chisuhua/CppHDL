# ADR-052: `multi_if` 的接受面 —— 五种形态逐一裁决（语义完备性 · 宽度 · 可构造性）

- Status: Accepted（proposed 2026-10-30）
- Date: 2026-10-30
- Author: build-agent（architect role）
- Parent: ADR-051（`chlib` 条件构造的语义模型 —— 守卫贡献 + 权威表面）· ADR-043（死 / 误导设施物理移除 —— 零消费者直接删、无别名期）· ADR-050（`ch_state_machine` 的「唯一写入点」纪律）
- 关联 OpenSpec change：`multi-if-acceptance-surface-2026-10`（实施本 ADR；归档**禁** `--skip-specs`）
- 关联 capability：`chlib-conditional-semantics`（**MODIFIED ×1**：权威表面的接受面条款 + E2 Scenario 订正）
- 证据：`docs/architecture/2026-10-30-n4-n6-evidence.md`（本 ADR 的全部实测数据在此，正文只留决策）

---

## 1. Context

`chlib::multi_if<T>` / `if_then` 是 ADR-051 确立的**唯一**表达式形式条件构造
（`include/chlib/if.h`）。其「权威表面」条款已登记三条接受面限制
（E1 不能直接赋给 `ch_out` / E2 分支宽度 / E3 条件不能是 `ch_in<ch_bool>`）。

⚠️ **E2 那条经实施期实测为假**：它是「权威表面」Requirement 下的 Scenario，`THEN`
写「分支值须同类型 `T` ⇒ **模板推导冲突 ⇒ 无法编译**」⇒ ⇒ 真实行为是
**编译通过、零告警、静默丢位**。

⇒ ⇒ 逐格实测后，`multi_if` 的接受面存在**五种**「框架放行了却无法正确执行」的形态
（证据 §1 / §3）：

| # | 形态 | 实测 | 严重度 |
|---|---|---|---|
| **A** | 首分支窄、后续宽 | 编译通过，**静默丢高位** | 高（数值错） |
| **B** | **单分支**（无 `elif` / `else_`） | 编译通过，**条件被完全忽略**（结果恒 `=x`） | **最高（控制流不生效）** |
| **C** | `T` 推导成 **port 类型** | 编译失败（模板深渊） | 中（可用性） |
| **D** | `T` = **纯算术类型** | ctor 放行，转换处失败（**「接受但不可用」死路**） | 中 |
| **E** | 显式 `T` **过窄** | 编译通过，**静默截断** | 高 |

**形态 B 是「条件被忽略」缺陷家族的第三个实例**（一 = `_if` 双重失效 / ADR-051 §1；
二 = `uart_tx` 的 FSM / ADR-050）⇒ 严重度高于数值丢位（控制流根本不生效）。

**为什么长期无人发现（假绿机制）**：截断在**进入 mux 之前**插入（证据 §1.2：
`add_add(9b) → bits(8b) → mux`）⇒ 两后端**同构** ⇒ parity 对这一类缺陷**结构性失明**；
Verilog 侧 `WIDTHTRUNC` 又被**全局关闭**（`src/codegen_verilog.cpp:299-300`）
⇒ 唯一能「变响」的位置是 **C++ 前端**。

**本决策要的是「可预测性」，不只是「可诊断性」**（三条理由，均有仓内先例）：
① 失败形态是**静默数值损坏** ⇒ 告警级方案是本仓实测过的**假安全网**
（`CHERROR`/`CHFATAL`/`CHREQUIRE` **只打日志不抛异常**，ADR-050 实施期实测）；
② 后端安全网已**正当关闭** ⇒ 契约必须定义「什么合法 / 什么拒绝」；
③ 「结果宽度 = `T`；后续分支只可更窄」**一句话可表述**，且天然携带可诊断性（反之不成立）。

---

## 2. Decision

**总原则**：`multi_if` 的接受面里，每一格 MUST **要么语义正确、要么响亮拒绝**；
**不允许第三态**。以下 D1–D8 是该原则在各格上的落实。

### D1. 结果宽度 = `width(T)`；更宽的分支 ⇒ 编译期拒绝

`T` = 显式模板实参（`if_then<ch_uint<W>>(...)`）；未给则 = **首分支值的类型**
（首分支为 port 时经 **D4 归一化**取 `value_type`）。
后续分支（`elif` / `else_`）**硬件类型**的值宽度 MUST **≤ width(T)**：
- 更窄 ⇒ **零扩展**到 `T`（`ch_uint<M<N>` 的转换天然如此，`include/core/uint.h:110-118`）；
- **更宽 ⇒ 编译期拒绝**（`static_assert`）⇒ 报错消息 MUST 含两个宽度数值与逃生口写法。

⚠️ 显式 `T` 与推导 `T` 适用**完全相同**的规则 ⇒ ⇒ 形态 E（`if_then<ch_uint<1>>(c, a)`
静默截断到 1 位）是**行为变更**，见 §3 后果。

### D2. `ch_literal` 与裸算术字面量走**不同**判据（这个不对称是**有意的**）

⇒ ⇒ 与 Verilog 的 **sized / unsized literal** 同构：`ch_literal_impl<V,W>` 是
**sized**（`W` 是身份的一部分 ⇒ 可静态判），裸算术是 **unsized**（宽度取自上下文 ⇒ 按值判）。

- `ch_literal_impl<V, W>`：`W` 是**编译期**模板参数 ⇒ 参与 D1 的**静态**判据
  （`elif(c, 0x1FF_h)` 进 `T=ch_uint<8>` ⇒ 编译期拒绝）；
- **裸算术字面量**（`elif(c, 1)`）：**不**按类型宽度参与静态判据 ——
  `ch_width_v<int>` = **32** 是**占位语义**（`include/core/traits.h:28-38`）⇒
  裸比较会把 `elif(c, 1)` **误拒**（今天它是合法且常用的写法）；
  ⇒ 改为**建图期值域检查**：值 MUST 可容纳于 `width(T)`，否则 `CHFATAL_EXCEPTION`；
- 负数算术字面量（`elif(c, -1)`）⇒ **拒绝**（现状是 `bv/utils.h:842` 的 `assert` abort +
  零上下文消息）。

⚠️ **签名约束（实施期的硬前置）**：`elif` / `else_` 的形参今天是 `const T &`
（`if.h:43,51`）⇒ ⇒ **实参在进入函数体前就已完成到 `T` 的转换** ⇒ ⇒
原始值 / `W` 信息在函数体内**不可恢复**（宽分支已被截断，截断信息丢失）。
⇒ ⇒ **两套判据 MUST 以模板重载形态在「转换之前」拦截** ⇒ ⇒ 这意味着
`ctor` / `elif` / `else_` 的**签名变更**（`<typename U>` + `if constexpr` 三分派），
而不是「在转换点追加检查」。按后者实现会走进**不可实现**的死路。

### D3. 单分支（无 `elif` 且无 `else_`）⇒ 拒绝

`operator T()` 的循环从 `branches_.rbegin() + 1` 起（`include/chlib/if.h:68`）⇒
**单分支时循环体一次都不执行** ⇒ 条件**在结构上不可能**参与合成（证据 §3.1 的
生成 Verilog 硬证据：无 mux、`cond` 零消费者）⇒ ⇒ 转换点 MUST 拒绝。

⚠️ **不接受「语义化为恒等」**（把 `if_then(c,x)` 定义成 `x`）：表达式形式**没有**
「否则」的语义对象（寄存器有当前值可保持、输出端口有显式默认）⇒ 定义不出 ⇒ 必须拒绝。
⇒ ⇒ 这与 ADR-051 **D3**（输出缺 `else_` ⇒ 构建期拒绝，**不**降级为默认 0）同一精神，
且**更严格**（`multi_if` 连 target 都不知道，无法区分寄存器/输出）。

### D4. `T` 的可构造性：port **归一化**、算术 **拒绝**

**两者失败机制相同**（结果需**两次**用户定义转换 ⇒ C++ 不允许），但**处置相反**：

| 首分支类型 | 现状 | 处置 | 理由 |
|---|---|---|---|
| **port**（`io().x`） | 编译失败（模板深渊） | **归一化** `T` = `port::value_type` | 与 `select` / `to_operand` 的**既有 port 特判一致**（`operators.h:60-64`）⇒ `select(c, io().a, io().b)` 今天**可用**而 `if_then(c, io().a)` 不可用 ⇒ 这是**框架内不一致**；归一化把形态 C 从「报错」变「**可用**」（符合总原则的「语义正确」分支） |
| **纯算术**（`1`） | ctor 放行、转换处失败（**接受但不可用**） | **拒绝**（`static_assert`） | `ch_width_v<int>` = **32** 是**占位语义**（`traits.h:28-38`）⇒ 归一化到 32 位会给**假精度**；且用户意图不明（要 8 位还是 32 位）⇒ MUST 显式 |

⚠️ **判据 MUST 分开表述**（实测）：`HardwareType<T>` **不能**用来拒绝 port —— 它的
定义是「有 `impl()` 且非算术」（`operators.h:40-42`），而 port **有** `impl()`
（`io.h:125/201`）⇒ 实测 `HardwareType<port<ch_uint<8>, input_direction>>` = **true**
⇒ 用它对形态 C **必然放行**。

⇒ ⇒ **port 判据** MUST 用 `requires { typename T::value_type; typename T::direction; }`
（与 `to_operand` 同款）；**算术判据**用 `!HardwareType<T>`（它已排除算术）。
⇒ ⇒ 归一化后 `T` MUST 满足 `HardwareType<T>` ⇒ 否则拒绝，消息指引
`ch_uint<N>(io().x)` / `if_then<ch_uint<N>>(c, 1)`（实测逃生口可用，证据 §5）。

⚠️ **`ch_bool` 端口边界**（实测）：port 只有带约束的 `operator ch_uint<W>()`
（`io.h:232`）+ 无约束的 `operator lnode<T>()`（`io.h:229`）⇒ `port<ch_bool>`
**不能**直接转 `ch_bool`，MUST 经 `lnode<ch_bool>` 取 `impl()` 构造（证据 §6.5）。

### D5. 结果**类型**恒为 `T`（含 `ch_bool` / `ch_uint<1>` 的混用裁决）

`select` 在 `result_width <= 1` 时返回 `ch_bool`（`include/core/operators.h:134-135`）
⇒ ⇒ 若 `T = ch_uint<1>`，构链中途的节点类型会漂成 `ch_bool` ⇒ ⇒
D5 钉住：**`operator T()` 的返回类型恒为 `T`**；宽度判据之外**不加**类型判据
（`ch_bool` 与 `ch_uint<1>` 同宽 ⇒ D1 放行），但结果类型必须收敛回 `T`。

### D6. 以下写法 MUST **保持合法且行为不变**（防过度实现）

把「更宽 ⇒ 拒绝」误写成「不等 ⇒ 拒绝」会打死既有合法用法 ⇒ ⇒ 显式列出：

| 写法 | 期望 |
|---|---|
| `if_then<ch_uint<16>>(c, ch_uint<8>(x))` + 全 8 位分支 | ✅ 零扩展（实测 E-1 正确） |
| `if_then(c, a).else_(b)`（仅两分支，带 `else_`） | ✅ 合法（实测 E-2） |
| `if_then(c, a).elif(c2, b)`（**多分支无 `else_`**，尾部 = 末分支值） | ✅ 合法（ADR-051 已刻意保留；**D3 只拒单分支**） |
| 每分支显式 `ch_uint<N>(...)` 收敛（`conditional_demo` 的既有形态） | ✅ 合法 |
| `elif(c, 1)` 等裸整数字面量 | ✅ 合法（D2 值域检查放行） |
| `if_then(c, io().a)`（首分支 port ⇒ D4 归一化） | ✅ 合法 |

### D7. 契约载体

- capability `chlib-conditional-semantics` 的「权威表面」Requirement ⇒ **MODIFIED**（见下）：
  E2 目前是**该 Requirement 下的一个 Scenario**（`#### Scenario: 表达式形式的分支值
  MUST 同类型`），其 `THEN` 写「⇒ **MUST 失败（无法编译）**」—— 该陈述**已被实测
  证伪**（今天编译通过、静默丢位）⇒ ⇒ MUST **订正**为本 ADR D1–D5 的正面条款
  （⇒ 这是**规范层的假绿**：文档声称的保护并不存在）；
- `include/chlib/if.h` 头注释 ⇒ 一句话契约（**引用** spec，不复制，防漂移）。

### D8. 一次性处理五种形态，不拆

五个形态是**同一根因**（「框架放行了无法正确执行的形态」）⇒ 同一份接受面契约 ⇒
拆开会让判据散落、修复者要拼多篇 ADR 才能理解全貌。⇒ 全部并入本 ADR。

---

## 3. 备选方案（否决）

| 选项 | 结论 | 理由 |
|---|---|---|
| **A** 只补可诊断性（告警 / 文档） | ❌ 否决 | 「只打日志不抛异常」= **假安全网**（仓内实测）；`WIDTHTRUNC` 已正当关闭 ⇒ 告警无第二道网 |
| **B** 容器去模板（`operator ch_uint<W>` 由**使用点**定宽） | ❌ 否决（**四条**） | ① 丢位**搬远**而非消除（可预测 ≠ 可见）② 与 `select` 的 `max(width)` **方向相反** ⇒ 两套宽度哲学 ③ 转换运算符**不参与** `get_lnode` / `operator<<=` 的模板推导 ⇒ `=` 能编译、`<<=` 不能的**接受面分裂** ④ 双宽度权威 |
| **C** 「显式定宽」**或**「全同宽」的对称契约 | ❌ 否决 | 消除顺序敏感，但把最常见用法永久钉在逐分支手工包 `ch_uint<W>(...)` ⇒ 用**便利**换**对称** |
| **D** 只修宽度（A/E），B/C/D 另立 | ❌ 否决 | B 是**最高严重度**（控制流不生效），且是同一根因的另一格 ⇒ 拆分会让「接受面」概念失去完整性（见 D8） |
| **E** 保全局面 `WIDTHTRUNC` 作第二道网 | ❌ 否决（**已实测**） | CH_MEM select-tree 合法触发 ⇒ 打开会让既有设计全报错。⇒ **可消化**：结构性消除后移除（见 §5 Non-Goals，独立 change） |
| **F** 本 ADR 的决策（D1–D8） | ✅ **采纳** | 见 §2 |

---

## 4. 后果 / 代价

### 正面

- 三种**静默**缺陷（A / B / E）全部转为**编译期拒绝** ⇒ 「绿 ≠ 对」的一个大类被消除；
- 「结果宽度 = `T`」一句话可表述、可从代码局部推导 ⇒ **可预测性**落地；
- 形态 B 的修复消除「条件被忽略」家族的第三个实例；规范层的假绿（E2 Scenario 的假
  `THEN`）被订正。

### 负面（**行为变更**，必须公告）

1. **形态 E**（`if_then<ch_uint<1>>(c, a)`）从「静默截断」变「编译期拒绝」；
2. **形态 B**（`if_then(c, x)` 无 `else_`）从「编译通过」变「拒绝」；
3. **形态 D**（`if_then(c, 1)`）从「构造放行」变「ctor 拒绝」（须改
   `if_then<ch_uint<N>>(c, 1)`）；
4. **形态 C** 是**正面**一侧（编译失败 ⇒ **可用**），但接受面变了，仍须公告。
⇒ 前三项在仓内**零消费者**：唯一活消费者 `samples/conditional_demo.cpp:62-68` 用的是
**推导 `T`** 形态（无显式模板实参 ⇒ `T` 由首分支推导为 `ch_uint<8>`，每分支显式
`ch_uint<8>(...)` 收敛 ⇒ 即 D6 的「每分支显式收敛」行 ⇒ **不是** `if_then<ch_uint<9>>`）
⇒ 迁移成本 **0**，但对外部使用者是破坏性变更。

### 风险

- **过度实现风险**（「更宽 ⇒ 拒绝」被误写成「不等 ⇒ 拒绝」）⇒ 会打死 E-1 与
  `conditional_demo` ⇒ 已用 **D6** 的 positive 清单 + V7 钉住；
- **算术字面量判据的复杂度**（静态 `ch_literal` / 建图期裸 int 两套）⇒ 实现者可能只做
  一套 ⇒ 已用 V6 的两条独立判据钉住。

---

## 5. Verification（前瞻 · 每条必须能**转红**）

**机制**：编译期判据 MUST 经新增的 `scripts/verify_multi_if_width.sh` 机械核验 ——
每个 fixture 以 `-fsyntax-only` 编译并 grep 报错文本；**每个「拒绝」fixture MUST 配
中性化对照**（注释对应 `static_assert` 后该 fixture 通过 ⇒ 证明「拒」来自此断言）；
护栏 **fail-closed**（意外通过 / 缺必需关键词 ⇒ exit 1）。⚠️ **禁止**以「本地编过
一次确实报错」作验收证据。

- [ ] **V1** 形态 A 编译期拒绝：`if_then(f, a-b).elif(t, a+b)` 编译失败 + 消息含两个宽度
      与 `if_then<ch_uint<`；中性化对照通过；
- [ ] **V2** 形态 E 编译期拒绝：`if_then<ch_uint<1>>(c, a)`（a 8 位）同 V1 判据；
- [ ] **V3** 形态 B 拒绝：单分支转换 ⇒ `REQUIRE_THROWS_AS`；**变异**「删 `size()==1`
      检查」⇒ 不抛 ⇒ 红；
- [ ] **V4-a** 形态 C **归一化生效**：`if_then(c, io().a)`（首分支是 port）⇒
      **编译通过**且 `static_assert(std::is_same_v<推导出的 T, 其 value_type>)` 成立；
      **变异**：去掉 port 归一化 ⇒ 该 fixture 退回模板深渊 ⇒ 红。
      ⚠️ **判据 MUST 用 port 检测概念**（`requires { typename T::value_type;
      typename T::direction; }`），**MUST NOT** 用 `HardwareType<T>` ——
      实测 `HardwareType<port<…>>` = **true** ⇒ 用它会**放行**形态 C（本 ADR 第 1 轮
      评审的 BLOCKER 正是此处）；
- [ ] **V4-b** 形态 D **可行动拒绝**：`if_then(c, 1)` 编译失败且消息含
      `if_then<ch_uint<N>>` 指引，**不得**是模板深渊；
- [ ] **V4-c** `ch_bool` 端口归一化：`if_then(c, io().flag)`（`flag: ch_in<ch_bool>`）
      ⇒ **编译通过**（经 `lnode<ch_bool>` 兜底，D4 注释）；V4-a/V4-c 的 fixture
      MUST **GCC 与 Clang 各编一次**（requires / `enable_if` 行为可能分叉）；
- [ ] **V5** 窄⇒宽零扩展的**运行期**正确性：`if_then<ch_uint<9>>(c, ch_uint<8>(0xA5))`
      + `else_(ch_uint<4>(0xF))` ⇒ 输出 == 手推值（`0x0A5` / `0x00F`）；**变异** zext→sext
      ⇒ `0x1A5` ⇒ 红；⚠️ 如实登记无判别力的变异：「zext 换恒等存储」**不红**
      （窄值存宽 `T` 天然无损）⇒ 该维度只能靠编译期结构证据；
- [ ] **V6** 算术字面量**两条**判据：`elif(c, 1)` 编译通过且值正确（变异：豁免误写成裸
      `ch_width_v` ⇒ 编译失败 ⇒ 红）+ `elif(c, 256)` 进 `T=8` ⇒ `REQUIRE_THROWS_AS`
      （变异：去值域检查 ⇒ 不抛 ⇒ 红）；
- [ ] **V7** **positive 清单全绿**（D6 的**六种**写法 —— 含**多分支无 `else_`**、首分支
      port 归一化、`T=ch_bool`、`T=ch_uint<1>`(1 位分支)、`ch_literal` 后缀两形态）⇒ 防过度实现；
- [ ] **V8** spec delta 落地：grep 新措辞存在 + `openspec validate`（**钉 1.4.1**）exit 0；
- [ ] **V9** 零回归：`conditional_demo` 自检全绿 · `ctest -LE perf` · samples · ported
      examples 全绿；计数锚点若变 MUST 显式更新并写明算术。

---

## 6. Non-Goals（逐条登记，防蒸发）

| # | 事项 | 处置 |
|---|---|---|
| 1 | `ch_uint<M> → ch_uint<N>`（M>N）的**全局**静默截断 | **独立议题**（用户已裁定；影响面覆盖全仓所有显式窄化写法，须先普查消费者） |
| 2 | 全局 `WIDTHTRUNC` 的移除 | **独立 change**（审计型）。codegen 已有结构性修宽先例（`print_reg` 的 `:743-746`、`print_mux` 的 `:1100-1110`）⇒ 可逐条消化 CH_MEM select-tree 后摘掉 `:299-300` |
| 3 | `operator>` 在 `ch_uint` 上不可用（`if_then(a > b, x)` 直接失败）· 顶层 `ch_in` 在 `describe()` 内作值源得 null 节点（`io.h:451`） | **各自独立议题** |
| 5 | `samples/if_demo.cpp` 归位 + 禁用清单订正 | **独立 change**（实测 5 类错误，清单只记 2 类 ⇒ 已过时） |
| 6 | E1（`multi_if` 不能直接赋给 `ch_out`）与 E3（条件不能是 `ch_in<ch_bool>`） | **不在本 ADR**。E1 的修法是 `chlib` 侧加 `get_lnode(const multi_if<T>&)`（`core/` 零改动）⇒ 与 ADR-053 的次序见该 ADR |
| 7 | `ch_width_impl<Derived>` 的 bundle 特化（无消费者） | 只**登记**（类模板特化的删除判据不同 ⇒ 见 ADR-053 Non-Goals） |
