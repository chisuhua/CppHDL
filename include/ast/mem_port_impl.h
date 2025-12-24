// include/core/mem_port_impl.h
#ifndef CH_CORE_MEM_PORT_IMPL_H
#define CH_CORE_MEM_PORT_IMPL_H

#include "ast/memimpl.h"
#include "core/context.h" // 添加context包含
#include "core/lnodeimpl.h"
#include <source_location>
#include <string>

namespace ch::core {

///////////////////////////////////////////////////////////////////////////////

// 内存端口基类
class mem_port_impl : public lnodeimpl {
protected:
    memimpl *parent_mem_;
    uint32_t port_id_;
    mem_port_type port_type_;
    int cd_idx_;     // 时钟域索引
    int addr_idx_;   // 地址索引
    int enable_idx_; // 使能索引

public:
    mem_port_impl(uint32_t id, memimpl *parent, uint32_t port_id,
                  mem_port_type type, uint32_t size, lnodeimpl *cd,
                  lnodeimpl *addr, lnodeimpl *enable, const std::string &name,
                  const std::source_location &sloc, context *ctx)
        : lnodeimpl(id,
                    (type == mem_port_type::write)
                        ? lnodetype::type_mem_write_port
                        : lnodetype::type_mem_read_port,
                    size, ctx, name, sloc),
          parent_mem_(parent), port_id_(port_id), port_type_(type), cd_idx_(-1),
          addr_idx_(-1), enable_idx_(-1) {

        // 添加时钟域
        if (cd) {
            cd_idx_ = add_src(cd);
        }

        // 添加地址
        if (addr) {
            addr_idx_ = add_src(addr);
        }

        // 添加使能
        if (enable) {
            enable_idx_ = add_src(enable);
        }
    }

    // 访问器
    memimpl *parent() const { return parent_mem_; }
    uint32_t port_id() const { return port_id_; }
    mem_port_type port_type() const { return port_type_; }

    bool has_cd() const { return cd_idx_ != -1; }
    bool has_addr() const { return addr_idx_ != -1; }
    bool has_enable() const { return enable_idx_ != -1; }

    lnodeimpl *cd() const { return has_cd() ? src(cd_idx_) : nullptr; }
    lnodeimpl *addr() const { return has_addr() ? src(addr_idx_) : nullptr; }
    lnodeimpl *enable() const {
        return has_enable() ? src(enable_idx_) : nullptr;
    }

    // 克隆支持
    lnodeimpl *clone(context *new_ctx,
                     const clone_map &cloned_nodes) const override = 0;
};

///////////////////////////////////////////////////////////////////////////////

// 内存读端口
class mem_read_port_impl : public mem_port_impl {
private:
    int data_output_idx_; // 数据输出索引

public:
    mem_read_port_impl(uint32_t id, memimpl *parent, uint32_t port_id,
                       mem_port_type type, uint32_t size, lnodeimpl *cd,
                       lnodeimpl *addr, lnodeimpl *enable,
                       lnodeimpl *data_output, const std::string &name,
                       const std::source_location &sloc, context *ctx)
        : mem_port_impl(id, parent, port_id, type, size, cd, addr, enable, name,
                        sloc, ctx),
          data_output_idx_(-1) {

        // 添加数据输出节点作为源
        if (data_output) {
            data_output->add_src(this);
            // data_output_idx_ = add_src(data_output);
        }

        // 添加父内存节点
        add_src(parent);

        // 注册到父内存
        parent->add_read_port(this);
    }

    lnodeimpl *data_output() const {
        return data_output_idx_ != -1 ? src(data_output_idx_) : nullptr;
    }

    lnodeimpl *clone(context *new_ctx,
                     const clone_map &cloned_nodes) const override;

    std::unique_ptr<ch::instr_base>
    create_instruction(ch::data_map_t &data_map) const override;
};

///////////////////////////////////////////////////////////////////////////////

// 内存写端口
class mem_write_port_impl : public mem_port_impl {
private:
    int wdata_idx_; // 写数据索引

public:
    mem_write_port_impl(uint32_t id, memimpl *parent, uint32_t port_id,
                        uint32_t size, lnodeimpl *cd, lnodeimpl *addr,
                        lnodeimpl *wdata, lnodeimpl *enable,
                        const std::string &name,
                        const std::source_location &sloc, context *ctx)
        : mem_port_impl(id, parent, port_id, mem_port_type::write, size, cd,
                        addr, enable, name, sloc, ctx),
          wdata_idx_(-1) {

        // 添加写数据
        if (wdata) {
            wdata_idx_ = add_src(wdata);
        }

        // 添加父内存节点作为源
        add_src(parent);

        // 注册到父内存
        parent->add_write_port(this);
    }

    lnodeimpl *wdata() const {
        return wdata_idx_ != -1 ? src(wdata_idx_) : nullptr;
    }

    lnodeimpl *clone(context *new_ctx,
                     const clone_map &cloned_nodes) const override;

    std::unique_ptr<ch::instr_base>
    create_instruction(ch::data_map_t &data_map) const override;
};

} // namespace ch::core

#endif // CH_CORE_MEM_PORT_IMPL_H