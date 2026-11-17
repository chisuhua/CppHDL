# 审稿记录 · ADR-074（RTL 验证的单一真相源与生成式 TB）

## review_profile

`arch`（产物路径 = `docs/adr/**` + `docs/architecture/**` ⇒ 架构决策面）

## 轮次

| 轮 | 审查者 | 结论 | BLOCKER | 闭合 |
|---|---|---|---|---|
| 1 | **oracle** | **REJECT** | **6** | ✅ 全闭（v2） |
| 1 | **critic** | **REJECT**（`mergeable: no` · `scope_exceeded: false`） | **4** | ✅ 全闭（v3） |
| 2 | **critic（复审 · 只验闭合）** | **APPROVE_WITH_CONDITIONS**（`mergeable: yes` · `scope_exceeded: false`） | **0** | — |

> ✅ **复审结论**：上一轮 4 条 BLOCKER **逐条判定为已闭合**，未引入新 BLOCKER ⇒ ⇒ 本 ADR 满足「ADR 完成的标志」第 2 条（BLOCKER 全闭 + 1 次复审确认）。
> ⚠️ 复审另提 6 条 SHOULD_FIX + 3 NIT（**不阻断合并**）⇒ ⇒ v4 已处置其中 5 条：① AC 编号对照表已加（§5 导语）② `min_checks` 跨 ADR 口径同步义务已写入 ③ tsv `emitter` 列已并入 M5 ④ `EXPECT` 读侧同需新访问器已补 ⑤ `SET → EXPECT` 序列语义改 **fail-closed**。⑥ Status 行已精简（轮次下沉到本文件）。NIT ①③ 已改（措辞 / `ADR-074 v2.2` 顺序改为在 v2 之后追加）。

## oracle 第 1 轮（6 BLOCKER + 9 SHOULD_FIX）—— 逐条应答

| # | finding | 处置 | 证据 / 落点 |
|---|---|---|---|
| **B1** | 「产品顶层 = 55 端口」错（实为 `AxiLiteTestMaster` 23 + `AxiSocTop` 12） | ✅ **改数** ⇒ 32（20 + 12）；23 条内层端口**显式移出范围** | `grep` 逐 `__io` 块计数 = 23 / 12 · ADR-075 §1.2④⑤ · 蓝图 §2.2⑦/§4.2.2 |
| **B2** | D1「一份 IR」机制断言错（宏展开 + 扫描 = 两个语义实现） | ✅ **改判** ⇒ `.verif` **不参与 C++ 编译** ⇒ 扫描 → IR → 两执行后端 | ADR-074 D1 · 选项表 ④ 拒 / ⑤ 采用 |
| **B3** | DSL 表达式文法未闭合（现有 TB 用循环 / 计算值 ⇒ 37 断言写不出） | ✅ 补**表达式文法**（字面量 / 算术 / 位选 / 括号 / `_i`）+ **禁止项** + 新增 `REPEAT(n)` 原语 | ADR-074 D2 · 蓝图 §4.1.2 |
| **B4** | `WAIT` 论证反了（`done` 粘滞 ⇒ 禁用等沿；现有 TB 用固定拍数） | ✅ **订正** ⇒ `WAIT` 只用于**非粘滞** + 至少推进 1 拍；真实依赖 = `REPEAT` + 表达式求值 | ADR-074 D3 · 蓝图 §4.1.2 |
| **B5** | 身份链「≡」自相矛盾 + 映射无载体（`port_table[]` 无 `cpp_member`） | ✅ 改为**显式映射**（join 键 = `name`）+ **1:1 唯一性 fail-closed**；不新增 schema | ADR-075 E2/E3 · 蓝图 §4.2.3 |
| **B6** | 分阶段顺序 / 逐文件清单不可执行（`AxiSocTop` 未抽取、N7 悬空） | ✅ 重排 6 步（抽取 → 发射收敛 → 具名 → DSL → 生成+护栏 → 文档）+ 补 N1–N13 / M1–M22 | 蓝图 §5–§6 |
| S1 | `RtlProductPort` 无 `cpp_member` | ✅ 已作为 B5 的载体依据 | 事实 ⑦ |
| S2 | `.md` 数值副本仍两处 | ✅ 如实登记为 N3 + 双向 1:1（且明写「只判存在，不判数值」） | ADR-074 D5 |
| S3 | `EXPECT` 用 Catch2 `REQUIRE` 会中断 ⇒ 拿不到全部结论 | ✅ 改**累加 + 收尾汇总** | ADR-074 D2 · 蓝图 §4.1.2 |
| S4 | AC8 应为 check_id **集合相等** | ✅ 已改 | ADR-074 AC8 |
| S5 | `min_checks` 等式才是真抓手 | ✅ 新增（并在 critic 轮进一步定为**运行期**口径） | ADR-074 D4/AC7 |
| S6 | R1 supersede 应改表格行而非内联注释 | ✅ 已改为**改表格行**（撰写 / 渲染两行）+ 点名被取代的句子 | ADR-072 §2.4 D4 |
| S7 | 一文档一决策（D7/E7 是「无变更」声明） | ✅ 移入 §6「边界与不变量」 | ADR-074 §6 · ADR-075 §6 |
| S8 | 变异自证需写注入点与期望签名 | ✅ 已写 | ADR-074 D8 |
| S9 | 期望源端口表在具名化后整体失效 | ✅ 列 M16 + Step 6 | 蓝图 §4.2.4 |

