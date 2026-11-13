# 审稿记录：ADR-070 —— RV32I 层次化成层粒度 + 发射器兄弟边修法

- **Date**: 2026-11-13
- **产出角色**: architect（build agent）
- **审稿档位**: `arch`
- **最终结论**: ✅ **APPROVE**（critic 第 2 轮 · `mergeable: yes` · 0 BLOCKER）

---

## 1. 产出与范围

| 文件 | 动作 | 行数 |
|---|---|---:|
| `docs/adr/ADR-070-rv32i-hierarchical-layering-granularity.md` | **新建**（Status: Draft → Accepted） | 257 |
| `docs/adr/ADR-069-mem-data-alignment.md` | **追加 §7 dated addendum**（§1–§6 逐字未动） | 300 → 370 |
| `docs/adr/ADR-047-rv32i-data-forwarding.md` | **追加 §5.5.2.1 dated note**（§5.5.2 表 ③ 行逐字保留） | 658 → 690 |

---

## 2. 审稿轨迹

### 2.1 oracle 第 1 轮（2026-11-13 · 对抗式复核 · 咨询 + 审查两段）

**范围**：两个 architect 域阻塞项 —— ① D11 旁路源在 A0 下的正确指法（ADR-069 A0-③ vs capability 距离矩阵）② RV32I 层次化成层粒度（6 选项 + 「部分成层是否可接受」两问）。

**结论**：
- ✅ **确认**实施期推导的拍级表（A0 后距离 3 的生产者写回沿 = `E_i+3`、消费者 ID 读拍 = `E_i+2` ⇒ 差 1 拍 ⇒ WB 档够不着；档位 2 源必须是 M2）
- ✅ **确认**「capability 距离矩阵在 A0 下整体右移一位」⇒ ⇒ `rv32i-data-forwarding` 的 MODIFIED delta 是**必须项**
- 🔴 **纠正**：「D11 源改指 M2」按字面是**替换语义** ⇒ ⇒ 距离 4 的生产者 + **4 拍停顿后**的 load-use 消费者会**静默读旧值**
  ⇒ ⇒ 正确形态 = **两级优先旁路**（M2 档优先 + WB 档保留）⇒ ⇒ `wb_stage` 的写回 mux **MUST NOT 前移**
  （前移 ⇒ `reg_file.write` 单写口采到更年轻指令 ⇒ 真正提交的那条**永不写回**）
- ✅ **裁决 R21-B**：选 **①**（实例级 `ch_layer`）；「部分成层」= **条件式可接受**（三条件）
- ✅ **确认 R21-A 与成层粒度正交** ⇒ ⇒ D1 可先行

**落点**：oracle 的纠正 → ADR-069 §7.1 / §7.1.2；裁决 → ADR-070 §2.2 / §2.4 / §3。

### 2.2 critic 第 1 轮（`arch` 档 · 2026-11-13）

**结论**：**REJECT**（`mergeable: no`）

| # | 类别 | 内容 |
|---|---|---|
| **BLOCKER-1** | 与已签批 ADR 冲突且未走修订流程 | `ADR-047:629`（§5.5.2 ③「`wb_stage` 的写回 mux 前移一级」）⇄ `ADR-069:302`（§7.1.2「MUST NOT 前移」）= **两份 Accepted ADR 对同一实施点给出相反处方**，且 **ADR-047 侧无反向指针** ⇒ ⇒ 只读 ADR-047 的实施方会落入陷阱 |
| SHOULD_FIX ① | 决策陈述被提前冻死 | ADR-070 §2.3 把「7 个名字 = 产物级契约名」写成已定，而 §6 ⑦ 才披露「契约强度待用户裁决」 |
| SHOULD_FIX ② | 第二份权威矩阵 | ADR-069 §7.1.3 的距离矩阵表与 capability 的「距离矩阵 MUST NOT 在别处自述」Requirement 字面冲突 |
| NIT ×2 | 措辞 / 流程 | ADR-070 §6 的 oracle 轮次记录（与 ADR-066/069 同型先例 ⇒ 不阻断）· ADR-069 §7.5 的 critic 记录指向（闭环后回填） |

