// include/sim/instr_clock.h
#ifndef INSTR_CLOCK_H
#define INSTR_CLOCK_H

#include "core/types.h"
#include "instr_base.h"
#include <cstdint>

namespace ch {

class instr_clock : public instr_base {
public:
    instr_clock(ch::core::sdata_type *clock_buf, bool is_posedge,
                bool is_negedge);
    void eval() override;

    // 获取当前时钟状态
    bool is_posedge_active() const { return posedge_active_; }
    bool is_negedge_active() const { return negedge_active_; }

    // 获取上一时钟状态
    bool last_clock_value() const { return last_clk_; }

private:
    ch::core::sdata_type *clock_buf_;
    bool is_posedge_;
    bool is_negedge_;
    bool last_clk_;
    bool posedge_active_;
    bool negedge_active_;
};

} // namespace ch

#endif // INSTR_CLOCK_H