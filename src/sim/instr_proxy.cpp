// src/sim/instr_proxy.cpp
#include "instr_proxy.h"
#include <cstring> // For memcpy if needed, but likely not needed now

namespace ch {

void instr_proxy::eval(const std::unordered_map<uint32_t, sdata_type>& data_map) {
    // Copy the value from the 'src' buffer (bitvector) to the 'dst' buffer (bitvector)
    // bitvector's assignment operator handles size and data copying efficiently
    *dst_ = *src_;
}

} // namespace ch
