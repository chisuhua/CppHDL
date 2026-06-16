#pragma once

#include "utils.h"
#include "bv_copy.h"
#include "bv_bit_accessor.h"
#include "bv_pad_slice.h"
#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <type_traits>
#include <vector>

namespace ch {
namespace internal {

template <bool is_signed, typename T,
          typename BitAccessor = DefaultBitAccessor<T, is_signed>>
void bv_neg_scalar(T *out, uint32_t out_size, const T *in, uint32_t in_size) {
    BitAccessor arg(in, in_size, out_size);

    out[0] = -arg.get();

    bv_clear_extra_bits(out, out_size);
}

template <bool is_signed, typename T,
          typename BitAccessor = DefaultBitAccessor<T, is_signed>>
void bv_neg_vector(T *out, uint32_t out_size, const T *in, uint32_t in_size) {
    static constexpr uint32_t WORD_SIZE = bitwidth_v<T>;

    BitAccessor arg(in, in_size, out_size);

    T borrow(0);
    uint32_t num_words = ceildiv(out_size, WORD_SIZE);
    for (uint32_t i = 0; i < num_words; ++i) {
        auto a = arg.get(i);
        T b = -a - borrow;
        borrow = (a != 0) || (b != 0);
        out[i] = b;
    }
    bv_clear_extra_bits(out, out_size);
}

template <bool is_signed, typename T,
          typename BitAccessor = DefaultBitAccessor<T, is_signed>>
void bv_neg(T *out, uint32_t out_size, const T *in, uint32_t in_size) {
    static constexpr uint32_t WORD_SIZE = bitwidth_v<T>;

    if (out_size <= WORD_SIZE && in_size <= WORD_SIZE) {
        bv_neg_scalar<is_signed, T, BitAccessor>(out, out_size, in, in_size);
    } else {
        bv_neg_vector<is_signed, T, BitAccessor>(out, out_size, in, in_size);
    }
}

///////////////////////////////////////////////////////////////////////////////

template <bool is_signed, typename T,
          typename BitAccessor = DefaultBitAccessor<T, is_signed>>
void bv_add_scalar(T *out, uint32_t out_size, const T *lhs, uint32_t lhs_size,
                   const T *rhs, uint32_t rhs_size) {
    BitAccessor arg0(lhs, lhs_size, out_size);
    BitAccessor arg1(rhs, rhs_size, out_size);

    out[0] = arg0.get() + arg1.get();

    bv_clear_extra_bits(out, out_size);
}

template <bool is_signed, typename T,
          typename BitAccessor = DefaultBitAccessor<T, is_signed>>
void bv_add_vector(T *out, uint32_t out_size, const T *lhs, uint32_t lhs_size,
                   const T *rhs, uint32_t rhs_size) {
    static constexpr uint32_t WORD_SIZE = bitwidth_v<T>;

    BitAccessor arg0(lhs, lhs_size, out_size);
    BitAccessor arg1(rhs, rhs_size, out_size);

    T carry(0);
    uint32_t num_words = ceildiv(out_size, WORD_SIZE);
    for (uint32_t i = 0; i < num_words; ++i) {
        auto a = arg0.get(i);
        auto b = arg1.get(i);
        T c = a + b;
        T d = c + carry;
        carry = (c < a) || (d < carry);
        out[i] = d;
    }
    bv_clear_extra_bits(out, out_size);
}

template <bool is_signed, typename T,
          typename BitAccessor = DefaultBitAccessor<T, is_signed>>
void bv_add(T *out, uint32_t out_size, const T *lhs, uint32_t lhs_size,
            const T *rhs, uint32_t rhs_size) {
    static constexpr uint32_t WORD_SIZE = bitwidth_v<T>;

    if (out_size <= WORD_SIZE && lhs_size <= WORD_SIZE &&
        rhs_size <= WORD_SIZE) {
        bv_add_scalar<is_signed, T, BitAccessor>(out, out_size, lhs, lhs_size,
                                                 rhs, rhs_size);
    } else {
        bv_add_vector<is_signed, T, BitAccessor>(out, out_size, lhs, lhs_size,
                                                 rhs, rhs_size);
    }
}

///////////////////////////////////////////////////////////////////////////////

template <bool is_signed, typename T,
          typename BitAccessor = DefaultBitAccessor<T, is_signed>>
void bv_sub_scalar(T *out, uint32_t out_size, const T *lhs, uint32_t lhs_size,
                   const T *rhs, uint32_t rhs_size) {
    BitAccessor arg0(lhs, lhs_size, out_size);
    BitAccessor arg1(rhs, rhs_size, out_size);

    out[0] = arg0.get() - arg1.get();

    bv_clear_extra_bits(out, out_size);
}

template <bool is_signed, typename T,
          typename BitAccessor = DefaultBitAccessor<T, is_signed>>
void bv_sub_vector(T *out, uint32_t out_size, const T *lhs, uint32_t lhs_size,
                   const T *rhs, uint32_t rhs_size) {
    static constexpr uint32_t WORD_SIZE = bitwidth_v<T>;

    BitAccessor arg0(lhs, lhs_size, out_size);
    BitAccessor arg1(rhs, rhs_size, out_size);

    T borrow(0);
    uint32_t num_words = ceildiv(out_size, WORD_SIZE);
    for (uint32_t i = 0; i < num_words; ++i) {
        auto a = arg0.get(i);
        auto b = arg1.get(i);
        T c = a - b;
        T d = c - borrow;
        borrow = (a < c) || (c < d);
        out[i] = d;
    }
    bv_clear_extra_bits(out, out_size);
}

template <bool is_signed, typename T,
          typename BitAccessor = DefaultBitAccessor<T, is_signed>>
void bv_sub(T *out, uint32_t out_size, const T *lhs, uint32_t lhs_size,
            const T *rhs, uint32_t rhs_size) {
    static constexpr uint32_t WORD_SIZE = bitwidth_v<T>;

    if (out_size <= WORD_SIZE && lhs_size <= WORD_SIZE &&
        rhs_size <= WORD_SIZE) {
        bv_sub_scalar<is_signed, T, BitAccessor>(out, out_size, lhs, lhs_size,
                                                 rhs, rhs_size);
    } else {
        bv_sub_vector<is_signed, T, BitAccessor>(out, out_size, lhs, lhs_size,
                                                 rhs, rhs_size);
    }
}

///////////////////////////////////////////////////////////////////////////////

template <typename T>
void bv_umul(T *out, uint32_t out_size, const T *lhs, uint32_t lhs_size,
             const T *rhs, uint32_t rhs_size) {
    using xword_t = std::conditional_t<
        sizeof(T) == 1, uint8_t,
        std::conditional_t<sizeof(T) == 2, uint16_t, uint32_t>>;
    using yword_t = std::conditional_t<
        sizeof(T) == 1, uint16_t,
        std::conditional_t<sizeof(T) == 2, uint32_t, uint64_t>>;
    assert(out_size <= lhs_size + rhs_size);
    static constexpr uint32_t XWORD_SIZE = bitwidth_v<xword_t>;

    auto u = reinterpret_cast<const xword_t *>(lhs);
    auto v = reinterpret_cast<const xword_t *>(rhs);
    auto w = reinterpret_cast<xword_t *>(out);

    auto m = ceildiv<int>(lhs_size, XWORD_SIZE);
    auto n = ceildiv<int>(rhs_size, XWORD_SIZE);
    auto p = ceildiv<int>(out_size, XWORD_SIZE);

    std::fill_n(w, p, 0);

    for (int i = 0; i < n; ++i) {
        xword_t tot(0);
        for (int j = 0, k = std::min(m, p - i); j < k; ++j) {
            auto c = yword_t(u[j]) * v[i] + w[i + j] + tot;
            tot = c >> XWORD_SIZE;
            w[i + j] = c;
        }
        if (i + m < p) {
            w[i + m] = tot;
        }
    }
    bv_clear_extra_bits(out, out_size);
}

template <bool is_signed, typename T>
void bv_mul_scalar(T *out, uint32_t out_size, const T *lhs, uint32_t lhs_size,
                   const T *rhs, uint32_t rhs_size) {
    if constexpr (is_signed) {
        auto u = sign_ext(lhs[0], lhs_size);
        auto v = sign_ext(rhs[0], rhs_size);
        out[0] = u * v;
    } else {
        CH_UNUSED(lhs_size, rhs_size);
        out[0] = lhs[0] * rhs[0];
    }
    bv_clear_extra_bits(out, out_size);
}

template <bool is_signed, typename T>
void bv_mul_vector(T *out, uint32_t out_size, const T *lhs, uint32_t lhs_size,
                   const T *rhs, uint32_t rhs_size) {
    if constexpr (is_signed) {
        static constexpr uint32_t WORD_SIZE = bitwidth_v<T>;

        const T *u(lhs), *v(rhs);
        std::vector<T> uv, vv;

        uint32_t num_words = ceildiv(out_size, WORD_SIZE);

        if (lhs_size < out_size && bv_is_neg(u, lhs_size)) {
            uv.resize(num_words);
            bv_pad<true>(uv.data(), out_size, u, lhs_size);
            u = uv.data();
            lhs_size = out_size;
        }

        if (rhs_size < out_size && bv_is_neg(v, rhs_size)) {
            vv.resize(num_words);
            bv_pad<true>(vv.data(), out_size, v, rhs_size);
            v = vv.data();
            rhs_size = out_size;
        }

        bv_umul(out, out_size, u, lhs_size, v, rhs_size);
    } else {
        bv_umul(out, out_size, lhs, lhs_size, rhs, rhs_size);
    }
}

template <bool is_signed, typename T>
void bv_mul(T *out, uint32_t out_size, const T *lhs, uint32_t lhs_size,
            const T *rhs, uint32_t rhs_size) {
    static constexpr uint32_t WORD_SIZE = bitwidth_v<T>;
    assert(out_size <= lhs_size + rhs_size);

    if (out_size <= WORD_SIZE && lhs_size <= WORD_SIZE &&
        rhs_size <= WORD_SIZE) {
        bv_mul_scalar<is_signed>(out, out_size, lhs, lhs_size, rhs, rhs_size);
    } else {
        bv_mul_vector<is_signed>(out, out_size, lhs, lhs_size, rhs, rhs_size);
    }
}

///////////////////////////////////////////////////////////////////////////////

template <typename T>
void bv_udiv(T *quot, uint32_t quot_size, T *rem, uint32_t rem_size,
             const T *lhs, uint32_t lhs_size, const T *rhs, uint32_t rhs_size) {
    assert(lhs_size && rhs_size);
    using xword_t = std::conditional_t<
        sizeof(T) == 1, uint8_t,
        std::conditional_t<sizeof(T) == 2, uint16_t, uint32_t>>;
    using yword_t = std::conditional_t<
        sizeof(T) == 1, uint16_t,
        std::conditional_t<sizeof(T) == 2, uint32_t, uint64_t>>;
    using syword_t = std::make_signed_t<yword_t>;

    static constexpr uint32_t XWORD_SIZE = bitwidth_v<xword_t>;
    static constexpr xword_t XWORD_MAX = std::numeric_limits<xword_t>::max();

    auto m = ceildiv<int>(bv_msb(lhs, lhs_size) + 1, XWORD_SIZE);
    auto n = ceildiv<int>(bv_msb(rhs, rhs_size) + 1, XWORD_SIZE);
    auto qn = ceildiv<int>(quot_size, XWORD_SIZE);
    auto rn = ceildiv<int>(rem_size, XWORD_SIZE);

    auto u = reinterpret_cast<const xword_t *>(lhs);
    auto v = reinterpret_cast<const xword_t *>(rhs);
    auto q = reinterpret_cast<xword_t *>(quot);
    auto r = reinterpret_cast<xword_t *>(rem);

    if (0 == n) {
        throw std::runtime_error("divide by zero");
    }

    if (qn) {
        std::fill_n(q, qn, 0);
    }
    if (rn) {
        std::fill_n(r, rn, 0);
    }

    if (m <= 0 || m < n) {
        if (rn) {
            for (int i = 0; i < std::min(m, rn); ++i) {
                r[i] = u[i];
            }
        }
        return;
    }

    std::vector<xword_t> tu(2 * (m + 1), 0), tv(2 * n, 0);
    auto un = tu.data();
    auto vn = tv.data();

    int s = count_leading_zeros<xword_t>(v[n - 1]);
    un[m] = u[m - 1] >> (XWORD_SIZE - s);
    for (int i = m - 1; i > 0; --i) {
        un[i] = (u[i] << s) | (u[i - 1] >> (XWORD_SIZE - s));
    }
    un[0] = u[0] << s;
    for (int i = n - 1; i > 0; --i) {
        vn[i] = (v[i] << s) | (v[i - 1] >> (XWORD_SIZE - s));
    }
    vn[0] = v[0] << s;

    auto h = vn[n - 1];

    for (int j = m - n; j >= 0; --j) {
        auto w = (yword_t(un[j + n]) << XWORD_SIZE) | un[j + n - 1];
        auto qhat = w / h;

        xword_t k(0);
        for (int i = 0; i < n; ++i) {
            auto p = qhat * vn[i];
            auto w = un[i + j] - k - (p & XWORD_MAX);
            k = (p >> XWORD_SIZE) - (w >> XWORD_SIZE);
            un[i + j] = w;
        }

        syword_t t(un[j + n] - k);
        un[j + n] = t;

        if (j < qn)
            q[j] = qhat;

        if (t < 0) {
            if (j < qn)
                --q[j];
            yword_t k(0);
            for (int i = 0; i < n; ++i) {
                auto w = un[i + j] + k + vn[i];
                k = (w >> XWORD_SIZE);
                un[i + j] = w;
            }
            un[j + n] += k;
        }
    }

    if (rn) {
        for (int i = 0; i < std::min(n, rn); ++i) {
            r[i] = (un[i] >> s) | (un[i + 1] << (XWORD_SIZE - s));
        }
    }
}

template <bool is_signed, typename T>
void bv_div_scalar(T *out, uint32_t out_size, const T *lhs, uint32_t lhs_size,
                   const T *rhs, uint32_t rhs_size) {
    if constexpr (is_signed) {
        auto u = sign_ext(lhs[0], lhs_size);
        auto v = sign_ext(rhs[0], rhs_size);
        out[0] = u / v;
    } else {
        CH_UNUSED(lhs_size, rhs_size);
        out[0] = lhs[0] / rhs[0];
    }
    bv_clear_extra_bits(out, out_size);
}

template <bool is_signed, typename T>
void bv_div_vector(T *out, uint32_t out_size, const T *lhs, uint32_t lhs_size,
                   const T *rhs, uint32_t rhs_size) {
    if constexpr (is_signed) {
        static constexpr uint32_t WORD_SIZE = bitwidth_v<T>;

        const T *u(lhs), *v(rhs);
        std::vector<T> uv, vv;

        if (bv_is_neg(u, lhs_size)) {
            uv.resize(ceildiv(lhs_size, WORD_SIZE));
            bv_neg_vector<is_signed, T, ClearBitAccessor<T>>(
                uv.data(), lhs_size, u, lhs_size);
            u = uv.data();
        }

        if (bv_is_neg(v, rhs_size)) {
            vv.resize(ceildiv(rhs_size, WORD_SIZE));
            bv_neg_vector<is_signed, T, ClearBitAccessor<T>>(
                vv.data(), rhs_size, v, rhs_size);
            v = vv.data();
        }

        T *r = nullptr;
        bv_udiv(out, out_size, r, 0, u, lhs_size, v, rhs_size);
        if (bv_is_neg_vector(lhs, lhs_size) ^ bv_is_neg_vector(rhs, rhs_size)) {
            bv_neg_vector<is_signed, T, ClearBitAccessor<T>>(out, out_size, out,
                                                             out_size);
        }
    } else {
        T *r = nullptr;
        bv_udiv(out, out_size, r, 0, lhs, lhs_size, rhs, rhs_size);
    }
    bv_clear_extra_bits(out, out_size);
}

template <bool is_signed, typename T>
void bv_div(T *out, uint32_t out_size, const T *lhs, uint32_t lhs_size,
            const T *rhs, uint32_t rhs_size) {
    static constexpr uint32_t WORD_SIZE = bitwidth_v<T>;

    if (out_size <= WORD_SIZE && lhs_size <= WORD_SIZE &&
        rhs_size <= WORD_SIZE) {
        bv_div_scalar<is_signed>(out, out_size, lhs, lhs_size, rhs, rhs_size);
    } else {
        bv_div_vector<is_signed>(out, out_size, lhs, lhs_size, rhs, rhs_size);
    }
}

///////////////////////////////////////////////////////////////////////////////

template <bool is_signed, typename T>
void bv_mod_scalar(T *out, uint32_t out_size, const T *lhs, uint32_t lhs_size,
                   const T *rhs, uint32_t rhs_size) {
    if constexpr (is_signed) {
        auto u = sign_ext(lhs[0], lhs_size);
        auto v = sign_ext(rhs[0], rhs_size);
        out[0] = u % v;
    } else {
        CH_UNUSED(lhs_size, rhs_size);
        out[0] = lhs[0] % rhs[0];
    }
    bv_clear_extra_bits(out, out_size);
}

template <bool is_signed, typename T>
void bv_mod_vector(T *out, uint32_t out_size, const T *lhs, uint32_t lhs_size,
                   const T *rhs, uint32_t rhs_size) {
    if constexpr (is_signed) {
        static constexpr uint32_t WORD_SIZE = bitwidth_v<T>;

        const T *u(lhs), *v(rhs);
        std::vector<T> uv, vv;

        if (bv_is_neg(u, lhs_size)) {
            uv.resize(ceildiv(lhs_size, WORD_SIZE));
            bv_neg_vector<is_signed, T, ClearBitAccessor<T>>(
                uv.data(), lhs_size, u, lhs_size);
            u = uv.data();
        }

        if (bv_is_neg(v, rhs_size)) {
            vv.resize(ceildiv(rhs_size, WORD_SIZE));
            bv_neg_vector<is_signed, T, ClearBitAccessor<T>>(
                vv.data(), rhs_size, v, rhs_size);
            v = vv.data();
        }

        T *q = nullptr;
        bv_udiv(q, 0, out, out_size, u, lhs_size, v, rhs_size);
        if (bv_is_neg_vector(lhs, lhs_size)) {
            bv_neg_vector<is_signed, T, ClearBitAccessor<T>>(out, out_size, out,
                                                             out_size);
        }
    } else {
        T *q = nullptr;
        bv_udiv(q, 0, out, out_size, lhs, lhs_size, rhs, rhs_size);
    }
    bv_clear_extra_bits(out, out_size);
}

template <bool is_signed, typename T>
void bv_mod(T *out, uint32_t out_size, const T *lhs, uint32_t lhs_size,
            const T *rhs, uint32_t rhs_size) {
    static constexpr uint32_t WORD_SIZE = bitwidth_v<T>;

    if (out_size <= WORD_SIZE && lhs_size <= WORD_SIZE &&
        rhs_size <= WORD_SIZE) {
        bv_mod_scalar<is_signed>(out, out_size, lhs, lhs_size, rhs, rhs_size);
    } else {
        bv_mod_vector<is_signed>(out, out_size, lhs, lhs_size, rhs, rhs_size);
    }
}

} // namespace internal
} // namespace ch