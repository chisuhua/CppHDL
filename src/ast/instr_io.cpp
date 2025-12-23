// src/sim/instr_io.cpp
#include "instr_io.h"
#include "types.h"
#include <iostream> // For potential logging if needed

namespace ch {

void instr_input::eval() {
    // for top level input, simulator driving it, other wise driving from src
    *dst_ = *src_;
}

void instr_output::eval() {
    // Output instruction: Copy the value from the source buffer ('src_->bv_')
    // to the destination buffer ('dst_->bv_').
    // This makes the output's value available in the data_map_ for external
    // retrieval via sim.get_value() after the eval loop.
    *dst_ = *src_; // Use bitvector's assignment operator for efficient copy
    // The bitvector assignment handles size and data copying internally.
}

} // namespace ch