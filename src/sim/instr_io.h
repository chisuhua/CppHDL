// include/sim/instr_io.h
#ifndef INSTR_IO_H
#define INSTR_IO_H

#include "instr_base.h"
#include "types.h" // For sdata_type
#include <cstdint> // For uint32_t



using namespace ch::core;

namespace ch {

// Instruction for input nodes (placeholder, actual value set externally)
// Might be a no-op during eval, or could update its buffer from an external source.
// In a simple simulator, this often just holds the buffer representing the input port's value.
class instr_input : public instr_base {
public:
    // Constructor takes a pointer to the destination buffer (representing the input port)
    instr_input(sdata_type* dst, uint32_t size)
        : instr_base(size), dst_(dst) {}

    // eval() is typically a no-op for input instructions in a basic simulator.
    // The value in 'dst_' is expected to be set externally before the simulation step (eval() loop).
    // However, if there's an external mechanism to drive the input during eval, it would go here.
    void eval(const std::unordered_map<uint32_t, sdata_type>& data_map) override; 

private:
    sdata_type* dst_; // Pointer to the buffer representing the input port's value
};

// Instruction for output nodes (placeholder, actual value read externally)
// Might be a no-op during eval, or could drive an external output.
// In a simple simulator, this often just copies the value from its source to its destination buffer.
class instr_output : public instr_base {
public:
    // Constructor takes pointers to the destination buffer (representing the output port)
    // and the source buffer (the value to be output).
    instr_output(sdata_type* dst, uint32_t size, sdata_type* src)
        : instr_base(size), dst_(dst), src_(src) {} // Store source for potential driving

    // eval() typically copies the value from the source buffer ('src_') to the destination buffer ('dst_').
    // This makes the output's value available in the data_map_ for external retrieval.
    void eval(const std::unordered_map<uint32_t, sdata_type>& data_map) override; 

private:
    sdata_type* dst_; // Pointer to the buffer representing the output port's value
    sdata_type* src_; // Pointer to the source buffer whose value is output
};

} // namespace ch

#endif // INSTR_IO_H
