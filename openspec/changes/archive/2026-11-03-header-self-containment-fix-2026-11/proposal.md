<!-- touched-components: priority_encoder -->

# 提案：头文件自包含性 —— 修 4 个公共头的编译失败 + 新增机械护栏

- **主题**：根 AGENTS.md CONVENTIONS「**Headers self-contained**」的**首次机械核验**
- **关联 capability**：**无**（既有约定，非新契约）
- **行为变更**：**无**（include 路径 / 注释 / 删除**零消费者**的死常量）
- **归档**：`--skip-specs`

## Why

根 AGENTS.md 早就写着「Headers self-contained」，但这条约定**从未被机械核验** ⇒ ⇒
于是「靠 `-I` 巧合才能编译」的头**长期存在**。2026-11-03 的**全量实测**
（162 个头逐个单独编译）查出 **4 个公共头**编译失败，其中一个是**仓内文档记载的
公共入口**：

| 头 | 症状 | 为什么长期没人发现 |
|---|---|---|
| `include/chlib.h` | **单独编译失败** | ⚠️ **仓内零消费者** include 它 ⇒ ⇒ 「公共入口可用」这件事**从未被验证过** |
| `include/cpu/pipeline/stages/id_stage.h` | `fatal error: rv32i_decoder.h: No such file` | 靠 `-I include/cpu/pipeline` 才找得到 |
| `include/cpu/pipeline/stages/if_stage.h` | `fatal error: rv32i_tcm.h: No such file` | 同上 |
| `include/cpu/pipeline/rv32i_pipeline.h` | `fatal error: rv32i_tcm.h`（级联） | 同上 |
| `include/cpu/pipeline/stages/ex_stage.h` | `error: 'OP_OPIMM' was not declared` | 用 decoder 常量却**没** include decoder 头 |
| `include/cpu/cache/{i,d}_cache_async.h` | `error: 'ICacheConfig'/'DCacheConfig' was not declared` | 用 Config 却**没** include `*_sync.h` |

### 🔴 `chlib.h` 的失败根因（**最隐蔽的一个**）

```
include/bundle/stream_bundle_member_inlines.h:12:19: error:
  'stream_m2s_pipe' is not a member of 'ch::chlib'; did you mean 'chlib::stream_m2s_pipe'?
```

⇒ 本仓有**两个** `chlib` 命名空间：顶层 `::chlib`（`include/chlib/*.h` 里**绝大多数**
组件实际所在）与 `ch::chlib`（`chlib/simulator_trace.h` + `cpu/riscv/clint.h`）⇒ ⇒
该文件位于 `namespace ch` **内部** ⇒ ⇒ 写 `chlib::X` 时名字查找**先在 `ch` 里找**
⇒ ⇒ 命中 `ch::chlib` ⇒ ⇒ **遮蔽**了顶层 `::chlib` ⇒ ⇒ 报错。

⇒ ⇒ 且这是**包含顺序相关**的：`chlib.h` 先 include 了声明 `ch::chlib` 的
`chlib/simulator_trace.h`（第 38 行）⇒ ⇒ 之后第 62 行的
`bundle/stream_bundle_member_inlines.h` 就炸了。

⇒ ⇒ **修法**：`chlib::` ⇒ `::chlib::`（显式全局限定）⇒ ⇒ 与 include 顺序**无关**
⇒ ⇒ **零行为变更**（那 4 个函数本来就在 `::chlib`）。

⚠️ **命名空间不一致本身**（文档说 `ch::chlib` · 实现是 `::chlib`）是**另一个决策**
（改命名空间 = 公开 API 变更 ⇒ 需 ADR）⇒ ⇒ 本 change **只修编译失败**。

## What Changes

