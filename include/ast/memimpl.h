// include/core/memimpl.h
#ifndef CH_CORE_MEMIMPL_H
#define CH_CORE_MEMIMPL_H

#include "core/lnodeimpl.h"
#include "core/types.h"
#include "ast/instr_base.h"
#include <vector>
#include <string>
#include <source_location>
#include <memory>

using namespace ch;

namespace ch::core {

class mem_read_port_impl;
class mem_write_port_impl;

class memimpl : public lnodeimpl {
private:
    uint32_t addr_width_;
    uint32_t data_width_;
    uint32_t depth_;
    uint32_t num_banks_;
    bool has_byte_enable_;
    uint32_t byte_width_;
    std::vector<sdata_type> init_data_;  // 改为vector格式
    bool is_rom_;
    
    // 端口管理
    std::vector<mem_read_port_impl*> read_ports_;
    std::vector<mem_write_port_impl*> write_ports_;
    uint32_t next_port_id_;

public:
    // 构造函数 - 支持vector初始化数据
    memimpl(uint32_t id, 
            uint32_t addr_width, uint32_t data_width, uint32_t depth,
            uint32_t num_banks, bool has_byte_enable, bool is_rom,
            const std::vector<sdata_type>& init_data,  // vector格式
            const std::string& name, const std::source_location& sloc,
            context* ctx)
        : lnodeimpl(id, lnodetype::type_mem, data_width * depth, ctx, name, sloc)
        , addr_width_(addr_width)
        , data_width_(data_width)
        , depth_(depth)
        , num_banks_(num_banks)
        , has_byte_enable_(has_byte_enable)
        , byte_width_(has_byte_enable ? 8 : data_width)
        , init_data_(init_data)  // 直接存储vector
        , is_rom_(is_rom)
        , next_port_id_(0) {}

    // 访问器
    uint32_t addr_width() const { return addr_width_; }
    uint32_t data_width() const { return data_width_; }
    uint32_t depth() const { return depth_; }
    uint32_t num_banks() const { return num_banks_; }
    bool has_byte_enable() const { return has_byte_enable_; }
    uint32_t byte_width() const { return byte_width_; }
    const std::vector<sdata_type>& init_data() const { return init_data_; }  // 返回vector
    bool is_rom() const { return is_rom_; }
    uint32_t next_port_id() { return next_port_id_++; }

    // 端口管理
    void add_read_port(mem_read_port_impl* port) { read_ports_.push_back(port); }
    void add_write_port(mem_write_port_impl* port) { write_ports_.push_back(port); }
    void remove_read_port(mem_read_port_impl* port);
    void remove_write_port(mem_write_port_impl* port);
    
    const std::vector<mem_read_port_impl*>& read_ports() const { return read_ports_; }
    const std::vector<mem_write_port_impl*>& write_ports() const { return write_ports_; }

    // 覆盖虚函数
    std::string to_string() const override {
        std::ostringstream oss;
        oss << name_ << " (mem, " << depth_ << "x" << data_width_ << " bits)";
        if (is_rom_) oss << " [ROM]";
        return oss.str();
    }

    bool is_const() const override { return false; }

    lnodeimpl* clone(context* new_ctx, const clone_map& cloned_nodes) const override;

    bool equals(const lnodeimpl& other) const override {
        if (!lnodeimpl::equals(other)) return false;
        const auto& mem_other = static_cast<const memimpl&>(other);
        return addr_width_ == mem_other.addr_width_ &&
               data_width_ == mem_other.data_width_ &&
               depth_ == mem_other.depth_ &&
               num_banks_ == mem_other.num_banks_ &&
               has_byte_enable_ == mem_other.has_byte_enable_ &&
               is_rom_ == mem_other.is_rom_ &&
               init_data_ == mem_other.init_data_;  // vector比较
    }

    // 创建对应的仿真指令
    std::unique_ptr<instr_base> create_instruction(
        data_map_t& data_map) const override;
};

} // namespace ch::core

#endif // CH_CORE_MEMIMPL_H
