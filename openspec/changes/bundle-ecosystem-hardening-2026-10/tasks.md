## Tasks

> **状态：草案（尚未实施）。**
> 立项自 2026-10-22 对「`bundle-master-slave-io-2026-10` 之后全仓遗留与遗漏」的
> 系统审计。审计结论：**10 项遗留 / 遗漏 + 5 项测试完备性缺口**（见 `proposal.md`）。
> 本 change 收敛 **L1-L9 + T3/T4**（bundle 生态的**边角**设施）；
> **T1/T2（samples 零覆盖 + 2 个既有 segfault）归独立的
> `samples-regression-guard-2026-10`**。
>
> **执行顺序**：Task 0 → 1 → 2 → 3 → 4 → 5 → 6 → 7。
> ⚠️ **TDD 纪律**：Task 1 先写会失败的用例（RED），Task 2-5 逐条转绿，
> Task 6-7 回归 + archive。每个 Task 的 verify 必须是**可执行命令**。

- [ ] ### Task 0: 审计取证（已完成，结论折进 `proposal.md`）
  - **action**:
    - 逐项确认 L1-L9 / T1-T5 的**现状与根因**（复现命令见 `proposal.md` 附录 A）
    - ⚠️ L1 的根因 MUST 用 **gdb 精确帧**确认（MUST NOT 依赖「崩在
      `set_name_prefix` 里」这种被内联误导的归因）：
      ```
      #0 bundle_field_traits<ch_stream>::set_name at bundle_traits.h:135
      #8 bundle_base::bundle_base at bundle_base.h:34   ← 基类构造期调虚函数
      ```
  - **verify**: `proposal.md` 附录 A 的每条命令 MUST 可复现

- [ ] ### Task 1: 写会失败的用例（RED）
  - **action**:
    - 新增 `tests/test_bundle_ecosystem.cpp`（或扩展 `test_bundle_master_slave.cpp`），
      覆盖 T3 的每一类边角：
      1. **嵌套 bundle 字段**的连接（`ch_stream` 成员 / `bundle_concat` / `bundle_slice_view`）
      2. **`flip()`** 的方向取反（master→slave、slave→master）
      3. **`master()` / `slave()` 工厂**的字段节点**独立性**
      4. **`deserialize_bits_to_fields()`** 路径（字段节点不与源位串共享）
      5. **`ch_stream`** 的连接语义（数据流边，非节点重绑定）
    - ⚠️ 每条用例 MUST 能**单独**抓住对应的回归（否则是空转）
    - MUST 在实现修复前 **全红**
  - **verify**:
    ```bash
    cmake --build build -j$(nproc) --target test_bundle_ecosystem
    ./build/tests/test_bundle_ecosystem
    ```
    MUST 观察到**红**

- [ ] ### Task 2: 修 prefix 命名的构造期崩溃（L1 / D1）
  - **action**:
    - `include/core/bundle/bundle_base.h`：
      - `bundle_base(const std::string &prefix)`（`:32-35`）MUST NOT 在基类构造期
        调用虚函数 `set_name_prefix()`
      - 命名 MUST **推迟到 `set_role()`**（`as_master()` / `as_slave()`）——
        与字段节点建立**同时**完成 ⇒ 派生类成员此时已构造 ✓
      - 命名 MUST **幂等**（不得重复拼接 prefix）
    - ⚠️ 影响**所有** bundle 的命名时机 ⇒ MUST 全量回归
  - **verify**:
    ```bash
    ./build/tests/test_bundle_ecosystem
    ./build/samples/nested_bundle_demo     # MUST 不再 segfault（exit=0）
    ```
    「嵌套 bundle 带 prefix 构造」的用例 MUST 转绿

