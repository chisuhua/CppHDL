// src/sim/instr_op.cpp
#include "instr_op.h"
#include <iostream>
#include <vector>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <unordered_map>
#include <iomanip>

// Include the bitvector library if specific functions (like bv_not_vector) are needed
// and not available through bitvector's operators.
// #include "bv/bitvector.h" // Adjust path if necessary

namespace ch {

// --- Implementation of eval() methods for concrete instruction classes ---
// These implementations now operate on sdata_type::bv_ (bitvector), using the data_map parameter.

// --- Binary Operations ---

void instr_op_add::eval(const std::unordered_map<uint32_t, sdata_type>& data_map) {
    // Perform operation directly on the bitvector members
    // The operands (src0_, src1_) and destination (dst_) are sdata_type* pointers
    // that were resolved during Simulator::initialize and are assumed valid.
    // The data_map parameter is passed but not used directly here as operands are stored as pointers.
    // If operands were stored as IDs, we would look them up in data_map.
    if (!dst_ || !src0_ || !src1_) {
        std::cerr << "[instr_op_add::eval] Error: Null pointer encountered!" << std::endl;
        return;
    }
    //dst_->bv_ = src0_->bv_ + src1_->bv_;

    // --- DEBUG PRINTS ---
    std::cout << "[instr_op_add::eval] --- OPERANDS STATE (BEFORE) ---" << std::endl;
    std::cout << "[instr_op_add::eval] dst_->bitwidth(): " << std::dec << dst_->bitwidth() << std::endl;
    std::cout << "[instr_op_add::eval] src0_->bitwidth(): " << std::dec << src0_->bitwidth() << std::endl;
    std::cout << "[instr_op_add::eval] src1_->bitwidth(): " << std::dec << src1_->bitwidth() << std::endl;
    const auto& dst_bv = dst_->bv_;
    const auto& src0_bv = src0_->bv_;
    const auto& src1_bv = src1_->bv_;
    std::cout << "[instr_op_add::eval] dst_->bv_.size(): " << dst_bv.size() << std::endl;
    std::cout << "[instr_op_add::eval] src0_->bv_.size(): " << src0_bv.size() << std::endl;
    std::cout << "[instr_op_add::eval] src1_->bv_.size(): " << src1_bv.size() << std::endl;
    if (src0_bv.num_words() > 0) {
        std::cout << "[instr_op_add::eval] src0_->bv_.words()[0]: 0x" << std::hex << std::setw(16) << std::setfill('0') << src0_bv.words()[0] << std::dec << std::endl;
        std::cout << "[instr_op_add::eval] src0_->bv_ as uint64_t (BEFORE): " << static_cast<uint64_t>(src0_bv) << std::endl;
        std::cout << "[instr_op_add::eval] src0_ bitstring: " << to_bitstring(src0_bv) << std::endl;
    }
    if (src1_bv.num_words() > 0) {
        std::cout << "[instr_op_add::eval] src1_->bv_.words()[0]: 0x" << std::hex << std::setw(16) << std::setfill('0') << src1_bv.words()[0] << std::dec << std::endl;
        std::cout << "[instr_op_add::eval] src1_->bv_ as uint64_t (BEFORE): " << static_cast<uint64_t>(src1_bv) << std::endl;
        std::cout << "[instr_op_add::eval] src1_ bitstring: " << to_bitstring(src1_bv) << std::endl;
    }

    //bv_add_truncate<uint64_t>(&dst_->bv_, &src0_->bv_, &src1_->bv_);
    *dst_ = *src0_ + *src1_;
    // --- END PERFORM THE OPERATION ---

    // --- DEBUG PRINTS (AFTER OPERATION) ---
    std::cout << "[instr_op_add::eval] --- RESULT STATE (AFTER) ---" << std::endl;
    const auto& result_bv = dst_->bv_; // Get the result bitvector after the operation
    std::cout << "[instr_op_add::eval] dst_->bv_.size() (AFTER): " << result_bv.size() << std::endl;
    if (result_bv.num_words() > 0) {
        std::cout << "[instr_op_add::eval] dst_->bv_.words()[0] (AFTER): 0x" << std::hex << std::setw(16) << std::setfill('0') << result_bv.words()[0] << std::dec << std::endl;
        std::cout << "[instr_op_add::eval] dst_->bv_ as uint64_t (AFTER): " << static_cast<uint64_t>(result_bv) << std::endl;
        std::cout << "[instr_op_add::eval] dst_ bitstring (AFTER): " << to_bitstring(result_bv) << std::endl;
    }
    std::cout << "[instr_op_add::eval] === EVAL END ===" << std::endl;
    std::cout << std::dec << std::setfill(' '); // Reset output format
}

void instr_op_sub::eval(const std::unordered_map<uint32_t, sdata_type>& data_map) {
    if (!dst_ || !src0_ || !src1_) {
        std::cerr << "[instr_op_sub::eval] Error: Null pointer encountered!" << std::endl;
        return;
    }
    *dst_ = *src0_ - *src1_;
}

void instr_op_mul::eval(const std::unordered_map<uint32_t, sdata_type>& data_map) {
    if (!dst_ || !src0_ || !src1_) {
        std::cerr << "[instr_op_mul::eval] Error: Null pointer encountered!" << std::endl;
        return;
    }
    *dst_ = *src0_ * *src1_;
}

void instr_op_and::eval(const std::unordered_map<uint32_t, sdata_type>& data_map) {
    if (!dst_ || !src0_ || !src1_) {
        std::cerr << "[instr_op_and::eval] Error: Null pointer encountered!" << std::endl;
        return;
    }
    *dst_ = *src0_ & *src1_;
}

void instr_op_or::eval(const std::unordered_map<uint32_t, sdata_type>& data_map) {
    if (!dst_ || !src0_ || !src1_) {
        std::cerr << "[instr_op_or::eval] Error: Null pointer encountered!" << std::endl;
        return;
    }
    *dst_ = *src0_ | *src1_;
}

void instr_op_xor::eval(const std::unordered_map<uint32_t, sdata_type>& data_map) {
    if (!dst_ || !src0_ || !src1_) {
        std::cerr << "[instr_op_xor::eval] Error: Null pointer encountered!" << std::endl;
        return;
    }
    *dst_ = *src0_ ^ *src1_;
}

// --- Unary Operations ---

void instr_op_not::eval(const std::unordered_map<uint32_t, sdata_type>& data_map) {
    if (!dst_ || !src_) {
        std::cerr << "[instr_op_not::eval] Error: Null pointer encountered!" << std::endl;
        return;
    }
    // Bitvector might not have a direct unary ~ operator.
    // Use bitvector's size and bitwise NOT on its internal representation or use XOR with all 1s.
    // Assuming bitvector has a constructor that takes size and initial value (like -1 for all 1s).
    // The size of the result should match the size of the source.
    ch::internal::bitvector<uint64_t> bv_ones(src_->bv_.size(), static_cast<uint64_t>(-1)); // Create all 1s vector of same size
    //dst_->bv_ = src_->bv_ ^ bv_ones; // Perform bitwise XOR with all 1s to get NOT
    // Alternative (if bitvector supports unary ~):
    *dst_ = ~*src_;
}

// --- Comparison Operations ---

void instr_op_eq::eval(const std::unordered_map<uint32_t, sdata_type>& data_map) {
    // Comparison result is always 1 bit wide
    if (!dst_ || !src0_ || !src1_) {
        std::cerr << "[instr_op_eq::eval] Error: Null pointer encountered!" << std::endl;
        return;
    }
    if (dst_->bitwidth() != 1) {
        std::cerr << "[instr_op_eq::eval] Error: Destination bitvector size must be 1 for comparison!" << std::endl;
        *dst_ = false; // Default to false
        return;
    }
    bool result_val = (*src0_ == *src1_);
    *dst_ = result_val ? 1 : 0; // Set the bitvector to 0 or 1
}

void instr_op_ne::eval(const std::unordered_map<uint32_t, sdata_type>& data_map) {
    if (!dst_ || !src0_ || !src1_) {
        std::cerr << "[instr_op_ne::eval] Error: Null pointer encountered!" << std::endl;
        return;
    }
    if (dst_->bitwidth() != 1) {
        std::cerr << "[instr_op_ne::eval] Error: Destination bitvector size must be 1 for comparison!" << std::endl;
        *dst_ = false; // Default to false
        return;
    }
    bool result_val = (*src0_ != *src1_);
    *dst_ = result_val ? 1 : 0; // Set the bitvector to 0 or 1
}

void instr_op_lt::eval(const std::unordered_map<uint32_t, sdata_type>& data_map) {
    if (!dst_ || !src0_ || !src1_) {
        std::cerr << "[instr_op_lt::eval] Error: Null pointer encountered!" << std::endl;
        return;
    }
    if (dst_->bitwidth() != 1) {
        std::cerr << "[instr_op_ne::eval] Error: Destination bitvector size must be 1 for comparison!" << std::endl;
        *dst_ = false; // Default to false
        return;
    }
    bool result_val = (*src0_ < *src1_); // Uses bitvector's operator<
    *dst_ = result_val ? 1 : 0; // Set the bitvector to 0 or 1
}

void instr_op_le::eval(const std::unordered_map<uint32_t, sdata_type>& data_map) {
    if (!dst_ || !src0_ || !src1_) {
        std::cerr << "[instr_op_le::eval] Error: Null pointer encountered!" << std::endl;
        return;
    }
    if (dst_->bitwidth() != 1) {
        std::cerr << "[instr_op_ne::eval] Error: Destination bitvector size must be 1 for comparison!" << std::endl;
        *dst_ = false; // Default to false
        return;
    }
    bool result_val = (*src0_ <= *src1_); // Uses bitvector's operator<=
    *dst_ = result_val ? 1 : 0; // Set the bitvector to 0 or 1
}

void instr_op_gt::eval(const std::unordered_map<uint32_t, sdata_type>& data_map) {
    if (!dst_ || !src0_ || !src1_) {
        std::cerr << "[instr_op_gt::eval] Error: Null pointer encountered!" << std::endl;
        return;
    }
    if (dst_->bitwidth() != 1) {
        std::cerr << "[instr_op_ne::eval] Error: Destination bitvector size must be 1 for comparison!" << std::endl;
        *dst_ = false; // Default to false
        return;
    }
    bool result_val = (*src0_ > *src1_); // Uses bitvector's operator>
    *dst_ = result_val ? 1 : 0; // Set the bitvector to 0 or 1
}

void instr_op_ge::eval(const std::unordered_map<uint32_t, sdata_type>& data_map) {
    if (!dst_ || !src0_ || !src1_) {
        std::cerr << "[instr_op_ge::eval] Error: Null pointer encountered!" << std::endl;
        return;
    }
    if (dst_->bitwidth() != 1) {
        std::cerr << "[instr_op_ne::eval] Error: Destination bitvector size must be 1 for comparison!" << std::endl;
        *dst_ = false; // Default to false
        return;
    }
    bool result_val = (*src0_ >= *src1_); // Uses bitvector's operator>=
    *dst_ = result_val ? 1 : 0; // Set the bitvector to 0 or 1
}

} // namespace ch
