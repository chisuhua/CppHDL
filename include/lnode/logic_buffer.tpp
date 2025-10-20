// include/core/logic_buffer.tpp
#ifndef CH_CORE_LOGIC_BUFFER_TPP
#define CH_CORE_LOGIC_BUFFER_TPP

#include "core/logic_buffer.h"

namespace ch::core {

// 构造函数实现
template<typename T>
logic_buffer<T>::logic_buffer(lnodeimpl* node) : node_impl_(node) {}

template<typename T>
logic_buffer<T>::logic_buffer() : node_impl_(nullptr) {}

template<typename T>
logic_buffer<T>::logic_buffer(const logic_buffer& other) : node_impl_(other.node_impl_) {}

// 赋值操作符实现
template<typename T>
logic_buffer<T>& logic_buffer<T>::operator=(const logic_buffer& other) {
    if (this != &other) {
        node_impl_ = other.node_impl_;
    }
    return *this;
}

// 比较操作实现
template<typename T>
bool logic_buffer<T>::operator==(const logic_buffer& other) const {
    return node_impl_ == other.node_impl_;
}

template<typename T>
bool logic_buffer<T>::operator!=(const logic_buffer& other) const {
    return node_impl_ != other.node_impl_;
}

// 位操作增强实现
template<typename T>
auto logic_buffer<T>::operator[](unsigned index) const {
    return bit_select(*static_cast<const T*>(this), index);
}

template<typename T>
auto logic_buffer<T>::operator()(unsigned msb, unsigned lsb) const {
    return bits(*static_cast<const T*>(this), msb, lsb);
}

template<typename T>
auto logic_buffer<T>::msb() const {
    return bit_select(*static_cast<const T*>(this), T::width - 1);
}

template<typename T>
auto logic_buffer<T>::lsb() const {
    return bit_select(*static_cast<const T*>(this), 0);
}

// 类型转换增强实现
template<typename T>
template<unsigned NewWidth>
auto logic_buffer<T>::as() const {
    if constexpr (NewWidth > T::width) {
        return zero_extend(*static_cast<const T*>(this), NewWidth);
    } else if constexpr (NewWidth < T::width) {
        return bits(*static_cast<const T*>(this), NewWidth - 1, 0);
    } else {
        return *static_cast<const T*>(this);
    }
}

template<typename T>
template<unsigned NewWidth>
auto logic_buffer<T>::sext() const {
    static_assert(NewWidth >= T::width, "New width must be >= current width");
    return sign_extend(*static_cast<const T*>(this), NewWidth);
}

template<typename T>
template<unsigned NewWidth>
auto logic_buffer<T>::zext() const {
    static_assert(NewWidth >= T::width, "New width must be >= current width");
    return zero_extend(*static_cast<const T*>(this), NewWidth);
}

// 查询操作实现
template<typename T>
bool logic_buffer<T>::is_zero() const {
    return (*static_cast<const T*>(this)) == T(0);
}

template<typename T>
bool logic_buffer<T>::is_ones() const {
    return (*static_cast<const T*>(this)) == ~T(0);
}

template<typename T>
bool logic_buffer<T>::is_power_of_two() const {
    auto val = *static_cast<const T*>(this);
    return (val != T(0)) && ((val & (val - T(1))) == T(0));
}

// 获取位宽实现
template<typename T>
constexpr unsigned logic_buffer<T>::width() {
    return T::width;
}

} // namespace ch::core

#endif // CH_CORE_LOGIC_BUFFER_TPP
