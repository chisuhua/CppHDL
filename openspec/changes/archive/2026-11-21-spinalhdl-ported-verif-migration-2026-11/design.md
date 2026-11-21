# Design — `spinalhdl-ported-verif-migration-2026-11`

> 决策面：ADR-074（`.verif` 单一真相源）· ADR-076（历史/序列原语）· ADR-075 E1（端口具名）。
> 本文件只写**实现细则**与**逐 target 的判据映射**。

## 1 三角色分离（每个 target 同一形态）

```
<dir>/<name>_top.h        设计定义（无 main；端口全部具名）
<dir>/tb_<name>.cpp       激励驱动：默认模式读 .verif 跑断言；--emit-rtl = 发射 RTL/TB
<dir>/tb_<name>.verif     验证源（唯一语义来源）
<dir>/tb_<name>.sv        生成式独立 TB（由 --emit-tb-verif 生成，**禁手改**）
```

`main()` 的固定骨架（8 份同构）：模式剥离 → 发射模式 fail-closed（`--out` 缺失 ⇒ exit 2）→
验证模式 fail-closed（未知参数 ⇒ exit 2）→ `run_verif_file` → FAIL 行进 stderr →
`RESULT: checks=N fails=M`（单行十进制）。

## 2 逐 target 的判据映射（原形态 → `.verif` 形态）

| target | 原手写 TB 的判据 | `.verif` 形态 | 说明 |
|---|---|---|---|
| `counter_simple` | 256 拍绝对计数 + 周期 256（相对） | `WAIT value low` 锚回绕点 + 255 拍绝对；512 记录 + `EXPECT_SEQ_SELF value 256 256` | 无输入可锚 ⇒ 锚**输出回绕点**（相位无关） |
| `counter` | 逐拍绝对 + 40 拍伪随机 `Δ==enable` | 关键点绝对 + `EXPECT_SEQ_SELF value 63 1`（静止性）+ `EXPECT_SEQ_STEP value enable 40` | STEP 的「同采样点」前提：每拍 `RECORD value` → `SET enable <下一拍>` → `SETTLE` → `RECORD enable`（ADR-076 D5） |
| `pwm` | 逐拍绝对 + 「256 拍内高电平数 == duty」 | 段长（先 `WAIT low` 再 `WAIT high`）+ 整周期 `EXPECT_SEQ_COUNT` | **`WAIT` 是电平不是边沿**：起跑已为高时单条 `WAIT high` 会立刻命中（实测红过） |
| `uart_tx` | 边沿时间戳测位宽 ⇒ 9 段等宽 | 434 锚定位窗 + 每窗首/尾两拍断言 | **方法变更**：`.verif` 无时间戳；判别力不降（每位被钉在恰好 434），代价 = 434 成为 impl-contract |
| `uart_rx` | 字节解码 + 采样相位夹逼（1/3、2/3 探针） | 同形态（`prescale=99` + 3 探针）+ `valid` 单拍性 | D6 的**唯一**判别力仍在探针（`prescale=9` 时 `div==4` 与 `prescale/2` 重合 ⇒ 普通字节对 D6 结构性失明） |
| `fifo` | 4 场景（含容量边界 16） | 同形态 + `EXPECT_SEQ_*` 用于序列恒等 | D5 回归 = 在途 15/16 的 `full` |
| `gpio` | 6 场景（impl-contract） | 同形态 + `EXPECT_SEQ_COUNT`（恒定输入无中断） | `spec_checks=0`（D1 裁决「移植简化」） |
| `quadrature_encoder` | 5 场景 | 同形态 + 往返恒等 | **确定性起点**：框架复位把 A/B tie-off 为 0 ⇒ 每场景先用 **Z 上升沿**清零 |

## 3 实施期发现的两条框架缺陷（已修 · 判据载体在 proposal 的 F1/F2）

- **F1 `WIDTHEXPAND`**：`hold_val` / 历史数组是 `logic [63:0]`，窄端口直接赋 ⇒ 护栏 ④
  （禁 `-Wno-fatal`）下 **TB 编译失败**。`EXPECT_STABLE` 此前**零单元消费者**（已迁产物的
  `EXPECT_STABLE` 只出现在 `.verif` 注释行）⇒ 属**结构性失明**。修法 = `u64_rhs()` 零扩展。
- **F2 寄存器 INIT**：`Simulator::reset()` 清所有节点 ⇒ `uart_rx` 的 `idle`/`rx_d`
  （INIT=true）起在「非空闲」⇒ 解码全错。修法 = 按 `regimpl::get_init_val()` 回填
  （⚠️ **不是**驱动 reset 节点：`build_register` 的 `rst_val` 是 nullptr ⇒ 解释器复位期间
  只 return、不赋值）。

## 4 已知局限（登记，不修）

1. `EXPECT` 字面量 MUST 落在端口位宽内（渲染侧截断 vs 解释器按 64 位比较 ⇒ 越界值两端相反）。
2. 周期 256 抓不到步长 2（128 整除 256）。
3. UART TX 位宽 434 变成 impl-contract（改位宽必须同步改 `.verif`）。
4. `EXPECT_STABLE` 的 N 条结论共用**同一个** `check_id`（原语级 id，非逐拍 id）——
   与 `EXPECT` 的逐条命名不同（历史行为，未改）。
