# Design: 公开 API 移除的跨仓可见性

> 推导 / 逐条核算 / 变异清单在本文件（ADR 纪律）。本 change **无 ADR**
> （improvement §Why 4：不需架构裁定；判决规则的**边界**由根 `AGENTS.md` + 两个 capability
> 之外的护栏承载，不是新契约）。

## 1. 载体分工（为什么是「两份」而不是一份）

| 载体 | 粒度 | 谁写 | 谁读 | 判据 |
|---|---|---|---|---|
| 归档件的 `## 用户可见变更` | **每个 change 一行以上** | 该 change 的作者 | 下一个上游作者 / 下游考古 | 护栏 R1（存在 + **非空**） |
| `docs/CHANGELOG.md` | **窗口级摘要** | 收口时回填 | 下游升级前扫一眼 | 护栏 R4（**token 清单**） |
| `.rddf/checks/public-api-surface.md` 的移除表 | **符号级** | 移除者 | 下游迁移 | 护栏 R2（表非空 + ADR 引用存在） |
| `.rddf/checks/public-api-removals.tsv` | **机器探针** | 移除者 | 护栏 | 护栏 R5/R6 |

⇒ ⇒ 四者不是重复：**R1 保「每次都有」**，**R4 保「可扫读」**，**R2 保「可迁移」**，
**R5 保「登记不说谎」**。少任何一层，就有一个已知的「静默穿透」形态：

- 只有 R4 ⇒ 逐 change 的破坏性变更仍可能被漏记（载体是「收口时的记忆」）；
- 只有 R1 ⇒ 下游要读 145 个归档目录才知道影响；
- 只有 R2 ⇒ 「登记了但符号其实还在」（说反了）/「移除了但没登记」都可能发生；
- 只有 R5 ⇒ 表格被删空时探针仍绿（⇒ 故 R6 有**数量锚点** + id↔表 互相绑定）。

## 2. 机械探针：为什么判在「剥注释与字符串之后」

实测（2026-11-09）：

- `include/chlib/state_machine.h:472` 有一条**说明性注释**「renamed from `current_state()`」
  ⇒ 原始文本里 `current_state` 命中 1 次 ⇒ 直接 grep ⇒ **假红**；
- `include/core/bundle/bundle_base.h:547` 的注释记录了 ADR-053 删掉的
  「无约束 `get_lnode(const Derived &)`」⇒ 用正则判「已删除的签名」⇒ **假红**。

⇒ ⇒ 探针 MUST 先剥注释 / 字符串字面量（脚本内 `strip_cxx()`），否则**修复本身会让护栏红**
⇒ 那会训练出「注释里不要写历史」的错误习惯（恰恰相反：注释里写历史是本仓的惯例）。

⚠️ 反向风险：剥注释会不会**掩盖真实回归**？不会 —— 重新引入的符号一定出现在**代码**里。

## 3. 探针的判定形态（四种）

| kind | 语义 | 例子 | 为什么不能合并 |
|---|---|---|---|
| `removed` | 字面不存在 | `is_built`（scope `include/chlib`） | 需 scope 收窄：`Component::is_built()` 仍在 `include/component.h` |
| `renamed` | 旧名**词界**不存在 + 新名存在 | `current_state` → `entry_state` | 旧名是 `current_state_uint` 的**前缀** ⇒ 必须词界判定 |
| `re` | 正则不匹配 | ADR-053 的「无约束入口」签名 | 该「符号」是一条**形状**，没有名字 |
| `nofile` | glob 无匹配文件 | `include/cpu/cache/*_stub.h` | ADR-037 删的是**文件族** |

⇒ ⇒ 四条都是「**具体缺陷形态**驱动」的，不是先有机制再找用例。

## 4. 下游 fixture：为什么必须「安装 + 独立工程 + 运行」

| 检查方式 | 能证明 | 不能证明 |
|---|---|---|
| 源码树 `-fsyntax-only` | 语法 | 安装后的**前缀**可用性 / 链接 / 运行 |
| `cmake --install` 后 `-fsyntax-only` | 前缀可编译 | 链接（`libcpphdl.a`）/ 运行期构造 |
| **独立工程 configure + build + run** | 上面全部 | ——（本 change 采用） |

⚠️ 实测发现（本 change 的副产品，**已登记在 fixture 的 CMakeLists 注释里**）：
安装后的头**不是单目录自足**的 —— `ast/ast_nodes.h` 写 `#include "lnodeimpl.h"`，
而该头装在 `core/` ⇒ ⇒ 下游 MUST 加 **5 个** include 目录（= 上游 `CH_INCLUDE_DIRS`
的口径）。这不是本 change 引入的缺陷，但**此前没有任何地方写下来**。

⇒ ⇒ 夹具因此按**下游真实口径**（5 个 `-I`）配置；若将来有人把安装布局改成自足，
本夹具仍绿（多出的 `-I` 无害），而「自足性」这条改进需要**独立立项**。

## 5. 变异清单（每条都实测）

| # | 变异 | 预期 | 实测 |
|---|---|---|---|
| **M1** | 删掉 `AGENTS.md` 的判决边界句 | R3 红 | ✅ |
| **M2** | 从 `docs/CHANGELOG.md` 抹掉 token `resolve_hold` | R4 红 | ✅ |
| **M3** | 把 `is_built()` **重新引入** `include/chlib/state_machine.h`（真实代码，非注释） | R5 红 | ✅ |
| **M4** | 删掉探针行 `tick_state` | R6 数量锚点红 | ✅ |
| **M5** | 投毒**已安装副本**（可组合形态退回 `noexcept(false)`） | 下游 build 必红 | ✅（= issue #28 的复现） |
| **M6** | 空 fixture 目录 | R1 红（fail-closed，不跳过） | ✅ |

## 6. 已知限制（如实登记）

- 探针只覆盖**可机械判定**的子集：`then()`（泛名）、`state_def::parent`（泛化成员名）不设探针
  ⇒ 那两行靠人读 + 评审 ⇒ 已写进 `.rddf/checks/public-api-removals.tsv` 的头注释；
- 祖父条款清单（`GRANDFATHERED`）是**显式**的：机制生效当日的存量归档不追溯
  ⇒ 双向校验（条目不存在 ⇒ 红）⇒ 防止它变成豁免池；
- 「安装头自足性」与「版本号 / tag 策略」**不在本 change**（improvement §Out of Scope）。
