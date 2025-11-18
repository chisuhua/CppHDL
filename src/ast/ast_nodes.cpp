// src/core/ast_nodes.cpp
#include "ast_nodes.h"
#include "instr_reg.h"
#include "instr_proxy.h"
#include "instr_io.h"
#include "instr_op.h"

namespace ch { namespace core {

// regimpl的指令创建实现
std::unique_ptr<ch::instr_base> regimpl::create_instruction(
        ch::data_map_t& data_map) const {
    // 获取next节点ID
    uint32_t next_node_id = static_cast<uint32_t>(-1); // 无效ID
    if (auto* next_node = get_next()) {
        next_node_id = next_node->id();
    }
    // 创建对应的寄存器指令
    return std::make_unique<ch::instr_reg>(id_, size_, next_node_id);
}

// 在 src/core/ast_nodes.cpp 中更新
std::unique_ptr<ch::instr_base> opimpl::create_instruction(
        ch::data_map_t& data_map) const {
    // 获取操作数缓冲区
    auto* dst_buf = &data_map[id_];
    auto* src0_buf = &data_map[lhs()->id()];
    auto* src1_buf = rhs() ? &data_map[rhs()->id()] : nullptr;
    
    // 根据操作类型创建对应的指令
    switch (op_) {
        // 算术操作
        case ch_op::add:
            return std::make_unique<ch::instr_op_add>(dst_buf, size_, src0_buf, src1_buf);
        case ch_op::sub:
            return std::make_unique<ch::instr_op_sub>(dst_buf, size_, src0_buf, src1_buf);
        case ch_op::mul:
            return std::make_unique<ch::instr_op_mul>(dst_buf, size_, src0_buf, src1_buf);
        case ch_op::div:
            return std::make_unique<ch::instr_op_div>(dst_buf, size_, src0_buf, src1_buf);
        case ch_op::mod:
            return std::make_unique<ch::instr_op_mod>(dst_buf, size_, src0_buf, src1_buf);
        // 位操作
        case ch_op::and_:
            return std::make_unique<ch::instr_op_and>(dst_buf, size_, src0_buf, src1_buf);
        case ch_op::or_:
            return std::make_unique<ch::instr_op_or>(dst_buf, size_, src0_buf, src1_buf);
        case ch_op::xor_:
            return std::make_unique<ch::instr_op_xor>(dst_buf, size_, src0_buf, src1_buf);
        case ch_op::not_:
            return std::make_unique<ch::instr_op_not>(dst_buf, size_, src0_buf);
        // 比较操作
        case ch_op::eq:
            return std::make_unique<ch::instr_op_eq>(dst_buf, 1, src0_buf, src1_buf);
        case ch_op::ne:
            return std::make_unique<ch::instr_op_ne>(dst_buf, 1, src0_buf, src1_buf);
        case ch_op::lt:
            return std::make_unique<ch::instr_op_lt>(dst_buf, 1, src0_buf, src1_buf);
        case ch_op::le:
            return std::make_unique<ch::instr_op_le>(dst_buf, 1, src0_buf, src1_buf);
        case ch_op::gt:
            return std::make_unique<ch::instr_op_gt>(dst_buf, 1, src0_buf, src1_buf);
        case ch_op::ge:
            return std::make_unique<ch::instr_op_ge>(dst_buf, 1, src0_buf, src1_buf);
        // 移位操作
        case ch_op::shl:
            return std::make_unique<ch::instr_op_shl>(dst_buf, size_, src0_buf, src1_buf);
        case ch_op::shr:
            return std::make_unique<ch::instr_op_shr>(dst_buf, size_, src0_buf, src1_buf);
        case ch_op::sshr:
            return std::make_unique<ch::instr_op_sshr>(dst_buf, size_, src0_buf, src1_buf);
        case ch_op::bit_sel:
            return std::make_unique<ch::instr_op_bit_sel>(dst_buf, 1, src0_buf, src1_buf);
        // 一元操作
        case ch_op::neg:
            return std::make_unique<ch::instr_op_neg>(dst_buf, size_, src0_buf);
        case ch_op::bits_extract:
            return std::make_unique<ch::instr_op_bits_extract>(dst_buf, size_, src0_buf, src1_buf);
        case ch_op::concat:
            return std::make_unique<ch::instr_op_concat>(dst_buf, size_, src0_buf, src1_buf);
        case ch_op::sext:
            return std::make_unique<ch::instr_op_sext>(dst_buf, size_, src0_buf);
        case ch_op::zext:
            return std::make_unique<ch::instr_op_zext>(dst_buf, size_, src0_buf);
        case ch_op::and_reduce:
            return std::make_unique<ch::instr_op_and_reduce>(dst_buf, 1, src0_buf);
        case ch_op::or_reduce:
            return std::make_unique<ch::instr_op_or_reduce>(dst_buf, 1, src0_buf);
        case ch_op::xor_reduce:
            return std::make_unique<ch::instr_op_xor_reduce>(dst_buf, 1, src0_buf);
        default:
            return nullptr;
    }
}

// proxyimpl的指令创建实现
std::unique_ptr<ch::instr_base> proxyimpl::create_instruction(
        ch::data_map_t& data_map) const {
    auto* dst_buf = &data_map[id_];
    if (num_srcs() > 0 && src(0)) {
        auto* src_buf = &data_map[src(0)->id()];
        return std::make_unique<ch::instr_proxy>(dst_buf, size_, src_buf);
    }
    return nullptr;
}

// inputimpl的指令创建实现
std::unique_ptr<ch::instr_base> inputimpl::create_instruction(
        ch::data_map_t& data_map) const {
    auto* dst_buf = &data_map[id_];
    return std::make_unique<ch::instr_input>(dst_buf, size_);
}

// outputimpl的指令创建实现
std::unique_ptr<ch::instr_base> outputimpl::create_instruction(
        ch::data_map_t& data_map) const {
    auto* dst_buf = &data_map[id_];
    if (num_srcs() > 0 && src(0)) {
        auto* src_buf = &data_map[src(0)->id()];
        return std::make_unique<ch::instr_output>(dst_buf, size_, src_buf);
    }
    return nullptr;
}

}} // namespace ch::core