| 文件 | 改动 |
|---|---|
| `include/bundle/stream_bundle_member_inlines.h` | `chlib::` → `::chlib::`（4 处）+ 说明「为什么必须显式限定」 |
| `include/cpu/pipeline/stages/id_stage.h` | include 路径改 `include/` 相对 |
| `include/cpu/pipeline/stages/if_stage.h` | include 路径改 `include/` 相对 |
| `include/cpu/pipeline/rv32i_pipeline.h` | 7 个 include 路径改 `include/` 相对 |
| `include/cpu/pipeline/stages/ex_stage.h` | ① 补 `rv32i_decoder.h`（用 `OP_*` 常量）② **删** `BRANCH_*` 死常量块（与实现相反且零消费者）③ `branch_type` 端口注释订正（编码 = funct3） |
| `include/cpu/cache/{i,d}_cache_async.h` | 补 `*_sync.h`（用 `I/DCacheConfig`） |
| `include/chlib/combinational.h` | `priority_encoder` 注释订正（**高位优先**，原文写「优先处理低位」⇒ 与实现相反） |
| `scripts/verify_header_self_contained.sh`（**新**） | 护栏：每个头 MUST 能单独编译 |
| `scripts/lib/header_self_contained.py`（**新**） | 分析器（R0 fail-closed · R1 · R2a/b/c） |
| `scripts/lib/header_self_contained_exempt.txt`（**新**） | 分片头豁免名单（**纯数据** · 双向核对） |
| `.github/workflows/ci.yml` | 护栏挂 CI **2 处**（主路径 + self-test） |

## 护栏设计（**为什么它不会变成「万能借口」**）

| 判据 | 内容 |
|---|---|
| **R0** | 编译器 / 豁免名单缺失 ⇒ ⇒ **红**（fail-closed） |
| **R1** | 未豁免的头 MUST 能单独编译 |
| **R2a** | 豁免 MUST **仍必要**（现在能编译了 ⇒ ⇒ 红）⇒ ⇒ 豁免不能变成万能借口 |
| **R2b** | 豁免 MUST 与实际失败集合**完全一致** |
| **R2c** | 豁免里的头 MUST 真实存在 |

⚠️ **豁免有两种 kind**：
- `fragment`（分片头 · 设计意图）⇒ **无条件豁免**（14 条）；
- `stdlib:<能力>` ⇒ **仅当「能力探测」失败时**豁免 ⇒ ⇒ 例如
  `core/verilator_backend.h` 用 `std::expected`（C++23）⇒ ⇒ **libc++ 不提供**
  ⇒ ⇒ 在 clang++ 上豁免、在 g++ 上**不豁免** ⇒ ⇒ **换编译器时豁免自动失效**
  ⇒ ⇒ 「能力」而不是「编译器名字」成为判据 ⇒ ⇒ 可移植。

## 护栏的判别力（self-test）

**3 种原地投毒全部被抓**（且还原后复绿）：

| 投毒 | 转红的判据 |
|---|---|
| `break-include`（把 `chlib/fifo.h` 的一个 include 路径写错） | R1 |
| `drop-exemption`（从豁免名单删一条） | R2a |
| `add-exemption`（给一个能自包含的头加豁免） | R2a |

⚠️ **投毒方式的一处修正（实测）**：首版「**删掉**一个 include」**恒绿** ⇒ ⇒ 原因：
该符号可能被**传递性** include 补上 ⇒ ⇒ 头**仍然**能编译 ⇒ ⇒ 判别力为零 ⇒ ⇒
改成「把 include 路径**写错**」⇒ ⇒ `fatal error` ⇒ ⇒ 确定性失败。

## 顺带挖出的第二个真缺陷（**独立立项**）

护栏用**两个**编译器各跑一遍 ⇒ ⇒ **只有 clang++ 那半边红** ⇒ ⇒ 定位到
**标准库可用性**而非「缺 include」：

> `include/core/verilator_backend.h` 用 `std::expected<void, VerilatorError>`
> （C++23）⇒ ⇒ **libc++ 直到 LLVM 19 才提供** ⇒ ⇒ **AppleClang 15（macOS 14）无**
> ⇒ ⇒ **macOS 上开 `BUILD_VERILATOR=ON` 会构建失败** ⇒ ⇒ macOS 被事实上排除在
> verilator 后端之外（与 `feat-platform-matrix` 的目标直接冲突）。
>
> ⇒ ⇒ **独立立项**：`.rddf/improvements/stdlib-expected-missing-in-libcxx-2026-11.md`
> ⇒ ⇒ 本 change **只按能力绕开**，**不**改公开 API。

## Verification

- [x] 全量重建 **0 error**
- [x] `ctest -LE perf` **230/230** 全绿
- [x] 新护栏 **g++ / clang++ 双编译器**全绿 + self-test **3/3**
- [x] 既有**全部**护栏 exit 0（含 `verify_no_duplicate_rv32i_headers.sh` 的
      C5 行数锚点 ⇒ 已同步 `include/cpu/AGENTS.md` 6464 → 6506）
- [x] **无 spec 变更** ⇒ 归档用 `--skip-specs`
