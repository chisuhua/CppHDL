# Change: fifo-read-data-path-2026-10 — `fifo_bundle` 的**读数据通路**

> **状态：草案（尚未实施）。**
> 立项自 `bundle-ecosystem-hardening-2026-10` 的 L10（用户裁决独立立项）。
> ⚠️ **公开 API 布局变更** ⇒ Task 1 的 ADR MUST **先于**任何代码改动落地。
> ⚠️ **ADR-044 编号可用**（`docs/adr/` 现存最大编号 ADR-043）。
> ⚠️ **TDD 排序**（Task 2）：行为级 RED 用例引用尚不存在的 `io.rdata` ⇒
> **编译错误**而非「测试红」⇒ 单一 target 编译断会拖垮既有 8 个用例 ⇒
> MUST 按 Task 2 的显式排序走。

- [ ] ### Task 0: 清点影响面（ADR 的输入）
  - **action**:
    - 清点 `fifo_bundle` 的**全部**使用点（字段布局变化 ⇒ 位偏移全变）：
      ```bash
      grep -rn "fifo_bundle" --include=*.cpp --include=*.h include/ tests/ examples/ samples/ src/ docs/
      ```
      （⚠️ 原命令**漏了** `src/` 与 `docs/`）
    - 清点**位偏移常量**的使用点（`kFifoPushBit` / `kFifoFullBit` / ...）：
      ```bash
      grep -rn "kFifo.*Bit\|bundle_width_v<fifo_bundle" --include=*.cpp tests/
      ```
    - 清点 `ch_stream` 的 `payload` 是否构成同病（**预期：不构成** —— stream 协议
      规定 `payload` 由 master 驱动）⇒ 在 ADR 里写明该清点结论
    - ⚠️ **实测已有结论（2026-10-22，落地前请复核）**：
      `fifo_bundle` 共 **9** 个文件 / 约 **49** 处引用；其中 **5 处是硬编码的
      宽度/字段数/位偏移断言**（`tests/test_bundle_bitstream.cpp:85,99` /
      `tests/test_bundle_advanced_features.cpp:75`（**`STATIC_REQUIRE`** ⇒ 编译期断）/
      `tests/test_bundle_ecosystem.cpp:487` /
      **`tests/test_bundle_master_slave.cpp:178-184`（位偏移常量，原提案漏掉）**）
      ⇒ 加 `rdata` 后 MUST 同步（`== 8` → `== 12`；`== 5` → `== 6`；
      位偏移常量**取决于 ADR 对 `rdata` 位置的裁决**）
  - **verify**:
    ```bash
    test -f docs/reports/verification-fifo-read-data-path-2026-10.md
    ```

- [ ] ### Task 1: 写 ADR-044（**6 项**裁决）
  - **action**:
    - 新建 `docs/adr/ADR-044-fifo-read-data-path.md`：
      1. **形态**裁决（A 加 `rdata` / B 拆两个 bundle / C 不可行）—— 逐项唯一；
      2. ⚠️ **`rdata` 的字段位置**裁决 —— **决定全部位偏移**：
         追加末尾（`data, push, full, pop, empty, rdata`）⇒ 既有 5 个偏移不变、
         `rdata` 占 bit 12..；插入 ⇒ `push/full/pop/empty` 全部 +W ⇒
         `test_bundle_master_slave.cpp` 的 5 个常量 + 9 处 `1ULL << ...` 全改、
         且打包 API 对外部用户的位语义被静默改变；
      3. **FIFO 协议**：`dout` 在 pop 沿**之后**指向**下一个**队首 ⇒ 消费者 MUST
        在 pop 沿**之前**采样；
      4. **`aread`（0-cycle）vs `sread`（1-cycle）**的选型 + 理由
        （⚠️ 现状 sample 用 `aread` —— 那是**描述**不是裁决，ADR MUST 明确裁决）；
      5. ⚠️ **`empty` 时 `rdata` 是否 don't-care** —— 组合读时它是**残留/垃圾**
        ⇒ MUST 定义，否则消费者会 latch 垃圾；
      6. ⚠️ **打包 API 的宽度边界** —— `W+4` → `2W+4` ⇒ `W ∈ [31, 60]` 从
        「可用」变成「**显式报错**」（`include/simulator.h:236-242`）⇒ 裁决
        （接受回退 / 提供逐字段替代路径 / 限制 T 宽度）。
  - **verify**:
    ```bash
    test -f docs/adr/ADR-044-fifo-read-data-path.md
    # 6 项裁决 MUST 全部出现
    grep -c "rdata" docs/adr/ADR-044-fifo-read-data-path.md
    ```

