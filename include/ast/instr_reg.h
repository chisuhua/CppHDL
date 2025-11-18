// include/sim/instr_reg.h
#ifndef INSTR_REG_H
#define INSTR_REG_H

#include "instr_base.h"
#include <cstdint>

namespace ch {

class instr_reg : public instr_base {
public:
    instr_reg(uint32_t current_node_id, uint32_t size, uint32_t next_node_id);
    void eval(const ch::data_map_t& data_map) override;
    
    // New overload for dual-map evaluation
    void eval(const ch::data_map_t& read_map, ch::data_map_t& write_map) override;
    
    uint32_t current_node_id() const { return current_node_id_; }
    uint32_t next_node_id() const { return next_node_id_; }

private:
    uint32_t current_node_id_;
    uint32_t next_node_id_;
};

} // namespace ch

#endif // INSTR_REG_H