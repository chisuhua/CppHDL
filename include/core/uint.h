#ifndef UINT_H
#define UINT_H

namespace ch { namespace core {

template<typename T>
struct logic_buffer {
    lnodeimpl* impl() const { return node_impl_; }
    logic_buffer(lnodeimpl* node) : node_impl_(node) {}
    logic_buffer() : node_impl_(nullptr) {}
    lnodeimpl* node_impl_ = nullptr;
};

template<unsigned N>
struct ch_uint : public logic_buffer<ch_uint<N>> {
    static constexpr unsigned width = N;
    static constexpr unsigned ch_width = N;
    // Inherit constructors from base
    using logic_buffer<ch_uint<N>>::logic_buffer;

    // Constructor from a literal value (for simulation, not IR building)
    // ch_uint(uint64_t val) : logic_buffer<ch_uint<N>>(/* create lit node for val * /) {} // Not for describe phase
};

// --- Specialization for ch_width_v for ch_uint<N> ---
template<unsigned N>
constexpr unsigned ch_width_v<ch_uint<N>> = N;

template<unsigned N>
struct ch_width_impl<ch_uint<N>, void> {
    static constexpr unsigned value = N;
};

}}

#endif