- [ ] ### Task 2: 写会失败的用例（RED）—— ⚠️ **排序必须显式**
  - **action**:
    - ⚠️ **问题**：行为级 RED 用例必然引用 `io.rdata` ⇒ 字段尚不存在 ⇒
      **编译错误**（不是「测试红」）⇒ 而 `test_bundle_master_slave` 是**单一
      target**（8 个 TEST_CASE）⇒ 编译断 ⇒ 既有 8 个用例**全部无法运行**
      ⇒ 这与「MUST 观察到红」的意图相悖。
    - **裁决（按 ADR 的形态二选一，MUST 在 ADR 里写死）**：
      - **路线 1（推荐）**：**先加字段**（编译恢复绿）→ **再写行为/时序断言**
        ⇒ 断言因「读通路未接通」而**红** ⇒ 既有 8 个用例全程可运行，
        且 RED 是真正的「断言红」而不是「编译断」；
      - **路线 2**：把 RED 放在 **sample 层**
        （`samples/fifo_bundle.cpp` 先加「弹出的数据 MUST 等于推入的数据」
        断言 ⇒ 运行期 exit≠0）⇒ 编译期不受影响。
    - 断言 MUST 是**行为级**（驱动 → 读值）：
      - **行为级**：TB 推入一个元素后，**master 侧 MUST 读到该元素**（读数据通路）
      - **时序级**：pop 沿**之后** `dout` MUST 指向**下一个**队首（协议）
      - ⚠️ MUST NOT 只断言 `impl() != nullptr`
  - **verify**:
    ```bash
    cmake --build build -j$(nproc) --target test_bundle_master_slave
    ./build/tests/test_bundle_master_slave       # MUST 观察到红（断言红，非编译断）
    # ⚠️ 若走路线 1 ⇒ 编译 MUST 成功（既有 8 个用例 MUST 仍可运行）
    ```

- [ ] ### Task 3: 实现（按 ADR 的裁决）
  - **action**:
    - `include/bundle/common_bundles.h`：`fifo_bundle` 加 `rdata` 字段 + 方向
      （master IN / slave OUT）
    - `samples/fifo_bundle.cpp`：读数据通路实现（不再丢弃 `data_out`）
      + ⚠️ **写死期望值表**（该 sample 是 `tick()` **之后**才采样 ⇒ 第 2 个元素在
      cycle 2 才可读、cycle 3 起 `rdata` 已指向下一个 ⇒ 极易差一拍）
    - `samples/minimal_fifo_bundle.cpp`：⚠️ MUST **显式驱动** `rdata`
      （否则新字段恒 0）；顺带修 `:26-27` 的 `io.full = false;` /
      `io.empty = true;`（**节点重绑定** —— ADR-042 已把 `operator=` 定性为
      「不是连接手段」）
    - `tests/test_bundle_master_slave.cpp`：位偏移常量更新 + 新断言
      + ⚠️ **方向元数据断言 MUST 补 `rdata`**（现有 `:281-305` 只断言 5 个字段
      ⇒ 「rdata 方向写反」无人能抓）
    - ⚠️ **5 处硬编码断言同步**（`test_bundle_bitstream.cpp:85,99` /
      `test_bundle_advanced_features.cpp:75` / `test_bundle_ecosystem.cpp:487` /
      `test_bundle_master_slave.cpp:178-184`）
      ⇒ 不改则**当场红**（其中 1 处是 `STATIC_REQUIRE` ⇒ 编译期断）
    - ⚠️ **生成 Verilog 的验收**：MUST 有一条「生成的 Verilog 含 `rdata` 输出
      且 MUST NOT 引用未声明信号」的断言（抄 `tests/test_mem_comb_read.cpp` 的
      同型护栏）—— `aread` 是 `sim-mem-read-timing-align-2026-10` 踩过 codegen
      三处配套坑的路径，而现有 `test_bundle_master_slave.cpp` **只跑 interpreter**
  - **verify**:
    ```bash
    ./build/tests/test_bundle_master_slave       # MUST 转绿
    ./build/samples/fifo_bundle                   # MUST exit=0
    ./build/samples/minimal_fifo_bundle           # MUST exit=0
    ```

