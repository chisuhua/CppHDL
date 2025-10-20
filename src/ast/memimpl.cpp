// src/core/memimpl.cpp
#include "ast/memimpl.h"
#include "ast/mem_port_impl.h"
#include "ast/instr_mem.h"
#include <algorithm>

namespace ch::core {

void memimpl::remove_read_port(mem_read_port_impl* port) {
    auto it = std::find(read_ports_.begin(), read_ports_.end(), port);
    if (it != read_ports_.end()) {
        read_ports_.erase(it);
    }
}

void memimpl::remove_write_port(mem_write_port_impl* port) {
    auto it = std::find(write_ports_.begin(), write_ports_.end(), port);
    if (it != write_ports_.end()) {
        write_ports_.erase(it);
    }
}

lnodeimpl* memimpl::clone(context* new_ctx, const clone_map& cloned_nodes) const {
    auto* cloned = new_ctx->create_node<memimpl>(
        addr_width_, data_width_, depth_, num_banks_, 
        has_byte_enable_, is_rom_, init_data_, name_, sloc_);
    
    // 注意：端口克隆由端口节点自己处理
    return cloned;
}

std::unique_ptr<ch::instr_base> memimpl::create_instruction(
    ch::data_map_t& data_map) const {
    
    auto instr = std::make_unique<ch::instr_mem>(
        id_, addr_width_, data_width_, depth_, is_rom_, init_data_);
    
    // 为每个读端口添加状态
    for (const auto& port : read_ports_) {
        uint32_t cd_id = port->has_cd() ? port->cd()->id() : static_cast<uint32_t>(-1);
        uint32_t addr_id = port->has_addr() ? port->addr()->id() : static_cast<uint32_t>(-1);
        uint32_t data_id = port->data_output() ? port->data_output()->id() : static_cast<uint32_t>(-1);
        uint32_t enable_id = port->has_enable() ? port->enable()->id() : static_cast<uint32_t>(-1);
        
        bool is_sync = (port->port_type() == mem_port_type::sync_read);
        
        ch::instr_mem::port_state port_state(
            port->port_id(), cd_id, addr_id, data_id, enable_id,
            false, is_sync);
        
        instr->add_port(port_state);
    }
    
    // 为每个写端口添加状态
    for (const auto& port : write_ports_) {
        uint32_t cd_id = port->has_cd() ? port->cd()->id() : static_cast<uint32_t>(-1);
        uint32_t addr_id = port->has_addr() ? port->addr()->id() : static_cast<uint32_t>(-1);
        uint32_t data_id = port->wdata() ? port->wdata()->id() : static_cast<uint32_t>(-1);
        uint32_t enable_id = port->has_enable() ? port->enable()->id() : static_cast<uint32_t>(-1);
        
        ch::instr_mem::port_state port_state(
            port->port_id(), cd_id, addr_id, data_id, enable_id,
            true, false); // 写端口，非同步（同步属性对写端口意义不大）
        
        instr->add_port(port_state);
    }
    
    return instr;
}

} // namespace ch::core
