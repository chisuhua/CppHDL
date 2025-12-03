// src/core/mem_port_impl.cpp
#include "ast/mem_port_impl.h"
#include "ast/instr_mem.h"
#include "core/context.h"

namespace ch::core
{

    // mem_read_port_impl implementation
    lnodeimpl *mem_read_port_impl::clone(context *new_ctx, const clone_map &cloned_nodes) const
    {
        auto *parent = reinterpret_cast<memimpl *>(cloned_nodes.at(parent_mem_->id()));
        auto *cd = has_cd() ? cloned_nodes.at(this->cd()->id()) : nullptr;
        auto *addr = has_addr() ? cloned_nodes.at(this->addr()->id()) : nullptr;
        auto *enable = has_enable() ? cloned_nodes.at(this->enable()->id()) : nullptr;
        auto *data_output_node = data_output() ? cloned_nodes.at(this->data_output()->id()) : nullptr;

        return new_ctx->create_node<mem_read_port_impl>(
            parent, port_id_, port_type_, size_, cd, addr, enable, data_output_node,
            name_, sloc_);
    }

    std::unique_ptr<ch::instr_base> mem_read_port_impl::create_instruction(
        ch::data_map_t &data_map) const
    {

        std::unique_ptr<ch::instr_base> instr;

        // 根据端口类型创建不同的指令对象
        if (port_type_ == mem_port_type::async_read)
        {
            instr = std::make_unique<ch::instr_mem_async_read_port>(
                port_id_, parent_mem_->id(), size_);
        }
        else if (port_type_ == mem_port_type::sync_read)
        {
            instr = std::make_unique<ch::instr_mem_sync_read_port>(
                port_id_, parent_mem_->id(), size_);
        }

        if (instr)
        {
            // 设置指令ID
            // instr->set_id(id());

            // 初始化端口指令对象
            if (port_type_ == mem_port_type::async_read)
            {
                auto *async_read_port = static_cast<ch::instr_mem_async_read_port *>(instr.get());
                uint32_t enable_node_id = has_enable() ? enable()->id() : 0;
                async_read_port->init_port(addr()->id(), enable_node_id, data_output()->id(), data_map);
            }
            else if (port_type_ == mem_port_type::sync_read)
            {
                auto *sync_read_port = static_cast<ch::instr_mem_sync_read_port *>(instr.get());
                uint32_t enable_node_id = has_enable() ? enable()->id() : 0;
                sync_read_port->init_port(cd()->id(), addr()->id(), enable_node_id, data_output()->id(), data_map);
            }

            // 设置data_map引用
            // instr->data_map = &data_map;
        }

        return instr;
    }

    // mem_write_port_impl implementation
    lnodeimpl *mem_write_port_impl::clone(context *new_ctx, const clone_map &cloned_nodes) const
    {
        auto *parent = reinterpret_cast<memimpl *>(cloned_nodes.at(parent_mem_->id()));
        auto *cd = has_cd() ? cloned_nodes.at(this->cd()->id()) : nullptr;
        auto *addr = has_addr() ? cloned_nodes.at(this->addr()->id()) : nullptr;
        auto *wdata_node = wdata() ? cloned_nodes.at(this->wdata()->id()) : nullptr;
        auto *enable = has_enable() ? cloned_nodes.at(this->enable()->id()) : nullptr;

        return new_ctx->create_node<mem_write_port_impl>(
            parent, port_id_, size_, cd, addr, wdata_node, enable,
            name_, sloc_);
    }

    std::unique_ptr<ch::instr_base> mem_write_port_impl::create_instruction(
        ch::data_map_t &data_map) const
    {

        auto instr = std::make_unique<ch::instr_mem_write_port>(
            port_id_, parent_mem_->id(), size_);

        if (instr)
        {
            // 设置指令ID
            // instr->set_id(id());

            // 初始化端口指令对象
            uint32_t enable_node_id = has_enable() ? enable()->id() : 0;
            auto *write_port = static_cast<ch::instr_mem_write_port *>(instr.get());
            write_port->init_port(cd()->id(), addr()->id(), wdata()->id(), enable_node_id, data_map);

            // 设置data_map引用
            // instr->data_map = &data_map;
        }

        return instr;
    }

} // namespace ch::core