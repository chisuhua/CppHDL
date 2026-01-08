// src/simulator.cpp
#include "simulator.h"
#include "ast/ast_nodes.h"
#include "ast/instr_mem.h"
#include "ast/mem_port_impl.h"
#include "bv/utils.h"
#include "core/lnodeimpl.h"
#include "logger.h"
#include "types.h"
#include "utils/destruction_manager.h"
#include <cassert>
#include <cstring> // 添加memcpy的头文件
#include <fstream> // 添加fstream头文件以支持文件输出
#include <iostream>
#include <set> // 添加set头文件

namespace ch {

Simulator::Simulator(ch::core::context *ctx, bool trace_on)
    : ctx_(ctx), trace_on_(trace_on) {
    CHDBG_FUNC();
    CHREQUIRE(ctx_ != nullptr, "Context cannot be null");

    // Register with destruction manager
    std::cout << "Registering simulator " << this << std::endl;
    std::cout.flush();

    ctx_curr_backup_ = ctx_curr_;
    ctx_curr_ = ctx_;

    initialize();

    // 如果启用了跟踪，则收集信号
    if (trace_on_) {
        collect_signals();
    }
}

// 新增构造函数实现
Simulator::Simulator(ch::core::context *ctx, const std::string &config_file)
    : ctx_(ctx), trace_on_(false) {
    CHDBG_FUNC();
    CHREQUIRE(ctx_ != nullptr, "Context cannot be null");

    ctx_curr_backup_ = ctx_curr_;
    ctx_curr_ = ctx_;

    // 从配置文件加载跟踪配置
    load_trace_config_from_file(config_file);

    initialize();

    // 检查是否需要启用跟踪
    trace_on_ = !trace_configs_.empty(); // 如果有任何配置，则启用跟踪
    if (trace_on_) {
        collect_signals();
    }
}

void Simulator::load_trace_config_from_file(const std::string &config_file) {
    CHDBG_FUNC();
    CHINFO("Loading trace configuration from file: %s", config_file.c_str());

    inipp::Ini<char> ini;
    std::ifstream is(config_file);
    if (!is.is_open()) {
        CHWARN("Could not open configuration file: %s", config_file.c_str());
        return;
    }

    ini.parse(is);

    // 遍历所有section，每个section代表一个模块实例
    for (const auto &section_pair : ini.sections) {
        const std::string &section_name = section_pair.first;
        const auto &section = section_pair.second;

        // 解析配置项
        bool trace_on = false;
        bool trace_reg = false;
        bool trace_wire = false;
        bool trace_input = false;
        bool trace_output = false;

        inipp::get_value(section, "trace_on", trace_on);
        inipp::get_value(section, "trace_reg", trace_reg);
        inipp::get_value(section, "trace_wire", trace_wire);
        inipp::get_value(section, "trace_input", trace_input);
        inipp::get_value(section, "trace_output", trace_output);

        trace_configs_[section_name] = TraceConfig(
            trace_on, trace_reg, trace_wire, trace_input, trace_output);

        CHINFO("Loaded trace config for section '%s': trace_on=%d, "
               "trace_reg=%d, trace_wire=%d, trace_input=%d, trace_output=%d",
               section_name.c_str(), trace_on, trace_reg, trace_wire,
               trace_input, trace_output);
    }
}

bool Simulator::should_trace_node(uint32_t node_id,
                                  const std::string &node_name,
                                  ch::core::lnodetype node_type) {
    // 首先检查是否有全局配置（"all" section）
    auto global_it = trace_configs_.find("all");
    if (global_it != trace_configs_.end()) {
        const TraceConfig &config = global_it->second;
        if (!config.trace_on)
            return false; // 全局关闭

        switch (node_type) {
        case ch::core::lnodetype::type_reg:
            return config.trace_reg;
        case ch::core::lnodetype::type_input:
            return config.trace_input;
        case ch::core::lnodetype::type_output:
            return config.trace_output;
        default:
            return config.trace_on; // 根据配置决定是否跟踪其他类型
        }
    }

    // 检查模块级别配置
    // 假设节点名称包含模块路径信息，例如 "top.counter.signal_name"
    size_t dot_pos = node_name.find('.');
    std::string module_name =
        (dot_pos != std::string::npos) ? node_name.substr(0, dot_pos) : "top";

    // 优先检查完整路径（如果适用）
    auto path_it = trace_configs_.find(node_name);
    if (path_it != trace_configs_.end()) {
        const TraceConfig &config = path_it->second;
        if (!config.trace_on)
            return false;

        switch (node_type) {
        case ch::core::lnodetype::type_reg:
            return config.trace_reg;
        case ch::core::lnodetype::type_input:
            return config.trace_input;
        case ch::core::lnodetype::type_output:
            return config.trace_output;
        default:
            return config.trace_on;
        }
    }

    // 然后检查模块名配置
    auto it = trace_configs_.find(module_name);
    if (it != trace_configs_.end()) {
        const TraceConfig &config = it->second;
        if (!config.trace_on)
            return false; // 模块级关闭

        switch (node_type) {
        case ch::core::lnodetype::type_reg:
            return config.trace_reg;
        case ch::core::lnodetype::type_input:
            return config.trace_input;
        case ch::core::lnodetype::type_output:
            return config.trace_output;
        default:
            return config.trace_on; // 根据配置决定是否跟踪其他类型
        }
    }

    // 检查顶层模块配置
    auto top_it = trace_configs_.find("top");
    if (top_it != trace_configs_.end()) {
        const TraceConfig &config = top_it->second;
        if (!config.trace_on)
            return false; // 顶层关闭

        switch (node_type) {
        case ch::core::lnodetype::type_reg:
            return config.trace_reg;
        case ch::core::lnodetype::type_input:
            return config.trace_input;
        case ch::core::lnodetype::type_output:
            return config.trace_output;
        default:
            return config.trace_on; // 根据配置决定是否跟踪其他类型
        }
    }

    // 如果没有任何配置，返回默认值（跟踪）
    return true;
}

Simulator::~Simulator() {
    // 释放跟踪数据块
    for (auto *block : trace_blocks_) {
        delete block;
    }
    trace_blocks_.clear();

    // ch::pre_static_destruction_cleanup();
    // ch::detail::set_static_destruction();

    // Explicitly disconnect to prevent accessing destroyed context
    disconnect();

    ctx_curr_ = ctx_curr_backup_;
}

void Simulator::disconnect() {
    CHDBG_FUNC();

    // Check if we're in static destruction phase
    // if (ch::detail::in_static_destruction()) {
    //     // During static destruction, minimize operations to prevent
    //     segfaults
    //     // Just mark as disconnected and return immediately
    //     disconnected_ = true;
    //     return;
    // }

    if (disconnected_) {
        return;
    }

    // Mark as disconnected first to prevent any further operations
    disconnected_ = true;

    // Clear all data that references the context
    instr_map_.clear();
    instr_cache_.clear();
    data_map_.clear();
    eval_list_.clear();

    // Clear the context pointer
    if (!ctx_)
        delete ctx_;
    ctx_ = nullptr;

    // Mark as uninitialized
    initialized_ = false;
}

void Simulator::reinitialize() {
    initialized_ = false;
    initialize();

    // 重新初始化跟踪
    if (trace_on_) {
        collect_signals();
    }
}

void Simulator::collect_signals() {
    CHDBG_FUNC();
    signals_.clear();
    traced_nodes_.clear();

    // 1. 添加系统时钟和复位信号（总是跟踪）
    if (ctx_->has_default_clock()) {
        auto *clock_node = ctx_->get_default_clock();
        if (should_trace_node(clock_node->id(), clock_node->name(),
                              clock_node->type())) {
            signals_.push_back(
                std::make_pair(clock_node->id(), clock_node->name()));
            traced_nodes_.insert(clock_node->id());
            CHDBG("Added clock signal for tracing: %s (ID: %u)", 
                  clock_node->name(), clock_node->id());
        }
    }

    if (ctx_->has_default_reset()) {
        auto *reset_node = ctx_->get_default_reset();
        if (should_trace_node(reset_node->id(), reset_node->name(),
                              ch::core::lnodetype::type_reset)) {
            signals_.push_back(
                std::make_pair(reset_node->id(), reset_node->name()));
            traced_nodes_.insert(reset_node->id());
            CHDBG("Added reset signal for tracing: %s (ID: %u)", 
                  reset_node->name(), reset_node->id());
        }
    }

    // 2. 添加所有符合条件的信号
    for (auto *node : eval_list_) {
        if (should_trace_node(node->id(), node->name(), node->type())) {
            signals_.push_back(std::make_pair(node->id(), node->name()));
            traced_nodes_.insert(node->id());
            CHDBG("Added signal for tracing: %s (ID: %u, Type: %s)", 
                  node->name(), node->id(), ch::core::to_string(node->type()));
        }
    }

    CHINFO("Collected %zu signals for tracing", signals_.size());

    // 计算总跟踪宽度
    trace_width_ = 0;
    for (const auto &signal_pair : signals_) {
        uint32_t node_id = signal_pair.first;
        auto it = data_map_.find(node_id);
        if (it != data_map_.end()) {
            trace_width_ += it->second.bitwidth();
        }
    }

    // 初始化跟踪相关数据结构
    prev_values_.resize(signals_.size());
    for (auto &prev : prev_values_) {
        prev.first = nullptr;
        prev.second = 0;
    }

    valid_mask_ = ch::core::sdata_type(0, signals_.size());
}

void Simulator::trace() {
    if (!trace_on_)
        return;

    CHDBG_FUNC();

    // 分配新的跟踪块（如果当前块已满）
    const size_t NUM_TRACES = 1024; // 每块存储的跟踪数量
    auto block_width = NUM_TRACES * trace_width_;
    if (nullptr == current_trace_block_ ||
        (current_trace_block_->size + trace_width_) > block_width) {
        allocate_new_trace_block();
    }

    // 记录跟踪数据
    valid_mask_.reset(); // 重置有效位掩码
    auto dst_block = current_trace_block_->data;
    auto dst_offset = current_trace_block_->size;
    dst_offset += (valid_mask_.bitwidth() + 31) / 32 *
                  4; // 跳过有效位掩码的空间（按4字节对齐）

    // 遍历所有信号
    for (uint32_t i = 0, n = signals_.size(); i < n; ++i) {
        auto node_id = signals_[i].first;

        auto it = data_map_.find(node_id);
        if (it == data_map_.end()) {
            continue; // 节点值不存在，跳过
        }

        auto &curr_value = it->second;
        auto &prev = prev_values_.at(i);

        // 如果之前有值，比较是否发生变化
        if (prev.first) {
            // 比较当前值与之前保存的值
            // 创建一个临时的sdata_type对象来进行比较
            auto temp_data = ch::core::sdata_type(
                curr_value.bitwidth()); // 创建指定宽度的临时对象

            // 将之前保存的数据复制到临时对象的bitvector中
            std::memcpy(
                temp_data.bitvector().data(),
                reinterpret_cast<
                    const ch::internal::bitvector<uint64_t>::block_type *>(
                    prev.first),
                (curr_value.bitwidth() + 7) / 8); // 根据位宽计算字节数

            if (curr_value == temp_data) {
                continue; // 值没有变化，跳过记录
            }
        }

        // 保存当前值的位置信息
        prev.first = dst_block;
        prev.second = dst_offset;

        // 复制信号值到跟踪缓冲区
        // 使用正确的bitvector方法
        std::memcpy(dst_block + dst_offset, curr_value.bitvector().data(),
                    (curr_value.bitwidth() + 7) / 8); // 根据位宽计算字节数
        dst_offset += (curr_value.bitwidth() + 7) / 8; // 按字节对齐

        // 使用[]操作符设置valid_mask_的位
        valid_mask_.bitvector()[i] = true; // 标记该信号在此次仿真中有变化
    }

    // 将有效位掩码写入跟踪数据块
    std::memcpy(dst_block + current_trace_block_->size,
                valid_mask_.bitvector().data(),
                (valid_mask_.bitwidth() + 31) / 32 * 4); // 按4字节对齐

    // 更新跟踪块大小
    current_trace_block_->size = dst_offset;
}

void Simulator::allocate_new_trace_block() {
    // 释放旧的当前块（如果不是在blocks列表中）
    if (current_trace_block_) {
        // 如果当前块不是最后一个块，说明需要新开辟
        if (trace_blocks_.empty() ||
            trace_blocks_.back() != current_trace_block_) {
            delete current_trace_block_;
        }
    }

    current_trace_block_ = new TraceBlock(current_trace_block_size_);
    trace_blocks_.push_back(current_trace_block_);
    current_trace_block_used_ = 0;
}

void Simulator::write_to_trace_block(const std::string &data) {
    if (!current_trace_block_) {
        allocate_new_trace_block();
    }

    size_t data_size = data.size();
    if (current_trace_block_->size + data_size >
        current_trace_block_->capacity) {
        allocate_new_trace_block();
    }

    if (current_trace_block_ && current_trace_block_->size + data_size <=
        current_trace_block_->capacity) {
        memcpy(current_trace_block_->data + current_trace_block_->size,
               data.data(), data_size);
        current_trace_block_->size += data_size;
    }
}

void Simulator::initialize() {
    CHDBG_FUNC();

    // 检查context是否有效
    if (initialized_ || disconnected_ || !ctx_) {
        update_instruction_pointers();
        return;
    }

    eval_list_ = ctx_->get_eval_list();
    CHDBG("Retrieved eval_list with %zu nodes", eval_list_.size());

    instr_map_.clear();
    instr_cache_.clear();
    data_map_.clear();

    // 清空所有分类指令列表
    default_clock_instr_ = nullptr;
    other_clock_instr_list_.clear();
    input_instr_list_.clear();
    sequential_instr_list_.clear();
    combinational_instr_list_.clear();

    // 分配数据缓冲区
    for (auto *node : eval_list_) {
        if (disconnected_)
            return;

        CHCHECK_NULL(node, "Null node found in evaluation list, skipping");

        if (!node)
            continue;

        uint32_t node_id = node->id();
        uint32_t size = node->size();
        CHDBG("Processing node ID %u with size %u", node_id, size);

        if (node->type() == ch::core::lnodetype::type_lit) {
            auto *lit_node = static_cast<ch::core::litimpl *>(node);
            data_map_[node_id] = lit_node->value();
            CHDBG("Set literal value(%d) for node %u", lit_node->value(),
                  node_id, data_map_[node_id].to_string_verbose().c_str());
        } else {
            data_map_[node_id] = ch::core::sdata_type(0, size);
            CHDBG("Allocated buffer for node %u, width %u, value %s", node_id,
                  size, data_map_[node_id].to_string_verbose().c_str());

            // 如果这是默认时钟节点，则保存其数据指针
            if (ctx_->has_default_clock() &&
                node_id == ctx_->get_default_clock()->id()) {
                default_clock_data_ = &data_map_[node_id];
                *default_clock_data_ = ch::core::sdata_type(0, 1);
                CHDBG("Saved default clock data pointer for node %u", node_id);
            }
        }
    }

    // 第一步：创建所有内存实体指令对象
    for (auto *node : eval_list_) {
        if (!node || disconnected_)
            continue;

        uint32_t node_id = node->id();

        auto instr = node->create_instruction(data_map_);

        if (instr.get()) {
            // 设置指令ID
            instr_map_[node_id] = instr.get();
            instr_cache_[node_id] = std::move(instr);
            CHDBG("Created instruction for node %u", node_id);

            // 根据节点类型分类
            switch (node->type()) {
            case ch::core::lnodetype::type_input:
                if (!node->get_parent())
                    input_instr_list_.emplace_back(node_id,
                                                   instr_map_[node_id]);
                else
                    combinational_instr_list_.emplace_back(node_id,
                                                           instr_map_[node_id]);
                break;
            case ch::core::lnodetype::type_reset:
                reset_instr_list_.emplace_back(node_id, instr_map_[node_id]);
                break;
            case ch::core::lnodetype::type_clock:
                // 区分默认时钟和其他输入节点
                if (ctx_->has_default_clock() &&
                    node_id == ctx_->get_default_clock()->id()) {
                    default_clock_instr_ = instr_map_[node_id];
                } else {
                    other_clock_instr_list_.emplace_back(node_id,
                                                         instr_map_[node_id]);
                }
                break;

            case ch::core::lnodetype::type_reg:
            case ch::core::lnodetype::type_mem_read_port:
            case ch::core::lnodetype::type_mem_write_port:
                sequential_instr_list_.emplace_back(node_id,
                                                    instr_map_[node_id]);
                break;
            default:
                // 其他所有节点归类为组合逻辑节点
                combinational_instr_list_.emplace_back(node_id,
                                                       instr_map_[node_id]);
                break;
            }

        } else {
            CHDBG("No instruction created for node %u (type: %s)", node_id,
                  ch::core::to_string(node->type()));
        }
    }

    if (sequential_instr_list_.empty()) {
        default_clock_instr_ = nullptr;
    }

    // 第二步：初始化内存端口指令对象，传递内存指令对象指针
    for (auto *node : eval_list_) {
        if (!node || disconnected_)
            continue;

        // 处理内存端口节点
        if (node->type() == ch::core::lnodetype::type_mem_read_port ||
            node->type() == ch::core::lnodetype::type_mem_write_port) {

            uint32_t node_id = node->id();
            auto it = instr_map_.find(node_id);
            if (it != instr_map_.end()) {
                // 获取父内存节点ID
                if (node->type() == ch::core::lnodetype::type_mem_read_port) {
                    auto *port_node =
                        static_cast<ch::core::mem_read_port_impl *>(node);
                    uint32_t parent_mem_id = port_node->parent()->id();

                    // 查找对应的内存指令对象
                    auto mem_it = instr_map_.find(parent_mem_id);
                    if (mem_it == instr_map_.end()) {
                        CHERROR(
                            "Simulator find a memport without mem instruction");
                        return;
                    }
                    // 初始化端口指令对象
                    if (auto *async_read_port =
                            dynamic_cast<instr_mem_async_read_port *>(
                                it->second)) {
                        async_read_port->set_mem_ptr(
                            reinterpret_cast<ch::instr_mem *>(mem_it->second));
                    } else if (auto *sync_read_port =
                                   dynamic_cast<instr_mem_sync_read_port *>(
                                       it->second)) {
                        sync_read_port->set_mem_ptr(
                            reinterpret_cast<ch::instr_mem *>(mem_it->second));
                    } else {
                        CHERROR("Simulator find a memport with invalid type ");
                        return;
                    }
                } else if (node->type() ==
                           ch::core::lnodetype::type_mem_write_port) {
                    auto *port_node =
                        static_cast<ch::core::mem_write_port_impl *>(node);
                    uint32_t parent_mem_id = port_node->parent()->id();

                    // 查找对应的内存指令对象
                    auto mem_it = instr_map_.find(parent_mem_id);
                    if (mem_it == instr_map_.end()) {
                        CHERROR("Simulator not initialized or disconnected");
                        return;
                    }
                    // 初始化端口指令对象
                    if (auto *write_port =
                            dynamic_cast<instr_mem_write_port *>(it->second)) {
                        write_port->set_mem_ptr(
                            reinterpret_cast<ch::instr_mem *>(mem_it->second));
                    } else {
                        CHERROR("Simulator find a memport with invalid type ");
                        return;
                    }
                }
            }
        }
    }

    initialized_ = true;
    CHINFO("Simulator initialization completed successfully");
}

void Simulator::update_instruction_pointers() {
    CHDBG_FUNC();

    if (disconnected_)
        return;

    instr_map_.clear();
    for (const auto &pair : instr_cache_) {
        if (disconnected_)
            return;
        instr_map_[pair.first] = pair.second.get();
    }
    CHDBG("Updated %zu instruction pointers", instr_map_.size());
}

void Simulator::eval_sequential() {
    CHDBG_FUNC();

    for (const auto &[node_id, instr] : sequential_instr_list_) {
        instr->eval();
        CHDBG("Evaluating sequential instruction for node %u, %s", node_id,
              data_map_[node_id].to_string_verbose().c_str());
    }

    CHDBG("Evaluation sequential completed");
}

void Simulator::eval_combinational() {
    CHDBG_FUNC();
    // 执行输入节点指令
    for (const auto &[node_id, instr] : input_instr_list_) {
        instr->eval();
        CHDBG("Evaluating input instruction for node %u: %s", node_id,
              data_map_[node_id].to_string_verbose().c_str());
    }

    // 执行组合逻辑节点
    for (const auto &[node_id, instr] : combinational_instr_list_) {
        instr->eval();
        CHDBG("Evaluating combinational instruction for node %u: %s", node_id,
              data_map_[node_id].to_string_verbose().c_str());
    }

    CHDBG("Evaluation combinational completed");
}

void Simulator::tick() {
    CHDBG_FUNC();

    // 检查context是否有效
    if (!initialized_ || disconnected_ || !ctx_) {
        CHERROR("Simulator not initialized or disconnected");
        return;
    }

    CHINFO("ticks count: %u", ticks_++);

    eval_combinational();

    if (default_clock_instr_) {
        eval();
        eval();
    }
}

void Simulator::eval() {
    CHDBG_FUNC();

    // 1. 首先执行默认时钟指令以更新时钟边沿状态
    CHDBG("Evaluating default clock instruction");
    default_clock_instr_->eval();

    // 2. 然后执行其他时钟指令
    for (const auto &[node_id, instr] : other_clock_instr_list_) {
        CHDBG("Evaluating other clock instruction for node %u", node_id);
        instr->eval();
    }

    // 对于包含时序逻辑的电路，按照特定顺序执行各类节点
    // 如果时钟为0，执行组合逻辑；如果时钟为1，执行时序逻辑
    if (default_clock_data_->is_zero()) {
        eval_combinational();
    } else {
        eval_sequential();
    }

    // 如果启用了跟踪，则记录信号值
    if (trace_on_) {
        trace();
    }

    CHDBG("Executed post-clock eval based on clock state %llu",
          static_cast<unsigned long long>(
              static_cast<uint64_t>(*default_clock_data_)));
}

void Simulator::tick(size_t count) {
    CHDBG_FUNC();
    CHDBG_VAR(count);

    if (count == 0) {
        return;
    }

    for (size_t i = 0; i < count; ++i) {
        if (disconnected_)
            return;
        CHDBG("Tick %zu/%zu", i + 1, count);
        tick();
    }
}

void Simulator::reset() {
    CHDBG_FUNC();

    // 检查context是否有效
    if (!initialized_ || disconnected_ || !ctx_) {
        CHERROR("Simulator not initialized or disconnected");
        return;
    }

    for (auto &pair : data_map_) {
        if (disconnected_)
            return;
        uint32_t node_id = pair.first;
        auto *node = ctx_->get_node_by_id(node_id);
        if (node && node->type() != ch::core::lnodetype::type_lit) {
            pair.second = ch::core::sdata_type(0, pair.second.bitwidth());
        }
    }
    CHDBG("Simulator state reset completed");
}

const ch::core::sdata_type &
Simulator::get_value_by_name(const std::string &name) const {
    CHDBG_FUNC();
    CHDBG("Looking for node with name: %s", name.c_str());

    // 检查context是否有效
    if (!initialized_ || disconnected_ || !ctx_) {
        CHERROR("Simulator not initialized or disconnected");
        // Return a safe empty value instead of crashing
        static ch::core::sdata_type empty_value(0, 1);
        return empty_value;
    }

    if (name.empty()) {
        CHERROR("Node name cannot be empty");
        static ch::core::sdata_type empty_value(0, 1);
        return empty_value;
    }

    // 查找节点
    for (const auto &node_ptr : ctx_->get_nodes()) {
        if (!node_ptr || disconnected_)
            continue;
        if (node_ptr && node_ptr->name() == name) {
            uint32_t node_id = node_ptr->id();
            auto it = data_map_.find(node_id);
            if (it != data_map_.end()) {
                return it->second;
            }
            CHWARN("Data not found for node '%s' with ID %u", name.c_str(),
                   node_id);
            static ch::core::sdata_type empty_value(0, 1);
            return empty_value;
        }
    }

    CHWARN("Node with name '%s' not found", name.c_str());
    static ch::core::sdata_type empty_value(0, 1);
    return empty_value;
}

void Simulator::toVCD(const std::string &filename) const {
    // 预留VCD输出功能的实现
    CHINFO("VCD output to file: %s", filename.c_str());

    if (!trace_on_) {
        CHWARN("Tracing is not enabled, nothing to export to VCD");
        return;
    }

    if (trace_blocks_.empty()) {
        CHWARN("No trace data available to export to VCD");
        return;
    }

    std::ofstream out(filename);
    if (!out.is_open()) {
        CHERROR("Failed to open file for VCD output: %s", filename.c_str());
        return;
    }

    // 输出VCD头部信息
    out << "$timescale 1 ns $end\n";
    out << "$scope module top $end\n";

    // 输出信号定义，使用更合适的信号标识符
    for (size_t i = 0; i < signals_.size(); ++i) {
        const auto& signal_info = signals_[i];
        auto signal_id = signal_info.first;
        auto signal_name = signal_info.second;
        
        // 确保信号名符合VCD规范，移除可能引起冲突的字符
        std::string sanitized_name = signal_name;
        for (char& c : sanitized_name) {
            if (c == ' ' || c == '.') c = '_';
        }
        
        auto signal_width = data_map_.at(signal_id).bitwidth();
        if (signal_width == 1) {
            out << "$var wire 1 " << static_cast<char>('!'+i) << " " 
                << sanitized_name << " $end\n";
        } else {
            out << "$var wire " << signal_width << " " 
                << static_cast<char>('!' + i) << " " << sanitized_name 
                << " $end\n";
        }
    }

    out << "$upscope $end\n";
    out << "$enddefinitions $end\n";

    // 输出跟踪数据
    uint64_t t = 0;  // 使用更大的类型避免溢出
    auto mask_width = signals_.size();

    // 遍历所有跟踪块
    for (const auto *trace_block : trace_blocks_) {
        auto src_block = trace_block->data;
        auto src_width = trace_block->size;
        uint32_t src_offset = 0;

        // 每个块包含多个时间点的数据
        while (src_offset < src_width) {
            // 读取有效位掩码（按4字节对齐）
            uint32_t mask_size_bytes = (mask_width + 31) / 32 * 4;
            const char *mask_ptr = src_block + src_offset;
            src_offset += mask_size_bytes;

            bool has_changes = false;
            // 遍历所有信号
            for (uint32_t i = 0, n = signals_.size(); i < n; ++i) {
                // 检查该信号在此时间点是否有变化
                uint32_t word_idx = i / 32;
                uint32_t bit_idx = i % 32;

                if (word_idx < (mask_size_bytes / sizeof(uint32_t))) {
                    // 读取掩码字
                    const uint32_t *mask_words = reinterpret_cast<const uint32_t*>(mask_ptr);
                    bool valid = (mask_words[word_idx] >> bit_idx) & 1;

                    if (valid) {
                        if (!has_changes) {
                            out << '#' << t << '\n'; // 输出时间戳
                            has_changes = true;
                        }

                        auto signal_id = signals_[i].first;
                        auto signal_width = data_map_.at(signal_id).bitwidth();

                        // 读取信号值（按字节对齐）
                        uint32_t signal_bytes = (signal_width + 7) / 8;
                        const char *signal_ptr = src_block + src_offset;
                        src_offset += signal_bytes;

                        // 构建信号值
                        ch::core::sdata_type value(0, signal_width);
                        std::memcpy(
                            const_cast<ch::internal::bitvector<uint64_t>&>(
                                value.bitvector()).data(),
                            signal_ptr,
                            std::min(signal_bytes, 
                                     static_cast<uint32_t>(value.bitvector().size() * sizeof(uint64_t))));

                        // 输出信号值
                        out << 'b';
                        // 输出二进制表示，从高位到低位
                        for (int j = signal_width - 1; j >= 0; --j) {
                            out << (value.bitvector()[static_cast<uint32_t>(j)] ? '1' : '0');
                        }
                        out << ' ' << static_cast<char>('!' + i) << '\n'; // 输出信号ID
                    }
                }
            }
            
            t++; // 时间递增
        }
    }

    out.close();
    CHINFO("VCD file written successfully: %s", filename.c_str());
}
} // namespace ch