**critic 同时确认无问题的面**：A0-③ 两级结构本身（§7.1.1 拍级表与 §2.1 自洽；WB 档保留与 capability 现行 Requirement **不冲突** —— 其「WB→ID 组合旁路」Scenario 是**同沿条件式**表述，在 A0 下对距离 4 仍成立）· ADR-070 D2 与 ADR-046 D10 / ADR-061 / ADR-060 / ADR-059 / ADR-066 **均不冲突** · ADR-070 四要素齐备 · 「关联 change」指针合规（`backlog:` 前缀如实标注）。

### 2.3 处置

| finding | 处置 | 落点 |
|---|---|---|
| **BLOCKER-1** | ✅ **已闭** —— ① ADR-047 新增 **§5.5.2.1** dated note（表 ③ 行**逐字保留** + 两级结构表 + 「WB 档是承重件，不是遗留件」推导 + 指向 ADR-069 §7.1）② ADR-069 **§6 ③** 范围句扩大（登记「已闭：ADR-047 §5.5.2.1」⇒ 义务有**唯一落点**）③ ADR-069 **§7.5** 登记 BLOCKER 与闭法 | 三份文件 |
| SHOULD_FIX ① | ✅ 已改 —— §2.3 标题下加「⚠️ **契约强度待用户裁决**（见 §6 ⑦）⇒ ⇒ 本节给出**本 ADR 的推荐取向**，**不冻结**」 | ADR-070 |
| SHOULD_FIX ② | ✅ 已改 —— 表前加「⚠️ **下表 = MODIFIED delta 的**目标内容**（**不是**第二份权威矩阵）⇒ ⇒ 权威仍是 capability 本身 ⇒ ⇒ **delta 落地后以 capability 文本为准**」 | ADR-069 §7.1.3 |
| NIT ×2 | 🟡 NIT 1 按 critic 判定保留（与 ADR-066/069 同型先例）；NIT 2 已回填 | ADR-069 §7.5 |

### 2.4 critic 第 2 轮（复审 · `继承第 1 轮：arch`）

**结论**：✅ **APPROVE**（`mergeable: yes` · 0 BLOCKER · 0 新 SHOULD_FIX · 1 NIT）

⇒ ⇒ critic **逐一读文件核实**三项闭合证据（未采信描述）⇒ ⇒ 未引入新 BLOCKER
⇒ ⇒ **本 ADR 的 Status 转 `Accepted`**（2026-11-13）。

---

## 3. 遗留（NIT · 不阻断 · 已登记）

| # | 内容 | 处置 |
|---|---|---|
| ① | ADR-047 §5.5.2.1 的「本段是**唯一的反向指针**」字面可被读成全局唯一（ADR-069 §6 ③ 也指向） | 语境自洽（「只读本 ADR 的实施方」）· **不改** · 记 NIT |
| ② | ADR-070 §6 末尾的 oracle 轮次记录属审稿台账内容 | 与 ADR-066 / ADR-069 同型先例 · **不改** |

---

## 4. 随裁决生效的**下游义务**（不在本 ADR 的 AC 面）

| # | 义务 | 归属 |
|---|---|---|
| ① | 🔴 **capability `rv32i-data-forwarding` 的 MODIFIED delta**（距离矩阵右移一位 + 「距离 ⇔ 生产者所在级一一对应」句改写 + 档位 2 源改指 M2 + D11 两级旁路）⇒ ⇒ 载体 = change `memory-timing-contract-2026-11` 的 `specs/` | planner（delta 起草）· builder（实施） |
| ② | **ADR-047 §5.5.3 ②** 已登记同一义务 ⇒ ⇒ **两份 capability 同批** | 同上 |
| ③ | **ADR-069 §7.4 的 V14a–V16**（含「删 WB 档必红」的变异自证）= B3 实施期的验收门 | builder（实施）+ verifier（验收） |
| ④ | ADR-070 的 **D1 spike**（H-A + H-B ⇒ 探针 23 → 0）与 **D2**（7 处 `ch_layer`，**排在 B3 之后**） | builder |
| ⑤ | **ADR-069 §6 ① / 关联 change 指针**已回填为 `openspec/changes/memory-timing-contract-2026-11/` | ✅ 已闭 |

---

## 5. 复跑命令（只读）

```bash
cd /workspace/project/CppHDL
grep -rn "前移一级" docs/adr/ | cut -c1-140
grep -n "5.5.2.1\|已闭" docs/adr/ADR-047-rv32i-data-forwarding.md docs/adr/ADR-069-mem-data-alignment.md
bash scripts/probe_r21_hier_granularity.sh    # ⇒ 当前基线：ok=false + 23 条（D1 spike 的对照）
```
