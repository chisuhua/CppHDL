// src/sim/instr_mem.cpp
#include "ast/instr_mem.h"
#include "logger.h"
#include <cassert>

namespace ch {

///////////////////////////////////////////////////////////////////////////////

// instr_mem implementation
instr_mem::instr_mem(uint32_t node_id, uint32_t addr_width, uint32_t data_width,
                     uint32_t depth, bool is_rom,
                     const std::vector<sdata_type> &init_data)
    : instr_base(data_width * depth), node_id_(node_id),
      addr_width_(addr_width), data_width_(data_width), depth_(depth),
      is_rom_(is_rom) {

    CHDBG_FUNC();
    CHINFO(
        "Created memory instruction for node_id=%u, %ux%u bits, depth=%u, %s",
        node_id_, data_width_, depth_, is_rom_ ? "ROM" : "RAM");

    initialize_memory(init_data);
}

void instr_mem::initialize_memory(const std::vector<sdata_type> &init_data) {
    // 1. 初始化内存数组为0
    memory_.resize(depth_);
    for (uint32_t i = 0; i < depth_; ++i) {
        memory_[i] = sdata_type(0, data_width_); // 默认初始化为0
    }

    // 2. 如果有初始数据，加载到内存中
    if (!init_data.empty()) {
        uint32_t load_count =
            std::min(depth_, static_cast<uint32_t>(init_data.size()));

        for (uint32_t i = 0; i < load_count; ++i) {
            // 检查数据宽度是否匹配
            if (init_data[i].bitwidth() == data_width_) {
                memory_[i] = init_data[i]; // 直接赋值，类型安全
            } else {
                // 宽度不匹配时进行转换
                CHERROR("Memory init data width mismatch at index %u: expected "
                        "%u, got %u",
                        i, data_width_, init_data[i].bitwidth());
            }
        }
    }

    CHDBG("Initialized %u memory locations with %s data",
          static_cast<uint32_t>(memory_.size()),
          init_data.empty() ? "default (0)" : "custom");
}

uint32_t instr_mem::get_address(const sdata_type &addr_data) const {
    uint64_t addr_val = static_cast<uint64_t>(addr_data);
    return static_cast<uint32_t>(addr_val) % depth_;
}

void instr_mem::write_memory(uint32_t addr, const sdata_type &data,
                             const sdata_type &enable) {
    if (is_rom_ || addr >= depth_)
        return;
    if (enable.is_zero())
        return;

    if (enable.bitwidth() > 1) {
        // 字节使能写入
        uint32_t bytes = (data_width_ + 7) / 8;
        for (uint32_t byte_idx = 0; byte_idx < bytes; ++byte_idx) {
            if (enable.get_bit(byte_idx)) { // 使用sdata_type的get_bit
                for (uint32_t bit = 0;
                     bit < 8 && (byte_idx * 8 + bit) < data_width_; ++bit) {
                    bool bit_val = data.get_bit(byte_idx * 8 +
                                                bit); // 使用sdata_type的get_bit
                    memory_[addr].set_bit(byte_idx * 8 + bit,
                                          bit_val); // 使用sdata_type的set_bit
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

void instr_mem::eval() {
    // mem节点本身不执行任何操作，所有操作由端口处理
    CHDBG_FUNC();
}

///////////////////////////////////////////////////////////////////////////////

// instr_mem_async_read_port implementation
instr_mem_async_read_port::instr_mem_async_read_port(uint32_t port_id,
                                                     uint32_t parent_mem_id,
                                                     uint32_t data_width)
    : instr_base(data_width), port_id_(port_id), parent_mem_id_(parent_mem_id),
      mem_ptr_(nullptr), addr_node_id_(0), enable_node_id_(0),
      addr_data_ptr_(nullptr), output_data_ptr_(nullptr),
      enable_data_ptr_(nullptr) {

    CHDBG_FUNC();
    CHINFO(
        "Created async memory read port instruction for port_id=%u, parent=%u",
        port_id_, parent_mem_id_);
}

void instr_mem_async_read_port::init_port(uint32_t addr_node_id,
                                          uint32_t enable_node_id,
                                          uint32_t output_node_id,
                                          data_map_t &data_map) {
    addr_node_id_ = addr_node_id;
    enable_node_id_ = enable_node_id;
    output_node_id_ = output_node_id;

    // 预获取数据指针
    // 获取地址信号指针
    auto addr_it = data_map.find(addr_node_id_);
    if (addr_it != data_map.end()) {
        addr_data_ptr_ = &addr_it->second;
    }

    // 获取使能信号指针（如果有）
    if (enable_node_id_ != 0) {
        auto enable_it = data_map.find(enable_node_id_);
        if (enable_it != data_map.end()) {
            enable_data_ptr_ = &enable_it->second;
        }
    }

    // 获取输出数据指针
    auto output_it = data_map.find(output_node_id_);
    if (output_it != data_map.end()) {
        output_data_ptr_ = &const_cast<sdata_type &>(output_it->second);
    }
}

void instr_mem_async_read_port::eval() {
    CHDBG_FUNC();

    // 检查必要指针是否存在
    if (!mem_ptr_ || !addr_data_ptr_ || !output_data_ptr_)
        return;

    // 获取地址
    uint32_t addr = mem_ptr_->get_address(*addr_data_ptr_);

    // 检查使能（如果有）
    bool enabled = true;
    if (enable_data_ptr_) {
        enabled = !enable_data_ptr_->is_zero();
    }

    if (!enabled)
        return;

    // 执行读取
    sdata_type read_data = mem_ptr_->read_memory(addr);

    // 写入输出
    *output_data_ptr_ = read_data;
}

///////////////////////////////////////////////////////////////////////////////

// instr_mem_sync_read_port implementation
instr_mem_sync_read_port::instr_mem_sync_read_port(uint32_t port_id,
                                                   uint32_t parent_mem_id,
                                                   uint32_t data_width)
    : instr_base(data_width), port_id_(port_id), parent_mem_id_(parent_mem_id),
      last_clk_(false), mem_ptr_(nullptr), clk_node_id_(0), addr_node_id_(0),
      enable_node_id_(0), clk_data_ptr_(nullptr), addr_data_ptr_(nullptr),
      output_data_ptr_(nullptr), enable_data_ptr_(nullptr) {

    CHDBG_FUNC();
    CHINFO(
        "Created sync memory read port instruction for port_id=%u, parent=%u",
        port_id_, parent_mem_id_);
}

void instr_mem_sync_read_port::init_port(uint32_t clk_node_id,
                                         uint32_t addr_node_id,
                                         uint32_t enable_node_id,
                                         uint32_t output_node_id,
                                         data_map_t &data_map) {
    clk_node_id_ = clk_node_id;
    addr_node_id_ = addr_node_id;
    enable_node_id_ = enable_node_id;
    output_node_id_ = output_node_id;

    // 预获取数据指针
    // 获取时钟信号指针
    auto clk_it = data_map.find(clk_node_id_);
    if (clk_it != data_map.end()) {
        clk_data_ptr_ = &clk_it->second;
    }

    // 获取地址信号指针
    auto addr_it = data_map.find(addr_node_id_);
    if (addr_it != data_map.end()) {
        addr_data_ptr_ = &addr_it->second;
    }

    // 获取使能信号指针（如果有）
    if (enable_node_id_ != 0) {
        auto enable_it = data_map.find(enable_node_id_);
        if (enable_it != data_map.end()) {
            enable_data_ptr_ = &enable_it->second;
        }
    }

    // 获取输出数据指针
    auto output_it = data_map.find(output_node_id_);
    if (output_it != data_map.end()) {
        output_data_ptr_ = &const_cast<sdata_type &>(output_it->second);
    }
}

void instr_mem_sync_read_port::eval() {
    CHDBG_FUNC();

    // 检查必要指针是否存在
    if (!mem_ptr_ || !clk_data_ptr_ || !addr_data_ptr_ || !output_data_ptr_)
        return;

    // 检查时钟上升沿
    bool current_clk = !clk_data_ptr_->is_zero();
    if (!(current_clk && !last_clk_)) {
        last_clk_ = current_clk;
        return;
    }
    last_clk_ = current_clk;

    // 获取地址
    uint32_t addr = mem_ptr_->get_address(*addr_data_ptr_);

    // 检查使能（如果有）
    bool enabled = true;
    if (enable_data_ptr_) {
        enabled = !enable_data_ptr_->is_zero();
    }

    if (!enabled)
        return;

    // 执行读取
    sdata_type read_data = mem_ptr_->read_memory(addr);

    // 写入输出
    *output_data_ptr_ = read_data;
}

///////////////////////////////////////////////////////////////////////////////

// instr_mem_write_port implementation
instr_mem_write_port::instr_mem_write_port(uint32_t port_id,
                                           uint32_t parent_mem_id,
                                           uint32_t data_width)
    : instr_base(data_width), port_id_(port_id), parent_mem_id_(parent_mem_id),
      last_clk_(false), mem_ptr_(nullptr), clk_node_id_(0), addr_node_id_(0),
      wdata_node_id_(0), enable_node_id_(0), clk_data_ptr_(nullptr),
      addr_data_ptr_(nullptr), wdata_ptr_(nullptr), enable_data_ptr_(nullptr) {

    CHDBG_FUNC();
    CHINFO("Created memory write port instruction for port_id=%u, parent=%u",
           port_id_, parent_mem_id_);
}

void instr_mem_write_port::init_port(uint32_t clk_node_id,
                                     uint32_t addr_node_id,
                                     uint32_t wdata_node_id,
                                     uint32_t enable_node_id,
                                     data_map_t &data_map) {
    clk_node_id_ = clk_node_id;
    addr_node_id_ = addr_node_id;
    wdata_node_id_ = wdata_node_id;
    enable_node_id_ = enable_node_id;

    // 预获取数据指针
    // 获取时钟信号指针
    auto clk_it = data_map.find(clk_node_id_);
    if (clk_it != data_map.end()) {
        clk_data_ptr_ = &clk_it->second;
    }

    // 获取地址信号指针
    auto addr_it = data_map.find(addr_node_id_);
    if (addr_it != data_map.end()) {
        addr_data_ptr_ = &addr_it->second;
    }

    // 获取写入数据指针
    auto wdata_it = data_map.find(wdata_node_id_);
    if (wdata_it != data_map.end()) {
        wdata_ptr_ = &wdata_it->second;
    }

    // 获取使能信号指针（如果有）
    if (enable_node_id_ != 0) {
        auto enable_it = data_map.find(enable_node_id_);
        if (enable_it != data_map.end()) {
            enable_data_ptr_ = &enable_it->second;
        }
    }
}

void instr_mem_write_port::eval() {
    CHDBG_FUNC();

    // 检查必要指针是否存在
    if (!mem_ptr_ || !clk_data_ptr_ || !addr_data_ptr_ || !wdata_ptr_)
        return;

    // 检查时钟上升沿
    bool current_clk = !clk_data_ptr_->is_zero();
    if (!(current_clk && !last_clk_)) {
        last_clk_ = current_clk;
        return;
    }
    last_clk_ = current_clk;

    // 获取地址
    uint32_t addr = mem_ptr_->get_address(*addr_data_ptr_);

    // 检查使能（如果有）
    sdata_type enable_data(1); // 默认使能
    if (enable_data_ptr_) {
        enable_data = *enable_data_ptr_;
    }

    // 检查使能是否为零
    if (enable_data.is_zero())
        return;

    // 执行写入
    mem_ptr_->write_memory(addr, *wdata_ptr_, enable_data);
}

} // namespace ch