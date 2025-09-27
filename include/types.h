#ifndef TYPES_H
#define TYPES_H

#include <cstdint>
#include <vector>
#include <string>
#include <source_location> // C++20

// --- Structure to hold simulation data values (借鉴 cash 的 sdata_type) ---
// This represents the value of a literal or a simulation buffer entry.
// A simple implementation using a vector of uint64_t blocks.
struct sdata_type {
    std::vector<uint64_t> blocks;
    uint32_t bitwidth_; // Renamed member variable to avoid conflict

    // Constructor from a raw value and bitwidth
    sdata_type(uint64_t value, uint32_t width) : bitwidth_(width) {
        // Calculate number of 64-bit blocks needed
        uint32_t num_blocks = (width + 63) / 64; // Ceiling division
        blocks.resize(num_blocks, 0); // Initialize blocks to 0

        if (num_blocks > 0) {
            blocks[0] = value; // Place the value in the first block
            // If width < 64, the upper bits of blocks[0] should be masked,
            // but for initial literal creation, this might be sufficient.
            // More complex logic might be needed for general value assignment.
        }
    }

    // Default constructor
    sdata_type() : bitwidth_(0) {}

    // Get the bit width (member function)
    uint32_t bitwidth() const { return bitwidth_; }

    // Check if the value is zero
    bool is_zero() const {
        for (const auto& block : blocks) {
            if (block != 0) return false;
        }
        return true;
    }

    // Equality operator (for lnodeimpl::equals)
    bool operator==(const sdata_type& other) const {
        return this->bitwidth_ == other.bitwidth_ && this->blocks == other.blocks;
    }

    // Assignment operator (if needed for simulation buffer updates)
    // sdata_type& operator=(const sdata_type& other) { ... }
};

#endif // TYPES_H
