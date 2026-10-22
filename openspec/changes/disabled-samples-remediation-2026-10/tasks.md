# Change: disabled-samples-remediation-2026-10 — 修复 14 个**禁用** sample 的编译

> **状态：草案（尚未实施）。**
> 立项自 `samples-regression-guard-2026-10` 的 Non-Goal 5。
> ⚠️ 本 change 的每个 Task 都以「**该组的 sample 已登记进
> `CH_SAMPLE_TARGETS` 且已从禁用清单移除**」为完成判据。
> ⚠️ **2026-10-22 实测**：14 个 sample 的**全部** error 数合计 **229**
> （不是「14 个问题」）⇒ 排期 MUST 参考 proposal 的 D1 分组顺序。
> ⚠️ 组 F（`spinalhdl_component_function_area_example`，**98 error**）接近重写
> ⇒ **建议独立立项**（proposal D3）。

- [ ] ### Task 0: 复核基线（14 个 sample 的**全部**编译错误）
  - **action**:
    - 逐个重跑编译探测，确认根因**未变**（自 2026-10-22 登记以来框架可能已变）：
      ```bash
      FLAGS=$(grep -m1 "CXX_FLAGS" build/samples/CMakeFiles/counter.dir/flags.make | sed 's/^CXX_FLAGS = //')
      INCS=$(grep -m1 "CXX_INCLUDES" build/samples/CMakeFiles/counter.dir/flags.make | sed 's/^CXX_INCLUDES = //')
      # 清单来自禁用清单（名字在第 1 列）
      for f in $(cut -f1 samples/disabled_samples.txt | grep -v '^#'); do
          n=$(g++ $FLAGS $INCS -fsyntax-only "samples/$f.cpp" 2>&1 | grep -c "error:")
          first=$(g++ $FLAGS $INCS -fsyntax-only "samples/$f.cpp" 2>&1 | grep -m1 "error:" | cut -c1-90)
          printf "%-42s errors=%-3s %s\n" "$f" "$n" "$first"
      done
      ```
    - ⚠️ MUST 数**全部** error（`grep -c "error:"`），**MUST NOT** 只看首个
      （`grep -m1`）—— 首个 error 一致 **≠** 根因分类一致
      （`spinalhdl_component_function_area_example` 首个是「基类名不认识」，
      实则 **98** 个 error ⇒ 那个分类几乎肯定不成立）
    - ⚠️ 若根因**已变**（框架已修）⇒ 该 sample MAY 直接编译通过 ⇒ 走 Task 2
      的「登记」路径（不需改代码）
  - **verify**:
    ```bash
    # ⚠️ 禁用清单的行数 MUST 排除注释行（实测：`grep -c .` = 35，含注释与分组行）
    grep -v '^#' samples/disabled_samples.txt | grep -c .   # 14（不含注释行）
    ```

- [ ] ### Task 1: 组 A — 十六进制字面量后缀（5 个）
  - **action**:
    - `stream_arbiter_example` / `stream_demux_example` / `stream_flow_basic_example` /
      `stream_fork_example` / `stream_join_example`：
      **十六进制**字面量的 `_d` 后缀 → `_h`（`0xAB_d` → `0xAB_h`）
    - ⚠️ **MUST NOT** 改成裸 `0xAB`（会退化成 `int`、宽度 32 ⇒ 信号宽度与
      打印输出可能改变 ⇒ 「绿 ≠ 对」）；`0xAB_h` 实测 value=171 / width=8 ✓
    - ⚠️ **MUST NOT** 改十进制字面量（`stream_demux_example:23,45` 的 `1_d` /
      `0_d` —— `_d` 本来就正确）
    - ⚠️ **优先「按 `examples/stream/*_demo.cpp` 改造」**（近乎同源 ⇒ 成本最低）
      ⇒ 每个 sample MUST 显式回答「修 / 删 / 留」三者之一（proposal D1）
    - ⚠️ 改完 MUST 逐个跑护栏（**运行**回归可能暴露新的失败）
  - **verify**:
    ```bash
    # ⚠️ verify 块 MUST 自带 FLAGS/INCS（Task 0 的定义在另一个代码块里 ⇒
    # 单独执行本块时两者为空 ⇒ 编译必然失败）
    FLAGS=$(grep -m1 "CXX_FLAGS" build/samples/CMakeFiles/counter.dir/flags.make | sed 's/^CXX_FLAGS = //')
    INCS=$(grep -m1 "CXX_INCLUDES" build/samples/CMakeFiles/counter.dir/flags.make | sed 's/^CXX_INCLUDES = //')
    for f in stream_arbiter_example stream_demux_example stream_flow_basic_example \
             stream_fork_example stream_join_example; do
        g++ $FLAGS $INCS -fsyntax-only "samples/$f.cpp" -o /dev/null || { echo "FAIL $f"; exit 1; }
    done
    # ⚠️ 循环 MUST 以非零退出码收尾（原写法以 `echo` 收尾 ⇒ 恒 exit 0 ⇒
    # 这个 verify 永远不可能失败）
    ```

