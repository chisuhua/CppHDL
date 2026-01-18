// src/core/ast_nodes.cpp
#include "ast_nodes.h"
#include "core/context.h"
#include "instr_clock.h"
#include "instr_io.h"
#include "instr_op.h"
#include "instr_proxy.h"
#include "instr_reg.h"
#include "types.h"

namespace ch {
namespace core {

// regimpl的指令创建实现
std::unique_ptr<ch::instr_base>
regimpl::create_instruction(ch::data_map_t &data_map) const {
    // 获取next节点ID
    uint32_t next_node_id = static_cast<uint32_t>(-1); // 无效ID
    auto *next_node = get_next();
    if (next_node) {
        next_node_id = next_node->id();
    }

    // 获取当前节点和下一节点的数据缓冲区指针
    auto *current_buf = &data_map[id_];
    auto *next_buf = (next_node_id != static_cast<uint32_t>(-1))
                         ? &data_map[next_node_id]
                         : nullptr;

    // 如果有初始值，则设置current_buf为初始值
    if (init_val_) {
        auto *lit_init_val = static_cast<litimpl *>(init_val_);
        current_buf->assign_truncate(lit_init_val->value());
    }

    // 添加用户跟踪：将寄存器节点添加到next节点的用户列表中
    if (next_node) {
        next_node->add_user(const_cast<regimpl *>(this));
    }

    // 获取时钟边沿信息
    sdata_type *clk_edge = nullptr;
    if (cd_ != static_cast<uint32_t>(-1)) {
        // 直接使用cd_作为data_map的键值获取时钟边沿信息
        auto it = data_map.find(cd_);
        if (it != data_map.end()) {
            clk_edge = &it->second;
        }
    }

    // 创建对应的寄存器指令
    return std::make_unique<ch::instr_reg>(
        current_buf, size_, next_buf, clk_edge,
        clk_en_ ? &data_map[clk_en_->id()] : nullptr,
        rst_ ? &data_map[rst_->id()] : nullptr,
        rst_val_ ? &data_map[rst_val_->id()] : nullptr);
}

// 在 src/core/ast_nodes.cpp 中更新
std::unique_ptr<ch::instr_base>
opimpl::create_instruction(ch::data_map_t &data_map) const {
    // 获取操作数缓冲区
    auto *dst_buf = &data_map[id_];
    auto *src0_buf = &data_map[lhs()->id()];
    auto *src1_buf = rhs() ? &data_map[rhs()->id()] : nullptr;

    // 添加用户跟踪：将操作节点添加到源节点的用户列表中
    lhs()->add_user(const_cast<opimpl *>(this));
    if (rhs()) {
        rhs()->add_user(const_cast<opimpl *>(this));
    }

    // 根据操作类型创建对应的指令
    switch (op_) {
    // 算术操作
    case ch_op::add:
        return std::make_unique<ch::instr_op_add>(dst_buf, size_, src0_buf,
                                                  src1_buf);
    case ch_op::sub:
        return std::make_unique<ch::instr_op_sub>(dst_buf, size_, src0_buf,
                                                  src1_buf);
    case ch_op::mul:
        return std::make_unique<ch::instr_op_mul>(dst_buf, size_, src0_buf,
                                                  src1_buf);
    case ch_op::div:
        return std::make_unique<ch::instr_op_div>(dst_buf, size_, src0_buf,
                                                  src1_buf);
    case ch_op::mod:
        return std::make_unique<ch::instr_op_mod>(dst_buf, size_, src0_buf,
                                                  src1_buf);
    // 位操作
    case ch_op::and_:
        return std::make_unique<ch::instr_op_and>(dst_buf, size_, src0_buf,
                                                  src1_buf);
    case ch_op::or_:
        return std::make_unique<ch::instr_op_or>(dst_buf, size_, src0_buf,
                                                 src1_buf);
    case ch_op::xor_:
        return std::make_unique<ch::instr_op_xor>(dst_buf, size_, src0_buf,
                                                  src1_buf);
    case ch_op::not_:
        return std::make_unique<ch::instr_op_not>(dst_buf, size_, src0_buf);
    // 比较操作
    case ch_op::eq:
        return std::make_unique<ch::instr_op_eq>(dst_buf, 1, src0_buf,
                                                 src1_buf);
    case ch_op::ne:
        return std::make_unique<ch::instr_op_ne>(dst_buf, 1, src0_buf,
                                                 src1_buf);
    case ch_op::lt:
        return std::make_unique<ch::instr_op_lt>(dst_buf, 1, src0_buf,
                                                 src1_buf);
    case ch_op::le:
        return std::make_unique<ch::instr_op_le>(dst_buf, 1, src0_buf,
                                                 src1_buf);
    case ch_op::gt:
        return std::make_unique<ch::instr_op_gt>(dst_buf, 1, src0_buf,
                                                 src1_buf);
    case ch_op::ge:
        return std::make_unique<ch::instr_op_ge>(dst_buf, 1, src0_buf,
                                                 src1_buf);
    // 移位操作
    case ch_op::shl:
        return std::make_unique<ch::instr_op_shl>(dst_buf, size_, src0_buf,
                                                  src1_buf);
    case ch_op::shr:
        return std::make_unique<ch::instr_op_shr>(dst_buf, size_, src0_buf,
                                                  src1_buf);
    case ch_op::sshr:
        return std::make_unique<ch::instr_op_sshr>(dst_buf, size_, src0_buf,
                                                   src1_buf);
    case ch_op::bit_sel:
        return std::make_unique<ch::instr_op_bit_sel>(dst_buf, 1, src0_buf,
                                                      src1_buf);
    // 一元操作
    case ch_op::assign:
        return std::make_unique<ch::instr_op_assign>(dst_buf, size_, src0_buf);
    case ch_op::neg:
        return std::make_unique<ch::instr_op_neg>(dst_buf, size_, src0_buf);
    case ch_op::bits_extract:
        return std::make_unique<ch::instr_op_bits_extract>(dst_buf, size_,
                                                           src0_buf, src1_buf);
    case ch_op::concat:
        return std::make_unique<ch::instr_op_concat>(dst_buf, size_, src0_buf,
                                                     src1_buf);
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
    case ch_op::popcount:
        return std::make_unique<ch::instr_op_popcount>(dst_buf, 1, src0_buf);
    default:
        return nullptr;
    }
}

// proxyimpl的指令创建实现
std::unique_ptr<ch::instr_base>
proxyimpl::create_instruction(ch::data_map_t &data_map) const {
    auto *dst_buf = &data_map[id_];
    if (num_srcs() > 0 && src(0)) {
        auto *src_buf = &data_map[src(0)->id()];
        // 添加用户跟踪：当创建proxy指令时，将proxy节点添加到源节点的用户列表中
        src(0)->add_user(const_cast<proxyimpl *>(this));
        return std::make_unique<ch::instr_proxy>(dst_buf, size_, src_buf);
    }
    return nullptr;
}

// inputimpl的指令创建实现
std::unique_ptr<ch::instr_base>
inputimpl::create_instruction(ch::data_map_t &data_map) const {
    auto *dst_buf = &data_map[id_];
    if (num_srcs() > 0 && src(0)) {
        auto *src_buf = &data_map[src(0)->id()];
        return std::make_unique<ch::instr_input>(dst_buf, size_, src_buf);
    }
    return nullptr;
}

// outputimpl的指令创建实现
std::unique_ptr<ch::instr_base>
outputimpl::create_instruction(ch::data_map_t &data_map) const {
    auto *dst_buf = &data_map[id_];
    if (num_srcs() > 0 && src(0)) {
        auto *src_buf = &data_map[src(0)->id()];
        // 添加用户跟踪：当创建output指令时，将output节点添加到源节点的用户列表中
        src(0)->add_user(const_cast<outputimpl *>(this));
        return std::make_unique<ch::instr_output>(dst_buf, size_, src_buf);
    }
    return nullptr;
}

} // namespace core
} // namespace ch