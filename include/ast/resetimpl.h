// include/core/resetimpl.h
#ifndef CH_CORE_RESETIMPL_H
#define CH_CORE_RESETIMPL_H

#include "core/lnodeimpl.h"
#include "core/types.h"
#include "ast/instr_base.h"
#include <vector>
#include <string>
#include <source_location>
#include <memory>

using namespace ch;

namespace ch::core {

// --- resetimpl ---
class resetimpl : public lnodeimpl {
public:
    enum class reset_type {
        sync_high,    // 同步高电平复位
        sync_low,     // 同步低电平复位
        async_high,   // 异步高电平复位
        async_low     // 异步低电平复位
    };

private:
    sdata_type init_value_;
    reset_type reset_type_;
    bool is_async_;

public:
    resetimpl(uint32_t id, const sdata_type& init_value, reset_type rtype,
              const std::string& name, const std::source_location& sloc, context* ctx)
        : lnodeimpl(id, lnodetype::type_input, 1, ctx, name, sloc)  // 复位通常是1位输入
        , init_value_(init_value)
        , reset_type_(rtype)
        , is_async_(rtype == reset_type::async_high || rtype == reset_type::async_low) {}

    // 访问器
    const sdata_type& init_value() const { return init_value_; }
    reset_type type() const { return reset_type_; }
    bool is_async() const { return is_async_; }
    bool is_sync() const { return !is_async_; }
    bool is_active_high() const { 
        return reset_type_ == reset_type::sync_high || reset_type_ == reset_type::async_high;
    }
    bool is_active_low() const { 
        return reset_type_ == reset_type::sync_low || reset_type_ == reset_type::async_low;
    }

    // 覆盖虚函数
    std::string to_string() const override {
        std::ostringstream oss;
        oss << name_ << " (reset, ";
        switch (reset_type_) {
            case reset_type::sync_high:  oss << "sync_high"; break;
            case reset_type::sync_low:   oss << "sync_low"; break;
            case reset_type::async_high: oss << "async_high"; break;
            case reset_type::async_low:  oss << "async_low"; break;
        }
        oss << ")";
        return oss.str();
    }

    bool is_const() const override { return false; }

    lnodeimpl* clone(context* new_ctx, const clone_map& cloned_nodes) const override ;

    bool equals(const lnodeimpl& other) const override {
        if (!lnodeimpl::equals(other)) return false;
        const auto& rst_other = static_cast<const resetimpl&>(other);
        return init_value_ == rst_other.init_value_ &&
               reset_type_ == rst_other.reset_type_;
    }

    std::unique_ptr<ch::instr_base> create_instruction(
        ch::data_map_t& data_map) const override {
        // 复位节点通常不需要特殊的仿真指令
        return lnodeimpl::create_instruction(data_map);
    }
};

} // namespace ch::core

#endif // CH_CORE_RESETIMPL_H
