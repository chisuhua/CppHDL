// include/sim/instr_reg.h
#ifndef INSTR_REG_H
#define INSTR_REG_H

#include "instr_base.h"
#include "core/types.h"
#include <cstdint>

// 前向声明
namespace ch::core {
    class context;
}

namespace ch {

class instr_reg : public instr_base {
public:
    instr_reg(ch::core::sdata_type* current_buf, uint32_t size, ch::core::sdata_type* next_buf,
              ch::core::sdata_type* clk_edge, ch::core::sdata_type* clk_en_buf, 
              ch::core::sdata_type* rst_buf, ch::core::sdata_type* rst_val_buf);
    void eval() override;
    
    uint32_t current_node_id() const { return current_node_id_; }
    uint32_t next_node_id() const { return next_node_id_; }

private:
    ch::core::sdata_type* current_buf_;
    ch::core::sdata_type* next_buf_;
    ch::core::sdata_type* clk_edge_;
    ch::core::sdata_type* clk_en_buf_;
    ch::core::sdata_type* rst_buf_;
    ch::core::sdata_type* rst_val_buf_;
    uint32_t current_node_id_;
    uint32_t next_node_id_;
};

} // namespace ch

#endif // INSTR_REG_H