# 任务清单：`hierarchical-rtl-emission-2026-11`

> 判据来源：proposal.md §Acceptance · capability delta（hierarchical ADDED ×7）。
> ⚠️ **前置**：**T3-②**（分析层）· **T3-③**（emitter 拆分）。
> ⚠️ **本 change 的判据面最大** ⇒ ⇒ 建议**逐条**落地，每条都要有判别性对照。

## T0 批次 B1（策略枚举收敛 · 零行为变化 · ✅ 2026-11-05）

> 判据来源：ADR-060 §2.4（`PortPolicy` / `UndrivenPolicy` 的**唯一定义**在 `include/component.h`）
> + ADR-061 v1.6.3 §2.5.1（粒度的**唯一入口** = RAII `Component::GranularityScope`）。

- [x] 0.1 删 `HierarchyPortPolicy` / `HierarchyUndrivenPolicy`（`include/hierarchy_verilog.h`）
      ⇒ ⇒ `HierarchyEmitOptions` 改用 `ch::PortPolicy` / `ch::UndrivenPolicy`
      ⇒ ⇒ **唯一权威** = `include/component.h`（ADR-060 §2.4）
- [x] 0.2 删 `Component::set_granularity()`（声明 + 定义）
      ⇒ ⇒ 实测**零调用点** ⇒ ⇒ 绕过 RAII 的永久泄漏通道 ⇒ ⇒ 粒度只能经作用域设置
- [x] 0.3 `HierarchyEmitOptions`：`top_module_name` 已有（默认 `top`）·
      **无** `granularity` 字段（全局权威）⇒ ⇒ 与 ADR-061 v1.6.3 §2.5 一致
- [x] 0.4 ⚠️ **C1 未答 ⇒「只加不删」的处置**：代码里**从未实现** `out_dir` /
      `one_file_per_module` ⇒ ⇒ **无需保留任何字段** ⇒ ⇒ C1 的答案只影响
      「是否需要新增多文件模式」（⇒ ⇒ 若 C1 答「必须」，则是**新增**而非**恢复**）
- [x] 0.5 **判据**：编译期 `static_assert`（选项字段类型 == 权威枚举 + 默认值）+
      **粒度作用域的三格判据**（半生效 ⇒ `RespectDeclarations` 形态 · 正向 `ForceInline`
      ⇒ 单 module · 正向 `ForceLayer` ⇒ 3 module）+ 既有 4 用例绿 +
      扁平外部基线 3 设计 `cmp` 全 0 + `ctest -LE perf` **238/238**

## T1 module 主体与实例化

- [x] 1.1 每个成层组件产出一个 `module`（`<module_type_name>_<序号>`）
- [x] 1.2 父 module 内例化子 module + **实例端口映射** ⇒ ⇒ **零跨 module `assign`**
      （跨 module 的驱动边 ⇒ `suppress_driver_assign` ⇒ 值由实例端口映射提供）
- [ ] 1.3 **MUST NOT 复用** module 定义；同名不同类型 ⇒ `CHFATAL_EXCEPTION`
- [x] 1.4 顶层 module 端口表 **逐字逐序**等于扁平端口表（clock → reset → input → output；
      **全 inline 时整份产物与扁平逐字节相同** —— 实测）

## T2 端口

- [ ] 2.1 自身 `ch_in` / `ch_out` ⇒ 成层且非根 module 的端口集合 **⊇**
- [ ] 2.2 隐式端口（ADR-060 §2.1 四行方向矩阵 + 第三轴 pass-through）
- [ ] 2.3 悬空冒泡（逐字同名 + 注释 + `CHWARN` + `strict` 抛）
- [x] 2.4 clock / reset：根**无条件**发；非根**分开判**（clock ⇒ reg ∨ 写端口 ∨ sread；
      reset ⇒ reg ∨ sread）+ **需求向上传导**（子要 clock ⇒ 父也要 ⇒ 实例化不缺 pin）
- [ ] 2.5 `type_output` 三角色（自身端口 / 隐式输出 / **体内 wire**）

## T3 拒绝与错误模型

- [ ] 3.1 跨模块 `ch_mem` ⇒ 响亮拒绝（判定 = **inline 折叠后**归属）
- [ ] 3.2 自驱动 `ch_in` ⇒ 抛；子 module 内 MUST NOT `assign <ch_in> = ...`
- [x] 3.3 已 build 未 `add_child` ⇒ 抛 + `add_child` 修法（由分析层前置校验给出）+ 负向对照
      （`[hierarchy][emit][error]`：失败 ⇒ `ok=false` + **不留任何文件**）
- [ ] 3.4 接口白名单违反 ⇒ `CHFATAL_EXCEPTION` + 点名端口
- [x] 3.5 错误模型：校验先于 I/O · 临时文件→rename · 失败**可被调用方观测**（返回结构体）·
      静态析构期用**返回码**兜底（不抛不写盘）

## T4 护栏

- [ ] 4.1 `scripts/verify_hierarchical_rtl.sh`（零未声明引用 · 实例端口名 ∈ 端口集 ·
      **端口宽度一致** · 产物确定性 · 跨模块 `ch_mem` 被拒）
- [ ] 4.2 判据 MUST 基于 `verilator --json-only` 的结构化提取（复用
      `scripts/lib/verilog_tree_extract.py`）⇒ ⇒ **MUST NOT** 自研 Verilog 文本解析器
- [ ] 4.3 CI 挂 **两处**（主路径 + `--self-test`）

## 验证命令

```bash
cd /workspace/project/CppHDL
cmake --build build -j"$(nproc)"
ctest -LE perf --output-on-failure
bash scripts/verify_hierarchical_rtl.sh
bash scripts/verify_hierarchical_rtl.sh --self-test
openspec validate hierarchical-rtl-emission-2026-11 --type change --strict
bash scripts/verify_openspec_delta.sh
```
