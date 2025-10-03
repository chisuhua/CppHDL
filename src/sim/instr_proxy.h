// include/sim/instr_proxy.h
#ifndef INSTR_PROXY_H
#define INSTR_PROXY_H

#include "instr_base.h"
#include "types.h" // For sdata_type
#include <cstdint> // For uint32_t

using namespace ch::core;

namespace ch {

// Instruction for proxy nodes (e.g., outputimpl connecting to proxyimpl)
// Copies the value from the 'src' buffer to the 'dst' buffer on eval.
class instr_proxy : public instr_base {
public:
    // Constructor takes pointers to destination and source buffers
    instr_proxy(sdata_type* dst, uint32_t size, sdata_type* src)
        : instr_base(size), dst_(dst), src_(src) {}

    void eval(const std::unordered_map<uint32_t, sdata_type>& data_map) override; 

private:
    sdata_type* dst_; // Pointer to the destination buffer
    sdata_type* src_; // Pointer to the source buffer
};

} // namespace ch

#endif // INSTR_PROXY_H
