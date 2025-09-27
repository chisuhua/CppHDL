// include/port.h
#ifndef PORT_H
#define PORT_H

#include "direction.h"
#include "logic.h"      // for lnode, get_lnode
#include "core/context.h"
#include <string>
#include <source_location>

namespace ch { namespace core {
#define CH_PORT(name, ...) \
    decltype(::ch::out(__VA_ARGS__)) name; \
    static_assert(true, "")


// Forward declaration
template<typename T, typename Dir>
class port;

// --- Primary port class ---
template<typename T, typename Dir = internal_direction>
class port {
public:
    using value_type = T;
    using direction = Dir;

    port() = default;
    explicit port(const std::string& name) : name_(name) {}

    // Assignment: only output ports can be assigned
    template<typename U>
    void operator=(const U& value) {
        static_assert(is_output_v<Dir>, "Only output ports can be assigned!");
        lnode<U> src = get_lnode(value);
        if (impl_node_ && src.impl()) {
            impl_node_->set_src(0, src.impl());
        }
    }

    // Implicit conversion to lnode<T> for use in expressions (input/internal only)
    operator lnode<T>() const {
        static_assert(!is_output_v<Dir>, "Output ports cannot be used as values!");
        return lnode<T>(impl_node_);
    }

    // Bind implementation node (called by Component::build)
    void bind(lnodeimpl* node) {
        impl_node_ = node;
    }

    // Flip direction
    auto flip() const {
        if constexpr (is_input_v<Dir>) {
            return port<T, output_direction>{};
        } else if constexpr (is_output_v<Dir>) {
            return port<T, input_direction>{};
        } else {
            return *this;
        }
    }

private:
    std::string name_;
    lnodeimpl* impl_node_ = nullptr;
};

// Type aliases
template<typename T> using ch_in  = port<T, input_direction>;
template<typename T> using ch_out = port<T, output_direction>;

template<typename T, typename Dir>
struct ch_width_impl<port<T, Dir>, void> {
    static constexpr unsigned value = ch_width_v<T>;
};

}} // namespace ch::core

#endif // PORT_H
