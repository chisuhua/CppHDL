// src/sim/instr_mem.cpp
#include "ast/instr_mem.h"
#include "logger.h"
#include <cassert>

namespace ch {

///////////////////////////////////////////////////////////////////////////////

// instr_mem implementation
instr_mem::instr_mem(uint32_t node_id, uint32_t addr_width, uint32_t data_width, 
                     uint32_t depth, bool is_rom, const std::vector<sdata_type>& init_data)
    : instr_base(data_width * depth)
    , node_id_(node_id)
    , addr_width_(addr_width)
    , data_width_(data_width)
    , depth_(depth)
    , is_rom_(is_rom) {
    
    CHDBG_FUNC();
    CHINFO("Created memory instruction for node_id=%u, %ux%u bits, depth=%u, %s", 
           node_id_, data_width_, depth_, is_rom_ ? "ROM" : "RAM");
    
    initialize_memory(init_data);
}

void instr_mem::initialize_memory(const std::vector<sdata_type>& init_data) {
    // 1. 初始化内存数组为0
    memory_.resize(depth_);
    for (uint32_t i = 0; i < depth_; ++i) {
        memory_[i] = sdata_type(0, data_width_);  // 默认初始化为0
    }
    
    // 2. 如果有初始数据，加载到内存中
    if (!init_data.empty()) {
        uint32_t load_count = std::min(depth_, static_cast<uint32_t>(init_data.size()));
        
        for (uint32_t i = 0; i < load_count; ++i) {
            // 检查数据宽度是否匹配
            if (init_data[i].bitwidth() == data_width_) {
                memory_[i] = init_data[i];  // 直接赋值，类型安全
            } else {
                // 宽度不匹配时进行转换
                CHERROR("Memory init data width mismatch at index %u: expected %u, got %u", 
                       i, data_width_, init_data[i].bitwidth());
            }
        }
    }
    
    CHDBG("Initialized %u memory locations with %s data", 
          static_cast<uint32_t>(memory_.size()), 
          init_data.empty() ? "default (0)" : "custom");
}
void instr_mem::add_port(const port_state& port) {
    size_t index = ports_.size();
    ports_.push_back(port);
    port_map_[port.port_id] = index;
}

void instr_mem::remove_port(uint32_t port_id) {
    auto it = port_map_.find(port_id);
    if (it != port_map_.end()) {
        ports_.erase(ports_.begin() + it->second);
        port_map_.erase(it);
        // 重新构建映射
        port_map_.clear();
        for (size_t i = 0; i < ports_.size(); ++i) {
            port_map_[ports_[i].port_id] = i;
        }
    }
}

uint32_t instr_mem::get_address(const sdata_type& addr_data) const {
    uint64_t addr_val = static_cast<uint64_t>(addr_data);
    return static_cast<uint32_t>(addr_val) % depth_;
}

bool instr_mem::is_enabled(const sdata_type& enable_data) const {
    if (enable_data.bitwidth() == 0) return true; // 无使能信号，默认使能
    return !enable_data.is_zero();
}

bool instr_mem::is_pos_edge(bool current_clk, bool last_clk) const {
    return current_clk && !last_clk;
}

void instr_mem::write_memory(uint32_t addr, const sdata_type& data, const sdata_type& enable) {
    if (is_rom_ || addr >= depth_) return;
    if (enable.is_zero()) return;
    
    if (enable.bitwidth() > 1) {
        // 字节使能写入
        uint32_t bytes = (data_width_ + 7) / 8;
        for (uint32_t byte_idx = 0; byte_idx < bytes; ++byte_idx) {
            if (enable.get_bit(byte_idx)) {  // 使用sdata_type的get_bit
                for (uint32_t bit = 0; bit < 8 && (byte_idx * 8 + bit) < data_width_; ++bit) {
                    bool bit_val = data.get_bit(byte_idx * 8 + bit);  // 使用sdata_type的get_bit
                    memory_[addr].set_bit(byte_idx * 8 + bit, bit_val); // 使用sdata_type的set_bit
                }
            }
        }
    } else {
        // 全字写入
        memory_[addr] = data;
    }
}

sdata_type instr_mem::read_memory(uint32_t addr) const {
    if (addr >= depth_) {
        return sdata_type(0, data_width_);
    }
    return memory_[addr];
}

void instr_mem::eval(const ch::data_map_t& data_map) {
    CHDBG_FUNC();
    
    // 处理所有端口
    for (auto& port : ports_) {
        try {
            // 获取时钟信号
            bool current_clk = false;
            if (port.cd_node_id != static_cast<uint32_t>(-1)) {
                auto cd_it = data_map.find(port.cd_node_id);
                if (cd_it != data_map.end()) {
                    current_clk = !cd_it->second.is_zero();
                }
            }
            
            // 获取地址信号
            auto addr_it = data_map.find(port.addr_node_id);
            if (addr_it == data_map.end()) continue;
            const sdata_type& addr_data = addr_it->second;
            
            // 获取使能信号
            sdata_type enable_data(1);
            if (port.enable_node_id != static_cast<uint32_t>(-1)) {
                auto enable_it = data_map.find(port.enable_node_id);
                if (enable_it != data_map.end()) {
                    enable_data = enable_it->second;
                }
            }
            
            uint32_t addr = get_address(addr_data);
            
            if (port.is_write) {
                // 写端口处理
                if (is_pos_edge(current_clk, port.last_clk) && is_enabled(enable_data)) {
                    auto data_it = data_map.find(port.data_node_id);
                    if (data_it != data_map.end()) {
                        write_memory(addr, data_it->second, enable_data);
                    }
                }
            } else {
                // 读端口处理
                if (!port.is_write) {
                    // 异步读取：立即响应
                    // 同步读取：在时钟边沿响应（这里简化处理）
                    sdata_type read_data = read_memory(addr);
                    auto data_it = data_map.find(port.data_node_id);
                    if (data_it != data_map.end()) {
                        const_cast<sdata_type&>(data_it->second) = read_data;
                    }
                }
            }
            
            // 更新时钟状态
            port.last_clk = current_clk;
            
        } catch (const std::exception& e) {
            CHERROR("Error processing memory port %u: %s", port.port_id, e.what());
        }
    }
}

///////////////////////////////////////////////////////////////////////////////

// instr_mem_read_port implementation
instr_mem_read_port::instr_mem_read_port(uint32_t port_id, uint32_t parent_mem_id, 
                                        uint32_t data_width, bool is_sync)
    : instr_base(data_width)
    , port_id_(port_id)
    , parent_mem_id_(parent_mem_id)
    , is_sync_(is_sync) {
    
    CHDBG_FUNC();
    CHINFO("Created memory read port instruction for port_id=%u, parent=%u", 
           port_id_, parent_mem_id_);
}

void instr_mem_read_port::eval(const ch::data_map_t& data_map) {
    CHDBG_FUNC();
    // 读端口的具体仿真逻辑主要在mem节点中处理
    // 这里可以处理端口特定的逻辑（如延迟等）
}

///////////////////////////////////////////////////////////////////////////////

// instr_mem_write_port implementation
instr_mem_write_port::instr_mem_write_port(uint32_t port_id, uint32_t parent_mem_id, 
                                          uint32_t data_width)
    : instr_base(data_width)
    , port_id_(port_id)
    , parent_mem_id_(parent_mem_id) {
    
    CHDBG_FUNC();
    CHINFO("Created memory write port instruction for port_id=%u, parent=%u", 
           port_id_, parent_mem_id_);
}

void instr_mem_write_port::eval(const ch::data_map_t& data_map) {
    CHDBG_FUNC();
    // 写端口的具体仿真逻辑主要在mem节点中处理
    // 这里可以处理端口特定的逻辑
}

} // namespace ch
