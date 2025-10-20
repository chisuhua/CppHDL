// src/ast_nodes/muximpl.cpp
#include "ast/ast_nodes.h"
#include "ast/instr_mux.h"  // 假设您有对应的指令类

namespace ch::core {

std::unique_ptr<ch::instr_base> muximpl::create_instruction(
    ch::data_map_t& data_map) const {
    auto* dst_buf = &data_map.at(id());           // 目标数据
    auto* condition_buf = &data_map.at(condition()->id());           // 目标数据
    auto* true_buf = &data_map.at(true_value()->id());           // 目标数据
    auto* false_buf = &data_map.at(false_value()->id());           // 目标数据
    
    // 创建多路选择器指令
    return std::make_unique<ch::instr_mux>(
        dst_buf,
        size_,
        condition_buf,
        true_buf,
        false_buf
    );
}

} // namespace ch::core
