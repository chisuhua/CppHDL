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

    // Perform truncating assignment - copy next value to current buffer with proper width handling
    *current_buf = sdata_type(static_cast<uint64_t>(*next_buf), reg_width);

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

// New overload for dual-map evaluation
void instr_reg::eval(const ch::data_map_t& read_map, ch::data_map_t& write_map) {
    CHDBG_FUNC();
    
    // --- DEBUG PRINTS (Optional, can be disabled for performance) ---
    CHDBG("=== EVAL START (IDs: curr=%u, next=%u, size=%u) ===", 
          current_node_id_, next_node_id_, this->size());
    // --- END DEBUG PRINTS ---

    // 1. Safety check: Ensure node IDs are valid keys in data_map
    auto current_it = read_map.find(current_node_id_);
    auto next_it = read_map.find(next_node_id_);

    if (current_it == read_map.end()) {
        CHERROR("Current node ID %u not found in read_map!", current_node_id_);
        return;
    }
    if (next_it == read_map.end()) {
        CHERROR("Next node ID %u not found in read_map!", next_node_id_);
        return;
    }

    // 2. Get references/pointers to the sdata_type buffers using the valid iterators
    // Read from the read_map but write to the write_map
    const sdata_type* current_buf = &current_it->second;  // Read from read_map
    const sdata_type* next_buf = &next_it->second;        // Read from read_map
    
    // Find the write location in the write_map
    auto write_it = write_map.find(current_node_id_);
    if (write_it == write_map.end()) {
        CHERROR("Current node ID %u not found in write_map!", current_node_id_);
        return;
    }
    sdata_type* write_buf = &write_it->second;            // Write to write_map

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

    // 3. Copy the next value to the write buffer with proper width handling
    const uint32_t reg_width = this->size();
    *write_buf = sdata_type(static_cast<uint64_t>(*next_buf), reg_width);
    
    // --- DEBUG PRINTS (AFTER ASSIGNMENT) ---
    if (write_buf) {
        CHDBG("--- WRITE BUFFER STATE (AFTER) ---");
        const auto& write_bv = write_buf->bv_;
        if (write_bv.num_words() > 0) {
            CHDBG("write_buf->bv_.words()[0] (AFTER): 0x%016llx", 
                  static_cast<unsigned long long>(write_bv.words()[0]));
            CHDBG("write_buf->bv_ as uint64_t (AFTER): %llu", 
                  static_cast<unsigned long long>(static_cast<uint64_t>(write_bv)));
        }
    }
    CHDBG("=== EVAL END ===");
    // --- END DEBUG PRINTS ---
}
} // namespace ch