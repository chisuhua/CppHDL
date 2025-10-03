// types.h
#ifndef TYPES_H
#define TYPES_H

#include <cstdint>
#include <vector>
#include <string>
#include <source_location> // C++20

// Include the provided bitvector library
#include "bv/bitvector.h" // Replace with the actual path to your bitvector.h
//
namespace ch { namespace core {
    struct sdata_type;
    // Declare the overloaded operators as friends so they can access private/protected members if needed
    // (Though sdata_type is a struct, so members are public by default.
    //  Declaring them as friends is good practice if they need direct access to bv_).
    sdata_type operator+(const sdata_type& lhs, const sdata_type& rhs);
    sdata_type operator-(const sdata_type& lhs, const sdata_type& rhs);
    sdata_type operator*(const sdata_type& lhs, const sdata_type& rhs);
    sdata_type operator&(const sdata_type& lhs, const sdata_type& rhs);
    sdata_type operator|(const sdata_type& lhs, const sdata_type& rhs);
    sdata_type operator^(const sdata_type& lhs, const sdata_type& rhs);
    sdata_type operator~(const sdata_type& operand);
    // Add others as needed (/, %, ==, !=, <, >, etc.)

// Structure to hold simulation data values and IR literal values
// Now uses ch::internal::bitvector<uint64_t> directly.
struct sdata_type {
    using block_t = uint64_t;
    // Use bitvector as the underlying data type
    ch::internal::bitvector<block_t> bv_;

    // Constructor from a raw value and bitwidth (delegates to bitvector constructor)
    sdata_type(block_t value, uint32_t width) : bv_(width) {
        bv_ = value; // Use bitvector's assignment operator to set the initial value
    }

    // Default constructor (creates a 0-bit vector, often resized later)
    sdata_type() : bv_() {}

    // Constructor from bit width only (initializes to zero)
    explicit sdata_type(uint32_t width) : bv_(width) {}

    // Get the bit width (delegates to bitvector's size method)
    uint32_t bitwidth() const { return bv_.size(); }

    // Check if the value is zero (delegates to bitvector's is_zero method)
    bool is_zero() const { return bv_.is_zero(); }

    // Equality operator (delegates to bitvector's operator==)
    /*
    bool operator==(const sdata_type& other) const {
        return bv_ == other.bv_;
    }
    */

    // Assignment operator (delegates to bitvector's operator=)
    sdata_type& operator=(const sdata_type& other) {
        if (this != &other) { // Self-assignment check
            bv_ = other.bv_;
        }
        return *this;
    }

    // Assignment operator from literal (delegates to bitvector's operator=)
    template<typename U>
    sdata_type& operator=(U value) {
        bv_ = value; // Uses bitvector's assignment from integral type
        return *this;
    }

    // Accessor for the underlying bitvector (if needed for direct manipulation)
    ch::internal::bitvector<uint64_t>& bitvector() { return bv_; }
    const ch::internal::bitvector<uint64_t>& bitvector() const { return bv_; }

    // Optional: Add common checks from bitvector
    bool is_one() const { return bv_.is_one(); }
    bool is_ones() const { return bv_.is_ones(); }
    bool is_neg() const { return bv_.is_neg(); }

