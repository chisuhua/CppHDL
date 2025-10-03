// include/sim/instr_op.h
#ifndef INSTR_OP_H
#define INSTR_OP_H

#include "instr_base.h" // Base class for instructions
#include "core/lnodeimpl.h" // For ch_op enum
#include "types.h" // For sdata_type (now using bitvector)
#include <cstdint> // For uint32_t

// Include the provided bitvector library
#include "bv/bitvector.h" // Replace with the actual path to your bitvector.h
//
using namespace ch::core;

namespace ch {

// Base class for operation instructions
// Holds pointers to destination and source buffers (sdata_type, which is now bitvector)
class instr_op : public instr_base {
public:
    // Constructor takes pointers to destination and source sdata_type (bitvector) objects
    instr_op(sdata_type* dst, uint32_t size, sdata_type* src0, sdata_type* src1)
        : instr_base(size), dst_(dst), src0_(src0), src1_(src1) {}

    // Pure virtual eval function to be implemented by derived classes
    //void eval(const std::unordered_map<uint32_t, sdata_type>& data_map) override; // Pass data_map for lookups

protected:
    sdata_type* dst_;  // Pointer to the destination sdata_type object (bitvector) for the result
    sdata_type* src0_; // Pointer to the first source sdata_type object (bitvector) (LHS)
    sdata_type* src1_; // Pointer to the second source sdata_type object (bitvector) (RHS)
};

// --- Concrete Instruction Classes for Specific Operations ---

class instr_op_add : public instr_op {
public:
    instr_op_add(sdata_type* dst, uint32_t size, sdata_type* src0, sdata_type* src1)
        : instr_op(dst, size, src0, src1) {}

    void eval(const std::unordered_map<uint32_t, sdata_type>& data_map) override; // Pass data_map for lookups
};

class instr_op_sub : public instr_op {
public:
    instr_op_sub(sdata_type* dst, uint32_t size, sdata_type* src0, sdata_type* src1)
        : instr_op(dst, size, src0, src1) {}

    void eval(const std::unordered_map<uint32_t, sdata_type>& data_map) override; // Pass data_map for lookups
};

class instr_op_mul : public instr_op {
public:
    instr_op_mul(sdata_type* dst, uint32_t size, sdata_type* src0, sdata_type* src1)
        : instr_op(dst, size, src0, src1) {}

    void eval(const std::unordered_map<uint32_t, sdata_type>& data_map) override;
};

class instr_op_and : public instr_op {
public:
    instr_op_and(sdata_type* dst, uint32_t size, sdata_type* src0, sdata_type* src1)
        : instr_op(dst, size, src0, src1) {}

    void eval(const std::unordered_map<uint32_t, sdata_type>& data_map) override;
};

class instr_op_or : public instr_op {
public:
    instr_op_or(sdata_type* dst, uint32_t size, sdata_type* src0, sdata_type* src1)
        : instr_op(dst, size, src0, src1) {}

    void eval(const std::unordered_map<uint32_t, sdata_type>& data_map) override;
};

class instr_op_xor : public instr_op {
public:
    instr_op_xor(sdata_type* dst, uint32_t size, sdata_type* src0, sdata_type* src1)
        : instr_op(dst, size, src0, src1) {}

    void eval(const std::unordered_map<uint32_t, sdata_type>& data_map) override;
};

// --- Unary Operation Base Class (e.g., NOT) ---
class instr_op_unary : public instr_base {
public:
    instr_op_unary(sdata_type* dst, uint32_t size, sdata_type* src)
        : instr_base(size), dst_(dst), src_(src) {}

    //void eval(const std::unordered_map<uint32_t, sdata_type>& data_map) override;

protected:
    sdata_type* dst_;
    sdata_type* src_;
};

class instr_op_not : public instr_op_unary {
public:
    instr_op_not(sdata_type* dst, uint32_t size, sdata_type* src)
        : instr_op_unary(dst, size, src) {}

    void eval(const std::unordered_map<uint32_t, sdata_type>& data_map) override;
};

// --- Comparison Operation Base Class (Result is 1-bit) ---
class instr_op_cmp : public instr_op {
public:
    instr_op_cmp(sdata_type* dst, uint32_t size, sdata_type* src0, sdata_type* src1) // size is typically 1 for comparison
        : instr_op(dst, size, src0, src1) {}

    // eval is still pure virtual, implemented by derived comparison classes
    //void eval(const std::unordered_map<uint32_t, sdata_type>& data_map) override;
};

class instr_op_eq : public instr_op_cmp {
public:
    instr_op_eq(sdata_type* dst, uint32_t size, sdata_type* src0, sdata_type* src1)
        : instr_op_cmp(dst, size, src0, src1) {}

    void eval(const std::unordered_map<uint32_t, sdata_type>& data_map) override;
};

class instr_op_ne : public instr_op_cmp {
public:
    instr_op_ne(sdata_type* dst, uint32_t size, sdata_type* src0, sdata_type* src1)
        : instr_op_cmp(dst, size, src0, src1) {}

    void eval(const std::unordered_map<uint32_t, sdata_type>& data_map) override;
};

class instr_op_lt : public instr_op_cmp {
public:
    instr_op_lt(sdata_type* dst, uint32_t size, sdata_type* src0, sdata_type* src1)
        : instr_op_cmp(dst, size, src0, src1) {}

    void eval(const std::unordered_map<uint32_t, sdata_type>& data_map) override;
};

class instr_op_le : public instr_op_cmp {
public:
    instr_op_le(sdata_type* dst, uint32_t size, sdata_type* src0, sdata_type* src1)
        : instr_op_cmp(dst, size, src0, src1) {}

    void eval(const std::unordered_map<uint32_t, sdata_type>& data_map) override;
};

class instr_op_gt : public instr_op_cmp {
public:
    instr_op_gt(sdata_type* dst, uint32_t size, sdata_type* src0, sdata_type* src1)
        : instr_op_cmp(dst, size, src0, src1) {}

    void eval(const std::unordered_map<uint32_t, sdata_type>& data_map) override;
};

class instr_op_ge : public instr_op_cmp {
public:
    instr_op_ge(sdata_type* dst, uint32_t size, sdata_type* src0, sdata_type* src1)
        : instr_op_cmp(dst, size, src0, src1) {}

    void eval(const std::unordered_map<uint32_t, sdata_type>& data_map) override;
};

} // namespace ch

#endif // INSTR_OP_H
