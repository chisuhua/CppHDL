// include/sim/instr_proxy.h
#ifndef INSTR_PROXY_H
#define INSTR_PROXY_H

#include "instr_base.h"
#include <cstdint>

namespace ch {

class instr_proxy : public instr_base {
public:
    instr_proxy(ch::core::sdata_type* dst, uint32_t size, ch::core::sdata_type* src)
        : instr_base(size), dst_(dst), src_(src) {}

    void eval(const ch::data_map_t& data_map) override;

private:
    ch::core::sdata_type* dst_;
    ch::core::sdata_type* src_;
};

} // namespace ch

#endif // INSTR_PROXY_H
