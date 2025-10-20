// include/core/reg.h
#ifndef CH_CORE_REG_H
#define CH_CORE_REG_H

#include "core/lnode.h"
#include "core/context.h"
#include "core/lnodeimpl.h" 
#include "core/operators.h"
#include "core/traits.h"
#include "core/node_builder.h"
#include "core/literal.h"
#include "utils/logger.h"
#include <string>
#include <typeinfo>
#include <memory> 
#include <source_location>
#include <type_traits>

namespace ch { namespace core {

template<typename T>
struct next_assignment_proxy;

template<typename T>
struct next_proxy;

template<typename T>
class ch_reg;

template<typename T>
struct next_assignment_proxy {
    lnodeimpl* regimpl_node_;

    next_assignment_proxy(lnodeimpl* impl) : regimpl_node_(impl) {}

    template<typename U>
    void operator=(const U& value) const;
};

template<typename T>
struct next_proxy {
    next_assignment_proxy<T> next;
    next_proxy(lnodeimpl* impl) : next(impl) {}
};

template<typename T>
class ch_reg : public T {
public:
    using value_type = T;
    using next_type = next_proxy<T>;

    // Constructors
    template<typename U>
    ch_reg(const U& initial_value,
           const std::string& name = "reg",
           const std::source_location& sloc = std::source_location::current());

    ch_reg(const std::string& name = "reg",
           const std::source_location& sloc = std::source_location::current());

    // Conversion operators
    operator lnode<T>() const;
    lnode<T> as_ln() const;

    const next_type* operator->() const { return __next__.get(); }

    // 非阻塞赋值操作符
    template<typename U>
    void operator<=(const U& value) const {
        if (__next__) {
            *__next__ = value;
        }
    }

private:
    std::unique_ptr<next_type> __next__;
    lnodeimpl* regimpl_node_ = nullptr;
};

// --- Width trait specialization ---
template <typename T>
struct ch_width_impl<ch_reg<T>, void> {
    static constexpr unsigned value = ch_width_v<T>;
};

template <typename T>
struct ch_width_impl<const ch_reg<T>, void> {
    static constexpr unsigned value = ch_width_v<T>;
};

template <typename T>
struct ch_width_impl<volatile ch_reg<T>, void> {
    static constexpr unsigned value = ch_width_v<T>;
};

template <typename T>
struct ch_width_impl<const volatile ch_reg<T>, void> {
    static constexpr unsigned value = ch_width_v<T>;
};

}} // namespace ch::core

#include "../lnode/reg.h"  // 包含内联实现

#endif // CH_CORE_REG_H
