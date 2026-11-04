# Proposal: 公共头 ODR 安全（`module.h` 的自由函数补 `inline`）

## Why

`include/module.h:13` 定义了一个**非 inline** 的自由函数：

```cpp
std::string build_hierarchical_name(Component* parent, const std::string& local_name) { ... }
```

该头经 `include/ch.hpp:10` 进入**库的唯一聚合入口** ⇒ ⇒ 任何**两个** TU 同时
`#include "ch.hpp"` 的项目 ⇒ ⇒ 链接期 **multiple definition**。

### 实测复现（可复算）

```bash
cd /workspace/project/CppHDL
INC="-I include -I include/core -I include/ast -I include/abstract -I include/utils -I external/inipp"
printf '#include "ch.hpp"\nint a(){return 1;}\n'                     > /tmp/a.cpp
printf '#include "ch.hpp"\nint a(); int main(){return a()==1?0:1;}\n' > /tmp/b.cpp
g++ -std=c++23 $INC -c /tmp/a.cpp -o /tmp/a.o
g++ -std=c++23 $INC -c /tmp/b.cpp -o /tmp/b.o
g++ /tmp/a.o /tmp/b.o -o /tmp/ab
# ⇒ /usr/bin/ld: /tmp/b.o: ... multiple definition of
#   `ch::build_hierarchical_name(ch::Component*, std::string const&)'
#   ⇒ 链接退出码 2
```

### 为什么至今没暴露（不是「没问题」，是「侥幸」）

- `include/module.h` **只被** `include/ch.hpp` 包含（全仓唯一入口）；
- `src/**` 的 **0 个** TU 包含 `ch.hpp` 或 `module.h` ⇒ ⇒ `build/libcpphdl.a` 里
  `nm -C | grep "T ch::build_hierarchical_name"` = **0** 条；
- Catch2 测试是**一个源文件一个二进制**（`add_catch_test` 每文件一个可执行）⇒ ⇒
  每个二进制**只有 1 个** TU 包含 `ch.hpp`。

⇒ ⇒ **条件一出现「multi-source target」就炸**（下游 ChipForge 的 `ip/cpu/core/` 就是这种形态）。

## What Changes

1. `include/module.h:13` 的 `build_hierarchical_name` 加 **`inline`**（1 个词）。
2. 新增 **链接 fixture**：`tests/fixtures/header_odr_link/`（两个 TU，均只 `#include "ch.hpp"`）
   ⇒ ⇒ 判据 = 「两 TU **编译**成功 **且链接**成功」。
3. 新增护栏 `scripts/verify_header_odr.sh`：R0 fixture 存在性（fail-closed）· R1 链接成功 ·
   R2 `--self-test` 原地投毒（把 `inline` 去掉 ⇒ **链接必须失败且失败文本含
   `multiple definition`**）。

> ⚠️ **护栏是链接 fixture，不是文本扫描**：本仓已两次证明「自研 C++ 子集分析器不可能被
> 完全闭合」（`verify_example_assertion_quality.sh` · `verify_no_duplicate_rv32i_headers.sh`）。
> 链接期判据**由编译器自己执行** ⇒ 覆盖**整个 `ch.hpp` 闭包**，不止这一个函数
> ⇒ ⇒ 将来任何人往头里再加一个非 inline 自由函数，**同一条判据就会红**。

## 不做（Non-Goals）

- **不做**「扫描全部公共头找出所有非 inline 自由函数」的普查式护栏 —— 见上，文本扫描不可闭合；
  链接 fixture 已经用**编译器**覆盖了同一件事。
- **不动** `ch.hpp` 的 include 结构（它在 29 个文件的闭包里 ⇒ 重排会引出无关的编译期差异）。
- **不改** `build_hierarchical_name` 的**语义**（它仍是「父路径 + `.` + 局部名」，
  `parent == nullptr` / 空名 / `"unnamed"` 三条既有分支一条不动）。

## 验收标准

- [ ] **R1（链接）**：`tests/fixtures/header_odr_link/` 的两个 TU 各自编译成功，**链接成功**
      （退出码 0）⇒ ⇒ 修复前**必失败**，且失败文本含 `multiple definition`
- [ ] **R2（判别力）**：`scripts/verify_header_odr.sh --self-test` 原地去掉 `inline`
      ⇒ ⇒ R1 **必转红**且失败原因**是** `multiple definition`（MUST NOT 是「编译失败」）
      ⇒ ⇒ 还原后 R1 复绿
- [ ] **R0（fail-closed）**：fixture 文件**缺失**时 MUST 报「缺 fixture」，
      **MUST NOT** 报成「链接失败」
- [ ] **零语义变更**：`build_hierarchical_name` 的三条既有分支逐字不变；
      `ctest -LE perf` 全绿（计数锚点不变 —— 本 change 不新增 ctest 条目）
- [ ] capability `core-header-odr-safety` delta 齐备（**归档禁 `--skip-specs`**）

## Impact

| 面 | 影响 |
|---|---|
| 公共 API | **零变更**（`inline` 不改变签名 / 语义 / 可见性） |
| 生成产物 | **零变更**（不涉及 Verilog 文本） |
| 链接行为 | 从「多 TU 必炸」变为「多 TU 正常」 ⇒ ⇒ **只放宽，不收紧** |
| 编译期 | 每个包含 `ch.hpp` 的 TU 少一次外部定义 ⇒ 略有收益；无新增依赖 |
| 回归面 | 仅「链接 fixture + 两条既有分支」⇒ 不需要跑双后端 / 产物比对 |
