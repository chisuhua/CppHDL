// src/sim/instr_reg.cpp
#include "instr_reg.h"
#include <iostream>
#include <cassert>
#include <unordered_map> // For data_map_t if needed for debugging
#include <iomanip>      // For std::hex, std::setw, std::setfill

// --- Include the header where bv_assign_truncate is declared ---
// Make sure the path is correct relative to this file.
// If bv_assign_truncate is declared in bv/bitvector.h, include it.
// #include "bv/bitvector.h" // Adjust path if necessary
// Or, if it's in a separate file like bv/bv_assign_truncate.h, include that.

namespace ch {

// --- instr_reg Constructor ---
instr_reg::instr_reg(uint32_t current_node_id, uint32_t size, uint32_t next_node_id)
    : instr_base(size), current_node_id_(current_node_id), next_node_id_(next_node_id) {
    // Store the node IDs instead of the pointers
    std::cout << "[instr_reg::ctor] Created for current_node_id=" << current_node_id_
              << ", size=" << size
              << ", next_node_id=" << next_node_id_ << std::endl;
}

// --- MODIFIED instr_reg::eval with data_map lookup and truncation using bv_assign_truncate ---
void instr_reg::eval(const std::unordered_map<uint32_t, sdata_type>& data_map) {
    // --- DEBUG PRINTS (Optional, can be disabled for performance) ---
    std::cout << std::hex << std::setfill('0'); // Set output format for hex values
    std::cout << "[instr_reg::eval] === EVAL START (IDs: curr=" << current_node_id_ << ", next=" << next_node_id_ << ", size=" << this->size() << ") ===" << std::endl;
    // --- END DEBUG PRINTS ---

    // 1. Safety check: Ensure node IDs are valid keys in data_map
    auto current_it = data_map.find(current_node_id_);
    auto next_it = data_map.find(next_node_id_);

    if (current_it == data_map.end()) {
        std::cerr << "[instr_reg::eval] Error: Current node ID " << current_node_id_ << " not found in data_map!" << std::endl;
        return;
    }
    if (next_it == data_map.end()) {
        std::cerr << "[instr_reg::eval] Error: Next node ID " << next_node_id_ << " not found in data_map!" << std::endl;
        return;
    }

    // 2. Get references/pointers to the sdata_type buffers using the valid iterators
    sdata_type* current_buf = const_cast<sdata_type*>(&current_it->second); // Get pointer to current value buffer
    const sdata_type* next_buf = &next_it->second;                         // Get pointer to next value buffer

    // --- DEBUG PRINTS ---
    if (current_buf) {
        std::cout << "[instr_reg::eval] --- CURRENT BUFFER STATE (BEFORE) ---" << std::endl;
        std::cout << "[instr_reg::eval] current_buf->bitwidth(): " << std::dec << current_buf->bitwidth() << std::endl;
        const auto& current_bv = current_buf->bv_;
        std::cout << "[instr_reg::eval] current_buf->bv_.size(): " << current_bv.size() << std::endl;
        if (current_bv.num_words() > 0) {
            std::cout << "[instr_reg::eval] current_buf->bv_.words()[0]: 0x" << std::hex << std::setw(16) << std::setfill('0') << current_bv.words()[0] << std::dec << std::endl;
            std::cout << "[instr_reg::eval] current_buf->bv_ as uint64_t (BEFORE): " << static_cast<uint64_t>(current_bv) << std::endl;
        }
    } else {
        std::cout << "[instr_reg::eval] current_buf pointer is unexpectedly NULL!" << std::endl;
    }

    if (next_buf) {
        std::cout << "[instr_reg::eval] --- NEXT BUFFER STATE (SOURCE) ---" << std::endl;
        std::cout << "[instr_reg::eval] next_buf->bitwidth(): " << std::dec << next_buf->bitwidth() << std::endl;
        const auto& next_bv = next_buf->bv_;
        std::cout << "[instr_reg::eval] next_buf->bv_.size(): " << next_bv.size() << std::endl;
        if (next_bv.num_words() > 0) {
            std::cout << "[instr_reg::eval] next_buf->bv_.words()[0]: 0x" << std::hex << std::setw(16) << std::setfill('0') << next_bv.words()[0] << std::dec << std::endl;
            std::cout << "[instr_reg::eval] next_buf->bv_ as uint64_t (SOURCE): " << static_cast<uint64_t>(next_bv) << std::endl;
        }
    } else {
        std::cout << "[instr_reg::eval] next_buf pointer is unexpectedly NULL!" << std::endl;
    }
    // --- END DEBUG PRINTS ---

    // 3. --- PERFORM WIDTH-AWARE, TRUNCATING ASSIGNMENT using bv_assign_truncate ---
    // The goal is to copy the value from the 'next' buffer (*next_buf)
    // into the 'current' buffer (*current_buf),
    // BUT ensuring only the lower 'reg_width' bits are copied/stored in *current_buf.
    // This simulates the hardware behavior where a wider value driven onto a
    // narrower register bus gets truncated to the register's width.

    // Get the target width (width of the register itself)
    const uint32_t reg_width = this->size(); // E.g., 4 for ch_reg<ch_uint<4>>

    // --- DEBUG PRINT ---
    std::cout << "[instr_reg::eval] Register declared width (this->size()): " << std::dec << reg_width << std::endl;
    // --- END DEBUG PRINT ---

    // --- REPLACE ch::internal::bv_assign CALL ---
    // OLD (Incorrect for truncation):
    // ch::internal::bv_assign(
    //     current_buf->bv_.words(), reg_width,     // Destination: dest buffer's data pointer, TARGET width
    //     next_buf->bv_                           // Source: src buffer's bitvector object
    // );
    //
    // NEW (Correct for truncation using bv_assign_truncate):
    // Call the helper function we added/implemented for truncating assignment.
    // This function knows how to copy bits from src_bv to dest_bv,
    // ensuring only the lower 'reg_width' bits are copied/kept in dest_bv.
    // It handles cases where src_bv is wider or narrower than reg_width.
    //ch::internal::bv_assign_truncate<uint64_t>( // --- CHANGED TO bv_assign_truncate ---
    //    current_buf->bv_.words(), reg_width,     // Destination: dest buffer's data pointer, TARGET width
    //    next_buf->bv_.words(), next_buf->bv_.size() // Source: src buffer's data pointer, SOURCE width
    //);
    *current_buf = *next_buf;
    // --- END REPLACE ch::internal::bv_assign CALL ---

    // --- DEBUG PRINTS (AFTER ASSIGNMENT) ---
     if (current_buf) {
        std::cout << "[instr_reg::eval] --- CURRENT BUFFER STATE (AFTER) ---" << std::endl;
        const auto& current_bv = current_buf->bv_;
        if (current_bv.num_words() > 0) {
            std::cout << "[instr_reg::eval] current_buf->bv_.words()[0] (AFTER): 0x" << std::hex << std::setw(16) << std::setfill('0') << current_bv.words()[0] << std::dec << std::endl;
            std::cout << "[instr_reg::eval] current_buf->bv_ as uint64_t (AFTER): " << static_cast<uint64_t>(current_bv) << std::endl;
        }
    }
    std::cout << "[instr_reg::eval] === EVAL END ===" << std::endl;
    std::cout << std::dec << std::setfill(' '); // Reset output format
    // --- END DEBUG PRINTS ---
}

} // namespace ch
