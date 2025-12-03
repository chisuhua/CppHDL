// include/sim/instr_mem.h
#ifndef INSTR_MEM_H
#define INSTR_MEM_H

#include "instr_base.h"
#include "types.h"
#include <cstdint>
#include <vector>
#include <unordered_map>

using namespace ch::core;
namespace ch
{

    class data_map_t;
    ///////////////////////////////////////////////////////////////////////////////

    class instr_mem : public instr_base
    {
    private:
        uint32_t node_id_;
        uint32_t addr_width_;
        uint32_t data_width_;
        uint32_t depth_;
        bool is_rom_;

        // 内存存储 - 直接使用vector<sdata_type>
        std::vector<sdata_type> memory_;

    public:
        // 构造函数 - 接收vector格式的初始化数据
        instr_mem(uint32_t node_id, uint32_t addr_width, uint32_t data_width, uint32_t depth,
                  bool is_rom, const std::vector<sdata_type> &init_data); // vector格式

        void eval() override;

        uint32_t node_id() const { return node_id_; }
        uint32_t addr_width() const { return addr_width_; }
        uint32_t data_width() const { return data_width_; }
        uint32_t depth() const { return depth_; }
        bool is_rom() const { return is_rom_; }

        // 内存访问接口
        sdata_type *get_memory() { return memory_.data(); }
        const sdata_type *get_memory() const { return memory_.data(); }
        sdata_type &get_data(uint32_t addr) { return memory_.at(addr); }
        const sdata_type &get_data(uint32_t addr) const { return memory_.at(addr); }

        uint32_t get_address(const sdata_type &addr_data) const;

        // 公共接口，供端口指令对象使用
        sdata_type read_memory(uint32_t addr) const;
        void write_memory(uint32_t addr, const sdata_type &data, const sdata_type &enable);

    private:
        void initialize_memory(const std::vector<sdata_type> &init_data); // 使用vector初始化
    };

    // 异步读端口指令对象
    class instr_mem_async_read_port : public instr_base
    {
    private:
        uint32_t port_id_;
        uint32_t parent_mem_id_;

        // 直接引用内存实体
        instr_mem *mem_ptr_; // 内存实体指针

        // 专用成员变量替代inputs_
        uint32_t addr_node_id_;   // 地址节点ID
        uint32_t enable_node_id_; // 使能节点ID（可选）
        uint32_t output_node_id_; // 使能节点ID（可选）

        // 数据指针（预获取）
        const sdata_type *addr_data_ptr_;   // 地址信号指针
        sdata_type *output_data_ptr_;       // 输出数据指针
        const sdata_type *enable_data_ptr_; // 使能信号指针（可选）

    public:
        instr_mem_async_read_port(uint32_t port_id, uint32_t parent_mem_id, uint32_t data_width);
        void eval() override;

        // 初始化函数，在创建后调用以设置指针
        void init_port(uint32_t addr_node_id, uint32_t enable_node_id, uint32_t output_node_id, data_map_t &data_map);

        // 访问器
        uint32_t port_id() const { return port_id_; }
        uint32_t parent_mem_id() const { return parent_mem_id_; }
        instr_mem *get_mem_ptr() const { return mem_ptr_; }
        void set_mem_ptr(instr_mem *mem_ptr) { mem_ptr_ = mem_ptr; }
    };

    // 同步读端口指令对象
    class instr_mem_sync_read_port : public instr_base
    {
    private:
        uint32_t port_id_;
        uint32_t parent_mem_id_;
        bool last_clk_;

        // 直接引用内存实体
        instr_mem *mem_ptr_; // 内存实体指针

        // 专用成员变量替代inputs_
        uint32_t clk_node_id_;    // 时钟节点ID
        uint32_t addr_node_id_;   // 地址节点ID
        uint32_t enable_node_id_; // 使能节点ID（可选）
        uint32_t output_node_id_; // 使能节点ID（可选）

        // 数据指针（预获取）
        const sdata_type *clk_data_ptr_;    // 时钟信号指针
        const sdata_type *addr_data_ptr_;   // 地址信号指针
        sdata_type *output_data_ptr_;       // 输出数据指针
        const sdata_type *enable_data_ptr_; // 使能信号指针（可选）

    public:
        instr_mem_sync_read_port(uint32_t port_id, uint32_t parent_mem_id, uint32_t data_width);
        void eval() override;

        // 初始化函数，在创建后调用以设置指针
        void init_port(uint32_t clk_node_id, uint32_t addr_node_id, uint32_t enable_node_id, uint32_t output_node_id, data_map_t &data_map);

        // 访问器
        uint32_t port_id() const { return port_id_; }
        uint32_t parent_mem_id() const { return parent_mem_id_; }
        bool &last_clk() { return last_clk_; }
        bool last_clk() const { return last_clk_; }
        instr_mem *get_mem_ptr() const { return mem_ptr_; }
        void set_mem_ptr(instr_mem *mem_ptr) { mem_ptr_ = mem_ptr; }
    };

    ///////////////////////////////////////////////////////////////////////////////

    class instr_mem_write_port : public instr_base
    {
    private:
        uint32_t port_id_;
        uint32_t parent_mem_id_;
        bool last_clk_;

        // 直接引用内存实体
        instr_mem *mem_ptr_; // 内存实体指针

        // 专用成员变量替代inputs_
        uint32_t clk_node_id_;    // 时钟节点ID
        uint32_t addr_node_id_;   // 地址节点ID
        uint32_t wdata_node_id_;  // 写入数据节点ID
        uint32_t enable_node_id_; // 使能节点ID（可选）

        // 数据指针（预获取）
        const sdata_type *clk_data_ptr_;    // 时钟信号指针
        const sdata_type *addr_data_ptr_;   // 地址信号指针
        const sdata_type *wdata_ptr_;       // 写入数据指针
        const sdata_type *enable_data_ptr_; // 使能信号指针（可选）

    public:
        instr_mem_write_port(uint32_t port_id, uint32_t parent_mem_id, uint32_t data_width);
        void eval() override;

        // 初始化函数，在创建后调用以设置指针
        void init_port(uint32_t clk_node_id, uint32_t addr_node_id,
                       uint32_t wdata_node_id, uint32_t enable_node_id, data_map_t &data_map);

        // 访问器
        uint32_t port_id() const { return port_id_; }
        uint32_t parent_mem_id() const { return parent_mem_id_; }
        bool &last_clk() { return last_clk_; }
        bool last_clk() const { return last_clk_; }
        instr_mem *get_mem_ptr() const { return mem_ptr_; }
        void set_mem_ptr(instr_mem *mem_ptr) { mem_ptr_ = mem_ptr; }
    };

} // namespace ch

#endif // INSTR_MEM_H