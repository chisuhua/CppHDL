# 任务清单：`component-io-const-and-node-name-policy-2026-11`

> 判据来源：proposal.md §Acceptance · capability delta（2 个 capability，各 ADDED ×1）。
> ⚠️ **与层次化主线无依赖** ⇒ ⇒ 可全程并行。

## T1 U-2：`io() const`

- [ ] 1.1 `__io` 宏加 const 重载（`const io_type& io() const`）
- [ ] 1.2 **编译期断言**（`static_assert` 或等价编译期探针）：const 上下文可读
      `impl()` / `name()` / `is_valid()`
- [ ] 1.3 **判别性对照**：移除 const 重载 ⇒ 断言 MUST **编译失败**

## T2 U-1：opt-in 节点名唯一性

- [ ] 2.1 `context` 加策略枚举（`kAllowDuplicates`（**默认**）/ `kRejectDuplicates`）+ setter
- [ ] 2.2 创建点（`create_input` / `create_output` 等）检测同名 ⇒ **抛异常**
      （MUST NOT 只打日志）⇒ ⇒ 错误信息含**冲突的节点名**
- [ ] 2.3 ctx MUST **不改名**
- [ ] 2.4 **两层**护栏：① 开启 ⇒ 抛 ② 默认 ⇒ 不抛

## T3 零回归

- [ ] 3.1 `ctest -LE perf` / samples / examples 全绿 · 计数锚点同步
- [ ] 3.2 ⚠️ `policy=on` 的判据 MUST 用**专门构造的重名 DUT**
      —— 仓内 `include/` 的 641 个 `ch_in` 声明**零**显式名字 ⇒ ⇒
      拿现有设计当 DUT ⇒ 全仓转红是**规格预期**，不是缺陷

## 验证命令

```bash
cd /workspace/project/CppHDL
cmake --build build -j"$(nproc)"
ctest -LE perf --output-on-failure
bash scripts/run_all_samples.sh
./run_all_ported_tests.sh
openspec validate component-io-const-and-node-name-policy-2026-11 --type change --strict
bash scripts/verify_openspec_delta.sh
```
