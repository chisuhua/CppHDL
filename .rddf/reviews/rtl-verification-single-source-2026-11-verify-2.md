# 第二轮复核 —— `rtl-verification-single-source-2026-11`

- **验收者**：verifier（`source: user` · session `ses_e123fdf50ffekNVuofg777ESL3`）
- **日期**：2026-11-19 · **HEAD**：`3f84e34`（工作区干净）
- **已用轮次**：**2 / 3**
- **复核范围（不是全量重审）**：第 1 轮判 ❌ 的 1 条（**AC18**）+ 判 ⚠️ 的 2 条（**AC17** · **AC23 ②**）+ 本轮新增 diff 是否引入新 BLOCKER。
- **结论**：**AC18 ✅ · AC17 ✅ · AC23 ② ✅ ⇒ 23/23 条全部成立 ⇒ 本 change 可归档**

## 一、逐条判定 + 证据

| AC | 第 1 轮 | 本轮 | 证据（本轮实跑） |
|---|---|---|---|
| **AC18** | ❌（`implementation_gap`） | ✅ | ① **精确目标集**（`examples/axi4/**` + `.rddf/checks/{examples,soc}-rtl-expectations.md`）的 `grep "只发扁平\|top_unnamed_input"` ⇒ **零命中**（`examples/axi4/AGENTS.md:27` 改为「未具名编号形态已退役」）；② `grep -n "产品面已具名" docs/usage_guide/05-component-design.md` ⇒ 命中 `:1018`；③ **通用机制教学段保留** ⇒ `grep -c top_unnamed_input` = 5，命中行 = **78 / 522 / 533 / 698 / 702** ⇒ 全部落在 §1.3 / §5.1 / §5.3.3（= 订正后 AC18 白名单的现行表述）⇒ §5.8「端口怎么找」段**零命中**（与「§5.8 移出白名单」自洽）；④ **对照表 vs 实际产物**：文档 12 行 vs SoC 产物 `manifest.port_table` 的 12 个数据端口 ⇒ **集合相等**（逐条比对无差异） |
| **AC17** | ⚠️（`proposal_drift`） | ✅ | dated addendum 已加（proposal:115）：`1415` 是**本 change 交付时**的终值（`git show 08298a0~1:.rddf/checks/port-naming-exempt.tsv` 实测 = **1415** ✓）；现值 = **1384**（`08298a0` 的 A7 退役 `axi4_full_example`，−31 行）；护栏锚点 `EXPECTED_EXEMPT_PORTS=1384`；`verify_port_naming.sh` ⇒ **PASS** ⇒ 当前态判据成立 |
| **AC23 ②** | ⚠️（`proposal_drift`） | ✅ | 口径澄清已加（proposal:121）：本条的「端口名」= **产物端口名**（由 C++ 成员名派生 ⇒ 改它 = 1 处）；**若指 `.verif` 引用的 C++ 成员名** ⇒ 改它要动 **C++ + `.verif`** 共 2 处（实测**行内总出现**口径：`awvalid` **18** 次 · `bvalid` **26** 次）⇒ 两种读法都已写明 |

## 二、新增 diff 是否引入新 BLOCKER

本轮增量 = 4 笔（`73cb612` / `efc383e` / `8df27c6` / `3f84e34`），diff 面 = **文档 + proposal 措辞 + 验收报告**，**零 `src/**`、零 `include/**`、零 tests**。

| 检查 | 结果 |
|---|---|
| `docs/usage_guide/05-component-design.md` 改动是否破坏别处引用 | 护栏 `verify_agents_size.sh` / `verify_openspec_delta.sh` / `verify_example_only_headers.sh` ⇒ 全 PASS；`openspec validate <change>`（非 strict）⇒ valid |
| `examples/axi4/AGENTS.md` 改动 | 只改一句措辞，语义不变（`top_<C++ 成员名>` 不变） |
| proposal 的 3 处 AC 文本改动 | 只加 dated 订正，**判据集合未变**；delta / tasks / 实现 / 蓝图均未动 |
| 是否引入了新的「跳过 / 放行」 | 无 |

## 三、遗留清扫（本次范围内）

| 项 | 结果 |
|---|---|
| TODO / 半截实现 / 调试日志 | 本轮 4 笔零新增代码 ⇒ 不适用（沿用第 1 轮的清扫结论：新增文件零未完成实现） |
| 被 skip 的测试 | 无新增 |
| 未替换的 mock / 临时 fallback | 无 |
| 临时文件 | `/tmp/opencode/vfy/**`（未跟踪） |

## 四、归档门状态（**全部满足**）

| 门 | 状态 |
|---|---|
| AC 全部成立 | ✅ **23/23**（第 1 轮 20 ✅ + 本轮闭合 3 条） |
| `openspec validate <change> --strict` | exit 1 —— **仅** 2 条 `>500 字符` requirement 的 strict-only 规则，仓级既有红（AGENTS.md 已登记该 CLI 漂移），**与本 change 无关**；**非 strict** = **valid** |
| `scripts/verify_openspec_delta.sh` | ✅ **4 条全绿** |
| delta 与主 spec 冲突 | ✅ **零**：3 个 ADDED 的标题均不与主 spec 撞名；2 个 MODIFIED 标题逐字匹配；`examples-rtl-sim-coverage` = 新建（7 reqs） |

⇒ ⇒ **可归档**。归档后 `openspec/specs/examples-rtl-sim-coverage/` 出现 ⇒ **change 1 的 G-1 前置满足**。

## 五、复现命令

```bash
# AC18
grep -rn "只发扁平\|top_unnamed_input" examples/axi4/ \
  .rddf/checks/examples-rtl-expectations.md .rddf/checks/soc-rtl-expectations.md     # ⇒ 空
grep -c "产品面已具名" docs/usage_guide/05-component-design.md                        # ⇒ 1
grep -n "top_unnamed_input" docs/usage_guide/05-component-design.md                  # ⇒ 78/522/533/698/702（全在教学段）

# AC17
git show 08298a0~1:.rddf/checks/port-naming-exempt.tsv | grep -vcE '^\s*(#|$)'      # ⇒ 1415
grep -vcE '^\s*(#|$)' .rddf/checks/port-naming-exempt.tsv                            # ⇒ 1384
bash scripts/verify_port_naming.sh                                                   # ⇒ PASS

# AC23 ②
grep -o awvalid examples/axi4/tb_axi4_lite_example.verif | wc -l                     # ⇒ 18
grep -o bvalid examples/axi4/tb_axi4_lite_example.verif | wc -l                      # ⇒ 26
```
