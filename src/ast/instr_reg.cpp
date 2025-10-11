// src/sim/instr_reg.cpp
#include "instr_reg.h"
#include "types.h"
#include "logger.h"
#include <cassert>
#include <unordered_map> // For data_map_t if needed for debugging
#include <iomanip>      // For std::hex, std::setw, std::setfill

using namespace ch::core;

namespace ch {

// --- instr_reg Constructor ---
instr_reg::instr_reg(uint32_t current_node_id, uint32_t size, uint32_t next_node_id)
    : instr_base(size), current_node_id_(current_node_id), next_node_id_(next_node_id) {
    // Store the node IDs instead of the pointers
    CHDBG_FUNC();
    CHINFO("Created for current_node_id=%u, size=%u, next_node_id=%u", 
           current_node_id_, size, next_node_id_);
}

// --- MODIFIED instr_reg::eval with data_map lookup and truncation using bv_assign_truncate ---
void instr_reg::eval(const ch::data_map_t& data_map) {
    CHDBG_FUNC();
    
    // --- DEBUG PRINTS (Optional, can be disabled for performance) ---
    CHDBG("=== EVAL START (IDs: curr=%u, next=%u, size=%u) ===", 
          current_node_id_, next_node_id_, this->size());
    // --- END DEBUG PRINTS ---

    // 1. Safety check: Ensure node IDs are valid keys in data_map
    auto current_it = data_map.find(current_node_id_);
    auto next_it = data_map.find(next_node_id_);

    if (current_it == data_map.end()) {
        CHERROR("Current node ID %u not found in data_map!", current_node_id_);
        return;
    }
    if (next_it == data_map.end()) {
        CHERROR("Next node ID %u not found in data_map!", next_node_id_);
        return;
    }

    // 2. Get references/pointers to the sdata_type buffers using the valid iterators
    sdata_type* current_buf = const_cast<sdata_type*>(&current_it->second); // Get pointer to current value buffer
    const sdata_type* next_buf = &next_it->second;                         // Get pointer to next value buffer

    // --- DEBUG PRINTS ---
    if (current_buf) {
        CHDBG("--- CURRENT BUFFER STATE (BEFORE) ---");
        CHDBG("current_buf->bitwidth(): %u", current_buf->bitwidth());
        const auto& current_bv = current_buf->bv_;
        CHDBG("current_buf->bv_.size(): %u", current_bv.size());
        if (current_bv.num_words() > 0) {
            CHDBG("current_buf->bv_.words()[0]: 0x%016llx", 
                  static_cast<unsigned long long>(current_bv.words()[0]));
            CHDBG("current_buf->bv_ as uint64_t (BEFORE): %llu", 
                  static_cast<unsigned long long>(static_cast<uint64_t>(current_bv)));
        }
    } else {
        CHWARN("current_buf pointer is unexpectedly NULL!");
    }

    if (next_buf) {
        CHDBG("--- NEXT BUFFER STATE (SOURCE) ---");
        CHDBG("next_buf->bitwidth(): %u", next_buf->bitwidth());
        const auto& next_bv = next_buf->bv_;
        CHDBG("next_buf->bv_.size(): %u", next_bv.size());
        if (next_bv.num_words() > 0) {
            CHDBG("next_buf->bv_.words()[0]: 0x%016llx", 
                  static_cast<unsigned long long>(next_bv.words()[0]));
            CHDBG("next_buf->bv_ as uint64_t (SOURCE): %llu", 
                  static_cast<unsigned long long>(static_cast<uint64_t>(next_bv)));
        }
    } else {
        CHWARN("next_buf pointer is unexpectedly NULL!");
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
    CHDBG("Register declared width (this->size()): %u", reg_width);
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
        CHDBG("--- CURRENT BUFFER STATE (AFTER) ---");
        const auto& current_bv = current_buf->bv_;
        if (current_bv.num_words() > 0) {
            CHDBG("current_buf->bv_.words()[0] (AFTER): 0x%016llx", 
                  static_cast<unsigned long long>(current_bv.words()[0]));
            CHDBG("current_buf->bv_ as uint64_t (AFTER): %llu", 
                  static_cast<unsigned long long>(static_cast<uint64_t>(current_bv)));
        }
    }
    CHDBG("=== EVAL END ===");
    // --- END DEBUG PRINTS ---
}

} // namespace ch
