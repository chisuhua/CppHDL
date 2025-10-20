// src/core/mem_port_impl.cpp
#include "ast/mem_port_impl.h"
#include "ast/instr_mem.h"
#include "core/context.h"

namespace ch::core {

// mem_read_port_impl implementation
lnodeimpl* mem_read_port_impl::clone(context* new_ctx, const clone_map& cloned_nodes) const {
    auto* parent = reinterpret_cast<memimpl*>(cloned_nodes.at(parent_mem_->id()));
    auto* cd = has_cd() ? cloned_nodes.at(this->cd()->id()) : nullptr;
    auto* addr = has_addr() ? cloned_nodes.at(this->addr()->id()) : nullptr;
    auto* enable = has_enable() ? cloned_nodes.at(this->enable()->id()) : nullptr;
    auto* data_output_node = data_output() ? cloned_nodes.at(this->data_output()->id()) : nullptr;
    
    return new_ctx->create_node<mem_read_port_impl>(
        parent, port_id_, port_type_, size_, cd, addr, enable, data_output_node,
        name_, sloc_);
}

std::unique_ptr<ch::instr_base> mem_read_port_impl::create_instruction(
    ch::data_map_t& /*data_map*/) const {
    
    return std::make_unique<ch::instr_mem_read_port>(
        port_id_, parent_mem_->id(), size_, 
        port_type_ == mem_port_type::sync_read);
}

// mem_write_port_impl implementation
lnodeimpl* mem_write_port_impl::clone(context* new_ctx, const clone_map& cloned_nodes) const {
    auto* parent = reinterpret_cast<memimpl*>(cloned_nodes.at(parent_mem_->id()));
    auto* cd = has_cd() ? cloned_nodes.at(this->cd()->id()) : nullptr;
    auto* addr = has_addr() ? cloned_nodes.at(this->addr()->id()) : nullptr;
    auto* wdata_node = wdata() ? cloned_nodes.at(this->wdata()->id()) : nullptr;
    auto* enable = has_enable() ? cloned_nodes.at(this->enable()->id()) : nullptr;
    
    return new_ctx->create_node<mem_write_port_impl>(
        parent, port_id_, size_, cd, addr, wdata_node, enable,
        name_, sloc_);
}

std::unique_ptr<ch::instr_base> mem_write_port_impl::create_instruction(
    ch::data_map_t& /*data_map*/) const {
    
    return std::make_unique<ch::instr_mem_write_port>(
        port_id_, parent_mem_->id(), size_);
}

} // namespace ch::core
