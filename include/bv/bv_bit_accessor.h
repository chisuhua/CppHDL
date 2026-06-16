#pragma once

#include "utils.h"
#include "bv_copy.h"
#include <algorithm>
#include <cstring>
#include <limits>

namespace ch {
namespace internal {

template <typename T, bool is_resizable, bool is_signed>
class StaticBitAccessor {
public:
    StaticBitAccessor(const T *value, uint32_t size, uint32_t other_size)
        : value_(value), last_value_(0), last_index_(0), resize_(false) {
        if constexpr (is_resizable) {
            if ((is_signed && (size % bitwidth_v<T>) != 0) ||
                (size < other_size)) {
                resize_ = true;
                last_index_ = (size - 1) / bitwidth_v<T>;
                if constexpr (is_signed) {
                    last_value_ =
                        sign_ext(value[last_index_], size % bitwidth_v<T>);
                }
            }
        }
    }

    T get() const {
        if constexpr (is_resizable && is_signed) {
            return resize_ ? last_value_ : value_[0];
        } else {
            return value_[0];
        }
    }

    T get(uint32_t index) const {
        if constexpr (is_resizable) {
            if (resize_) {
                if constexpr (is_signed) {
                    if (index < last_index_) {
                        return value_[index];
                    } else {
                        if (index == last_index_) {
                            return last_value_;
                        } else {
                            return -(last_value_ >> (bitwidth_v<T> - 1));
                        }
                    }
                } else {
                    return (index <= last_index_) ? value_[index] : 0;
                }
            } else {
                return value_[index];
            }
        } else {
            return value_[index];
        }
    }

