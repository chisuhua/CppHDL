// src/sim/instr_io.cpp
#include "instr_io.h"
#include "types.h"
#include <iostream> // For potential logging if needed

namespace ch {

void instr_input::eval() {
    // Input instruction: Typically, the value in 'dst_->bv_' is set externally
    // before the eval loop runs (e.g., by sim.set_input_value() or clock driver).
    // This function is a no-op during the main eval cycle in a simple simulator.
    // The simulation framework should provide mechanisms to update 'dst_->bv_' externally.
    // For now, assume external driving and keep it as a no-op.
}

void instr_output::eval() {
    // Output instruction: Copy the value from the source buffer ('src_->bv_')
    // to the destination buffer ('dst_->bv_').
    // This makes the output's value available in the data_map_ for external retrieval
    // via sim.get_value() after the eval loop.
    *dst_ = *src_; // Use bitvector's assignment operator for efficient copy
    // The bitvector assignment handles size and data copying internally.
}

} // namespace ch