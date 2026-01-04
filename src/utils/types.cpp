// src/core/types.cpp
#include "core/types.h"
#include "bv/bitvector.h"
#include <iomanip>
#include <bit>
#include <algorithm>

namespace ch {
namespace core {

// 实现sdata_type与uint64_t的比较运算符
bool operator==(const sdata_type &lhs, uint64_t rhs) {
    return static_cast<uint64_t>(lhs) == rhs;
}

bool operator!=(const sdata_type &lhs, uint64_t rhs) {
    return static_cast<uint64_t>(lhs) != rhs;
}

bool operator<(const sdata_type &lhs, uint64_t rhs) {
    return static_cast<uint64_t>(lhs) < rhs;
}

bool operator<=(const sdata_type &lhs, uint64_t rhs) {
    return static_cast<uint64_t>(lhs) <= rhs;
}

bool operator>(const sdata_type &lhs, uint64_t rhs) {
    return static_cast<uint64_t>(lhs) > rhs;
}

bool operator>=(const sdata_type &lhs, uint64_t rhs) {
    return static_cast<uint64_t>(lhs) >= rhs;
}

bool operator==(uint64_t lhs, const sdata_type &rhs) {
    return lhs == static_cast<uint64_t>(rhs);
}

bool operator!=(uint64_t lhs, const sdata_type &rhs) {
    return lhs != static_cast<uint64_t>(rhs);
}

bool operator<(uint64_t lhs, const sdata_type &rhs) {
    return lhs < static_cast<uint64_t>(rhs);
}

bool operator<=(uint64_t lhs, const sdata_type &rhs) {
    return lhs <= static_cast<uint64_t>(rhs);
}

bool operator>(uint64_t lhs, const sdata_type &rhs) {
    return lhs > static_cast<uint64_t>(rhs);
}

bool operator>=(uint64_t lhs, const sdata_type &rhs) {
    return lhs >= static_cast<uint64_t>(rhs);
}

sdata_type &sdata_type::operator=(const sdata_type &other) {
    if (this != &other) {
        bv_ = other.bv_;
    }
    return *this;
}

sdata_type &sdata_type::assign_truncate(const sdata_type &other) {
    if (this != &other) {
        internal::bv_assign_truncate(&bv_, &other.bv_);
    }
    return *this;
}
std::string sdata_type::to_string() const {
    if (bv_.size() == 0) {
        return "0";
    }

    std::ostringstream oss;
    oss << bv_;
    return oss.str();
}

std::string sdata_type::to_string_dec() const {
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

std::string sdata_type::to_string_hex() const {
    if (bv_.size() == 0) {
        return "0x0";
    }

    std::ostringstream oss;
    oss << "0x" << bv_;
    return oss.str();
}

std::string sdata_type::to_string_bin() const {
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

std::string sdata_type::to_bitstring() const {
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

std::string sdata_type::to_string_verbose() const {
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
bool sdata_type::is_one() const { return bv_.is_one(); }
bool sdata_type::is_ones() const { return bv_.is_ones(); }
bool sdata_type::is_neg() const { return bv_.is_neg(); }

bool sdata_type::get_bit(uint32_t index) const {
    if (index < bv_.size()) {
        return bv_[index];
    }
    return false;
}

void sdata_type::set_bit(uint32_t index, bool value) {
    if (index < bv_.size()) {
        bv_.at(index) = value;
    }
}

bool sdata_type::is_value(uint64_t value) const {
    if (bv_.size() <= 64) {
        return static_cast<uint64_t>(*this) == value;
    }
    return false;
}

bool sdata_type::msb() const {
    return bv_.size() > 0 ? bv_[bv_.size() - 1] : false;
}

bool sdata_type::lsb() const { return bv_.size() > 0 ? bv_[0] : false; }

void sdata_type::reset() { bv_.reset(); }

// Stream output operator
std::ostream &operator<<(std::ostream &os, const sdata_type &sdata) {
    if (sdata.bv_.size() == 0) {
        os << "0";
    } else {
        os << sdata.bv_;
    }
    return os;
}

// ========== Extended Operator Implementations ==========
// --- Helper macro to reduce boilerplate for binary operators ---
#define CH_SDATA_BINARY_OP_TRUNCATE(op_name, op_func, bv_op_func, width_func)  \
    sdata_type operator op_name(const sdata_type &lhs,                         \
                                const sdata_type &rhs) {                       \
        uint32_t result_width = width_func(lhs, rhs);                          \
        sdata_type result(0, result_width);                                    \
        bv_op_func(&result.bv_, &lhs.bv_, &rhs.bv_);                           \
        return result;                                                         \
    }

// Helper functions to calculate result width for different operations
inline uint32_t add_width(const sdata_type &lhs, const sdata_type &rhs) {
    return std::max(lhs.bitwidth(), rhs.bitwidth()) + 1;
}

inline uint32_t sub_width(const sdata_type &lhs, const sdata_type &rhs) {
    return std::max(lhs.bitwidth(), rhs.bitwidth()) + 1;
}

inline uint32_t mul_width(const sdata_type &lhs, const sdata_type &rhs) {
    return lhs.bitwidth() + rhs.bitwidth();
}

inline uint32_t div_width(const sdata_type &lhs, const sdata_type &rhs) {
    return lhs.bitwidth();  // Division result has width of dividend
}

inline uint32_t mod_width(const sdata_type &lhs, const sdata_type &rhs) {
    return std::min(lhs.bitwidth(), rhs.bitwidth());  // Mod result has width of divisor
}

inline uint32_t and_width(const sdata_type &lhs, const sdata_type &rhs) {
    return std::max(lhs.bitwidth(), rhs.bitwidth());  // Bitwise AND uses max width
}

inline uint32_t or_width(const sdata_type &lhs, const sdata_type &rhs) {
    return std::max(lhs.bitwidth(), rhs.bitwidth());  // Bitwise OR uses max width
}

inline uint32_t xor_width(const sdata_type &lhs, const sdata_type &rhs) {
    return std::max(lhs.bitwidth(), rhs.bitwidth());  // Bitwise XOR uses max width
}

// --- Arithmetic Operations ---
CH_SDATA_BINARY_OP_TRUNCATE(+, add, ch::internal::bv_add_truncate<uint64_t>, add_width)
CH_SDATA_BINARY_OP_TRUNCATE(-, sub, ch::internal::bv_sub_truncate<uint64_t>, sub_width)
CH_SDATA_BINARY_OP_TRUNCATE(*, mul, ch::internal::bv_mul_truncate<uint64_t>, mul_width)
CH_SDATA_BINARY_OP_TRUNCATE(/, div, ch::internal::bv_div_truncate<uint64_t>, div_width)
CH_SDATA_BINARY_OP_TRUNCATE(%, mod, ch::internal::bv_mod_truncate<uint64_t>, mod_width)

// --- Bitwise Operations ---
CH_SDATA_BINARY_OP_TRUNCATE(&, and, ch::internal::bv_and_truncate<uint64_t>, and_width)
CH_SDATA_BINARY_OP_TRUNCATE(|, or, ch::internal::bv_or_truncate<uint64_t>, or_width)
CH_SDATA_BINARY_OP_TRUNCATE(^, xor, ch::internal::bv_xor_truncate<uint64_t>, xor_width)

// --- Unary Bitwise NOT ---
sdata_type operator~(const sdata_type &operand) {
    sdata_type result(0, operand.bitwidth());
    ch::internal::bv_inv_truncate<uint64_t>(&result.bv_, &operand.bv_);
    return result;
}

sdata_type operator-(const sdata_type &operand) {
    // 对于有符号数的负值：按位取反 + 1
    sdata_type inverted = ~operand; // 按位取反
    sdata_type one(1, operand.bitwidth());
    return inverted + one; // 加1得到二进制补码
}

// --- Comparison Operations ---
#define CH_SDATA_COMPARISON_OP(op_name, op_func, bv_op_func)                   \
    bool operator op_name(const sdata_type &lhs, const sdata_type &rhs) {      \
        bool cmp_result = bv_op_func<uint64_t>(lhs.bv_, rhs.bv_);              \
        return cmp_result;                                                     \
    }

CH_SDATA_COMPARISON_OP(==, eq, ch::internal::bv_eq_truncate)
CH_SDATA_COMPARISON_OP(!=, ne, ch::internal::bv_ne_truncate)
CH_SDATA_COMPARISON_OP(<, lt, ch::internal::bv_lt_truncate)
CH_SDATA_COMPARISON_OP(<=, le, ch::internal::bv_le_truncate)
CH_SDATA_COMPARISON_OP(>, gt, ch::internal::bv_gt_truncate)
CH_SDATA_COMPARISON_OP(>=, ge, ch::internal::bv_ge_truncate)

// Clean up macro
#undef CH_SDATA_BINARY_OP_TRUNCATE
#undef CH_SDATA_COMPARISON_OP

// --- Shift Operations ---
sdata_type operator<<(const sdata_type &lhs, uint32_t rhs) {
    uint32_t width = lhs.compute_bitwidth() + rhs;
    sdata_type result(0, width > lhs.bitwidth() ? width : lhs.bitwidth());
    ch::internal::bv_shl_truncate<uint64_t>(&result.bv_, &lhs.bv_, rhs);
    return result;
}

sdata_type operator>>(const sdata_type &lhs, uint32_t rhs) {
    sdata_type result(0, lhs.bitwidth());
    ch::internal::bv_shr_truncate<uint64_t>(&result.bv_, &lhs.bv_, rhs);
    return result;
}

// ========== Utility Functions ==========
namespace utils {
// Format and print sdata_type
void print_sdata(const sdata_type &sdata, const std::string &name) {
    if (!name.empty()) {
        std::cout << name << ": ";
    }
    std::cout << sdata.to_string_verbose() << std::endl;
}

// Debug print
void debug_print(const sdata_type &sdata, const std::string &context) {
    std::cout << "[DEBUG] ";
    if (!context.empty()) {
        std::cout << context << " - ";
    }
    std::cout << "sdata" << sdata.to_string_verbose() << std::endl;
}

// Print all available formats
void print_all_formats(const sdata_type &sdata, const std::string &name) {
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
void compare_sdata(const sdata_type &lhs, const sdata_type &rhs,
                   const std::string &name1, const std::string &name2) {
    std::cout << "Comparing " << name1 << " and " << name2 << ":" << std::endl;
    std::cout << name1 << ": " << lhs.to_string_verbose() << std::endl;
    std::cout << name2 << ": " << rhs.to_string_verbose() << std::endl;
    std::cout << "Equal: " << (lhs == rhs) << std::endl;
}

// Print binary format with separators for readability
std::string to_binary_readable(const sdata_type &sdata, int group_size) {
    std::string bitstr = sdata.to_bitstring();
    if (bitstr.empty() || group_size <= 0)
        return bitstr;

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
bool validate(const sdata_type &sdata) {
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
std::string dump(const sdata_type &sdata, const std::string &name) {
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
void print_aligned(const sdata_type &sdata, const std::string &name, int width,
                   int value_width) {
    std::cout << std::left << std::setw(width) << (name + ":") << std::right
              << std::setw(value_width) << sdata
              << " | width=" << sdata.bitwidth()
              << ", dec=" << sdata.to_string_dec() << std::endl;
}
} // namespace utils

namespace constants {

// 定义常量
const sdata_type zero_1bit{0, 1};
const sdata_type zero_16bit{0, 16};
const sdata_type zero_32bit{0, 32};
const sdata_type zero_64bit{0, 64};
const sdata_type one_1bit{1, 1};
const sdata_type one_16bit{1, 16};
const sdata_type one_32bit{1, 32};
const sdata_type one_64bit{1, 64};
const sdata_type all_ones_8bit{0xFF, 8};
const sdata_type all_ones_16bit{0xFFFF, 16};
const sdata_type all_ones_32bit{0xFFFFFFFF, 32};

// ones 函数的实现
const sdata_type &ones(uint32_t width) {
    static /*thread_local*/ std::unordered_map<uint32_t, sdata_type> ones_cache;
    auto it = ones_cache.find(width);
    if (it == ones_cache.end()) {
        sdata_type s(width);
        // Set all bits to 1 for small widths
        if (width <= 64) {
            s = (width == 64) ? UINT64_MAX : ((1ULL << width) - 1);
        } else {
            // For larger widths, set bits manually
            for (uint32_t i = 0; i < width && i < s.bv_.size(); ++i) {
                s.bv_.at(i) = true;
            }
        }
        it = ones_cache.emplace(width, std::move(s)).first;
    }
    return it->second;
}

} // namespace constants
} // namespace core
} // namespace ch