    bool need_resize() const { return resize_; }

private:
    const T *value_;
    T last_value_;
    uint32_t last_index_;
    bool resize_;
};

template <typename T>
using ClearBitAccessor = StaticBitAccessor<T, false, false>;
template <typename T, bool is_signed>
using DefaultBitAccessor = StaticBitAccessor<T, true, is_signed>;

///////////////////////////////////////////////////////////////////////////////

template <bool is_signed, typename T,
          typename BitAccessor = DefaultBitAccessor<T, is_signed>>
bool bv_eq_scalar(const T *lhs, uint32_t lhs_size, const T *rhs,
                  uint32_t rhs_size) {
    BitAccessor arg0(lhs, lhs_size, rhs_size);
    BitAccessor arg1(rhs, rhs_size, lhs_size);
    return (arg0.get() == arg1.get());
}

template <bool is_signed, typename T,
          typename BitAccessor = DefaultBitAccessor<T, is_signed>>
bool bv_eq_vector(const T *lhs, uint32_t lhs_size, const T *rhs,
                  uint32_t rhs_size) {
    BitAccessor arg0(lhs, lhs_size, rhs_size);
    BitAccessor arg1(rhs, rhs_size, lhs_size);
    auto num_words = ceildiv(std::max(lhs_size, rhs_size), bitwidth_v<T>);
    for (uint32_t i = 0; i < num_words; ++i) {
        if (arg0.get(i) != arg1.get(i))
            return false;
    }
    return true;
}

template <bool is_signed, typename T,
          typename BitAccessor = DefaultBitAccessor<T, is_signed>>
bool bv_eq(const T *lhs, uint32_t lhs_size, const T *rhs, uint32_t rhs_size) {
    static constexpr uint32_t WORD_SIZE = bitwidth_v<T>;
    if (lhs_size <= WORD_SIZE && rhs_size <= WORD_SIZE) {
        return bv_eq_scalar<is_signed, T, BitAccessor>(lhs, lhs_size, rhs,
                                                       rhs_size);
    } else {
        return bv_eq_vector<is_signed, T, BitAccessor>(lhs, lhs_size, rhs,
                                                       rhs_size);
    }
}

///////////////////////////////////////////////////////////////////////////////

template <bool is_signed, typename T,
          typename BitAccessor = DefaultBitAccessor<T, is_signed>>
bool bv_lt_scalar(const T *lhs, uint32_t lhs_size, const T *rhs,
                  uint32_t rhs_size) {
    if constexpr (is_signed) {
        // compare most signicant bits
        bool lhs_is_neg = bv_is_neg_scalar(lhs, lhs_size);
        bool rhs_is_neg = bv_is_neg_scalar(rhs, rhs_size);
        if (lhs_is_neg != rhs_is_neg)
            return lhs_is_neg;
    }
    BitAccessor arg0(lhs, lhs_size, rhs_size);
    BitAccessor arg1(rhs, rhs_size, lhs_size);
    return (arg0.get() < arg1.get());
}

template <bool is_signed, typename T,
          typename BitAccessor = DefaultBitAccessor<T, is_signed>>
bool bv_lt_vector(const T *lhs, uint32_t lhs_size, const T *rhs,
                  uint32_t rhs_size) {
    static constexpr uint32_t WORD_SIZE = bitwidth_v<T>;

    if constexpr (is_signed) {
        // compare most signicant bits
        bool lhs_is_neg = bv_is_neg_vector(lhs, lhs_size);
        bool rhs_is_neg = bv_is_neg_vector(rhs, rhs_size);
        if (lhs_is_neg != rhs_is_neg)
            return lhs_is_neg;
    }

    BitAccessor arg0(lhs, lhs_size, rhs_size);
    BitAccessor arg1(rhs, rhs_size, lhs_size);

    // same-sign words comparison
    uint32_t num_words = ceildiv(std::max(lhs_size, rhs_size), WORD_SIZE);
    for (int32_t i = static_cast<int32_t>(num_words) - 1; i >= 0; --i) {
        if (arg0.get(i) != arg1.get(i))
            return (arg0.get(i) < arg1.get(i));
    }

    return false;
}

template <bool is_signed, typename T,
          typename BitAccessor = DefaultBitAccessor<T, is_signed>>
bool bv_lt(const T *lhs, uint32_t lhs_size, const T *rhs, uint32_t rhs_size) {
    static constexpr uint32_t WORD_SIZE = bitwidth_v<T>;

    if (lhs_size <= WORD_SIZE && rhs_size <= WORD_SIZE) {
        return bv_lt_scalar<is_signed, T, BitAccessor>(lhs, lhs_size, rhs,
                                                       rhs_size);
    } else {
        return bv_lt_vector<is_signed, T, BitAccessor>(lhs, lhs_size, rhs,
                                                       rhs_size);
    }
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> bool bv_andr_scalar(const T *in, uint32_t size) {
    static constexpr uint32_t WORD_SIZE = bitwidth_v<T>;
    static constexpr T WORD_MAX = std::numeric_limits<T>::max();
    auto max = WORD_MAX >> (WORD_SIZE - size);
    return (in[0] == max);
}

template <typename T> bool bv_andr_vector(const T *in, uint32_t size) {
    static constexpr uint32_t WORD_SIZE = bitwidth_v<T>;
    static constexpr T WORD_MAX = std::numeric_limits<T>::max();
    uint32_t num_words = ceildiv(size, WORD_SIZE);

    auto rem = size % WORD_SIZE;
    auto max = WORD_MAX >> ((WORD_SIZE - rem) % WORD_SIZE);
    int32_t i = static_cast<int32_t>(num_words) - 1;
    if (in[i--] != max)
        return false;
    for (; i >= 0; --i) {
        if (in[i] != WORD_MAX)
            return false;
    }
    return true;
}

template <typename T> bool bv_andr(const T *in, uint32_t size) {
    static constexpr uint32_t WORD_SIZE = bitwidth_v<T>;

    if (size <= WORD_SIZE) {
        return bv_andr_scalar<T>(in, size);
    } else {
        return bv_andr_vector<T>(in, size);
    }
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> bool bv_orr_scalar(const T *in) { return in[0]; }

template <typename T> bool bv_orr_vector(const T *in, uint32_t size) {
    static constexpr uint32_t WORD_SIZE = bitwidth_v<T>;

    uint32_t num_words = ceildiv(size, WORD_SIZE);
    for (uint32_t i = 0; i < num_words; ++i) {
        if (in[i])
            return true;
    }
    return false;
}

template <typename T> bool bv_orr(const T *in, uint32_t size) {
    static constexpr uint32_t WORD_SIZE = bitwidth_v<T>;

    if (size <= WORD_SIZE) {
        return bv_orr_scalar<T>(in);
    } else {
        return bv_orr_vector<T>(in, size);
    }
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> bool bv_xorr_scalar(const T *in) {
    static constexpr uint32_t WORD_SIZE = bitwidth_v<T>;

    T tmp = in[0];
    if constexpr (WORD_SIZE > 32) {
        tmp ^= tmp >> 32;
    }
    if constexpr (WORD_SIZE > 16) {
        tmp ^= tmp >> 16;
    }
    if constexpr (WORD_SIZE > 8) {
        tmp ^= tmp >> 8;
    }
    if constexpr (WORD_SIZE > 4) {
        tmp ^= tmp >> 4;
    }
    if constexpr (WORD_SIZE > 2) {
        tmp ^= tmp >> 2;
    }
    if constexpr (WORD_SIZE > 1) {
        tmp ^= tmp >> 1;
    }
    return tmp & 0x1;
}

template <typename T> bool bv_xorr_vector(const T *in, uint32_t size) {
    static constexpr uint32_t WORD_SIZE = bitwidth_v<T>;

    uint32_t num_words = ceildiv(size, WORD_SIZE);

    T tmp = in[0];
    for (uint32_t i = 1; i < num_words; ++i) {
        tmp ^= in[i];
    }
    bool ret = false;
    while (tmp) {
        ret ^= tmp & 0x1;
        tmp >>= 1;
    }
    return ret;
}

template <typename T> bool bv_xorr(const T *in, uint32_t size) {
    static constexpr uint32_t WORD_SIZE = bitwidth_v<T>;

    if (size <= WORD_SIZE) {
        return bv_xorr_scalar<T>(in);
    } else {
        return bv_xorr_vector<T>(in, size);
    }
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> bool bv_not_scalar(const T *in) {
    return !bv_orr_scalar(in);
}

template <typename T> bool bv_not_vector(const T *in, uint32_t size) {
    return !bv_orr_vector(in, size);
}

template <typename T> bool bv_not(const T *in, uint32_t size) {
    static constexpr uint32_t WORD_SIZE = bitwidth_v<T>;

    if (size <= WORD_SIZE) {
        return bv_not_scalar<T>(in);
    } else {
        return bv_not_vector<T>(in, size);
    }
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> bool bv_andl_scalar(const T *lhs, const T *rhs) {
    return bv_orr_scalar(lhs) && bv_orr_scalar(rhs);
}

template <typename T>
bool bv_andl_vector(const T *lhs, uint32_t lhs_size, const T *rhs,
                    uint32_t rhs_size) {
    return bv_orr_vector(lhs, lhs_size) && bv_orr_vector(rhs, rhs_size);
}

template <typename T>
bool bv_andl(const T *lhs, uint32_t lhs_size, const T *rhs, uint32_t rhs_size) {
    static constexpr uint32_t WORD_SIZE = bitwidth_v<T>;

    if (lhs_size <= WORD_SIZE && rhs_size <= WORD_SIZE) {
        return bv_andl_scalar<T>(lhs, rhs);
    } else {
        return bv_andl_vector<T>(lhs, lhs_size, rhs, rhs_size);
    }
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> bool bv_orl_scalar(const T *lhs, const T *rhs) {
    return bv_orr_scalar(lhs) || bv_orr_scalar(rhs);
}

template <typename T>
bool bv_orl_vector(const T *lhs, uint32_t lhs_size, const T *rhs,
                   uint32_t rhs_size) {
    return bv_orr_vector(lhs, lhs_size) || bv_orr_vector(rhs, rhs_size);
}

template <typename T>
bool bv_orl(const T *lhs, uint32_t lhs_size, const T *rhs, uint32_t rhs_size) {
    static constexpr uint32_t WORD_SIZE = bitwidth_v<T>;

    if (lhs_size <= WORD_SIZE && rhs_size <= WORD_SIZE) {
        return bv_orl_scalar<T>(lhs, rhs);
    } else {
        return bv_orl_vector<T>(lhs, lhs_size, rhs, rhs_size);
    }
}

} // namespace internal
} // namespace ch