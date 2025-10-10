// src/sim/instr_io.cpp
#include "instr_io.h"
#include "types.h"
#include <iostream> // For potential logging if needed
#include <cstring>  // For memcpy if needed for direct copying (though bitvector assignment is preferred)

namespace ch {

void instr_input::eval(const ch::data_map_t& data_map) {
    // Input instruction: Typically, the value in 'dst_->bv_' is set externally
    // before the eval(const std::unordered_map<uint32_t, sdata_type>& data_map) loop runs (e.g., by sim.set_input_value() or clock driver).
    // This function is a no-op during the main eval(const std::unordered_map<uint32_t, sdata_type>& data_map) cycle in a simple simulator.
    // The simulation framework should provide mechanisms to update 'dst_->bv_' externally.
    // For now, assume external driving and keep it as a no-op.
    // std::cout << "[instr_input::eval] Input eval called for buffer ID: " << /* some ID if available */ << std::endl;
}

void instr_output::eval(const ch::data_map_t& data_map) {
    // Output instruction: Copy the value from the source buffer ('src_->bv_')
    // to the destination buffer ('dst_->bv_').
    // This makes the output's value available in the data_map_ for external retrieval
    // via sim.get_value() after the eval(const std::unordered_map<uint32_t, sdata_type>& data_map) loop.
    *dst_ = *src_; // Use bitvector's assignment operator for efficient copy
    // The bitvector assignment handles size and data copying internally.
    // No need for explicit memcpy or size checks here if src_ and dst_ are correctly sized.
}

} // namespace ch
