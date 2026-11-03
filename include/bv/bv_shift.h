#pragma once

#include "utils.h"
#include "bv_copy.h"
#include "bv_pad_slice.h"
#include <algorithm>
#include <cstring>

namespace ch {
namespace internal {

template <bool is_signed, typename T>
void bv_shl_scalar(T *out, uint32_t out_size, const T *in, uint32_t in_size,
                   uint32_t dist) {
    T value;
    if constexpr (is_signed) {
        value = sign_ext(in[0], in_size);
    } else {
        // 对 unsigned，只保留低 in_size 位，清除高位垃圾
        value = in[0] & low_bit_mask<T>(in_size);
    }
    out[0] = (dist < bitwidth_v<T>) ? (value << dist) : 0;
    bv_clear_extra_bits(out, out_size);
}

template <bool is_signed, typename T>
void bv_shl_vector(T *out, uint32_t out_size, const T *in, uint32_t in_size,
                   uint32_t dist) {
    static constexpr uint32_t WORD_SIZE = bitwidth_v<T>;
    auto in_num_words = ceildiv<int32_t>(in_size, WORD_SIZE);
    auto out_num_words = ceildiv<int32_t>(out_size, WORD_SIZE);
    int32_t shift_words = dist / WORD_SIZE;
    uint32_t shift_bits = dist % WORD_SIZE;

    // ⚠️ **`out` 与 `in` MUST NOT 别名** —— 下面第一件事就是 `std::fill_n(out, ..)`，
    //    而后面的循环仍要读 `in`。`bitvector::operator<<=` 是同址调用，所以它在
    //    调用前自己拷了一份（见 `include/bv/bitvector.h`）。
    std::fill_n(out, out_num_words, T(0));

    if (shift_words >= out_num_words) {
        // Result is all zeros
        bv_clear_extra_bits(out, out_size);
        return;
    }

    if (shift_bits == 0) {
        for (int32_t i = out_num_words - 1; i >= shift_words; --i) {
            out[i] =
                (i - shift_words < in_num_words) ? in[i - shift_words] : T(0);
        }
    } else {
        // 目标 word i 的取值（左移 dist = shift_words * WORD_SIZE + shift_bits）：
        //
        //   out[i] = (in[i - shift_words]     << shift_bits)
        //          | (in[i - shift_words - 1] >> (WORD_SIZE - shift_bits))
        //
        // ⚠️⚠️ **第二项是右移**（把**低一个 word** 的顶部位挤进当前 word 的
        // 低端）。2026-11-03 之前这里写的是 `<< (WORD_SIZE - shift_bits)`
        // ⇒ ⇒ **方向反了**，并且**完全丢掉了**跨 word 进位的那些位
        // ⇒ ⇒ 多 word 左移的结果带上**虚假高位**（实测 `1 << 2` 在 127 位槽位里
        // 得到 `0x4000...0004`）。⇒ ⇒ Verilog 的 `<<` 是对的
        // ⇒ ⇒ 这是 **interpreter ↔ Verilog 的分歧**（`eq` 之类因此判错）。
        // 详见 `.rddf/improvements/interpreter-eq-wide-operand-miscompare-2026-11.md`。
        for (int32_t i = out_num_words - 1; i > shift_words; --i) {
            int32_t src_idx = i - shift_words;
            T value = (src_idx < in_num_words) ? (in[src_idx] << shift_bits)
                                               : T(0);
            int32_t carry_idx = src_idx - 1;
            if (carry_idx >= 0 && carry_idx < in_num_words) {
                value |= in[carry_idx] >> (WORD_SIZE - shift_bits);
            }
            out[i] = value;
        }
        if (shift_words < out_num_words) {
            out[shift_words] =
                (0 < in_num_words) ? (in[0] << shift_bits) : T(0);
        }
    }
    bv_clear_extra_bits(out, out_size);
}

template <bool is_signed, typename T>
void bv_shl(T *out, uint32_t out_size, const T *in, uint32_t in_size,
            uint32_t dist) {
    static constexpr uint32_t WORD_SIZE = bitwidth_v<T>;
    if (out_size <= WORD_SIZE && in_size <= WORD_SIZE) {
        bv_shl_scalar<is_signed>(out, out_size, in, in_size, dist);
    } else {
        bv_shl_vector<is_signed>(out, out_size, in, in_size, dist);
    }
}

///////////////////////////////////////////////////////////////////////////////

template <bool is_signed, typename T>
void bv_shr_scalar(T *out, uint32_t out_size, const T *in, uint32_t in_size,
                   uint32_t dist) {
    if constexpr (is_signed) {
        auto value = sign_ext<T>(in[0], in_size);
        out[0] = (dist < bitwidth_v<T>) ? (value >> dist)
                                        : (value >> (bitwidth_v<T> - 1));
        bv_clear_extra_bits(out, out_size);
    } else {
        CH_UNUSED(out_size, in_size);
        out[0] = (dist < bitwidth_v<T>) ? (in[0] >> dist) : 0;
    }
}

template <bool is_signed, typename T>
void bv_shr_vector(T *out, uint32_t out_size, const T *in, uint32_t in_size,
                   uint32_t dist) {
    static constexpr uint32_t WORD_SIZE = bitwidth_v<T>;

    auto in_num_words = ceildiv<int32_t>(in_size, WORD_SIZE);
    auto out_num_words = ceildiv<int32_t>(out_size, WORD_SIZE);

    int32_t shift_words = ceildiv<int32_t>(dist, WORD_SIZE);
    int32_t shift_bits = dist % WORD_SIZE;

    auto r = std::min(in_num_words - shift_words, out_num_words);
    T ext = 0;
    if constexpr (is_signed) {
        ext = bv_is_neg_vector(in, in_size) ? std::numeric_limits<T>::max() : 0;
    }

    if (shift_bits) {
        T prev = (shift_words <= in_num_words)
                     ? (in[shift_words - 1] >> shift_bits)
                     : ext;
        int32_t i = 0;
        if constexpr (is_signed) {
            if (r > 0) {
                for (; i < r - 1; ++i) {
                    auto curr = in[i + shift_words];
                    out[i] = (curr << (WORD_SIZE - shift_bits)) | prev;
                    prev = curr >> shift_bits;
                }
                auto src = in[i + shift_words];
                auto rem = in_size % WORD_SIZE;
                auto curr = rem ? sign_ext<T>(src, rem) : src;
                out[i++] = (curr << (WORD_SIZE - shift_bits)) | prev;
                prev = curr >> shift_bits;
            }
        } else {
            for (; i < r; ++i) {
                auto curr = in[i + shift_words];
                out[i] = (curr << (WORD_SIZE - shift_bits)) | prev;
                prev = curr >> shift_bits;
            }
        }
        for (; i < out_num_words; ++i) {
            out[i] = (ext << (WORD_SIZE - shift_bits)) | prev;
            prev = ext;
        }
    } else {
        int32_t i = 0;
        if constexpr (is_signed) {
            if (r > 0) {
                for (; i < r - 1; ++i) {
                    out[i] = in[i + shift_words];
                }
                auto src = in[i + shift_words];
                auto rem = in_size % WORD_SIZE;
                auto curr = rem ? sign_ext(src, rem) : src;
                out[i++] = curr;
            }
        } else {
            for (; i < r; ++i) {
                out[i] = in[i + shift_words];
            }
        }
        for (; i < out_num_words; ++i) {
            out[i] = ext;
        }
    }

    if constexpr (is_signed) {
        bv_clear_extra_bits(out, out_size);
    }
}

template <bool is_signed, typename T>
void bv_shr(T *out, uint32_t out_size, const T *in, uint32_t in_size,
            uint32_t dist) {
    static constexpr uint32_t WORD_SIZE = bitwidth_v<T>;

    if (out_size <= WORD_SIZE && in_size <= WORD_SIZE) {
        bv_shr_scalar<is_signed>(out, out_size, in, in_size, dist);
    } else {
        bv_shr_vector<is_signed>(out, out_size, in, in_size, dist);
    }
}

} // namespace internal
} // namespace ch