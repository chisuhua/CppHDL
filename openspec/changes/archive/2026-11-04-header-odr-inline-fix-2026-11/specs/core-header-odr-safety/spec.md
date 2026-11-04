# core-header-odr-safety

## ADDED Requirements

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
