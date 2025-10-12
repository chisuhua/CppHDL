// include/utils/types.h
#ifndef UTILS_TYPES_H
#define UTILS_TYPES_H

// This file is included by core/types.h to provide extended functionality
// Users should include core/types.h which automatically includes this file

#include <sstream>
#include <iostream>
#include <iomanip>

namespace ch { 
namespace core {
    
// ========== Extended sdata_type Method Implementations ==========
inline std::string sdata_type::to_string() const {
    if (bv_.size() == 0) {
        return "0";
    }
    
    std::ostringstream oss;
    oss << bv_;
    return oss.str();
}

inline std::string sdata_type::to_string_dec() const {
    if (bv_.size() == 0) {
        return "0";
    }
    
    try {
        if (bv_.size() <= 64) {
            std::ostringstream oss;
            uint64_t value = static_cast<uint64_t>(*this);
            oss << value;
            return oss.str();
        }
        return "<large_value_" + std::to_string(bv_.size()) + "bits>";
    } catch (...) {
        return "<dec_error>";
    }
}

inline std::string sdata_type::to_string_hex() const {
    if (bv_.size() == 0) {
        return "0x0";
    }
    
    std::ostringstream oss;
    oss << "0x" << bv_;
    return oss.str();
}

inline std::string sdata_type::to_string_bin() const {
    if (bv_.size() == 0) {
        return "0b0";
    }
    
    try {
        std::ostringstream oss;
        oss << "0b";
        
        for (int i = bv_.size() - 1; i >= 0; --i) {
            oss << (bv_[i] ? '1' : '0');
        }
        
        return oss.str();
    } catch (...) {
        return "0b<error>";
    }
}

inline std::string sdata_type::to_bitstring() const {
    if (bv_.size() == 0) {
        return "";
    }
    
    try {
        std::string result;
        result.reserve(bv_.size());
        for (uint32_t i = 0; i < bv_.size(); ++i) {
            result += bv_[i] ? '1' : '0';
        }
        return result;
    } catch (...) {
        return "<bitstring_error>";
    }
}

inline std::string sdata_type::to_string_verbose() const {
    std::ostringstream oss;
    oss << "[width=" << bv_.size();
    
    try {
        oss << ", dec=" << to_string_dec();
    } catch (...) {
        oss << ", dec=<error>";
    }
    
    try {
        oss << ", hex=" << to_string_hex();
    } catch (...) {
        oss << ", hex=<error>";
    }
    
    try {
        if (bv_.size() <= 32) {
            oss << ", bin=" << to_string_bin();
        }
    } catch (...) {
        oss << ", bin=<error>";
    }
    
    oss << "]";
    return oss.str();
}

// Extended convenience methods
inline bool sdata_type::is_one() const { return bv_.is_one(); }
inline bool sdata_type::is_ones() const { return bv_.is_ones(); }
inline bool sdata_type::is_neg() const { return bv_.is_neg(); }

inline bool sdata_type::get_bit(uint32_t index) const {
    if (index < bv_.size()) {
        return bv_[index];
    }
    return false;
}

inline void sdata_type::set_bit(uint32_t index, bool value) {
    if (index < bv_.size()) {
        bv_.at(index) = value;
    }
}

inline bool sdata_type::is_value(uint64_t value) const {
    if (bv_.size() <= 64) {
        return static_cast<uint64_t>(*this) == value;
    }
    return false;
}

inline bool sdata_type::msb() const {
    return bv_.size() > 0 ? bv_[bv_.size() - 1] : false;
}

inline bool sdata_type::lsb() const {
    return bv_.size() > 0 ? bv_[0] : false;
}

inline void sdata_type::reset() {
    bv_.reset();
}

// Stream output operator
inline std::ostream& operator<<(std::ostream& os, const sdata_type& sdata) {
    if (sdata.bv_.size() == 0) {
        os << "0";
    } else {
        os << sdata.bv_;
    }
    return os;
}

// ========== Extended Operator Implementations ==========
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
    sdata_type result(0, operand.bitwidth());
    ch::internal::bv_inv_truncate<uint64_t>(&result.bv_, &operand.bv_);
    return result;
}

inline sdata_type operator-(const sdata_type& operand) {
    // 对于有符号数的负值：按位取反 + 1
    sdata_type inverted = ~operand;  // 按位取反
    sdata_type one(1, operand.bitwidth());
    return inverted + one;  // 加1得到二进制补码
}

// --- Comparison Operations ---
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
inline sdata_type operator<<(const sdata_type& lhs, uint32_t rhs) {
    sdata_type result(0, lhs.bitwidth());
    ch::internal::bv_shl_truncate<uint64_t>(&result.bv_, &lhs.bv_, rhs);
    return result;
}

inline sdata_type operator>>(const sdata_type& lhs, uint32_t rhs) {
    sdata_type result(0, lhs.bitwidth());
    ch::internal::bv_shr_truncate<uint64_t>(&result.bv_, &lhs.bv_, rhs);
    return result;
}

// ========== Utility Functions ==========
namespace utils {
    // Format and print sdata_type
    inline void print_sdata(const sdata_type& sdata, const std::string& name = "") {
        if (!name.empty()) {
            std::cout << name << ": ";
        }
        std::cout << sdata.to_string_verbose() << std::endl;
    }

