---
id: decision-request-fifo-full-threshold-2026-11
日期: 2026-11-12
提出者: builder（B2 / E3 · `fifo_stream_example` 的独立 RTL TB）
状态: **已裁决（2026-11-12 · 选项 A 落地）**
关联: .rddf/checks/examples-rtl-expectations.md#fifo-stream-example · .rddf/checks/examples-rtl-sim-targets.tsv
---

# 决策请求单：`fifo_stream_example` 的 `full` 门限算错 ⇒ 第 16 次写入起**地址混叠**

> ⚠️ **本单不是「请 architect 重新决策」** ⇒ ⇒ 全部事实已**实测**（产物级 + 内部指针对齐）
> ⇒ ⇒ 裁决点只剩「**怎么处理**」。

---

## 1. 现象（实测 · 可复现）

`examples/spinalhdl-ported/fifo/stream_fifo_example.cpp` 的 `FiFo<ch_uint<8>, 16>`：

```cpp
ch_reg<ch_uint<addr_width + 1>> rd_ptr(0_d, "rd_ptr");   // addr_width = 4 ⇒ 指针 5 位
ch_reg<ch_uint<addr_width + 1>> wr_ptr(0_d, "wr_ptr");
ch_uint<addr_width> wr_a = bits<addr_width - 1, 0>(wr_ptr);  // 地址 = 指针低 4 位 ⇒ 16 槽

io().empty = (rd_ptr == wr_ptr);                  // 5 位比较
auto wr_plus_one = wr_ptr + 1_b;
io().full = (wr_plus_one == rd_ptr);              // 5 位比较 ⇒ 门限 = 31 在途
```

⇒ ⇒ **指针是 5 位（mod 32）**，**内存只有 16 槽** ⇒ ⇒ `full = (wr+1 == rd)` 在
**5 位空间**里表示「相距 31」⇒ ⇒ **`full` 要到第 31 个在途元素才置位**。

**实测证据**（用内部指针 `dut.top_fifo_inst_rd_ptr` / `wr_ptr` 对齐时序后复跑）：

```
after 20 pushes: rd=0 wr=20 empty=0 full=0        ← full 从不置位
pop  1 -> 20   pop  2 -> 21   pop  3 -> 22   pop  4 -> 23
pop  5 -> 14   pop  6 -> 15  ...  pop 16 -> 1f
pop 17 -> 20   pop 18 -> 21  pop 19 -> 22  pop 20 -> 23
```

⇒ ⇒ push 的是 `10 11 12 ... 23`（20 个）⇒ 读回的是 `20 21 22 23 14 15 ...`
⇒ ⇒ **最老的 4 个（`10 11 12 13`）已被覆盖丢失** ⇒ ⇒ 内存只有 16 槽，
第 16 次写入起**地址混叠**。

⚠️ **判据来源与纪律**：期望值是**手推**的（5 位指针 + 16 槽 ⇒ 混叠），
**没有抄仿真输出**；实测只用于**确认**手推结论。首版诊断时我的实验 harness
自身有采样时序错误（用内部指针对齐后才发现）⇒ ⇒ 已在登记文档留档。

---

## 2. 为什么 TB **没有**判它（刻意）

`fifo_stream_example` 的 TB（`tb_fifo_stream_example.sv` · `checks=58 fails=0`）对 `full` **只判**
「在途 ≤ 14 时为 0」，**不判** 15/16 在途处的取值 ⇒ ⇒ 理由：

- 16 槽 FIFO 的**正确** `full` 语义可以是 **15** 在途（浪费 1 槽）或 **16** 在途
  （经典「多一位指针位区分 full/empty」）⇒ ⇒ **两种正确实现在此处不一致**
  ⇒ ⇒ 判它 = **固化某个偶然选择**（与 `gpio` / `quadrature` 同型的纪律）。
- **变异自证**已证明这一点：把 `full` 改成**4 位地址比较**（= 更接近正确的实现）
  ⇒ ⇒ TB **仍然全绿**（`checks=58 fails=0`）⇒ ⇒ TB 不锁死当前实现的门限。

⇒ ⇒ 因此**缺陷必须走裁决**，不能靠 TB 悄悄带过。

---

## 3. 三个选项

| 选项 | 内容 | 代价 / 影响 |
|---|---|---|
| **A** | **修实现**：`full` 改用**地址位**比较（经典式：`wr[3:0] == rd[3:0] && wr[4] != rd[4]`）⇒ ⇒ `full` 在 15 或 16 在途处正确置位 | 改 example 的 `.cpp` ⇒ **产品面改动** ⇒ 超出 E3 范围 ⇒ **需独立立项**；example 的 `main()` 演示输出可能变；`examples 28/28` 需复跑 |
| **B** | **判为已知缺陷并登记**：example 保持现状 ⇒ ⇒ 缺陷进残差登记（`.rddf/checks/arch-residual-registry.md` + 台账 `docs/architecture/…` §7，**两侧必须同批**）⇒ ⇒ TB 继续限定 ≤ 15 在途 | example 保持一个**真缺陷**（`full` 从不置位 ⇒ 依赖它的调用方会溢出）⇒ ⇒ 债留在仓里 |
| **C** | **口径对齐（文档层）**：把 example 的注释与 `main()` 的打印（`FIFO size: 16`）改成**实际可用槽数**与 `full` 的真实门限 ⇒ ⇒ 行为零改动 | 只改文字 ⇒ ⇒ 但**缺陷仍在**（只是不再被误述） |

