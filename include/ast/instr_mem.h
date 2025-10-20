// include/sim/instr_mem.h
#ifndef INSTR_MEM_H
#define INSTR_MEM_H

#include "instr_base.h"
#include "types.h"
#include <cstdint>
#include <vector>
#include <unordered_map>

using namespace ch::core;
namespace ch {

class data_map_t;
///////////////////////////////////////////////////////////////////////////////

class instr_mem : public instr_base {
public:
    struct port_state {
        uint32_t port_id;
        uint32_t cd_node_id;      // 时钟域节点ID
        uint32_t addr_node_id;    // 地址节点ID
        uint32_t data_node_id;    // 数据节点ID（读端口输出或写数据输入）
        uint32_t enable_node_id;  // 使能节点ID
        bool last_clk;           // 上一个时钟状态
        bool is_write;           // 是否为写端口
        bool is_sync;            // 是否为同步端口
        
        port_state(uint32_t id, uint32_t cd, uint32_t addr, uint32_t data, 
                  uint32_t enable, bool write, bool sync)
            : port_id(id), cd_node_id(cd), addr_node_id(addr), data_node_id(data), 
              enable_node_id(enable), last_clk(false), is_write(write), is_sync(sync) {}
    };

private:
    uint32_t node_id_;
    uint32_t addr_width_;
    uint32_t data_width_;
    uint32_t depth_;
    bool is_rom_;
    
    // 内存存储 - 直接使用vector<sdata_type>
    std::vector<sdata_type> memory_;
    
    // 端口状态管理
    std::vector<port_state> ports_;
    std::unordered_map<uint32_t, size_t> port_map_;

public:
    // 构造函数 - 接收vector格式的初始化数据
    instr_mem(uint32_t node_id, uint32_t addr_width, uint32_t data_width, uint32_t depth,
              bool is_rom, const std::vector<sdata_type>& init_data);  // vector格式
    
    void eval(const ch::data_map_t& data_map) override;
    
    uint32_t node_id() const { return node_id_; }
    uint32_t addr_width() const { return addr_width_; }
    uint32_t data_width() const { return data_width_; }
    uint32_t depth() const { return depth_; }
    bool is_rom() const { return is_rom_; }

    // 端口管理
    void add_port(const port_state& port);
    void remove_port(uint32_t port_id);

private:
    void initialize_memory(const std::vector<sdata_type>& init_data);  // 使用vector初始化
    uint32_t get_address(const sdata_type& addr_data) const;
    void write_memory(uint32_t addr, const sdata_type& data, const sdata_type& enable);
    sdata_type read_memory(uint32_t addr) const;
    bool is_enabled(const sdata_type& enable_data) const;
    bool is_pos_edge(bool current_clk, bool last_clk) const;
};

class instr_mem_read_port : public instr_base {
private:
    uint32_t port_id_;
    uint32_t parent_mem_id_;
    bool is_sync_;

public:
    instr_mem_read_port(uint32_t port_id, uint32_t parent_mem_id, uint32_t data_width, bool is_sync);
    void eval(const ch::data_map_t& data_map) override;
};

///////////////////////////////////////////////////////////////////////////////

class instr_mem_write_port : public instr_base {
private:
    uint32_t port_id_;
    uint32_t parent_mem_id_;

public:
    instr_mem_write_port(uint32_t port_id, uint32_t parent_mem_id, uint32_t data_width);
    void eval(const ch::data_map_t& data_map) override;
};

} // namespace ch

#endif // INSTR_MEM_H
