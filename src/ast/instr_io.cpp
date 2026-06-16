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
    // Output instruction: copy the value from the source buffer ('src_') to
    // the destination buffer ('dst_'). Use assign_truncate (not operator=) so
    // the destination's bitwidth is preserved and the source is truncated or
    // zero-extended to fit. This matches:
    //   - instr_proxy::eval (src/ast/instr_proxy.cpp) for the proxy path
    //   - instr_reg::eval (src/ast/instr_reg.cpp) for register propagation
    //   - JIT STORE_DATA (src/jit/jit_compiler.cpp:686) which masks the stored
    //     value to the ch_out's declared bitwidth via store_node
    // Using plain `*dst_ = *src_` would call bitvector::operator= which RESIZES
    // the destination to the source's size — a divergence from JIT when the
    // source is wider than the declared ch_out (e.g. ch_uint<8> + ch_uint<8>
    // = 9-bit sum → ch_out<ch_uint<8>> would silently widen to 9 bits).
    // The bitvector assignment handles size and data copying internally;
    // assign_truncate handles that plus the required bitwidth clamping.
    dst_->assign_truncate(*src_);
}

} // namespace ch