---

## 4. 建议

**建议 = C 立即做（零风险）+ A 独立立项（真修）**。

理由：

1. **C 是「诚实性」义务**：当前 example 的打印把容量说成 16，而 `full` 的真实门限是 31
   ⇒ ⇒ 任何照抄这个 demo 的用户都会踩坑 ⇒ ⇒ **文字口径必须先对齐**（零行为风险）。
2. **A 是真缺陷**：一个**永不置位**的 `full` 端口是接口级谎言 ⇒ ⇒ 应修，但它是
   **产品面改动** ⇒ ⇒ 按仓内纪律走独立立项 + 公开面/行为变更登记，
   **不塞进 B2**（E3 是「加覆盖」，不是「改语义」）。
3. **B 单独不够**：只登记不修 ⇒ 债留在仓里 ⇒ 但它是 A 未获批时的**兜底**。

---

## 5. 裁决点

- 选 **A**（修实现）？⇒ ⇒ 需要单独立项（建议名：`fix-fifo-full-threshold-2026-12`），
  且 MUST 同步：`examples 28/28` 复跑 + TB 的 `full` 判据**放开**到 15/16 在途
  （届时两种正确实现的差别才能被区分）+ `docs/CHANGELOG.md` 一行。
- 选 **B**（登记为已知缺陷）？⇒ ⇒ MUST 走残差登记的**两侧同批**落点
  （登记表 + 台账 `docs/architecture/**` §7 ⇒ ⇒ **architect 域** ⇒ ⇒ 需切 architect）。
- 选 **C**（只对齐口径）？⇒ ⇒ 改动落在 example 的注释与打印 ⇒ ⇒ 我可在 S 阶段顺带做。

⇒ ⇒ **未裁决前**：TB 保持现状（限定 ≤ 15 在途 + 只判 ≤ 14 处 `full==0`）⇒ ⇒ **不固化缺陷**。

---

## 6. ✅ 裁决结果（2026-11-12 · 用户「按建议推进」⇒ ⇒ **选项 A 落地**）

**采纳 A（真修）+ C（口径对齐）**：

| 项 | 处置 | 落点 |
|---|---|---|
| **A** 修 `full` 门限 | **已改**为经典方案（**地址位相等 + wrap 位不同** ⇒ 容量 = N） | `samples/fifo_example.cpp` + `examples/spinalhdl-ported/fifo/stream_fifo_example.cpp`（**两处同改**） |
| **C** 口径对齐 | 源文件注释已改写为新判定 + 旧写法的**错误原因**（门限 = `2^(addr_width+1)-1`） | 同上两处 |
| **B** 登记为已知缺陷 | **不适用**（缺陷已修 ⇒ 无需进残差登记） | — |

**追加的根因发现（比原判断更强）**：该写法**不是移植引入的** ⇒ ⇒
**上游参考 `samples/fifo_example.cpp`（`FiFo<ch_uint<2>, 2>`）用的是同一公式**
⇒ ⇒ **同型缺陷 ⇒ 两处同改**（Zero-Debt）。
⚠️ 库组件 `chlib::sync_fifo` **本身是对的**（`tests/chlib/test_fifo.cpp` 断言满态）
⇒ ⇒ 缺陷只在**手写 demo** ⇒ ⇒ 不涉及库 API 契约变更。

**TB 判据随之正面放开**（不再回避 `full` 门限）：
在途 ≤ 15 ⇒ `full == 0` · 在途 == 16 ⇒ `full == 1`（= 容量 = N · spec-derived）。

**判别力自证（5 条变异全部转红）**：

| 变异 | 实测 |
|---|---|
| M1 删掉 wrap 位条件 | `fails=1` |
| M2 读地址 `rd+1` | `fails=25` |
| M3 `empty` 粘住 | `fails=6` |
| M4 写地址 `wr+1` | `fails=27` |
| **M5 `full` 回退成旧公式** | `fails=1` ⇒ ⇒ **修复前的实现正是 M5 ⇒ ⇒ 修复可被本 TB 判出（承重）** |

**行为变更登记**：`.rddf/checks/public-api-surface.md` 的「行为变更（非符号移除）」节 +1 行
+ `docs/CHANGELOG.md` 一条。