- [ ] ### Task 4: capability delta + 全量回归 + archive
  - **action**:
    - `bundle-master-slave-io` 的 delta（**必然**，见 proposal D3）：
      Requirement 标题与 Scenario 名 MUST **逐字**匹配主 spec；
      ⚠️ delta MUST **删掉自认暂定的措辞**（「本段文字按形态 A 起草…若 ADR
      裁决了其它形态，本段 MUST 随之订正」）—— delta 会**逐字并入主 spec** ⇒
      留待定条款 = 把不确定性固化进 spec
    - ⚠️ delta 的「fifo_bundle 的方向指认」Scenario MUST **列出 `rdata`**
      （master IN / slave OUT）⇒ 否则合并后主 spec 仍隐含 5 字段 ⇒ spec 与实现漂移
    - ⚠️ delta 的「读数据 MUST 有独立字段」Scenario MUST **限定作用域**
      （当前措辞是通用规则 ⇒ 会让含读写两侧的 `axi_lite_bundle` 变成违规
      ⇒ 与 Non-Goal 1 冲突）
    - `ctest -LE perf` / `run_all_ported_tests.sh` / `run_all_samples.sh`
    - 计数同步（⚠️ **按新增 TEST_CASE 数**同步 `CTEST_TOTAL` + NOTES +
      `scripts/verify_test_counts.sh` —— 行为级 + 时序级**至少 +2**）
    - `openspec archive fifo-read-data-path-2026-10 -y`
  - **verify**:
    ```bash
    (cd build && ctest -LE perf -j4 --output-on-failure)
    ./run_all_ported_tests.sh
    bash scripts/run_all_samples.sh
    bash scripts/verify_test_counts.sh
    # ⚠️ 不得用 `openspec validate --specs --strict` 当验收 ——
    # 它在 pristine HEAD 上就是红的（CLI 1.14.3 实测：3 passed, 15 failed）⇒ 不具判别力
    openspec validate fifo-read-data-path-2026-10 --strict
    # ⚠️ capability 级校验在 pristine HEAD 上**也是红的**（`bundle-master-slave-io`
    # 的 2 条 Requirement >500 字符 ⇒ CLI 1.14.x 新规则，与 AGENTS.md 记录的
    # 1.4.1 → 1.14.x 漂移一致）⇒ 只能判「**MUST NOT 比基线更差**」
    git stash push --quiet -- openspec/specs/ 2>/dev/null || true
    openspec validate bundle-master-slave-io --type spec --strict >/dev/null 2>&1; base=$?
    git stash pop --quiet 2>/dev/null || true
    openspec validate bundle-master-slave-io --type spec --strict >/dev/null 2>&1; now=$?
    test "$now" -le "$base" || { echo "REGRESSION: capability validate got worse ($base -> $now)"; exit 1; }
    grep -rn "fifo-read-data-path-2026-10" openspec/specs/   # MUST 命中 0 行
    ```
