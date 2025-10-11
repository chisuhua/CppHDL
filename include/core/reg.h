// include/reg.h
#ifndef REG_H
#define REG_H

#include "logic.h"        // For lnode, get_lnode, lnodeimpl, ch_literal, is_ch_literal_v
#include "core/context.h" // For ctx_curr_
#include "lnodeimpl.h"    // For regimpl, proxyimpl, lnodetype
#include "bitbase.h"      // For ch_uint, ch_width_v, logic_buffer
#include "traits.h"       // For ch_uint, ch_width_v, logic_buffer
#include "logger.h"    // For logging macros
#include <string>
#include <typeinfo>
#include <memory>         // For std::unique_ptr
#include <source_location> // C++20
#include <type_traits>    // For std::is_same_v

namespace ch { namespace core {

// Forward declaration
template<typename T>
class ch_reg_impl;

template<typename T>
using ch_reg = const ch_reg_impl<T>;

// --- next_assignment_proxy ---
template<typename T>
struct next_assignment_proxy {
    lnodeimpl* regimpl_node_;

    next_assignment_proxy(lnodeimpl* impl) : regimpl_node_(impl) {}

    template<typename U>
    void operator=(const U& value) const {
        lnode<U> src_lnode = get_lnode(value);
        CHDBG("[next_assignment_proxy::operator=] Assigning value (node ID: %d) to regimpl node ID %d", 
              (src_lnode.impl() ? static_cast<int>(src_lnode.impl()->id()) : -1), 
              (regimpl_node_ ? static_cast<int>(regimpl_node_->id()) : -1));
        
        if (regimpl_node_ && src_lnode.impl() && regimpl_node_->type() == lnodetype::type_reg) {
            regimpl* reg_node_impl = static_cast<regimpl*>(regimpl_node_);
            reg_node_impl->set_next(src_lnode.impl());
        } else {
            CHERROR("[next_assignment_proxy::operator=] Error: regimpl_node_ is null, not a regimpl, or src_lnode is null!");
        }
    }
};

// --- next_proxy ---
template<typename T>
struct next_proxy {
    next_assignment_proxy<T> next;
    next_proxy(lnodeimpl* impl) : next(impl) {}
};

// --- ch_reg_impl ---
template<typename T>
class ch_reg_impl : public T {
public:
    using value_type = T;
    using next_type = next_proxy<T>;

    // Constructor with initial value (supports literals and HDL types)
    template<typename U>
    ch_reg_impl(const U& initial_value) {
        CHDBG("  [ch_reg] Creating register with initial value");

        auto ctx = ch::core::ctx_curr_;
        if (!ctx) {
            CHERROR("[ch_reg] Error: No active context!");
            return;
        }

        auto init_result = get_lnode(initial_value);
        using InitType = decltype(init_result);

        lnodeimpl* init_node_impl = nullptr;

        if constexpr (std::is_same_v<InitType, ch_literal>) {
            // Handle arithmetic literal: use register's width T
            constexpr unsigned width = ch_width_v<T>;
            // Optional: warn if value doesn't fit
            if (init_result.value >= (1ULL << width)) {
                CHWARN("[ch_reg] Warning: Initial literal %llu exceeds %u-bit width!", 
                       static_cast<unsigned long long>(init_result.value), width);
            }
            sdata_type sval(init_result.value, width);
            init_node_impl = ctx->create_literal(sval);
        } else {
            // Handle HDL type (e.g., ch_uint<N>, another reg)
            init_node_impl = init_result.impl();
            if (!init_node_impl) {
                CHERROR("[ch_reg] Error: Initial value is not a valid HDL node!");
                return;
            }
        }

        // Create regimpl node with width = T's width
        constexpr unsigned size = ch_width_v<T>;
        regimpl* reg_node = ctx->create_node<regimpl>(
            size, 0, nullptr, nullptr, nullptr, nullptr, init_node_impl,
            "reg", std::source_location::current()
        );
        proxyimpl* proxy_node = ctx->create_node<proxyimpl>(reg_node, "reg", std::source_location::current());

        // Initialize base class (logic_buffer<T>)
        static_cast<logic_buffer<T>*>(this)->node_impl_ = proxy_node;

        // Extract regimpl node for next assignment
        if (proxy_node && proxy_node->num_srcs() > 0 && 
            proxy_node->src(0) && proxy_node->src(0)->type() == lnodetype::type_reg) {
            regimpl_node_ = proxy_node->src(0);
        } else {
            CHERROR("[ch_reg_impl] Error: Could not get regimpl node from proxyimpl source!");
            regimpl_node_ = nullptr;
        }
        __next__ = std::make_unique<next_type>(regimpl_node_);

        CHDBG("  [ch_reg] Created regimpl and next_proxy.");
    }

    // Constructor without initial value
    ch_reg_impl() {
        CHDBG("  [ch_reg] Creating register without initial value.");

        auto ctx = ch::core::ctx_curr_;
        if (!ctx) {
            CHERROR("[ch_reg] Error: No active context!");
            return;
        }

        constexpr unsigned size = ch_width_v<T>;
        CHDBG("[ch_reg_impl::ctor (no init)] Type T is: %s", typeid(T).name());
        CHDBG("[ch_reg_impl::ctor (no init)] Calculated register size (ch_width_v<T>): %u", size);

        // Create regimpl without initial value
        regimpl* reg_node = ctx->create_node<regimpl>(
            size, 0, nullptr, nullptr, nullptr, nullptr, nullptr,
            "reg", std::source_location::current()
        );
        proxyimpl* proxy_node = ctx->create_node<proxyimpl>(reg_node, "reg", std::source_location::current());

        static_cast<logic_buffer<T>*>(this)->node_impl_ = proxy_node;

        if (proxy_node && proxy_node->num_srcs() > 0 && 
            proxy_node->src(0) && proxy_node->src(0)->type() == lnodetype::type_reg) {
            regimpl_node_ = proxy_node->src(0);
        } else {
            CHERROR("[ch_reg_impl] Error: Could not get regimpl node from proxyimpl source (no init)!");
            regimpl_node_ = nullptr;
        }
        __next__ = std::make_unique<next_type>(regimpl_node_);

        CHDBG("  [ch_reg] Created regimpl (no init) and next_proxy.");
    }

    const next_type* operator->() const { return __next__.get(); }

private:
    std::unique_ptr<next_type> __next__;
    lnodeimpl* regimpl_node_ = nullptr;
};

// --- Width trait specialization ---
template <typename T>
struct ch_width_impl<ch_reg_impl<T>, void> {
    static constexpr unsigned value = ch_width_v<T>;
};

// 处理 const ch_reg_impl
template <typename T>
struct ch_width_impl<const ch_reg_impl<T>, void> {
    static constexpr unsigned value = ch_width_v<T>;
};

// 处理 volatile ch_reg_impl
template <typename T>
struct ch_width_impl<volatile ch_reg_impl<T>, void> {
    static constexpr unsigned value = ch_width_v<T>;
};

// 处理 const volatile ch_reg_impl
template <typename T>
struct ch_width_impl<const volatile ch_reg_impl<T>, void> {
    static constexpr unsigned value = ch_width_v<T>;
};

}} // namespace ch::core

#endif // REG_H
