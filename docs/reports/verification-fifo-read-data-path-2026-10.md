# Verification Report — fifo-read-data-path-2026-10

> Change: `openspec/changes/fifo-read-data-path-2026-10/`
> ADR: `docs/adr/ADR-044-fifo-read-data-path.md`
> Date: 2026-10-22

## 1. 缺陷（实施前）

`fifo_bundle<T>` 的 `data` 字段被**写数据**与**读数据****双工共用** ⇒
slave 侧**无处输出**读到的数据 ⇒ 该协议的**读通路根本不存在**。

`samples/fifo_bundle.cpp` 是活标本 —— 算出 `data_out` 之后**直接丢弃**：

```cpp
auto data_out = mem.aread(rd_a);
// 注意：io.data是输入，不能赋值，我们只需要使用它作为输入
// 数据输出通过其他方式提供，这里我们不直接连接io.data
```

⚠️ 该缺陷**不可见**是因为 `bundle-master-slave-io-2026-10`（ADR-042）只修掉了
「只读字段恒为 0」，**没碰**「读数据通路缺失」⇒ sample 变**绿**了，却仍然
**没有实现 FIFO 的读通路**（「**绿 ≠ 对**」）。

## 2. ADR-044 的 6 项裁决

| # | 裁决项 | 结论 |
|---|---|---|
| 1 | 形态 | **A** —— 加独立的 `rdata` 字段 |
| 2 | `rdata` 的字段位置 | **追加末尾** ⇒ 既有 5 个字段的位偏移**完全不变** |
| 3 | 读通路时序 | **`aread`（0-cycle 组合读）** ⇒ 与 Verilog `assign dout = mem[rd_ptr];` 一致 |
| 4 | `empty` 时 `rdata` | **don't-care** ⇒ MUST NOT 被强制为 0 |
| 5 | 采样规则 | pop 沿**之前**采样才是「正在被弹出的元素」；pop 沿**之后**指向**下一个**队首 |
| 6 | 打包 API 宽度边界 | `2W+4` ⇒ **`W ≥ 31` 显式报错** ⇒ 接受回退 + 写明逐字段替代路径 |

## 3. 行为级覆盖

`tests/test_bundle_master_slave.cpp` 新增用例
「bundle master/slave: **读数据通路** —— 推入的元素真的被读回」
（`[bundle][master-slave][read-path]`），断言：

1. 推入 0x5A ⇒ `empty == 0` 且 `rdata == 0x5A`；
2. 再推入 0x3C ⇒ `rdata` **仍是 0x5A**（0-cycle ⇒ `rd_ptr` 未动 ⇒ 队首不变）；
3. pop 沿生效 ⇒ `rdata == 0x3C`（**下一个**队首 —— 裁决 5）。

⇒ 覆盖 **9 个 TEST_CASE / 38 个断言**（实施前 8 / 30）。

**变异验证**：把 DUT 的 `io.rdata <<= mem.aread(rd_a);` 去掉 ⇒
**1 failed / 33 assertions** ⇒ 证明是真护栏（不是恒绿断言）。

## 4. 同步的硬编码断言（6 处）

| 文件 | 断言 | 改后 |
|---|---|---|
| `tests/test_bundle_bitstream.cpp:85` | `get_bundle_width<fifo_bundle<ch_uint<4>>>() == 8` | `== 12` |
| `tests/test_bundle_bitstream.cpp:99` | 同上 | `== 12` |
| `tests/test_bundle_bitstream.cpp:93` | `tuple_size_v<decltype(fields)> == 5` | `== 6` |
| `tests/test_bundle_bitstream.cpp:109` | 同上 | `== 6` |
| `tests/test_bundle_advanced_features.cpp:75` | `STATIC_REQUIRE(bundle_field_count_v<...> == 5)` | `== 6`（**编译期**） |
| `tests/test_bundle_ecosystem.cpp:487` | `bundle_field_count_v<...> == 5` | `== 6` |

⚠️ **实测更正**：提案原表只列了 **4 处** ⇒ `tuple_size_v<decltype(__bundle_fields())>`
那 **2 处**漏网 ⇒ `ctest` 报 `test_bundle_bitstream` **失败**（`6 == 5`）⇒ 才被补上。
⇒ 教训：字段数假设**不止**出现在 `bundle_field_count_v` / `get_bundle_width` 里。

## 5. 生成 Verilog 的验收

`samples/fifo_bundle.cpp` 生成的 `fifo_bundle.v`（实测）：

```verilog
logic [1:0] top_rdata;
...
assign top_fifo_inst_mem_aread_data_proxy = top_fifo_inst_fifo_mem[top_fifo_inst__zext]; // Comb read
assign top_rdata = top_fifo_inst_mem_aread_data_proxy;
```

⇒ 读通路在 **Verilog 侧同样成立**（0-cycle 组合读）⇒ interpreter 与 Verilog
逐拍一致 ⇒ 且**无未声明信号引用**（`aread` 的 codegen 三处配套坑
—— `print_proxy` 跳过 mem read port / `print_output` 的代理链遍历 /
`type_mem_read_port` 不进 wire 声明列表 —— 已被 `sim-mem-read-timing-align-2026-10`
覆盖，本次实测仍然成立）。

## 6. 回归

| 项 | 结果 |
|---|---|
| `ctest -LE perf` | **173/173 全绿** |
| `ctest -N` 总数 | **184**（不变 —— 本次是**在既有 target 里加用例**） |
| `scripts/run_all_samples.sh` | 全绿 |
| `run_all_ported_tests.sh` | 28/28 全绿 |
| `scripts/verify_test_counts.sh` | 绿 |
| `openspec validate fifo-read-data-path-2026-10 --strict` | 绿 |
