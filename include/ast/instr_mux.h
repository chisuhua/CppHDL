// include/instr/mux.h
#pragma once

#include "instr_base.h"
#include "core/types.h"
#include <iostream>

namespace ch {

class instr_mux : public instr_base {
public:
    instr_mux(ch::core::sdata_type* dst, uint32_t size,
              ch::core::sdata_type* cond, 
              ch::core::sdata_type* true_val, 
              ch::core::sdata_type* false_val)
        : instr_base(size), dst_(dst), cond_(cond), true_val_(true_val), false_val_(false_val) {}

    void eval() override {
        if (!dst_ || !cond_ || !true_val_ || !false_val_) {
            std::cerr << "[MUX] Error: Null pointer encountered!" << std::endl;
            return;
        }

        // Perform the mux operation: if cond != 0 then true_val else false_val
        //
        // ⚠️⚠️ **MUST 用 `assign_truncate`，MUST NOT 用 `*dst_ = *分支`**（2026-11-03 修复）
        //
        // 理由与 `instr_output::eval`（`src/ast/instr_io.cpp`）**完全相同** ——
        //   `bitvector::operator=` 会**把 dst 的宽度改成源的宽度**（RESIZE），
        //   而 `assign_truncate` 保持 dst 的声明宽度（源被截断 / 零扩展）。
        //
        // ⇒ ⇒ `mux` 的 node 宽度 = `max(真分支宽, 假分支宽)`（`build_mux`），
        //      而**较窄的那个分支**一旦被选中，`*dst_ = <窄分支>` 就会把
        //      整个 mux 节点的槽位**压窄** ⇒ ⇒ 下游按**声明宽度**读它
        //      （如 `bit_select(mux_result, 2)`）⇒ ⇒ `operand_width` 变小
        //      ⇒ ⇒ **比特索引越界**（实测 `operand_width=1, idx=2`）。
        //
        // ⚠️ **该缺陷的表现形态极具误导性**（2026-11-03 实测，B2）：
        //      - 只在 **interpreter** 侧出现（Verilog 的 `assign d = c ? t : f;`
        //        天然保持声明宽度 ⇒ ⇒ **这是两后端分歧**）；
        //      - 是否报错取决于 `eval_combinational` 的**求值顺序**
        //        （`mux` 与消费它的 `bit_select` 谁先）⇒ ⇒ 换个激励 / 多个 DUT
        //        （node id 全局唯一 ⇒ 拓扑序 tie-break 变）就可能**时好时坏**；
        //      - 越界分支会打印错误并**写 0** ⇒ ⇒ 若落在被断言路径上 ⇒ ⇒
        //        **静默的错误值**（本仓「假绿」的典型形态）。
        //
        // ⇒ ⇒ 同一惯例的其它三处（MUST 保持一致）：
        //      `instr_proxy::eval`（`src/ast/instr_proxy.cpp`）·
        //      `instr_reg::eval`（`src/ast/instr_reg.cpp`）·
        //      JIT 的 `STORE_DATA` mask（`src/jit/jit_compiler.cpp`）。
        if (!cond_->is_zero()) {
            dst_->assign_truncate(*true_val_);
        } else {
            dst_->assign_truncate(*false_val_);
        }
    }

private:
    ch::core::sdata_type* dst_;
    ch::core::sdata_type* cond_;
    ch::core::sdata_type* true_val_;
    ch::core::sdata_type* false_val_;
};

} // namespace ch
