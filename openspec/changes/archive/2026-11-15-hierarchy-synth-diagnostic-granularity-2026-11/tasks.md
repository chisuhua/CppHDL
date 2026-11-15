# Tasks

## 1. 记录与归属（`src/hierarchy_verilog.cpp`）

- [x] 1.1 新增 `SynthEdgeRecord` / `synth_edge_records` / `record_synth_edge`，删除 `synth_events` 及其 2 个 push 点
- [x] 1.2 新增 `SynthEdgeContext cur_edge` + `record_synth_object`（无上下文 ⇒ no-op ⇒ 「产物必有边」不变量）
- [x] 1.3 两个边循环的迭代入口设置 `cur_edge`
- [x] 1.4 6 个产物创建点改用 `record_synth_object`（换 `synth_events.push_back` 的同位置）
- [x] 1.5 `build_chain` 每级端口登记（含 `add_pass_port` 幂等复用）⇒ `Fatal` 不漏链路中段端口

## 2. 渲染（三档一份记录）

- [x] 2.1 `edge_text` / `objects_text` / `warning_text(synthesized)` 三个渲染 helper
- [x] 2.2 `Warn`：每边一行 ⇒ `result.warnings` + `CHWARN`
- [x] 2.3 `Fatal`：首行 `N` + `[i]` 逐边（含产物名）+ `warnings` 逐条
- [x] 2.4 `Reject`：首行 `1 of N` + 1 条边级记录（**不含**产物名）+ 条件式 warning
- [x] 2.5 `Reject` 措辞改 `would be synthesized`（本档不合成 ⇒ 不得声称已合成）

## 3. 判据（`tests/test_hierarchy_verilog.cpp`）

- [x] 3.1 helper：`count_synth_comments` / `count_indexed_lines` / `count_before`
- [x] 3.2 样本 `MixRow2SiblingTop`（行 2 + 兄弟边 ⇒ **N = 2** · 含端口 + 网 · 无悬空 `ch_in`）
- [x] 3.3 元组用例：`Warn (true, 2, 有)` / `Fatal (false, 2, —)` / `Reject (false, 1, —)`，三个 `std::filesystem::remove` 均断言成功
- [x] 3.4 `implicit port` 与 `net` **分别计数**（各 == 1）
- [x] 3.5 `Reject` 的不含产物名断言覆盖 `port '` / `net '`（M2 初版恒绿的修正）
- [x] 3.6 既有 `TwoOpTop` 用例（失败路径广度）**保留**并在新用例里注明分工

## 4. 验证

- [x] 4.1 `ctest -LE perf` 254/254
- [x] 4.2 变异自证 M1–M5（逐条转红 · 还原后**重建**复绿）
- [x] 4.3 全部护栏脚本 PASS（含 `verify_hierarchical_rtl.sh` / `verify_hierarchical_port_naming.sh`）

## 5. 归档与台账

- [x] 5.1 `openspec archive`（**禁** `--skip-specs`）⇒ delta 合入 `openspec/specs/rtl-hierarchical-output/`
- [x] 5.2 主 spec 的新增句可 grep（见下方 verify 块）
- [x] 5.3 台账 §7 行 15 + `.rddf/checks/arch-residual-registry.md` 同批闭口（规范句由 `--render` 生成）

## Verify

```bash
# ① 编译 + 本 change 的判据
cmake --build build -j"$(nproc)" --target test_hierarchy_verilog
./build/tests/test_hierarchy_verilog "hierarchical emission: port policy tuple on synthesizable edges"

# ② 观测载体（R3.2a）：warnings 里确实有合成行
grep -n "result.warnings.push_back" src/hierarchy_verilog.cpp

# ③ delta 自检（提案阶段 4 条）
bash scripts/verify_openspec_delta.sh 2026-11-15-hierarchy-synth-diagnostic-granularity-2026-11

# ④ 归档判据：archive 路径 + 主 spec 新增句可 grep
test -f openspec/changes/archive/2026-11-15-hierarchy-synth-diagnostic-granularity-2026-11/proposal.md
grep -q "诊断的计数与列举单位 MUST 是「边」" openspec/specs/rtl-hierarchical-output/spec.md

# ⑤ 台账闭口护栏
bash scripts/verify_arch_ledger_residuals.sh
```
