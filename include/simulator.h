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

    // 统一的端口值获取接口 - 支持所有端口类型
    template <typename T, typename Dir>
    const ch::core::sdata_type get_port_value(const ch::core::port<T, Dir>& port) const {
        CHDBG_FUNC();
        
        if (!initialized_) {
            CHABORT("Simulator not initialized when getting port value");
        }

        auto* node = port.impl();
        CHDBG("Port impl pointer: %p", static_cast<void*>(node));
        
        if (!node) {
            CHABORT("Port implementation is null - port may not be properly initialized");
            return ch::core::constants::zero(ch::core::ch_width_v<T>);
        }

        uint32_t node_id = node->id();
        CHDBG("Getting value for node ID: %u", node_id);
        
        auto it = data_map_.find(node_id);
        if (it != data_map_.end()) {
            CHDBG("Found value for node ID: %u", node_id);
            return it->second;
        }
        
        CHWARN("Value not found for port node ID: %u", node_id);
        return ch::core::constants::zero(ch::core::ch_width_v<T>);
    }

    // 统一的端口值设置接口
    template <typename T, typename Dir>
    void set_port_value(const ch::core::port<T, Dir>& port, uint64_t value) {
        CHDBG_FUNC();
        
        if (!initialized_) {
            CHERROR("Simulator not initialized when setting port value");
            return;
        }
        
        // 只允许设置输入端口
        static_assert(ch::core::is_input_v<Dir>, "Can only set value for input ports!");
        
        auto* node = port.impl();
        if (!node) {
            CHERROR("Port implementation is null");
            return;
        }
        
        uint32_t node_id = node->id();
        auto it = data_map_.find(node_id);
        if (it != data_map_.end()) {
            try {
                it->second = ch::core::sdata_type(value, it->second.bitwidth());
                CHDBG("Set port value for node %u to %llu", node_id, (unsigned long long)value);
            } catch (const std::exception& e) {
                CHERROR("Failed to set port value: %s", e.what());
            }
        } else {
            CHERROR("Port node ID not found: %u", node_id);
        }
    }

    // 保持兼容性的旧接口
    template <typename T>
    const ch::core::sdata_type get_value(const ch::core::ch_out<T>& port) const {
        return get_port_value(port);
    }

    template <typename T>
    void set_input_value(const ch::core::ch_in<T>& port, uint64_t value) {
        set_port_value(port, value);
    }    

    const ch::core::sdata_type& get_value_by_name(const std::string& name) const ;

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
