// include/sim/instr_io.h
#ifndef INSTR_IO_H
#define INSTR_IO_H

#include "instr_base.h"
#include <cstdint>

namespace ch {

class instr_input : public instr_base {
public:
    instr_input(ch::core::sdata_type* dst, uint32_t size)
        : instr_base(size), dst_(dst) {}

    void eval() override;

private:
    ch::core::sdata_type* dst_;
};

class instr_output : public instr_base {
public:
    instr_output(ch::core::sdata_type* dst, uint32_t size, ch::core::sdata_type* src)
        : instr_base(size), dst_(dst), src_(src) {}

    void eval() override;

private:
    ch::core::sdata_type* dst_;
    ch::core::sdata_type* src_;
};

} // namespace ch

#endif // INSTR_IO_H