- [ ] ### Task 2: 登记机制（每修好一个就跑）
  - **action**:
    - ① 补进 `samples/CMakeLists.txt` 的 `CH_SAMPLE_TARGETS`
    - ② 从 `samples/disabled_samples.txt` 移除该行
    - ③ 跑护栏
    - ④ ⚠️ **同步两处嵌了计数的头注释**（每移出一行就漂移）：
      - `samples/CMakeLists.txt:6`（「未注册的 sample 见 samples/disabled_samples.txt（**N 个**，2026-10-22 实测）」）
      - `samples/disabled_samples.txt:15`（「已注册 **N** / 源文件 40 / 本清单 **M**」）
    - ⚠️ 若采纳了「删源文件」的归宿 ⇒ 源文件总数也变（40 → 39）⇒ 根 `AGENTS.md`
      的「40 sample programs」条目同样要同步
  - **verify**:
    ```bash
    bash scripts/run_all_samples.sh
    # C1/C2 守恒 MUST 仍绿
    # 头注释里的计数 MUST 与实际一致
    grep -o "本清单 [0-9]*" samples/disabled_samples.txt
    grep -o "（[0-9]* 个" samples/CMakeLists.txt
    ```

- [ ] ### Task 3: 组 B / C / D / E（6 个）
  - **action**:
    - 按 proposal 的 D1 分组推进（context 访问器改名 → `stream_fifo` 签名 →
      头文件/命名空间 → POD bundle 分区 API）
    - ⚠️ 每组**单独**一个可勾选项（不要把 4 组塞进一个 Task ⇒ 进度不可见）
    - 每组完成后走 Task 2 的登记流程
  - **verify**:
    ```bash
    bash scripts/run_all_samples.sh
    ```

- [ ] ### Task 4: 组 F（`spinalhdl_component_function_area_example`，**98 error**）
  - **action**:
    - ⚠️ 它**不是**「组件 API 改名」⇒ **接近重写** ⇒ **建议独立立项**
      （proposal D3 的「需要重写 sample 本身」那一档）
    - 若本 change 承担 ⇒ MUST 显式记录「允许重写该 sample」为 Non-Goal 例外
    - 若不承担 ⇒ 留在禁用清单，归位条件写成「依赖 <独立 change 名>」
  - **verify**:
    ```bash
    grep -c "spinalhdl_component_function_area_example" samples/disabled_samples.txt   # 0 或 1（视裁决）
    ```

- [ ] ### Task 5: 组 G（`if_demo`，可能需要框架变更）
  - **action**:
    - 尝试修复；若需要**框架层**新能力 ⇒ **不**在本 change 范围 ⇒
      留在禁用清单，把归位条件改成「依赖 <具体框架变更>」（D3）
    - ⚠️ **期望值 MUST 钉死为 1**（D3 明确「需要框架能力 ⇒ 留在清单」⇒ 归档时
      它**必然**还在清单里）
  - **verify**:
    ```bash
    # ⚠️ 期望值 MUST 是 1 —— 原写法「0 或 1」= 两种结果都算通过 = 零判别力
    grep -c "^if_demo" samples/disabled_samples.txt   # MUST == 1
    ```

- [ ] ### Task 6: 全量回归 + 文档 + archive
  - **action**:
    - `bash scripts/run_all_samples.sh` ⇒ MUST 全绿（**无 stale 豁免**）
    - `bash scripts/verify_test_counts.sh`（**CTEST_TOTAL 不变**）
    - 根 `AGENTS.md` 的「40 sample programs」条目同步（26 已注册 → N 已注册）
    - ⚠️ **禁用清单最终长度 MUST 作为验收**（预期 ≤ 2，且每项的归位条件指向一个
      具体 change ⇒ 否则「清单可收缩」这条 Requirement 在归档时可能即为假）
    - `openspec archive disabled-samples-remediation-2026-10 -y`
  - **verify**:
    ```bash
    bash scripts/run_all_samples.sh
    bash scripts/verify_test_counts.sh
    # ⚠️ 不得用 `openspec validate --specs --strict` 当验收 ——
    # 它在 pristine HEAD 上就是红的（CLI 1.14.3 实测：3 passed, 15 failed）⇒ 不具判别力
    openspec validate disabled-samples-remediation-2026-10 --strict
    # ⚠️ capability 级校验：MUST NOT 比 pristine HEAD 的基线更差
    # （CLI 1.14.x 新增了「Requirement >500 字符」规则 ⇒ 部分主 spec 在 HEAD 上即红）
    git stash push --quiet -- openspec/specs/ 2>/dev/null || true
    openspec validate sample-regression-guard --type spec --strict >/dev/null 2>&1; base=$?
    git stash pop --quiet 2>/dev/null || true
    openspec validate sample-regression-guard --type spec --strict >/dev/null 2>&1; now=$?
    test "$now" -le "$base" || { echo "REGRESSION: capability validate got worse ($base -> $now)"; exit 1; }
    grep -v '^#' samples/disabled_samples.txt | grep -c .   # MUST <= 2
    grep -rn "disabled-samples-remediation-2026-10" openspec/specs/   # MUST 命中 0 行
    ```
