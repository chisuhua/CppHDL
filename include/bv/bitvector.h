#pragma once

#include "bv.h"

namespace ch {
namespace internal {

template <typename T>
struct is_bitvector_extended_type_impl : std::false_type {};

template <typename T, std::size_t N>
struct is_bitvector_extended_type_impl<std::array<T, N>> : std::true_type {};

template <typename T>
struct is_bitvector_extended_type_impl<std::vector<T>> : std::true_type {};

template <>
struct is_bitvector_extended_type_impl<std::string> : std::true_type {};

template <>
struct is_bitvector_extended_type_impl<const char *> : std::true_type {};

template <typename T>
inline constexpr bool is_bitvector_extended_type_v =
    is_bitvector_extended_type_impl<std::decay_t<T>>::value;

template <typename word_t> class bitvector {
public:
    static_assert(std::is_integral_v<word_t> && std::is_unsigned_v<word_t>,
                  "invalid type");
    typedef word_t block_type;
    typedef size_t size_type;

    class const_iterator;
    class iterator;

    using const_reference = bool;

    class reference {
    public:
        reference &operator=(bool other) {
            auto mask = word_t(1) << lsb_;
            if (other) {
                word_ |= mask;
            } else {
                word_ &= ~mask;
            }
            return *this;
        }

        operator bool() const { return (word_ >> lsb_) & 0x1; }

    protected:
        reference(word_t &word, uint32_t lsb) : word_(word), lsb_(lsb) {}

        word_t &word_;
        uint32_t lsb_;

        friend class iterator;
        friend class bitvector;
    };

    class iterator_base {
    public:
        bool operator==(const iterator_base &other) const {
            return offset_ == other.offset_;
        }

        bool operator!=(const iterator_base &other) const {
            return !(*this == other);
        }

    protected:
        iterator_base(const word_t *words, uint32_t offset)
            : words_(words), offset_(offset) {}

        iterator_base(const iterator_base &other)
            : words_(other.words_), offset_(other.offset_) {}

        iterator_base &operator=(const iterator_base &other) {
            words_ = other.words_;
            offset_ = other.offset_;
            return *this;
        }

        void increment() {
            if (0 == (++offset_ % bitwidth_v<word_t>)) {
                ++words_;
            }
        }

        void decrement() {
            if (0 == (offset_-- % bitwidth_v<word_t>)) {
                --words_;
            }
        }

        void advance(int delta) {
            int offset = (offset_ % bitwidth_v<word_t>)+delta;
            if (offset >= 0) {
                words_ += (offset / bitwidth_v<word_t>);
            } else {
                words_ -=
                    ((bitwidth_v<word_t> - 1 - offset) / bitwidth_v<word_t>);
            }
            offset_ += delta;
        }

        const_reference const_ref() const {
            uint32_t lsb = offset_ % bitwidth_v<word_t>;
            return (*words_ >> lsb) & 0x1;
        }

        reference ref() const {
            uint32_t lsb = offset_ % bitwidth_v<word_t>;
            return reference(const_cast<word_t &>(*words_), lsb);
        }

        const word_t *words_;
        uint32_t offset_;
    };

    class const_iterator : public iterator_base {
    public:
        using base = iterator_base;

        const_iterator(const const_iterator &other) : base(other) {}

        const_iterator &operator=(const const_iterator &other) {
            base::operator=(other);
            return *this;
        }

        const_reference operator*() const { return this->const_ref(); }

        const_iterator &operator++() {
            this->increment();
            return *this;
        }

        const_iterator &operator--() {
            this->decrement();
            return *this;
        }

        const_iterator operator++(int) {
            const_iterator ret(*this);
            this->operator++();
            return ret;
        }

        const_iterator operator--(int) {
            const_iterator ret(*this);
            this->operator--();
            return ret;
        }

        const_iterator &operator+=(int incr) {
            this->advance(incr);
            return *this;
        }

        const_iterator &operator-=(int dec) {
            this->advance(-dec);
            return *this;
        }

        const_iterator operator+(int incr) const {
            const_iterator ret(*this);
            ret += incr;
            return ret;
        }

        const_iterator operator-(int dec) const {
            const_iterator ret(*this);
            ret -= dec;
            return ret;
        }

    protected:
        const_iterator(const word_t *words, uint32_t offset)
            : iterator_base(words, offset) {}

        friend class iterator;
        friend class bitvector;
    };

    class iterator : public const_iterator {
    public:
        using base = const_iterator;

        iterator(const iterator &other) : base(other) {}

        iterator(const const_iterator &other) : base(other) {}

        reference operator*() const { return this->ref(); }

        iterator &operator++() {
            this->increment();
            return *this;
        }

        iterator &operator--() {
            this->decrement();
            return *this;
        }

        iterator operator++(int) {
            iterator ret(*this);
            this->operator++();
            return ret;
        }

        iterator operator--(int) {
            iterator ret(*this);
            this->operator--();
            return ret;
        }

        iterator &operator+=(int incr) {
            this->advance(incr);
            return *this;
        }

        iterator &operator-=(int dec) {
            this->advance(-dec);
            return *this;
        }

        iterator operator+(int incr) const {
            iterator ret(*this);
            ret += incr;
            return ret;
        }

        iterator operator-(int dec) const {
            iterator ret(*this);
            ret -= dec;
            return ret;
        }

    protected:
        iterator(word_t *words, uint32_t offset)
            : const_iterator(words, offset) {}

        friend class bitvector;
    };

    class const_reverse_iterator : public iterator_base {
    public:
        using base = iterator_base;

        const_reverse_iterator(const const_reverse_iterator &other)
            : base(other) {}

        const_reverse_iterator &operator=(const const_reverse_iterator &other) {
            base::operator=(other);
            return *this;
        }

        const_reference operator*() const { return this->const_ref(); }

        const_reverse_iterator &operator++() {
            this->decrement();
            return *this;
        }

        const_reverse_iterator &operator--() {
            this->increment();
            return *this;
        }

        const_reverse_iterator operator++(int) {
            const_reverse_iterator ret(*this);
            this->operator++();
            return ret;
        }

        const_reverse_iterator operator--(int) {
            const_reverse_iterator ret(*this);
            this->operator--();
            return ret;
        }

        const_reverse_iterator &operator+=(int dec) {
            this->advance(-dec);
            return *this;
        }

        const_reverse_iterator &operator-=(int incr) {
            this->advance(incr);
            return *this;
        }

        const_reverse_iterator operator+(int dec) const {
            const_reverse_iterator ret(*this);
            ret += dec;
            return ret;
        }

        const_reverse_iterator operator-(int incr) const {
            const_reverse_iterator ret(*this);
            ret -= incr;
            return ret;
        }

    protected:
        const_reverse_iterator(const word_t *words, uint32_t offset)
            : iterator_base(words, offset) {}

        friend class reverse_iterator;
        friend class bitvector;
    };

    class reverse_iterator : public const_reverse_iterator {
    public:
        using base = const_reverse_iterator;

        reverse_iterator(const reverse_iterator &other) : base(other) {}

        reverse_iterator(const const_reverse_iterator &other) : base(other) {}

        reference operator*() const { return this->ref(); }

        reverse_iterator &operator++() {
            this->decrement();
            return *this;
        }

        reverse_iterator &operator--() {
            this->increment();
            return *this;
        }

        reverse_iterator operator++(int) {
            reverse_iterator ret(*this);
            this->operator++();
            return ret;
        }

        reverse_iterator operator--(int) {
            reverse_iterator ret(*this);
            this->operator--();
            return ret;
        }

        reverse_iterator &operator+=(int dec) {
            this->advance(-dec);
            return *this;
        }

        reverse_iterator &operator-=(int incr) {
            this->advance(incr);
            return *this;
        }

        reverse_iterator operator+(int dec) const {
            reverse_iterator ret(*this);
            ret += dec;
            return ret;
        }

        reverse_iterator operator-(int incr) const {
            reverse_iterator ret(*this);
            ret -= incr;
            return ret;
        }

    protected:
        reverse_iterator(word_t *words, uint32_t offset)
            : const_reverse_iterator(words, offset) {}

        friend class bitvector;
    };

    bitvector() : words_(nullptr), size_(0) {}

    explicit bitvector(uint32_t size) : bitvector() { this->resize(size); }

    template <typename U, CH_REQUIRES(std::is_integral_v<U> ||
                                      is_bitvector_extended_type_v<U>)>
    explicit bitvector(uint32_t size, U value) : bitvector(size) {
        this->operator=(value);
    }

    bitvector(const bitvector &other) : bitvector(other.size_) {
        std::copy_n(other.words_, other.num_words(), words_);
    }

    bitvector(bitvector &&other) {
        words_ = other.words_;
        size_ = other.size_;
        other.size_ = 0;
        other.words_ = nullptr;
    }

    ~bitvector() { this->clear(); }

    bitvector &operator=(const bitvector &other) {
        this->resize(other.size_);
        std::copy_n(other.words_, other.num_words(), words_);
        return *this;
    }

    bitvector &operator=(bitvector &&other) {
        if (words_) {
            delete[] words_;
        }
        size_ = other.size_;
        words_ = other.words_;
        other.size_ = 0;
        other.words_ = nullptr;
        return *this;
    }

    template <typename U,
              CH_REQUIRES(std::is_integral_v<U> &&std::is_signed_v<U>)>
    bitvector &operator=(U value) {
        bv_assign(words_, size_, value);
        return *this;
    }

    template <typename U,
              CH_REQUIRES(std::is_integral_v<U> &&std::is_unsigned_v<U>)>
    bitvector &operator=(U value) {
        bv_assign(words_, size_, value);
        return *this;
    }

    bitvector &operator=(const std::string &value) {
        bv_assign(words_, size_, value);
        return *this;
    }

    template <typename U, std::size_t N>
    bitvector &operator=(const std::array<U, N> &value) {
        bv_assign(words_, size_, value);
        return *this;
    }

    template <typename U> bitvector &operator=(const std::vector<U> &value) {
        bv_assign(words_, size_, value);
        return *this;
    }

    const_reference at(uint32_t index) const {
        assert(index < size_);
        uint32_t idx = index / bitwidth_v<word_t>;
        uint32_t lsb = index % bitwidth_v<word_t>;
        return (words_[idx] >> lsb) & 0x1;
    }

    reference at(uint32_t index) {
        assert(index < size_);
        uint32_t idx = index / bitwidth_v<word_t>;
        uint32_t lsb = index % bitwidth_v<word_t>;
        return reference(words_[idx], lsb);
    }

    auto operator[](uint32_t index) const { return this->at(index); }

    auto operator[](uint32_t index) { return this->at(index); }

    auto word(uint32_t index) const {
        assert(index < this->num_words());
        return words_[index];
    }

    auto &word(uint32_t index) {
        assert(index < this->num_words());
        return words_[index];
    }

    auto *words() const { return words_; }

    auto *words() { return words_; }

    auto *emplace(word_t *words) {
        std::swap(words_, words);
        return words;
    }

    auto *emplace(word_t *words, uint32_t size) {
        std::swap(words_, words);
        size_ = size;
        return words;
    }

    uint32_t num_words() const { return ceildiv(size_, bitwidth_v<word_t>); }

    const void *data() const { return words_; }

    void *data() { return words_; }

    uint32_t size() const { return size_; }

    bool empty() const { return (0 == size_); }

    void clear() {
        delete[] words_;
        size_ = 0;
    }

    void resize(uint32_t size) {
        uint32_t old_num_words = ceildiv(size_, bitwidth_v<word_t>);
        uint32_t new_num_words = ceildiv(size, bitwidth_v<word_t>);
        if (new_num_words != old_num_words) {
            auto words = new word_t[new_num_words];
            if (words_) {
                delete[] words_;
            }
            words_ = words;
        }
        size_ = size;

        // initialize content
        bv_init(words_, size);
    }

    void copy(uint32_t dst_offset, const bitvector<word_t> &src,
              uint32_t src_offset, uint32_t length) {
        assert(size_ && src.size_);
        assert(src_offset + length <= src.size_);
        assert(dst_offset + length <= size_);
        bv_copy<word_t>(words_, dst_offset, src.words(), src_offset, length);
    }

    template <typename U,
              CH_REQUIRES(std::is_integral_v<U> &&std::is_unsigned_v<U>)>
    void read(uint32_t src_offset, U *out, uint32_t dst_offset,
              uint32_t length) const {
        this->read(src_offset, out, sizeof(U), dst_offset, length);
    }

    template <typename U,
              CH_REQUIRES(std::is_integral_v<U> &&std::is_unsigned_v<U>)>
    void write(uint32_t dst_offset, const U *in, uint32_t src_offset,
               uint32_t length) {
        this->write(dst_offset, in, sizeof(U), src_offset, length);
    }

    int find_first() const { return bv_lsb(words_, size_); }

    int find_last() const { return bv_msb(words_, size_); }

    auto front() const { return this->at(0); }

    auto front() { return this->at(0); }

    auto back() const { return this->at(size_ - 1); }

    auto back() { return this->at(size_ - 1); }

    auto begin() { return iterator(words_, 0); }

    auto end() { return iterator(nullptr, size_); }

    auto begin() const { return const_iterator(words_, 0); }

    auto end() const { return const_iterator(nullptr, size_); }

    auto rbegin() { return reverse_iterator(words_, 0) - (size_ - 1); }

    auto rend() { return reverse_iterator(nullptr, -1); }

    auto rbegin() const { return const_reverse_iterator(words_, size_ - 1); }

    auto rend() const { return const_reverse_iterator(nullptr, -1); }

    bool operator==(const bitvector &other) const {
        if (size_ != other.size_)
            return false;
        for (uint32_t i = 0, n = other.num_words(); i < n; ++i) {
            if (words_[i] != other.words_[i])
                return false;
        }
        return true;
    }

    bool operator!=(const bitvector<word_t> &other) const {
        return !(*this == other);
    }

    bool operator<(const bitvector &other) const {
        return bv_lt<false>(words_, size_, other.words_, other.size_);
    }

    bool operator>=(const bitvector &other) const { return !(*this < other); }

    bool operator>(const bitvector<word_t> &other) const {
        return (other < *this);
    }

    bool operator<=(const bitvector &other) const { return !(other < *this); }

    bitvector &operator+=(const bitvector &other) {
        // 确保本对象有足够空间容纳结果 (取两个操作数的最大尺寸)
        uint32_t max_size = std::max(size_, other.size_);
        if (max_size > size_) {
            this->resize(max_size);
        }
        // 调用底层的 bv_add 函数
        bv_add<false>(words_, size_, words_, size_, other.words_, other.size_);
        return *this;
    }

    bitvector &operator-=(const bitvector &other) {
        uint32_t max_size = std::max(size_, other.size_);
        if (max_size > size_) {
            this->resize(max_size);
        }
        bv_sub<false>(words_, size_, words_, size_, other.words_, other.size_);
        return *this;
    }

    bitvector &operator*=(const bitvector &other) {
        uint32_t max_size = std::max(size_, other.size_);
        if (max_size > size_) {
            this->resize(max_size);
        }
        bv_mul<false>(words_, size_, words_, size_, other.words_, other.size_);
        // 结果可能需要调整大小以适应乘法结果，但这里假设 resize 到 max
        // 已足够或由 bv_mul 内部处理 如果 bv_mul
        // 可能产生更长的结果，可能需要重新计算 size_ 并再次 resize
        // 简化起见，这里假设不会溢出当前 size_ (如果会，则 bv_mul 应负责处理)
        bv_clear_extra_bits(words_, size_);
        return *this;
    }

    bitvector &operator/=(const bitvector &other) {
        uint32_t max_size = std::max(size_, other.size_);
        if (max_size > size_) {
            this->resize(max_size);
        }
        bv_div<false>(words_, size_, words_, size_, other.words_, other.size_);
        bv_clear_extra_bits(words_, size_);
        return *this;
    }

    bitvector &operator%=(const bitvector &other) {
        uint32_t max_size = std::max(size_, other.size_);
        if (max_size > size_) {
            this->resize(max_size);
        }
        bv_mod<false>(words_, size_, words_, size_, other.words_, other.size_);
        bv_clear_extra_bits(words_, size_);
        return *this;
    }

    bitvector &operator|=(const bitvector &other) {
        uint32_t max_size = std::max(size_, other.size_);
        if (max_size > size_) {
            this->resize(max_size);
        }
        bv_or<false>(words_, size_, words_, size_, other.words_, other.size_);
        // 对于 OR 操作，如果 other 更大，可能需要 sign_extend 或 zero_extend
        // 本对象 bv_or 通常会处理对齐，但确保 extra bits 被正确清除
        bv_clear_extra_bits(words_, size_);
        return *this;
    }

    // 按位与等于
    bitvector &operator&=(const bitvector &other) {
        uint32_t max_size = std::max(size_, other.size_);
        if (max_size > size_) {
            this->resize(max_size);
        }
        bv_and<false>(words_, size_, words_, size_, other.words_, other.size_);
        bv_clear_extra_bits(words_, size_);
        return *this;
    }

    // 按位异或等于
    bitvector &operator^=(const bitvector &other) {
        uint32_t max_size = std::max(size_, other.size_);
        if (max_size > size_) {
            this->resize(max_size);
        }
        bv_xor<false>(words_, size_, words_, size_, other.words_, other.size_);
        bv_clear_extra_bits(words_, size_);
        return *this;
    }

    // 左移赋值
    bitvector &operator<<=(uint32_t dist) {
        if (dist >= size_) {
            // 如果位移量大于等于大小，结果应为 0
            this->reset();
        } else {
            bv_shl<false>(words_, size_, words_, size_, dist);
        }
        // bv_shl 应该已经处理了 extra bits
        // bv_clear_extra_bits(words_, size_); // 通常不需要，bv_shl 应该已处理
        return *this;
    }

    // 右移赋值
    bitvector &operator>>=(uint32_t dist) {
        if (dist >= size_) {
            // 如果位移量大于等于大小，结果应为 0 (对于逻辑右移)
            this->reset();
        } else {
            bv_shr<false>(words_, size_, words_, size_, dist);
        }
        // bv_shr 应该已经处理了 extra bits
        // bv_clear_extra_bits(words_, size_); // 通常不需要，bv_shr 应该已处理
        return *this;
    }

    void reset() { bv_reset(words_, size_); }

    bool is_zero() const { return bv_is_zero(words_, size_); }

    bool is_one() const { return bv_is_one(words_, size_); }

    bool is_ones() const { return bv_is_ones(words_, size_); }

    bool is_neg() const { return bv_is_neg(words_, size_); }

#define CH_DEF_CAST(type)                                                      \
    explicit operator type() const {                                           \
        return bv_cast<type, word_t>(words_, size_);                           \
    }
    CH_DEF_CAST(bool)
    CH_DEF_CAST(int8_t)
    CH_DEF_CAST(uint8_t)
    CH_DEF_CAST(int16_t)
    CH_DEF_CAST(uint16_t)
    CH_DEF_CAST(int32_t)
    CH_DEF_CAST(uint32_t)
    CH_DEF_CAST(int64_t)
    CH_DEF_CAST(uint64_t)
#undef CH_DEF_CAST

    void read(uint32_t src_offset, void *dst, uint32_t byte_alignment,
              uint32_t dst_offset, uint32_t length) const {
        CH_CHECK(src_offset + length <= size_, "out of bound access");
        assert(ispow2(byte_alignment) && byte_alignment <= 8);
        byte_alignment = std::min<uint32_t>(byte_alignment, sizeof(word_t));

        switch (byte_alignment) {
        case 1:
            bv_copy<uint8_t>(reinterpret_cast<uint8_t *>(dst), dst_offset,
                             reinterpret_cast<const uint8_t *>(words_),
                             src_offset, length);
            break;
        case 2:
            bv_copy<uint16_t>(reinterpret_cast<uint16_t *>(dst), dst_offset,
                              reinterpret_cast<const uint16_t *>(words_),
                              src_offset, length);
            break;
        case 4:
            bv_copy<uint32_t>(reinterpret_cast<uint32_t *>(dst), dst_offset,
                              reinterpret_cast<const uint32_t *>(words_),
                              src_offset, length);
            break;
        case 8:
            bv_copy<uint64_t>(reinterpret_cast<uint64_t *>(dst), dst_offset,
                              reinterpret_cast<const uint64_t *>(words_),
                              src_offset, length);
            break;
        default:
            throw std::invalid_argument(sstreamf() << "invalid alignment: "
                                                   << byte_alignment);
        }
    }

    void write(uint32_t dst_offset, const void *src, uint32_t byte_alignment,
               uint32_t src_offset, uint32_t length) {
        CH_CHECK(dst_offset + length <= size_, "out of bound access");
        byte_alignment = std::min<uint32_t>(byte_alignment, sizeof(word_t));

        switch (byte_alignment) {
        case 1:
            bv_copy<uint8_t>(reinterpret_cast<uint8_t *>(words_), dst_offset,
                             reinterpret_cast<const uint8_t *>(src), src_offset,
                             length);
            break;
        case 2:
            bv_copy<uint16_t>(reinterpret_cast<uint16_t *>(words_), dst_offset,
                              reinterpret_cast<const uint16_t *>(src),
                              src_offset, length);
            break;
        case 4:
            bv_copy<uint32_t>(reinterpret_cast<uint32_t *>(words_), dst_offset,
                              reinterpret_cast<const uint32_t *>(src),
                              src_offset, length);
            break;
        case 8:
            bv_copy<uint64_t>(reinterpret_cast<uint64_t *>(words_), dst_offset,
                              reinterpret_cast<const uint64_t *>(src),
                              src_offset, length);
            break;
        default:
            throw std::invalid_argument(sstreamf() << "invalid alignment: "
                                                   << byte_alignment);
        }
    }

protected:
    word_t *words_;
    uint32_t size_;
};

// 加法
template <typename word_t>
bitvector<word_t> operator+(bitvector<word_t> lhs,
                            const bitvector<word_t> &rhs) {
    lhs += rhs;
    return lhs;
}

// 减法
template <typename word_t>
bitvector<word_t> operator-(bitvector<word_t> lhs,
                            const bitvector<word_t> &rhs) {
    lhs -= rhs;
    return lhs;
}

// 乘法
template <typename word_t>
bitvector<word_t> operator*(bitvector<word_t> lhs,
                            const bitvector<word_t> &rhs) {
    lhs *= rhs;
    return lhs;
}

// 除法
template <typename word_t>
bitvector<word_t> operator/(bitvector<word_t> lhs,
                            const bitvector<word_t> &rhs) {
    lhs /= rhs;
    return lhs;
}

// 取模
template <typename word_t>
bitvector<word_t> operator%(bitvector<word_t> lhs,
                            const bitvector<word_t> &rhs) {
    lhs %= rhs;
    return lhs;
}

// 按位或
template <typename word_t>
bitvector<word_t> operator|(bitvector<word_t> lhs,
                            const bitvector<word_t> &rhs) {
    lhs |= rhs;
    return lhs;
}

// 按位与
template <typename word_t>
bitvector<word_t> operator&(bitvector<word_t> lhs,
                            const bitvector<word_t> &rhs) {
    lhs &= rhs;
    return lhs;
}

// 按位异或
template <typename word_t>
bitvector<word_t> operator^(bitvector<word_t> lhs,
                            const bitvector<word_t> &rhs) {
    lhs ^= rhs;
    return lhs;
}

// 左移 (注意：左移通常不满足交换律，所以只定义 lhs 为 bitvector 的版本)
template <typename word_t>
bitvector<word_t> operator<<(bitvector<word_t> lhs, uint32_t rhs) {
    lhs <<= rhs;
    return lhs;
}

// 右移 (注意：右移通常不满足交换律，所以只定义 lhs 为 bitvector 的版本)
template <typename word_t>
bitvector<word_t> operator>>(bitvector<word_t> lhs, uint32_t rhs) {
    lhs >>= rhs;
    return lhs;
}

// --- NEW FUNCTION TEMPLATE: bv_assign_truncate ---
// Assigns bits from source (src) to destination (dst), truncating or
// zero-extending to fit exactly dst_size bits. This function ensures that only
// the lower dst_size bits from src (or src itself if src is smaller) are copied
// to dst, and dst's size is considered fixed at dst_size.
//
// Template Parameters:
//   T: The word type (e.g., uint32_t, uint64_t) used by the bitvectors.
// Parameters:
//   dst:       Pointer to the destination word array.
//   dst_size:  Number of bits in the destination.
//   src:       Pointer to the source word array.
//   src_size:  Number of bits in the source.
//-------------------------------------------------------------------------------
/*
template <typename T>
void bv_assign_truncate(T* dst, uint32_t dst_size, const T* src, uint32_t
src_size) { static constexpr uint32_t WORD_SIZE = bitwidth_v<T>; // Bits per
word (e.g., 32 or 64)

    if (dst_size == 0) {
        // Nothing to assign to.
        return;
    }

    if (src_size == 0) {
        // Source is empty, clear destination.
        uint32_t num_dst_words = ceildiv(dst_size, WORD_SIZE);
        std::fill_n(dst, num_dst_words, T(0));
        bv_clear_extra_bits(dst, dst_size); // Ensure any partial high word is
cleared return;
    }

    // Determine the actual number of bits to copy: the minimum of source and
destination size. uint32_t copy_bits = std::min(src_size, dst_size);

    // Calculate full words and remaining bits for the copy operation.
    uint32_t copy_full_words = copy_bits / WORD_SIZE;
    uint32_t copy_rem_bits   = copy_bits % WORD_SIZE;

    // --- Copy full words ---
    if (copy_full_words > 0) {
        std::memcpy(dst, src, copy_full_words * sizeof(T));
    }

    // --- Copy remaining partial word bits ---
    if (copy_rem_bits > 0) {
        uint32_t partial_word_index = copy_full_words;
        T mask = (T(1) << copy_rem_bits) - 1; // Create mask for the remaining
bits (e.g., 0b1111 for 4 bits) dst[partial_word_index] =
(src[partial_word_index] & mask);
        // No need to explicitly mask higher bits of dst beyond copy_rem_bits
        // as bv_clear_extra_bits below will handle the very last bits of dst.
    }

    // --- Clear bits in dst beyond the copied range (if dst is larger than src)
---
    // This also ensures any unused high bits within the last word of dst are
zeroed. if (dst_size > copy_bits) {
        // Zero out words in dst that are completely beyond the copied data
        uint32_t zero_start_word = ceildiv(copy_bits, WORD_SIZE); // First word
index to start zeroing uint32_t zero_end_word   = ceildiv(dst_size, WORD_SIZE);
// One past the last word index of dst if (zero_end_word > zero_start_word) {
             std::fill(dst + zero_start_word, dst + zero_end_word, T(0));
        }
        // Note: If copy_bits landed exactly on a word boundary (copy_rem_bits
== 0),
        // zero_start_word might equal the index of the word containing the last
copied bit.
        // In that case, we still need to clear extra bits within that word if
dst_size
        // doesn't fill it completely.
    }

    // --- Final cleanup: Clear any extra bits beyond dst_size within the last
word ---
    // This is crucial to ensure dst conforms strictly to dst_size bits.
    bv_clear_extra_bits(dst, dst_size);
}
*/

/*
template <typename T>
void bv_assign_truncate(T* dst, uint32_t dst_size, const T* src, uint32_t
src_size) { static constexpr uint32_t WORD_SIZE = bitwidth_v<T>; // Bits per
word (e.g., 32 or 64)

    // --- DEBUG PRINTS ---
    // std::cout << "[DEBUG bv_assign_truncate] Called with dst=" <<
static_cast<void*>(dst)
    //           << ", dst_size=" << dst_size
    //           << ", src=" << static_cast<const void*>(src)
    //           << ", src_size=" << src_size << std::endl;
    // --- END DEBUG PRINTS ---

    if (dst_size == 0) {
        // Nothing to assign to.
        // --- DEBUG PRINT ---
        // std::cout << "[DEBUG bv_assign_truncate] dst_size is 0, returning."
<< std::endl;
        // --- END DEBUG PRINT ---
        return;
    }

    if (src_size == 0) {
        // Source is empty, clear destination.
        uint32_t num_dst_words = ceildiv(dst_size, WORD_SIZE);
        // --- DEBUG PRINT ---
        // std::cout << "[DEBUG bv_assign_truncate] src_size is 0, clearing " <<
num_dst_words << " words." << std::endl;
        // --- END DEBUG PRINT ---
        std::fill_n(dst, num_dst_words, T(0));
        bv_clear_extra_bits(dst, dst_size); // Ensure any partial high word is
cleared return;
    }

    // Determine the actual number of bits to copy: the minimum of source and
destination size. uint32_t copy_bits = std::min(src_size, dst_size);
    // --- DEBUG PRINT ---
    // std::cout << "[DEBUG bv_assign_truncate] copy_bits (min of src/dst size)
= " << copy_bits << std::endl;
    // --- END DEBUG PRINT ---

    // Calculate full words and remaining bits for the copy operation.
    uint32_t copy_full_words = copy_bits / WORD_SIZE;
    uint32_t copy_rem_bits   = copy_bits % WORD_SIZE;
    // --- DEBUG PRINT ---
    // std::cout << "[DEBUG bv_assign_truncate] copy_full_words=" <<
copy_full_words << ", copy_rem_bits=" << copy_rem_bits << std::endl;
    // --- END DEBUG PRINT ---

    // --- Copy full words ---
    if (copy_full_words > 0) {
        // --- DEBUG PRINT ---
        // std::cout << "[DEBUG bv_assign_truncate] Copying " << copy_full_words
<< " full words." << std::endl;
        // --- END DEBUG PRINT ---
        std::memcpy(dst, src, copy_full_words * sizeof(T));
    }

    // --- Copy remaining partial word bits ---
    if (copy_rem_bits > 0) {
        uint32_t partial_word_index = copy_full_words;
        T mask = (T(1) << copy_rem_bits) - 1; // Create mask for the remaining
bits (e.g., 0b1111 for 4 bits)
        // --- DEBUG PRINT ---
        // T src_val_before_mask = src[partial_word_index];
        // T masked_val = src[partial_word_index] & mask;
        // std::cout << "[DEBUG bv_assign_truncate] Partial word copy: index="
<< partial_word_index
        //           << ", mask=0x" << std::hex << mask << std::dec
        //           << ", src_val_before_mask=0x" << std::hex <<
src_val_before_mask << std::dec
        //           << ", masked_val=0x" << std::hex << masked_val << std::dec
<< std::endl;
        // --- END DEBUG PRINT ---
        dst[partial_word_index] = (src[partial_word_index] & mask);
        // --- DEBUG PRINT ---
        // std::cout << "[DEBUG bv_assign_truncate] Assigned masked value 0x" <<
std::hex << (src[partial_word_index] & mask) << std::dec
        //           << " to dst[" << partial_word_index << "]" << std::endl;
        // T dst_val_after_assign = dst[partial_word_index];
        // std::cout << "[DEBUG bv_assign_truncate] dst[" << partial_word_index
<< "] after assign = 0x" << std::hex << dst_val_after_assign << std::dec <<
std::endl;
        // --- END DEBUG PRINT ---
    }

    // --- Clear bits in dst beyond the copied range (if dst is larger than src)
---
    // This also ensures any unused high bits within the last word of dst are
zeroed. if (dst_size > copy_bits) {
        // Zero out words in dst that are completely beyond the copied data
        uint32_t zero_start_word = ceildiv(copy_bits, WORD_SIZE); // First word
index to start zeroing uint32_t zero_end_word   = ceildiv(dst_size, WORD_SIZE);
// One past the last word index of dst
        // --- DEBUG PRINT ---
        // std::cout << "[DEBUG bv_assign_truncate] dst_size > copy_bits,
zeroing words from " << zero_start_word << " to " << zero_end_word << std::endl;
        // --- END DEBUG PRINT ---
        if (zero_end_word > zero_start_word) {
             std::fill(dst + zero_start_word, dst + zero_end_word, T(0));
        }
    }

    // --- Final cleanup: Clear any extra bits beyond dst_size within the last
word ---
    // This is crucial to ensure dst conforms strictly to dst_size bits.
    // --- DEBUG PRINT ---
    // std::cout << "[DEBUG bv_assign_truncate] Calling bv_clear_extra_bits(dst,
" << dst_size << ")" << std::endl;
    // --- END DEBUG PRINT ---
    bv_clear_extra_bits(dst, dst_size);
    // --- DEBUG PRINT ---
    // std::cout << "[DEBUG bv_assign_truncate] Finished. Final dst[0] = 0x" <<
std::hex << dst[0] << std::dec << std::endl;
    // --- END DEBUG PRINT ---
}


template<typename T>
void bv_add_truncate(bitvector<T>* dst, const bitvector<T>* lhs, const
bitvector<T>* rhs) {
    // Implementation using bv_add_vector/bv_add_scalar and ensuring result fits
dst->size()
    // ...
    // Example sketch:
    uint32_t result_width = std::max(lhs->size(), rhs->size()) + 1;
    // Create a temporary bitvector for the full result
    bitvector<T> temp_result(result_width);
    // Perform the full addition
    bv_add<false>(temp_result.words(), result_width, lhs->words(), lhs->size(),
rhs->words(), rhs->size());
    // Truncate the result to fit dst
    bv_assign_truncate(dst->words(), dst->size(), temp_result.words(),
result_width);
    // Or, if dst is pre-sized correctly, use bv_assign_truncate directly on dst
    // bv_assign_truncate(dst->words(), dst->size(), temp_result.words(),
temp_result.size());
}

*/

// --- Helper function to perform width-aware, truncating assignment ---
// Assigns bits from source (src) to destination (dst), truncating or
// zero-extending to fit exactly dst_size bits. This function ensures that only
// the lower dst_size bits from src (or src itself if src is smaller) are copied
// to dst, and dst's size is considered fixed at dst_size.
//
// Template Parameters:
//   T: The word type (e.g., uint32_t, uint64_t) used by the bitvectors.
// Parameters:
//   dst:       Pointer to the destination word array.
//   dst_size:  Number of bits in the destination.
//   src:       Pointer to the source word array.
//   src_size:  Number of bits in the source.
//-------------------------------------------------------------------------------
template <typename T>
void bv_assign_truncate(T *dst, uint32_t dst_size, const T *src,
                        uint32_t src_size) {
    static constexpr uint32_t WORD_SIZE =
        bitwidth_v<T>; // Bits per word (e.g., 32 or 64)

    if (dst_size == 0) {
        // Nothing to assign to.
        return;
    }

    if (src_size == 0 || src == nullptr) {
        // Source is empty or invalid. Clear the destination buffer relevant to
        // dst_size.
        uint32_t num_dst_words_for_size = ceildiv(dst_size, WORD_SIZE);
        if (dst && num_dst_words_for_size > 0) {
            std::fill_n(dst, num_dst_words_for_size, T(0));
        }
        // Clear extra bits in the last word if dst_size is not a multiple of
        // WORD_SIZE
        bv_clear_extra_bits(dst, dst_size);
        return;
    }

    if (dst == nullptr) {
        // Destination is invalid. Cannot proceed.
        return;
    }

    // Determine the actual number of bits to copy: the minimum of source and
    // destination size.
    uint32_t copy_bits = std::min(src_size, dst_size);

    // Calculate full words and remaining bits for the copy operation.
    uint32_t copy_full_words = copy_bits / WORD_SIZE;
    uint32_t copy_rem_bits = copy_bits % WORD_SIZE;

    // --- Copy full words ---
    if (copy_full_words > 0) {
        std::memcpy(dst, src, copy_full_words * sizeof(T));
    }

    // --- Copy remaining partial word bits ---
    if (copy_rem_bits > 0) {
        uint32_t partial_word_index = copy_full_words;
        // Create a mask for the remaining bits (e.g., 0b1111 for 4 bits)
        // Be careful with shift if copy_rem_bits == WORD_SIZE, which would
        // create an invalid shift. However, if copy_rem_bits == WORD_SIZE,
        // copy_full_words would be incremented, and copy_rem_bits would be 0,
        // so this branch wouldn't execute.
        T mask = (T(1) << copy_rem_bits) - 1;
        // Read source bits, apply mask, write to destination
        T src_partial_word = src[partial_word_index];
        dst[partial_word_index] = src_partial_word & mask;
        // No need to explicitly mask higher bits of dst beyond copy_rem_bits
        // within this word, as bv_clear_extra_bits below will handle the very
        // last bits of dst.
    }

    // --- Clear bits in dst beyond the copied range (if dst is larger than
    // src/copy_bits) --- This also ensures any unused high bits within the last
    // word of dst are zeroed
    if (dst_size > copy_bits) {
        // Zero out words in dst that are completely beyond the copied data
        uint32_t zero_start_word =
            ceildiv(copy_bits, WORD_SIZE); // First word index to start zeroing
        uint32_t zero_end_word =
            ceildiv(dst_size, WORD_SIZE); // One past the last word index of dst
        if (zero_end_word > zero_start_word && dst != nullptr) {
            std::fill(dst + zero_start_word, dst + zero_end_word, T(0));
        }
        // Note: If copy_bits landed exactly on a word boundary (copy_rem_bits
        // == 0), zero_start_word might equal the index of the word containing
        // the last copied bit. In that case, we still need to clear extra bits
        // within that word if dst_size doesn't fill it completely.
    }

    // --- Final cleanup: Clear any extra bits beyond dst_size within the last
    // word --- This is crucial to ensure dst conforms strictly to dst_size
    // bits. It clears any bits in the highest word of the destination that are
    // beyond the specified dst_size. This handles the case where dst_size is
    // not a multiple of WORD_SIZE and also ensures cleanliness if dst_size <=
    // copy_bits.
    bv_clear_extra_bits(dst, dst_size);
}
// --- END Helper function ---

// --- Width-Aware Arithmetic Operations for bitvector<T> ---
// Performs assignment with truncation: dst = src
// Copies bits from src to dst, ensuring only the lower dst->size() bits are
// copied. Handles cases where src is wider or narrower than dst.
template <typename T>
void bv_assign_truncate(bitvector<T> *dst, const bitvector<T> *src) {
    // Implementation using bv_assign_truncate on words
    bv_assign_truncate(dst->words(), dst->size(), src->words(), src->size());
}

// Performs addition: dst = lhs + rhs
// Truncates or zero-extends the result to fit exactly dst->size() bits.
// Handles cases where lhs, rhs, and dst have different sizes.
template <typename T>
void bv_add_truncate(bitvector<T> *dst, const bitvector<T> *lhs,
                     const bitvector<T> *rhs) {
    if (!dst || !lhs || !rhs)
        return; // Safety check

    static constexpr uint32_t WORD_SIZE = bitwidth_v<T>;
    const uint32_t dst_size = dst->size();
    const uint32_t lhs_size = lhs->size();
    const uint32_t rhs_size = rhs->size();

    if (dst_size == 0)
        return; // Nothing to compute

    // Calculate the width needed for the full addition result before truncation
    // max operand width + 1 for potential carry
    uint32_t full_result_width = std::max(lhs_size, rhs_size) + 1;

    // If the destination is wide enough for the full result, perform the
    // operation directly
    if (dst_size >= full_result_width) {
        bv_add<false>(dst->words(), dst_size, lhs->words(), lhs_size,
                      rhs->words(), rhs_size);
        bv_clear_extra_bits(dst->words(),
                            dst_size); // Ensure extra bits are cleared
        return;
    }

    // If the destination is narrower, we need to compute the full result first,
    // then truncate it.
    // Allocate temporary storage for the full result.
    // The temporary needs to be at least as wide as the full result width.
    uint32_t temp_num_words = ceildiv(full_result_width, WORD_SIZE);
    std::vector<T> temp_words(temp_num_words, T(0));

    // Perform the full addition into the temporary buffer
    bv_add<false>(temp_words.data(), full_result_width, lhs->words(), lhs_size,
                  rhs->words(), rhs_size);

    // Truncate the full result from the temporary buffer into the destination
    bv_assign_truncate<T>(dst->words(), dst_size, temp_words.data(),
                          full_result_width);
    // bv_assign_truncate already calls bv_clear_extra_bits
}

// Performs subtraction: dst = lhs - rhs
// Truncates or zero-extends the result to fit exactly dst->size() bits.
template <typename T>
void bv_sub_truncate(bitvector<T> *dst, const bitvector<T> *lhs,
                     const bitvector<T> *rhs) {
    if (!dst || !lhs || !rhs)
        return; // Safety check

    static constexpr uint32_t WORD_SIZE = bitwidth_v<T>;
    const uint32_t dst_size = dst->size();
    const uint32_t lhs_size = lhs->size();
    const uint32_t rhs_size = rhs->size();

    if (dst_size == 0)
        return; // Nothing to compute

    // Calculate the width needed for the full subtraction result before
    // truncation max operand width (subtraction doesn't typically increase
    // width beyond operands)
    uint32_t full_result_width = std::max(lhs_size, rhs_size);

    // If the destination is wide enough for the full result, perform the
    // operation directly
    if (dst_size >= full_result_width) {
        bv_sub<false>(dst->words(), dst_size, lhs->words(), lhs_size,
                      rhs->words(), rhs_size);
        bv_clear_extra_bits(dst->words(),
                            dst_size); // Ensure extra bits are cleared
        return;
    }

    // If the destination is narrower, compute the full result first, then
    // truncate.
    uint32_t temp_num_words = ceildiv(full_result_width, WORD_SIZE);
    std::vector<T> temp_words(temp_num_words, T(0));

    bv_sub<false>(temp_words.data(), full_result_width, lhs->words(), lhs_size,
                  rhs->words(), rhs_size);

    bv_assign_truncate<T>(dst->words(), dst_size, temp_words.data(),
                          full_result_width);
    // bv_assign_truncate already calls bv_clear_extra_bits
}

// Performs multiplication: dst = lhs * rhs
// Truncates or zero-extends the result to fit exactly dst->size() bits.
template <typename T>
void bv_mul_truncate(bitvector<T> *dst, const bitvector<T> *lhs,
                     const bitvector<T> *rhs) {
    if (!dst || !lhs || !rhs)
        return; // Safety check

    static constexpr uint32_t WORD_SIZE = bitwidth_v<T>;
    const uint32_t dst_size = dst->size();
    const uint32_t lhs_size = lhs->size();
    const uint32_t rhs_size = rhs->size();

    if (dst_size == 0)
        return; // Nothing to compute

    // Calculate the width needed for the full multiplication result before
    // truncation sum of operand widths
    uint32_t full_result_width = lhs_size + rhs_size;

    // If the destination is wide enough for the full result, perform the
    // operation directly
    if (dst_size >= full_result_width) {
        bv_mul<false>(dst->words(), dst_size, lhs->words(), lhs_size,
                      rhs->words(), rhs_size);
        bv_clear_extra_bits(dst->words(),
                            dst_size); // Ensure extra bits are cleared
        return;
    }

    // If the destination is narrower, compute the full result first, then
    // truncate.
    uint32_t temp_num_words = ceildiv(full_result_width, WORD_SIZE);
    std::vector<T> temp_words(temp_num_words, T(0));

    bv_mul<false>(temp_words.data(), full_result_width, lhs->words(), lhs_size,
                  rhs->words(), rhs_size);

    bv_assign_truncate<T>(dst->words(), dst_size, temp_words.data(),
                          full_result_width);
    // bv_assign_truncate already calls bv_clear_extra_bits
}

// Performs division: dst = lhs / rhs
// Truncates or zero-extends the result to fit exactly dst->size() bits.
template <typename T>
void bv_div_truncate(bitvector<T> *dst, const bitvector<T> *lhs,
                     const bitvector<T> *rhs) {
    if (!dst || !lhs || !rhs)
        return; // Safety check

    static constexpr uint32_t WORD_SIZE = bitwidth_v<T>;
    const uint32_t dst_size = dst->size();
    const uint32_t lhs_size = lhs->size();
    const uint32_t rhs_size = rhs->size();

    if (dst_size == 0)
        return; // Nothing to compute

    // Division result width is typically the width of the dividend (lhs)
    uint32_t full_result_width = lhs_size;

    // If the destination is wide enough for the full result, perform the
    // operation directly
    if (dst_size >= full_result_width) {
        bv_div<false>(dst->words(), dst_size, lhs->words(), lhs_size,
                      rhs->words(), rhs_size);
        bv_clear_extra_bits(dst->words(),
                            dst_size); // Ensure extra bits are cleared
        return;
    }

    // If the destination is narrower, compute the full result first, then
    // truncate.
    uint32_t temp_num_words = ceildiv(full_result_width, WORD_SIZE);
    std::vector<T> temp_words(temp_num_words, T(0));

    bv_div<false>(temp_words.data(), full_result_width, lhs->words(), lhs_size,
                  rhs->words(), rhs_size);

    bv_assign_truncate<T>(dst->words(), dst_size, temp_words.data(),
                          full_result_width);
    // bv_assign_truncate already calls bv_clear_extra_bits
}

// Performs modulo: dst = lhs % rhs
// Truncates or zero-extends the result to fit exactly dst->size() bits.
template <typename T>
void bv_mod_truncate(bitvector<T> *dst, const bitvector<T> *lhs,
                     const bitvector<T> *rhs) {
    if (!dst || !lhs || !rhs)
        return; // Safety check

    static constexpr uint32_t WORD_SIZE = bitwidth_v<T>;
    const uint32_t dst_size = dst->size();
    const uint32_t lhs_size = lhs->size();
    const uint32_t rhs_size = rhs->size();

    if (dst_size == 0)
        return; // Nothing to compute

    // Modulo result width is typically the width of the divisor (rhs)
    uint32_t full_result_width = rhs_size;

    // If the destination is wide enough for the full result, perform the
    // operation directly
    if (dst_size >= full_result_width) {
        bv_mod<false>(dst->words(), dst_size, lhs->words(), lhs_size,
                      rhs->words(), rhs_size);
        bv_clear_extra_bits(dst->words(),
                            dst_size); // Ensure extra bits are cleared
        return;
    }

    // If the destination is narrower, compute the full result first, then
    // truncate.
    uint32_t temp_num_words = ceildiv(full_result_width, WORD_SIZE);
    std::vector<T> temp_words(temp_num_words, T(0));

    bv_mod<false>(temp_words.data(), full_result_width, lhs->words(), lhs_size,
                  rhs->words(), rhs_size);

    bv_assign_truncate<T>(dst->words(), dst_size, temp_words.data(),
                          full_result_width);
    // bv_assign_truncate already calls bv_clear_extra_bits
}

// --- Width-Aware Bitwise Operations for bitvector<T> ---

// Performs bitwise AND: dst = lhs & rhs
// Truncates or zero-extends the result to fit exactly dst->size() bits.
template <typename T>
void bv_and_truncate(bitvector<T> *dst, const bitvector<T> *lhs,
                     const bitvector<T> *rhs) {
    if (!dst || !lhs || !rhs)
        return; // Safety check

    static constexpr uint32_t WORD_SIZE = bitwidth_v<T>;
    const uint32_t dst_size = dst->size();
    const uint32_t lhs_size = lhs->size();
    const uint32_t rhs_size = rhs->size();

    if (dst_size == 0)
        return; // Nothing to compute

    // Bitwise operation result width is max operand width
    uint32_t full_result_width = std::max(lhs_size, rhs_size);

    // If the destination is wide enough for the full result, perform the
    // operation directly
    if (dst_size >= full_result_width) {
        bv_and<false>(dst->words(), dst_size, lhs->words(), lhs_size,
                      rhs->words(), rhs_size);
        bv_clear_extra_bits(dst->words(),
                            dst_size); // Ensure extra bits are cleared
        return;
    }

    // If the destination is narrower, compute the full result first, then
    // truncate.
    uint32_t temp_num_words = ceildiv(full_result_width, WORD_SIZE);
    std::vector<T> temp_words(temp_num_words, T(0));

    bv_and<false>(temp_words.data(), full_result_width, lhs->words(), lhs_size,
                  rhs->words(), rhs_size);

    bv_assign_truncate<T>(dst->words(), dst_size, temp_words.data(),
                          full_result_width);
    // bv_assign_truncate already calls bv_clear_extra_bits
}

// Performs bitwise OR: dst = lhs | rhs
template <typename T>
void bv_or_truncate(bitvector<T> *dst, const bitvector<T> *lhs,
                    const bitvector<T> *rhs) {
    if (!dst || !lhs || !rhs)
        return; // Safety check

    static constexpr uint32_t WORD_SIZE = bitwidth_v<T>;
    const uint32_t dst_size = dst->size();
    const uint32_t lhs_size = lhs->size();
    const uint32_t rhs_size = rhs->size();

    if (dst_size == 0)
        return; // Nothing to compute

    uint32_t full_result_width = std::max(lhs_size, rhs_size);

    if (dst_size >= full_result_width) {
        bv_or<false>(dst->words(), dst_size, lhs->words(), lhs_size,
                     rhs->words(), rhs_size);
        bv_clear_extra_bits(dst->words(), dst_size);
        return;
    }

    uint32_t temp_num_words = ceildiv(full_result_width, WORD_SIZE);
    std::vector<T> temp_words(temp_num_words, T(0));

    bv_or<false>(temp_words.data(), full_result_width, lhs->words(), lhs_size,
                 rhs->words(), rhs_size);

    bv_assign_truncate<T>(dst->words(), dst_size, temp_words.data(),
                          full_result_width);
    // bv_assign_truncate already calls bv_clear_extra_bits
}

// Performs bitwise XOR: dst = lhs ^ rhs
template <typename T>
void bv_xor_truncate(bitvector<T> *dst, const bitvector<T> *lhs,
                     const bitvector<T> *rhs) {
    if (!dst || !lhs || !rhs)
        return; // Safety check

    static constexpr uint32_t WORD_SIZE = bitwidth_v<T>;
    const uint32_t dst_size = dst->size();
    const uint32_t lhs_size = lhs->size();
    const uint32_t rhs_size = rhs->size();

    if (dst_size == 0)
        return; // Nothing to compute

    uint32_t full_result_width = std::max(lhs_size, rhs_size);

    if (dst_size >= full_result_width) {
        bv_xor<false>(dst->words(), dst_size, lhs->words(), lhs_size,
                      rhs->words(), rhs_size);
        bv_clear_extra_bits(dst->words(), dst_size);
        return;
    }

    uint32_t temp_num_words = ceildiv(full_result_width, WORD_SIZE);
    std::vector<T> temp_words(temp_num_words, T(0));

    bv_xor<false>(temp_words.data(), full_result_width, lhs->words(), lhs_size,
                  rhs->words(), rhs_size);

    bv_assign_truncate<T>(dst->words(), dst_size, temp_words.data(),
                          full_result_width);
    // bv_assign_truncate already calls bv_clear_extra_bits
}

// If it exists, you can use it like this:
template <typename T>
void bv_inv_truncate(T *dst, uint32_t dst_size, const T *src,
                     uint32_t src_size) {
    // ... (checks) ...
    // Use bv_inv which should handle truncation/extension correctly
    bv_inv<false, T>(dst, dst_size, src,
                     src_size); // Assuming bv_inv truncates result to out_size
    bv_clear_extra_bits(dst, dst_size); // Ensure final cleanup
}

template <typename T>
void bv_inv_truncate(bitvector<T> *dst, const bitvector<T> *src) {
    // Implementation using bv_assign_truncate on words
    bv_inv_truncate(dst->words(), dst->size(), src->words(), src->size());
}

// Performs bitwise NOT: dst = ~src
// Truncates or zero-extends the result to fit exactly dst->size() bits.
template <typename T> bool bv_not_truncate(bitvector<T> *dst) {
    if (!dst)
        return false; // Safety check

    static constexpr uint32_t WORD_SIZE = bitwidth_v<T>;
    const uint32_t dst_size = dst->size();

    if (dst_size == 0)
        return false; // Nothing to compute
    return bv_not<T>(dst->words(), dst_size);
}

// Performs left shift: dst = src << dist
// Truncates or zero-extends the result to fit exactly dst->size() bits.
template <typename T>
void bv_shl_truncate(bitvector<T> *dst, const bitvector<T> *src,
                     uint32_t dist) {
    if (!dst || !src)
        return; // Safety check

    const uint32_t dst_size = dst->size();
    const uint32_t src_size = src->size();

    if (dst_size == 0)
        return; // Nothing to compute

    // Left shift result width is src width + dist (but capped by implementation
    // limits) For truncation, we just perform the shift and then truncate.
    bv_shl<false>(dst->words(), dst_size, src->words(), src_size, dist);
    // bv_shl should handle truncation if dst_size < src_size + dist
    bv_clear_extra_bits(dst->words(),
                        dst_size); // Ensure extra bits are cleared
}

// Performs left shift: dst = src << dist
// Creates a destination bitvector with size src->size() + dist, then performs
// the shift.
template <typename T>
void bv_shl(bitvector<T> *dst, const bitvector<T> *src, uint32_t dist) {
    if (!dst || !src)
        return; // Safety check

    const uint32_t src_size = src->size();
    const uint32_t result_size = src_size + dist; // Calculate result width

    bv_shl<false>(dst->words(), result_size, src->words(), src_size, dist);
    // bv_clear_extra_bits(result->words(),
    //                     result_size); // Ensure extra bits are cleared
}

// Performs right shift: dst = src >> dist
// Truncates or zero-extends the result to fit exactly dst->size() bits.
template <typename T>
void bv_shr_truncate(bitvector<T> *dst, const bitvector<T> *src,
                     uint32_t dist) {
    if (!dst || !src)
        return; // Safety check

    const uint32_t dst_size = dst->size();
    const uint32_t src_size = src->size();

    if (dst_size == 0)
        return; // Nothing to compute

    // Right shift result width is src width (logical right shift zeros high
    // bits)
    bv_shr<false>(dst->words(), dst_size, src->words(), src_size, dist);
    // bv_shr should handle truncation if dst_size < src_size
    bv_clear_extra_bits(dst->words(),
                        dst_size); // Ensure extra bits are cleared
}

// --- 相等 (==) ---
template <typename word_t>
bool bv_eq_truncate(const bitvector<word_t> &lhs,
                    const bitvector<word_t> &rhs) {
    // Delegate to the underlying bv_eq function from bv.h
    // which handles size differences correctly.
    return ch::internal::bv_eq</* is_signed */ false, word_t>(
        lhs.words(), lhs.size(), // Pass lhs's data pointer and size
        rhs.words(), rhs.size()  // Pass rhs's data pointer and size
    );
    // bv_eq<false, word_t> internally calls bv_eq_scalar or bv_eq_vector
    // based on operand sizes, ensuring correct comparison.
}

// --- 不等 (!=) ---
template <typename word_t>
bool bv_ne_truncate(const bitvector<word_t> &lhs,
                    const bitvector<word_t> &rhs) {
    // a != b 等价于 !(a == b)
    return !bv_eq_truncate(lhs, rhs); // Delegate to bv_eq
}

// --- 小于 (<) ---
template <typename word_t>
bool bv_lt_truncate(const bitvector<word_t> &lhs,
                    const bitvector<word_t> &rhs) {
    // Delegate to the underlying bv_lt function from bv.h
    // which handles size differences correctly.
    return ch::internal::bv_lt</* is_signed */ false, word_t>(
        lhs.words(), lhs.size(), // Pass lhs's data pointer and size
        rhs.words(), rhs.size()  // Pass rhs's data pointer and size
    );
    // bv_lt<false, word_t> internally calls bv_lt_scalar or bv_lt_vector
    // based on operand sizes, ensuring correct comparison.
}

// --- 小于等于 (<=) ---
template <typename word_t>
bool bv_le_truncate(const bitvector<word_t> &lhs,
                    const bitvector<word_t> &rhs) {
    // a <= b 等价于 !(b < a)
    // This relies on bv_lt's correct handling of size differences.
    return !bv_gt_truncate(lhs, rhs); // Delegate to bv_gt (a <= b iff !(b < a)
                                      // is equivalent to !(a > b))
    // Or equivalently: return !bv_lt(rhs, lhs);
}

// --- 大于 (>) ---
template <typename word_t>
bool bv_gt_truncate(const bitvector<word_t> &lhs,
                    const bitvector<word_t> &rhs) {
    // a > b 等价于 b < a
    // This relies on bv_lt's correct handling of size differences.
    return bv_lt_truncate(rhs, lhs); // Delegate to bv_lt with swapped operands
}

// --- 大于等于 (>=) ---
template <typename word_t>
bool bv_ge_truncate(const bitvector<word_t> &lhs,
                    const bitvector<word_t> &rhs) {
    // a >= b 等价于 !(a < b)
    // This relies on bv_lt's correct handling of size differences.
    return !bv_lt_truncate(lhs, rhs); // Delegate to bv_lt
}

// template <typename word_t>
// std::ostream& operator<<(std::ostream& out, const bitvector<word_t>& in);

template <typename word_t>
std::ostream &operator<<(std::ostream &out, const bitvector<word_t> &in) {
    out << "0x";
    auto oldflags = out.flags();
    out.setf(std::ios_base::hex, std::ios_base::basefield);

    uint32_t quad(0);
    bool skip_zeros = true;
    uint32_t size = in.size();

    for (auto it = in.begin() + (size - 1); size;) {
        quad = (quad << 0x1) | *it--;
        if (0 == (--size & 0x3)) {
            if (0 == size || (quad != 0) || !skip_zeros) {
                out << quad;
                skip_zeros = false;
            }
            quad = 0;
        }
    }
    if (0 != (size & 0x3)) {
        out << quad;
    }
    out.flags(oldflags);
    return out;
}
} // namespace internal
} // namespace ch

template <typename T>
inline std::string to_bitstring(const ch::internal::bitvector<T> &bv) {
    std::string s;
    for (size_t i = 0; i < bv.size(); ++i)
        s += bv[i] ? '1' : '0';
    // Reverse to show LSB on the right, MSB on the left (conventional
    // representation) Optional: std::reverse(s.begin(), s.end()); Or read
    // backwards for LSB-first indexing:
    std::string reversed_s;
    for (int i = s.length() - 1; i >= 0; --i) {
        reversed_s += s[i];
    }
    return reversed_s; // Now MSB is left, LSB is right in string
}
