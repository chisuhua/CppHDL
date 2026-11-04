# 任务清单：`hierarchical-rtl-granularity-declaration-2026-11`

> 判据来源：proposal.md §Acceptance · capability delta（ownership ADDED ×1 · hierarchical ADDED ×2）。
> ⚠️ **前置**：**T3-①a**（类级宏与编译期可见名）· **T3-①b**（组件树正确 ⇒ 装配点可判定）。

## T1 两个新 API 名（`include/module.h`）

- [ ] 1.1 `ch_layer<T>`：显式成层；**T 已声明成层 ⇒ 继承其类级名**；**T 未声明 ⇒ 实例处 MUST 给名**
- [ ] 1.2 `ch_inline<T>`：显式 inline
- [ ] 1.3 两者 MUST **复用** `ch_module` 的机制（内部薄封装）⇒ ⇒ 零破坏
- [ ] 1.4 `ch_module` 签名**零变更**

## T2 运行期判定（`include/component.h`）

- [ ] 2.1 `Component::is_layer()`：类级宏 / 实例级覆盖 / `Granularity` 三者合成
- [ ] 2.2 `Granularity` 枚举：`RespectDeclarations` / `ForceInline` / `ForceLayer`
- [ ] 2.3 `PortPolicy` / `UndrivenPolicy` 两个线性枚举 + **默认值**（§10.6-C-㉓）

## T3 四档来源（`include/module.h` + `include/component.h`）

- [ ] 3.1 ② `ch_layer<T>` 继承 `T` 的类级名（MUST NOT 另给）
- [ ] 3.2 ③ `ch_layer<Helper>` 实例处给名 ⇒ 缺名 **编译期**错误
      （⇒ 依赖 ①a 的**编译期可见**名字）
- [ ] 3.3 ④ `ForceLayer` 回退 `sanitize(typeid)` + **`CHWARN`**

## T4 判据

- [ ] 4.1 类级 × 实例级**三组合**的成层结果与声明一致
- [ ] 4.2 **无 active parent** 处装配仍能成层
- [ ] 4.3 四档来源逐档可判定 + **编译期 fixture**（缺名 ⇒ 编译失败）
- [ ] 4.4 `ForceInline` / `ForceLayer` 的全局强制生效
- [ ] 4.5 **零回归**：`ctest -LE perf` / samples / examples 全绿 · 计数锚点同步

## 验证命令

```bash
cd /workspace/project/CppHDL
cmake --build build -j"$(nproc)"
ctest -LE perf --output-on-failure
bash scripts/run_all_samples.sh
./run_all_ported_tests.sh
openspec validate hierarchical-rtl-granularity-declaration-2026-11 --type change --strict
bash scripts/verify_openspec_delta.sh
```
