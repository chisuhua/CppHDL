# core-header-odr-safety Specification

## Purpose
定义**公共头**的 **ODR 安全性**契约：聚合头 `include/ch.hpp` 的闭包内，
任何**自由函数**的定义 MUST 满足 ODR（加 `inline`，或不在头内定义）⇒ ⇒
**多个 TU 同时包含该头 MUST 链接成功**。

**为什么需要**（实测 · 2026-11-04）：`include/module.h` 的 `build_hierarchical_name`
曾是**非 inline 的自由函数定义**，而该头经 `include/ch.hpp:10` 进入**唯一**聚合入口
⇒ ⇒ 每个包含它的 TU 都发出一份**外部定义** ⇒ ⇒ 任何 2+ TU 的项目链接期报
`multiple definition of ch::build_hierarchical_name`（退出码 2）。

⚠️ **上游此前只是「侥幸」，不是「没问题」**：① `src/**` 的 **0 个** TU 包含 `ch.hpp`
⇒ ⇒ `libcpphdl.a` 里 `nm -C | grep "T ch::build_hierarchical_name"` = **0** 条；
② Catch2 测试是**一个源文件一个二进制**（`add_catch_test` per source file）
⇒ ⇒ 每个二进制**只有 1 个** TU 包含 `ch.hpp`。
⇒ ⇒ 条件一出现「multi-source target」（下游 ChipForge 的 `ip/cpu/core/` 就是这种形态）
就炸 ⇒ ⇒ 该缺陷对**下游**是**链接期硬错**，不是运行期问题。

⇒ ⇒ **判据 MUST 是链接期，不是文本扫描**：本仓已两次证明「自研 C++ 子集分析器
不可能被完全闭合」（`verify_example_assertion_quality.sh` ·
`verify_no_duplicate_rv32i_headers.sh`）⇒ ⇒ 链接器执行的是**真**判据
⇒ ⇒ 它覆盖**整个 `ch.hpp` 闭包**，而不止某一个函数 ⇒ ⇒ 将来任何人往头里再加一个
非 inline 自由函数，**同一条判据**就会红。

⚠️ **边界**：本 capability 只管**链接期**的可包含性 ⇒ ⇒ **不**管符号可见性、
ABI 稳定性、模板实例化策略 ⇒ ⇒ **不**替代「公共头自包含性」判据
（`scripts/verify_header_self_contained.sh`，另一条独立义务）。

## Requirements

### Requirement: 公共头中的自由函数 MUST 可被多个 TU 安全包含

经聚合头（`include/ch.hpp`）对外提供的**自由函数**，其定义 MUST 满足 ODR：
要么加 `inline`，要么不在头内定义。

⇒ ⇒ 判据 MUST 是**链接期**判据（同一声明被 2+ TU 包含后**链接成功**），
**MUST NOT** 依赖「仓内暂无 multi-source target」这类未复算的现状豁免。

#### Scenario: 两个 TU 同时包含聚合头 MUST 链接成功

- **WHEN** 两个独立的 TU 各自 `#include "ch.hpp"`，并一起链接
- **THEN** 链接 MUST 成功（退出码 0）
- **AND** MUST NOT 出现 `multiple definition`

#### Scenario: 新增一个非 inline 自由函数 MUST 被链接判据抓到

- **WHEN** 有人往聚合头的闭包内**新增**一个非 `inline` 的自由函数定义
- **THEN** 同一条链接判据 MUST **转红**（⇒ 无需为该函数单独写判据）
- **AND** 失败原因 MUST 可识别为 `multiple definition`
      （⇒ MUST NOT 与其他编译错误混淆 ⇒ ⇒ 否则「编译失败」会被误读成「判据生效」）

#### Scenario: 判据自身 MUST 有判别力

- **WHEN** 把 `build_hierarchical_name` 的 `inline` **去掉**（原地投毒）
- **THEN** 该判据 MUST **转红**
- **AND** 还原后 MUST 复绿（⇒ 证明红的来源是这一个词，而不是环境或缓存）

#### Scenario: fixture 缺失 MUST 被单独报告

- **WHEN** 链接 fixture 的源文件不存在
- **THEN** 判据 MUST 报「缺 fixture」
- **AND** MUST NOT 报成「链接失败」（⇒ 本仓已踩过：fixture 缺失被当成编译失败
      ⇒ ⇒ 两条判据同时假绿）
