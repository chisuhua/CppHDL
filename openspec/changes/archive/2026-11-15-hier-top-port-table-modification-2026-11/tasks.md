# Tasks — hier-top-port-table-modification-2026-11

- [x] 1 从主 spec **抽取**旧 Requirement 全文（逐字）⇒ 只改**一个 Scenario**
- [x] 2 新 Scenario 写死：① 根段逐字逐序（不含 inline 后代）② 其余 ⊆ ③ 扁平 MAY 多出成层
      `ch_out` ④ **排他句** ⑤ **机械切法**（`PortProvenanceOrigin`）
- [x] 3 `proposal.md` 记 **F1 的根因**（镜像单向漂移）与**能力面零收缩**的论证
- [x] 4 `scripts/verify_openspec_delta.sh` 全绿（标题逐字 / 无占位符 / MODIFIED 匹配 / tasks 可达）
- [x] 5 `openspec validate --specs` 绿
- [x] 6 `openspec archive`（**禁** `--skip-specs`）⇒ ⇒ 主 spec 的旧句被替换
- [x] 7 grep 复核：主 spec 里「顶层端口表 … 逐字逐序相同」的旧句**零命中**