- [ ] ### Task 3: 嵌套 bundle 设施的语义（L2 / L3 / D2）
  - **action**:
    - `bundle_concat` / `bundle_slice_view`（`bundle_operations.h:14,58`）：
      **实现正确语义**（拷贝后逐字段 `<<=` 连接）**或**显式废弃
      （MUST 二选一，MUST NOT 留静默失效的公开类型）
    - `deserialize_bits_to_fields()`（`bundle_serialization.h:85-97`）：
      改为**字段一等节点**（MUST NOT 切片重绑定）
    - ⚠️ 若选「废弃」⇒ MUST 同步 `bundle-base-integer-ctor` capability 的
      22 个公开派生类清单 + 走 ADR 流程
  - **verify**:
    ```bash
    ./build/tests/test_bundle_ecosystem
    ./build/tests/test_bundle_serialization
    ./build/tests/test_bundle_bitstream
    ./build/tests/test_nested_bundle
    ```

- [ ] ### Task 4: `flip()` / 工厂 / `operator=` 的契约（L4 / L5 / L6 / D3-D5）
  - **action**:
    - `flip()`（`bundle_base.h:181-188`）：方向**取反**（MUST NOT 硬编码
      `as_slave()`）
    - `master()` / `slave()`（`bundle_utils.h:10-17`）：产出**节点独立**的 bundle
    - `bundle_base::operator=`（`:170-175`）：MUST NOT 改变行为，但 MUST 加注释
      写明它是**软件拷贝**语义（与连接语义区分）
  - **verify**:
    ```bash
    ./build/tests/test_bundle_ecosystem
    ./build/tests/test_bundle
    ./build/tests/test_bundle_advanced
    ./build/tests/test_bundle_connection
    ./build/tests/test_nested_bundle
    ```

- [ ] ### Task 5: `ch_stream` 的连接语义收敛 + 双后端覆盖（L7 / T4 / D6）
  - **action**:
    - `include/chlib/stream_operators.h:25-33` 的自由 `operator<<=`：
      收敛到**数据流边**（与 ADR-042 一致）
      —— ⚠️ 注意 `ch_stream` 的 `ready` 是**反向**流动的 ⇒ 需确认
      「sink.valid ← source.valid / source.ready ← sink.ready」在数据流边下的
      正确接法（可能需要两条边，而非一条）
    - `ch_stream` MUST 进**双后端矩阵**（新增 dual-backend 用例或扩展既有
      verilator stream e2e）
  - **verify**:
    ```bash
    ./build/tests/test_bundle_ecosystem
    ./build/tests/test_ch_stream
    ctest -R "stream" --output-on-failure
    ```

- [ ] ### Task 6: 死代码清理 + 文档同步（L8 / L9 / D8 / D9）
  - **action**:
    - 删除 `include/core/direction.h` 的 `internal_direction` + `is_internal_v`
      （全仓零使用 ⇒ ZERO-DEBT POLICY 的 "No dead code"）
    - 文档写明 `get/set_bundle_value` 的**适用范围**（仅未连接 bundle）
    - `include/core/bundle/bundle_base.h` / `bundle_operations.h` /
      `bundle_serialization.h` / `bundle_utils.h` 的注释同步
    - 根 `AGENTS.md`：「已消除的过时信息」新增本 change 条目
  - **verify**:
    ```bash
    grep -rn "internal_direction\|is_internal_v" --include=*.cpp --include=*.h . | grep -v third_party
    # MUST 命中 0 行
    ```

- [ ] ### Task 7: 全量回归 + archive
  - **action**:
    - `ctest -LE perf`（若新增测试 target，期望数 +1）
    - `ctest -L perf`
    - `./run_all_ported_tests.sh`
    - `./build/samples/nested_bundle_demo`（MUST exit=0）
    - `openspec archive bundle-ecosystem-hardening-2026-10 -y`
      （**MUST NOT** 用 `--skip-specs`）
  - **verify**:
    ```bash
    cd build && ctest -LE perf -j4 --output-on-failure
    ctest -L perf --output-on-failure
    ./run_all_ported_tests.sh
    openspec validate --specs --strict
    grep -rn "bundle-ecosystem-hardening-2026-10" openspec/specs/   # MUST 命中 0 行
    ```
