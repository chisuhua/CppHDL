// include/core/clockimpl.h
#ifndef CH_CORE_CLOCKIMPL_H
#define CH_CORE_CLOCKIMPL_H

#include "core/lnodeimpl.h"
#include "core/types.h"
#include "ast/instr_base.h"
#include <vector>
#include <string>
#include <source_location>
#include <memory>

using namespace ch;

namespace ch::core {

class clockimpl : public lnodeimpl {

private:
    sdata_type init_value_;
    bool is_posedge_;
    bool is_negedge_;

public:
    clockimpl(uint32_t id, const sdata_type& init_value, bool posedge, bool negedge,
              const std::string& name, const std::source_location& sloc, context* ctx)
        : lnodeimpl(id, lnodetype::type_input, 1, ctx, name, sloc)  // 时钟通常是1位输入
        , init_value_()
        , is_posedge_(posedge)
        , is_negedge_(negedge) {
            init_value_ = init_value;
        }

    // 访问器
    const sdata_type& init_value() const { return init_value_; }
    bool is_posedge() const { return is_posedge_; }
    bool is_negedge() const { return is_negedge_; }
    bool is_edge_triggered() const { return is_posedge_ || is_negedge_; }

    // 覆盖虚函数
    std::string to_string() const override {
        std::ostringstream oss;
        oss << name_ << " (clock";
        if (is_posedge_) oss << ", posedge";
        if (is_negedge_) oss << ", negedge";
        oss << ")";
        return oss.str();
    }

    bool is_const() const override { return false; }

    lnodeimpl* clone(context* new_ctx, const clone_map& cloned_nodes) const override ;

    bool equals(const lnodeimpl& other) const override {
        if (!lnodeimpl::equals(other)) return false;
        const auto& clk_other = static_cast<const clockimpl&>(other);
        return init_value_ == clk_other.init_value_ &&
               is_posedge_ == clk_other.is_posedge_ &&
               is_negedge_ == clk_other.is_negedge_;
    }

    std::unique_ptr<ch::instr_base> create_instruction(
        ch::data_map_t& data_map) const override {
        // 时钟节点通常不需要特殊的仿真指令
        return lnodeimpl::create_instruction(data_map);
    }
};

} // namespace ch::core

#endif // CH_CORE_CLOCKIMPL_H