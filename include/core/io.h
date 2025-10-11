// include/io.h
#ifndef IO_H
#define IO_H

#include "logic.h"
#include "core/context.h"
#include "lnodeimpl.h"
#include "traits.h"
#include "ast_nodes.h" 
#include "logger.h"
#include <source_location>
#include <string>

namespace ch { namespace core {

template <typename T>
class ch_logic_out {
public:
    using value_type = T;
    ch_logic_out(const std::string& name = "io", const std::source_location& sloc = std::source_location::current())
        : name_(name) {
        CHDBG_FUNC();
        auto ctx = ch::core::ctx_curr_;
        if (!ctx) {
            CHERROR("[ch_logic_out] Error: No active context for output '%s'!", name.c_str());
            return;
        }
        outputimpl* node_ptr = ctx->create_output(ch_width_v<T>, name, sloc);
        output_node_ = node_ptr;
        CHDBG("  [ch_logic_out] Created outputimpl node for '%s'", name.c_str());
    }

    template <typename U>
    void operator=(const U& value) {
        CHDBG_FUNC();
        lnode<U> src_lnode = get_lnode(value);
        if (output_node_ && src_lnode.impl()) {
            output_node_->set_src(0, src_lnode.impl());
        } else {
            CHERROR("[ch_logic_out::operator=] Error: output_node_ or src_lnode is null for '%s'!", name_.c_str());
        }
    }

    lnodeimpl* impl() const { return output_node_; }

private:
    std::string name_;
    outputimpl* output_node_ = nullptr;
};

template <typename T>
class ch_logic_in {
public:
    using value_type = T;
    ch_logic_in(const std::string& name = "io", const std::source_location& sloc = std::source_location::current())
        : name_(name) {
        CHDBG_FUNC();
        auto ctx = ch::core::ctx_curr_;
        if (ctx) {
            inputimpl* node_ptr = ctx->create_input(ch_width_v<T>, name, sloc);
            input_node_ = node_ptr;
            CHDBG("  [ch_logic_in] Created inputimpl node for '%s'", name.c_str());
        }
    }

    operator lnode<T>() const { return lnode<T>(input_node_); }
    lnodeimpl* impl() const { return input_node_; }

private:
    std::string name_;
    inputimpl* input_node_ = nullptr;
};

// --- 为旧类型特化 ch_width_v ---
template <typename T>
struct ch_width_impl<ch_logic_out<T>, void> {
    static constexpr unsigned value = ch_width_v<T>;
};

template <typename T>
struct ch_width_impl<ch_logic_in<T>, void> {
    static constexpr unsigned value = ch_width_v<T>;
};

// --- 新增：__io 宏（保持兼容）---
// #define __io(ports) struct { ports; } io;
// include/io.h （在 namespace ch::core 的最后）
template<typename T> using ch_in  = ch_logic_in<T>;
template<typename T> using ch_out = ch_logic_out<T>;

}} // namespace ch::core

#endif // IO_H