## critic 第 1 轮（4 BLOCKER + 6 SHOULD_FIX）—— 逐条应答

| # | finding | 处置 | 证据 / 落点 |
|---|---|---|---|
| **B1** | IR 缺「驱动/采样锚点」，`SETTLE→#0` 与事实⑩矛盾（posedge 同 region / `#0` 在 inactive region ⇒ 两后端分叉） | ✅ 新增**时序锚点表**（驱动 = 沿后采样窗 · 观测 = 最近一次结算后 · `.sv` 统一 `@(posedge clk); #1;`）+ **明确否决 `#0`** + AC19（判别性构例） | ADR-074 D2 · 蓝图 §4.1.2 · AC19 |
| **B2** | 「DSL 标识符 → C++ 端口对象」绑定载体未定义（`set_input_value` 是类型化的，无反射） | ✅ 决策 = **新增 node_id 级非类型化访问器**，IR 解释器按 `context` 节点名建 `name → node_id` 表（非手写 · **不是**被禁的回调表）+ AC20；⇒ 进一步确认端口具名是**双必需** | ADR-074 D2 · M19 · AC20 |
| **B3** | ADR-073 `:107` 仍写 35（与同文件 `:16` 的 12 自相矛盾） | ✅ 已改 12 并点名 23 条为子 module 端口 | `docs/adr/ADR-073-soc-rtl-product-and-tb.md:107` |
| **B4** | AC1（`examples/` 全域 `toVerilogHierarchical` == 0）不可满足（riscv-mini 测试真实调用） | ✅ **收窄**为「发射入口 `.cpp` 无直接调用」+ 列白名单 | ADR-074 AC1 · 蓝图 §8 AC1 |
| S1 | 「G-期望 3 → 2」与 `tests/*` 仍存在矛盾 | ✅ 如实改：`tests/*` 是**组件级判据（(A) 路径）· 不在本 change 范围** + 登记 R10 | 蓝图 §5.3 · §7 R10 |
| S2 | ADR 说 `.md` 禁生成 ↔ 蓝图说端口映射段由生成器产出 | ✅ 拆开：**期望值/锚点正文**禁生成；**机械端口映射段**可生成 | ADR-074 D5 |
| S3 | `min_checks == EXPECT 条数` 二义（静态 vs 展开后运行期） | ✅ 定为**运行期**口径（= `.sv` 的 `RESULT: checks=N`）+ tsv 表头/`#COLUMNS`/读取三处同批 | ADR-074 D4/AC7 · M20 |
| S4 | 清单缺测试/变异载体与锚点连带面 | ✅ 补 N11–N13 / M19–M22（CMake 注册 / tsv / 锚点） | 蓝图 §5.1–§5.2 |
| S5 | 「两后端」一词与既有 `dual-backend-test-coverage` 冲突 | ✅ 改称**双执行后端**（IR 解释器 / Verilog 渲染器）+ 明写新套件 MUST NOT 按 `DUAL_BACKEND_TEST_PAIR` 注册 | ADR-074 D8 |
| S6 | 双向 1:1 只判存在性，N3 却用它当数值缓解 | ✅ 订正措辞：**数值漂移无机械判据**，靠 process + review + 变异 | ADR-074 D5 |
| N1 | `Draft` vs `Proposed` 状态词不统一 | ✅ 明确转换序 | ADR-074 元数据 |
| N2 | `top_<成员名>` 被当既成事实 | ✅ 改为「由 `port_table[].name` 读取，预期形态 `top_<成员名>`」 | ADR-075 §4.1 P2 |
| N3 | ADR-075 具名示例宽度不符 | ✅ 改 `ch_in<ch_uint<32>> awaddr{"awaddr"};` | ADR-075 §2.1 |
| N4 | M18 漏「examples 的 main() 程序」计数 | ✅ 已补 | 蓝图 M18 |
| N5 | P1「只有一份实现」与 D8「两份后端实现」张力 | ✅ 改为「**语义来源**只有一份；原语求值有两份执行后端实现」 | ADR-074 §4.1 P1 |
| N6 | ADR-072 应点名被取代的句子 | ✅ 已点名「不可自动化」 | ADR-072 §2.4 D4 · v2.2 表 ① |

## 未闭合 / 已知边界（如实登记）

| # | 项 | 处置 |
|---|---|---|
| 1 | **复审未跑** | 按预算不自行认定；由 planner 立项时的 metis 复核（见上「轮次」注） |
| 2 | **数值漂移无机械判据** | 如实声明（ADR-074 D5）；只能靠 process + review + 变异 |
| 3 | **`tests/*` 的期望值载体不动** | 登记 R10（组件级判据，不同层） |
| 4 | **手写 TB 逃生口存在** | D7 明写 + 覆盖度登记（AC12） |
