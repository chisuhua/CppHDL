// include/simulator.h
#ifndef SIMULATOR_H
#define SIMULATOR_H

#include "core/context.h"
#include "core/types.h"
#include "core/io.h"
#include "core/reg.h"
#include "ast/instr_base.h"
#include "logger.h"
#include <unordered_map>
#include <vector>
#include <memory>
#include <cstdint>

namespace ch {

class Simulator {
public:
    explicit Simulator(ch::core::context* ctx);
    ~Simulator();

    Simulator(const Simulator&) = delete;
    Simulator& operator=(const Simulator&) = delete;
    Simulator(Simulator&&) = default;
    Simulator& operator=(Simulator&&) = default;

    void tick();
    void eval();
    void tick(size_t count);
    void reset();
    
    template <typename T>
    const ch::core::sdata_type get_value(const ch::core::ch_logic_out<T>& port) const {
        CHDBG_FUNC();
        
        if (!initialized_) {
            CHABORT("Simulator not initialized when getting value");
            //return 0;
        }
        
        auto* output_node = port.impl();
        if (!output_node) {
            CHABORT("Port implementation is null");
            //return 0;
        }

        // 直接从 output 节点获取值
        uint32_t node_id = output_node->id();
        auto it = data_map_.find(node_id);
        if (it != data_map_.end()) {
            return it->second;
        }
        
        CHWARN("Value not found for output node ID: %u", node_id);
        return ch::core::constants::zero(ch::core::ch_width_v<T>);
    }

    const ch::core::sdata_type& get_value_by_name(const std::string& name) const ;
    
    template <typename T>
    void set_input_value(const ch::core::ch_logic_in<T>& port, uint64_t value) {
        CHDBG_FUNC();
        
        if (!initialized_) {
            CHERROR("Simulator not initialized when setting input value");
            return;
        }
        
        auto* input_node = port.impl();
        if (!input_node) {
            CHERROR("Input port implementation is null");
            return;
        }
        
        uint32_t node_id = input_node->id();
        auto it = data_map_.find(node_id);
        if (it != data_map_.end()) {
            try {
                it->second = ch::core::sdata_type(value, it->second.bitwidth());
                CHDBG("Set input value for node %u to %llu", node_id, (unsigned long long)value);
            } catch (const std::exception& e) {
                CHERROR("Failed to set input value: %s", e.what());
            }
        } else {
            CHERROR("Input node ID not found: %u", node_id);
        }
    }

    ch::core::context* context() const { return ctx_; }
    const ch::data_map_t& data_map() const { return data_map_; }

private:
    void initialize();
    void update_instruction_pointers();
    void eval_range(size_t start, size_t end);

    ch::core::context* ctx_;
    std::vector<ch::core::lnodeimpl*> eval_list_;
    std::unordered_map<uint32_t, ch::instr_base*> instr_map_;
    std::unordered_map<uint32_t, std::unique_ptr<ch::instr_base>> instr_cache_;
    ch::data_map_t data_map_;
    bool initialized_ = false;
};

} // namespace ch

#endif // SIMULATOR_H