    // --- NEW: Friend declarations for overloaded operators ---
    // These allow the operators defined outside the struct to access private/protected members.
    // Since sdata_type is a struct, members are public, but it's good practice.
    friend sdata_type operator+(const sdata_type& lhs, const sdata_type& rhs);
    friend sdata_type operator-(const sdata_type& lhs, const sdata_type& rhs);
    friend sdata_type operator*(const sdata_type& lhs, const sdata_type& rhs);
    friend sdata_type operator&(const sdata_type& lhs, const sdata_type& rhs);
    friend sdata_type operator|(const sdata_type& lhs, const sdata_type& rhs);
    friend sdata_type operator^(const sdata_type& lhs, const sdata_type& rhs);
    friend sdata_type operator~(const sdata_type& operand);
    friend bool operator==(const sdata_type& lhs, const sdata_type& rhs);
};

// --- Helper macro to reduce boilerplate for binary operators ---
#define CH_SDATA_BINARY_OP_TRUNCATE(op_name, op_func, bv_op_func) \
inline sdata_type operator op_name(const sdata_type& lhs, const sdata_type& rhs) { \
    sdata_type result(0, lhs.bitwidth()); \
    bv_op_func(&result.bv_, &lhs.bv_, &rhs.bv_); \
    return result; \
}

// --- Arithmetic Operations ---
CH_SDATA_BINARY_OP_TRUNCATE(+, add, ch::internal::bv_add_truncate<uint64_t>)
CH_SDATA_BINARY_OP_TRUNCATE(-, sub, ch::internal::bv_sub_truncate<uint64_t>)
CH_SDATA_BINARY_OP_TRUNCATE(*, mul, ch::internal::bv_mul_truncate<uint64_t>)
CH_SDATA_BINARY_OP_TRUNCATE(/, div, ch::internal::bv_div_truncate<uint64_t>)
CH_SDATA_BINARY_OP_TRUNCATE(%, mod, ch::internal::bv_mod_truncate<uint64_t>)

// --- Bitwise Operations ---
CH_SDATA_BINARY_OP_TRUNCATE(&, and, ch::internal::bv_and_truncate<uint64_t>)
CH_SDATA_BINARY_OP_TRUNCATE(|, or, ch::internal::bv_or_truncate<uint64_t>)
CH_SDATA_BINARY_OP_TRUNCATE(^, xor, ch::internal::bv_xor_truncate<uint64_t>)

// --- Unary Bitwise NOT ---
inline sdata_type operator~(const sdata_type& operand) {
    sdata_type result(0, operand.bitwidth()); // Create result with operand width
    ch::internal::bv_inv_truncate<uint64_t>(&result.bv_, &operand.bv_);
    return result;
}
/*
inline bool operator==(const sdata_type& lhs, const sdata_type& rhs) {
    bool cmp_result = ch::internal::bv_eq_truncate<uint64_t>(lhs.bv_, rhs.bv_);
    return cmp_result;
}
*/

// --- Comparison Operations (Result is always 1-bit boolean) ---
#define CH_SDATA_COMPARISON_OP(op_name, op_func, bv_op_func) \
inline bool operator op_name(const sdata_type& lhs, const sdata_type& rhs) { \
    bool cmp_result = bv_op_func<uint64_t>(lhs.bv_, rhs.bv_); \
    return cmp_result; \
}

CH_SDATA_COMPARISON_OP(==, eq, ch::internal::bv_eq_truncate)
CH_SDATA_COMPARISON_OP(!=, ne, ch::internal::bv_ne_truncate)
CH_SDATA_COMPARISON_OP(<, lt, ch::internal::bv_lt_truncate)
CH_SDATA_COMPARISON_OP(<=, le, ch::internal::bv_le_truncate)
CH_SDATA_COMPARISON_OP(>, gt, ch::internal::bv_gt_truncate)
CH_SDATA_COMPARISON_OP(>=, ge, ch::internal::bv_ge_truncate)


// --- Shift Operations ---
// Left shift (<<)
inline sdata_type operator<<(const sdata_type& lhs, uint32_t rhs) {
    sdata_type result(0, lhs.bitwidth()); // Create result with LHS width
    // Perform the left shift operation using the underlying bitvector library's function
    // This function handles the shift distance and operand width.
    // The result.width() should match lhs.bitwidth().
    // The bv_shl_truncate function is assumed to be the width-aware truncate version.
    ch::internal::bv_shl_truncate<uint64_t>(&result.bv_, &lhs.bv_, rhs);
    // bv_shl_truncate should handle truncation/extension (though for shl, width usually stays same)
    // and ensure the result in result.bv_.words() conforms to result.bitwidth().
    // bv_clear_extra_bits is typically called inside bv_shl_truncate.
    return result;
}

// Right shift (>>)
inline sdata_type operator>>(const sdata_type& lhs, uint32_t rhs) {
    sdata_type result(0, lhs.bitwidth()); // Create result with LHS width
    ch::internal::bv_shr_truncate<uint64_t>(&result.bv_, &lhs.bv_, rhs);
    return result;
}


}} // namespace ch

#endif // TYPES_H
