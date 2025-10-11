// include/core/types.h
#ifndef CORE_TYPES_H
#define CORE_TYPES_H

#include <cstdint>
#include <string>
#include <iostream>
#include <unordered_map>

// Include the provided bitvector library
#include "bv/bitvector.h"

namespace ch { namespace core {
    struct sdata_type;
    
    // Forward declarations for operators
    sdata_type operator+(const sdata_type& lhs, const sdata_type& rhs);
    sdata_type operator-(const sdata_type& lhs, const sdata_type& rhs);
    sdata_type operator*(const sdata_type& lhs, const sdata_type& rhs);
    sdata_type operator&(const sdata_type& lhs, const sdata_type& rhs);
    sdata_type operator|(const sdata_type& lhs, const sdata_type& rhs);
    sdata_type operator^(const sdata_type& lhs, const sdata_type& rhs);
    sdata_type operator~(const sdata_type& operand);
    sdata_type operator<<(const sdata_type& lhs, uint32_t rhs);
    sdata_type operator>>(const sdata_type& lhs, uint32_t rhs);
    bool operator==(const sdata_type& lhs, const sdata_type& rhs);
    bool operator!=(const sdata_type& lhs, const sdata_type& rhs);
    bool operator<(const sdata_type& lhs, const sdata_type& rhs);
    bool operator<=(const sdata_type& lhs, const sdata_type& rhs);
    bool operator>(const sdata_type& lhs, const sdata_type& rhs);
    bool operator>=(const sdata_type& lhs, const sdata_type& rhs);

// Minimal core structure to hold simulation data values
struct sdata_type {
    using block_t = uint64_t;
    ch::internal::bitvector<block_t> bv_;

    // Minimal constructors
    sdata_type(block_t value, uint32_t width) : bv_(width) {
        bv_ = value;
    }

    sdata_type() : bv_() {}
    explicit sdata_type(uint32_t width) : bv_(width) {}

    // Essential accessors
    uint32_t bitwidth() const { return bv_.size(); }
    bool is_zero() const { return bv_.is_zero(); }

    // Essential assignment operators
    sdata_type& operator=(const sdata_type& other) {
        if (this != &other) {
            bv_ = other.bv_;
        }
        return *this;
    }

    template<typename U>
    sdata_type& operator=(U value) {
        bv_ = value;
        return *this;
    }

    // Essential accessors for the underlying bitvector
    ch::internal::bitvector<uint64_t>& bitvector() { return bv_; }
    const ch::internal::bitvector<uint64_t>& bitvector() const { return bv_; }

    // Essential type conversion
    explicit operator uint64_t() const {
        if (bv_.num_words() > 0) {
            return bv_.words()[0];
        }
        return 0;
    }

    // ========== Extended method declarations (to be implemented in utils/types.h) ==========
    // String conversion methods
    std::string to_string() const;
    std::string to_string_dec() const;
    std::string to_string_hex() const;
    std::string to_string_bin() const;
    std::string to_bitstring() const;
    std::string to_string_verbose() const;
    
    // Extended convenience methods
    bool is_one() const;
    bool is_ones() const;
    bool is_neg() const;
    bool get_bit(uint32_t index) const;
    void set_bit(uint32_t index, bool value);
    bool is_value(uint64_t value) const;
    bool msb() const;
    bool lsb() const;
    void reset();

private:
    // Friend declarations
    friend sdata_type operator+(const sdata_type& lhs, const sdata_type& rhs);
    friend sdata_type operator-(const sdata_type& lhs, const sdata_type& rhs);
    friend sdata_type operator*(const sdata_type& lhs, const sdata_type& rhs);
    friend sdata_type operator&(const sdata_type& lhs, const sdata_type& rhs);
    friend sdata_type operator|(const sdata_type& lhs, const sdata_type& rhs);
    friend sdata_type operator^(const sdata_type& lhs, const sdata_type& rhs);
    friend sdata_type operator~(const sdata_type& operand);
    friend sdata_type operator<<(const sdata_type& lhs, uint32_t rhs);
    friend sdata_type operator>>(const sdata_type& lhs, uint32_t rhs);
    friend bool operator==(const sdata_type& lhs, const sdata_type& rhs);
    friend bool operator!=(const sdata_type& lhs, const sdata_type& rhs);
    friend bool operator<(const sdata_type& lhs, const sdata_type& rhs);
    friend bool operator<=(const sdata_type& lhs, const sdata_type& rhs);
    friend bool operator>(const sdata_type& lhs, const sdata_type& rhs);
    friend bool operator>=(const sdata_type& lhs, const sdata_type& rhs);
    
    // Friend for stream output
    friend std::ostream& operator<<(std::ostream& os, const sdata_type& sdata);
};

// ========== Minimal Constants ==========
namespace constants {
    inline const sdata_type empty{};
    inline const sdata_type zero_8bit{0, 8};
    inline const sdata_type one_8bit{1, 8};
    
    inline const sdata_type& empty_singleton() {
        static const sdata_type instance;
        return instance;
    }
    
    inline const sdata_type& zero(uint32_t width = 1) {
        static thread_local std::unordered_map<uint32_t, sdata_type> zero_cache;
        auto it = zero_cache.find(width);
        if (it == zero_cache.end()) {
            it = zero_cache.emplace(width, sdata_type(0, width)).first;
        }
        return it->second;
    }
    
    // Extended constants (declarations)
    extern const sdata_type zero_1bit;
    extern const sdata_type zero_16bit;
    extern const sdata_type zero_32bit;
    extern const sdata_type zero_64bit;
    extern const sdata_type one_1bit;
    extern const sdata_type one_16bit;
    extern const sdata_type one_32bit;
    extern const sdata_type one_64bit;
    extern const sdata_type all_ones_8bit;
    extern const sdata_type all_ones_16bit;
    extern const sdata_type all_ones_32bit;
    
    const sdata_type& ones(uint32_t width);
};

}} // namespace ch::core

// Include extended functionality
#include "../utils/types.h"

#endif // CORE_TYPES_H
