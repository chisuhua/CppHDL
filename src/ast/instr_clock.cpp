// src/sim/instr_clock.cpp
#include "instr_clock.h"
#include "logger.h"

namespace ch {
// --- instr_clock Constructor ---
instr_clock::instr_clock(ch::core::sdata_type *clock_buf, bool is_posedge,
                         bool is_negedge)
    : instr_base(1), clock_buf_(clock_buf), is_posedge_(is_posedge),
      is_negedge_(is_negedge), last_clk_(false), posedge_active_(false),
      negedge_active_(false) {
    CHDBG_FUNC();

    // 初始化last_clk_为当前时钟值（如果有时钟）
    if (clock_buf_) {
        last_clk_ = !clock_buf_->is_zero();
    }

    CHINFO("Created clock instruction, posedge=%d, negedge=%d", is_posedge_,
           is_negedge_);
}

// --- instr_clock::eval ---
void instr_clock::eval() {
    CHDBG_FUNC();

    // 重置边沿状态
    posedge_active_ = false;
    negedge_active_ = false;

    // 检查时钟边沿
    *clock_buf_ = ch::core::sdata_type(clock_buf_->is_zero() ? 1 : 0,
                                       clock_buf_->bitwidth());
}
} // namespace ch