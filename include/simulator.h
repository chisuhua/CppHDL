
// include/simulator.h
#ifndef SIMULATOR_H
#define SIMULATOR_H

#include "core/context.h"
#include "core/types.h"
#include "core/io.h"
#include "core/reg.h"
#include "ast/instr_base.h"
#include <unordered_map>
#include <vector>
#include <iostream>
#include <memory>
#include <cstdint>

namespace ch {

class Simulator {
public:
    explicit Simulator(ch::core::context* ctx);
    ~Simulator();

    // 禁止拷贝，允许移动
    Simulator(const Simulator&) = delete;
    Simulator& operator=(const Simulator&) = delete;
    Simulator(Simulator&&) = default;
    Simulator& operator=(Simulator&&) = default;

    // 仿真控制方法
    void tick();
    void eval();
    
    // 批量仿真方法
    void tick(size_t count);
    
    // 重置仿真状态
    void reset();
    
    // 获取输出值的方法
    template <typename T>
    uint64_t get_value(const ch::core::ch_logic_out<T>& port) const {
        std::cout << "[Simulator::get_value] Called for port impl " << port.impl() << std::endl;
        auto* output_node = port.impl();
        if (!output_node) {
            std::cout << "[Simulator::get_value] Port impl is null." << std::endl;
            return 0;
        }
        auto* proxy = output_node->src(0);
        if (!proxy) return 0;
        std::cout << "[Simulator::get_value] Looking up proxy ID " << proxy->id() << " in data_map." << std::endl;
        auto it = data_map_.find(proxy->id());
        if (it != data_map_.end()) {
            std::cout << "[Simulator::get_value] Found value in data_map for proxy ID " << proxy->id() << std::endl;
            const auto& bv = it->second.bv_;
            if (bv.num_words() > 0) {
                return bv.words()[0];
            }
        }
        std::cout << "[Simulator::get_value] Proxy ID " << proxy->id() << " not found in data_map." << std::endl;
        return 0;
    }

    // 通过名称获取值
    uint64_t get_value_by_name(const std::string& name) const;
    
    // 设置输入值
    template <typename T>
    void set_input_value(const ch::core::ch_logic_in<T>& port, uint64_t value) {
        auto* input_node = port.impl();
        if (!input_node) return;
        
        uint32_t node_id = input_node->id();
        auto it = data_map_.find(node_id);
        if (it != data_map_.end()) {
            // 更新输入节点的值
            it->second = ch::core::sdata_type(value, it->second.bitwidth());
        }
    }

    // 获取上下文
    ch::core::context* context() const { return ctx_; }
    
    // 获取数据映射（只读）
    const ch::data_map_t& data_map() const { return data_map_; }

private:
    void initialize();
    void update_instruction_pointers(); // 更新指令使用的缓冲区指针
    
    // 评估指定范围的节点（用于并行化扩展）
    void eval_range(size_t start, size_t end);

    ch::core::context* ctx_;
    std::vector<ch::core::lnodeimpl*> eval_list_;
    
    // 指令映射 - 使用原始指针（由 instr_cache_ 管理生命周期）
    std::unordered_map<uint32_t, ch::instr_base*> instr_map_;
    
    // 指令缓存 - 管理指令的生命周期
    std::unordered_map<uint32_t, std::unique_ptr<ch::instr_base>> instr_cache_;
    
    // 数据映射
    ch::data_map_t data_map_;
    
    // 状态标志
    bool initialized_ = false;
};

} // namespace ch

#endif // SIMULATOR_H
