// include/sim/instr_reg.h
#ifndef INSTR_REG_H
#define INSTR_REG_H

#include "instr_base.h"
#include "types.h" // For sdata_type
#include <cstdint> // For uint32_t

namespace ch {

// Forward declaration
namespace core {
    class lnodeimpl; // Need the node ID
}
// Instruction for register update
// Copies the value from the 'next' buffer to the 'current' buffer on eval.
class instr_reg : public instr_base {
public:
    instr_reg(uint32_t current_node_id, uint32_t size, uint32_t next_node_id);
    // Constructor takes pointers to the 'current' value buffer and the 'next' value buffer

    void eval(const std::unordered_map<uint32_t, sdata_type>& data_map) override; // Pass data_map for lookups
    uint32_t current_node_id() const { return current_node_id_; }
    uint32_t next_node_id() const { return next_node_id_; }

private:
    uint32_t current_node_id_; // NEW: ID of the node holding the register's current value (e.g., regimpl node ID 5)
    uint32_t next_node_id_; 
};

} // namespace ch

#endif // INSTR_REG_H