    // Debug print
    inline void debug_print(const sdata_type& sdata, const std::string& context = "") {
        std::cout << "[DEBUG] ";
        if (!context.empty()) {
            std::cout << context << " - ";
        }
        std::cout << "sdata" << sdata.to_string_verbose() << std::endl;
    }

    // Print all available formats
    inline void print_all_formats(const sdata_type& sdata, const std::string& name = "") {
        if (!name.empty()) {
            std::cout << "=== " << name << " ===" << std::endl;
        }
        std::cout << "Default:  " << sdata << std::endl;
        std::cout << "Decimal:  " << sdata.to_string_dec() << std::endl;
        std::cout << "Hex:      " << sdata.to_string_hex() << std::endl;
        if (sdata.bitwidth() <= 32) {
            std::cout << "Binary:   " << sdata.to_string_bin() << std::endl;
        }
        std::cout << "Bitstr:   " << sdata.to_bitstring() << std::endl;
        std::cout << "Verbose:  " << sdata.to_string_verbose() << std::endl;
        if (!name.empty()) {
            std::cout << "================" << std::endl;
        }
    }

    // Compare two sdata_type values
    inline void compare_sdata(const sdata_type& lhs, const sdata_type& rhs, 
                             const std::string& name1 = "lhs", const std::string& name2 = "rhs") {
        std::cout << "Comparing " << name1 << " and " << name2 << ":" << std::endl;
        std::cout << name1 << ": " << lhs.to_string_verbose() << std::endl;
        std::cout << name2 << ": " << rhs.to_string_verbose() << std::endl;
        std::cout << "Equal: " << (lhs == rhs) << std::endl;
    }

    // Print binary format with separators for readability
    inline std::string to_binary_readable(const sdata_type& sdata, int group_size = 4) {
        std::string bitstr = sdata.to_bitstring();
        if (bitstr.empty() || group_size <= 0) return bitstr;
        
        std::string result;
        int count = 0;
        for (int i = bitstr.length() - 1; i >= 0; --i) {
            if (count > 0 && count % group_size == 0) {
                result = '_' + result;
            }
            result = bitstr[i] + result;
            count++;
        }
        return "0b" + result;
    }

    // Validate sdata_type consistency
    inline bool validate(const sdata_type& sdata) {
        try {
            if (sdata.bitwidth() > 0) {
                return sdata.bv_.size() == sdata.bitwidth();
            }
            return true;
        } catch (...) {
            return false;
        }
    }

    // Create a formatted dump of sdata_type for debugging
    inline std::string dump(const sdata_type& sdata, const std::string& name = "") {
        std::ostringstream oss;
        if (!name.empty()) {
            oss << "sdata[" << name << "]: ";
        } else {
            oss << "sdata: ";
        }
        
        oss << "{width=" << sdata.bitwidth();
        if (sdata.bitwidth() <= 64) {
            oss << ", value=0x" << sdata.to_string_hex();
            oss << ", dec=" << sdata.to_string_dec();
        } else {
            oss << ", large_value";
        }
        oss << ", is_zero=" << sdata.is_zero();
        oss << ", is_one=" << sdata.is_one();
        oss << "}";
        
        return oss.str();
    }

    // Pretty print with alignment
    inline void print_aligned(const sdata_type& sdata, const std::string& name, 
                             int width = 15, int value_width = 20) {
        std::cout << std::left << std::setw(width) << (name + ":") 
                  << std::right << std::setw(value_width) << sdata 
                  << " | width=" << sdata.bitwidth()
                  << ", dec=" << sdata.to_string_dec() << std::endl;
    }
}

// Clean up macro
#undef CH_SDATA_BINARY_OP_TRUNCATE
#undef CH_SDATA_COMPARISON_OP

}} // namespace ch::core

#endif // UTILS_TYPES_H
