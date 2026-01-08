// include/simulator.h
#ifndef SIMULATOR_H
#define SIMULATOR_H

#include "ast/instr_base.h"
#include "core/bool.h" // 添加对 ch_bool 的支持
#include "core/bundle/bundle_base.h"
#include "core/context.h"
#include "core/io.h"
#include "core/reg.h"
#include "core/types.h"
#include "logger.h"
#include "utils/destruction_manager.h"

// 添加inipp头文件
#include "inipp/inipp.h"

#include <bitset>
#include <cstdint>
#include <deque>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

namespace ch {

// 跟踪配置结构
struct TraceConfig {
    bool trace_on = false;
    bool trace_reg = false;
    bool trace_wire = false;
    bool trace_input = false;
    bool trace_output = false;

    TraceConfig() = default;
    TraceConfig(bool on, bool reg, bool wire, bool input, bool output)
        : trace_on(on), trace_reg(reg), trace_wire(wire), trace_input(input),
          trace_output(output) {}
};

// 跟踪数据块结构
struct TraceBlock {
    char *data;
    size_t size;
    size_t capacity;

    TraceBlock(size_t cap) : size(0), capacity(cap) {
        data = new char[capacity];
    }

    ~TraceBlock() { delete[] data; }
};

class Simulator {
public:
    explicit Simulator(ch::core::context *ctx, bool trace_on = false);
    // 新增构造函数，支持从配置文件加载
    explicit Simulator(ch::core::context *ctx, const std::string &config_file);
    ~Simulator();

    Simulator(const Simulator &) = delete;
    Simulator &operator=(const Simulator &) = delete;
    Simulator(Simulator &&) = default;
    Simulator &operator=(Simulator &&) = default;

    void tick();
    void eval();
    void eval_sequential();
    void eval_combinational();
    void tick(size_t count);
    void reset();

    // 统一的端口值获取接口 - 支持所有端口类型
    template <typename T, typename Dir>
    const ch::core::sdata_type
    get_port_value(const ch::core::port<T, Dir> &port) const {
        CHDBG_FUNC();

        if (!initialized_) {
            CHABORT("Simulator not initialized when getting port value");
        }

        auto *node = port.impl();
        CHDBG("Port impl pointer: %p", static_cast<void *>(node));

        if (!node) {
            CHABORT("Port implementation is null - port may not be properly "
                    "initialized");
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

    // 信号值获取接口 - 支持ch_uint<N>类型
    template <unsigned N>
    const ch::core::sdata_type
    get_signal_value(const ch::core::ch_uint<N> &signal) const {
        CHDBG_FUNC();

        if (!initialized_) {
            CHABORT("Simulator not initialized when getting signal value");
        }

        auto *node = signal.impl();
        CHDBG("Signal impl pointer: %p", static_cast<void *>(node));

        if (!node) {
            CHABORT(
                "Signal implementation is null - signal may not be properly "
                "initialized");
            return ch::core::constants::zero(N);
        }

        uint32_t node_id = node->id();
        CHDBG("Getting value for node ID: %u", node_id);

        auto it = data_map_.find(node_id);
        if (it != data_map_.end()) {
            CHDBG("Found value for node ID: %u", node_id);
            return it->second;
        }

        CHWARN("Value not found for signal node ID: %u", node_id);
        return ch::core::constants::zero(N);
    }

    // 统一的端口值设置接口
    template <typename T, typename Dir>
    void set_port_value(const ch::core::port<T, Dir> &port, uint64_t value) {
        CHDBG_FUNC();

        if (!initialized_) {
            CHERROR("Simulator not initialized when setting port value");
            return;
        }

        // 只允许设置输入端口
        static_assert(ch::core::is_input_v<Dir>,
                      "Can only set value for input ports!");

        auto *node = port.impl();
        if (!node) {
            CHERROR("Port implementation is null");
            return;
        }

        uint32_t node_id = node->id();
        auto it = data_map_.find(node_id);
        if (it != data_map_.end()) {
            try {
                it->second = ch::core::sdata_type(value, it->second.bitwidth());
                CHDBG("Set port value for node %u to %llu", node_id,
                      (unsigned long long)value);
            } catch (const std::exception &e) {
                CHERROR("Failed to set port value: %s", e.what());
            }
        } else {
            CHERROR("Port node ID not found: %u", node_id);
        }
    }

    // 为Bundle类型添加特殊支持
    template <typename BundleT>
        requires std::is_base_of_v<ch::core::bundle_base<BundleT>, BundleT>
    void set_bundle_value(BundleT &bundle, uint64_t value) {
        CHDBG_FUNC();

        if (!initialized_) {
            CHERROR("Simulator not initialized when setting bundle value");
            return;
        }

        // 获取Bundle的总宽度
        constexpr unsigned width = ch::core::get_bundle_width<BundleT>();

        // 遍历Bundle的所有字段并设置值
        const auto &fields = bundle.__bundle_fields();
        unsigned offset = 0;

        std::apply(
            [&](auto &&...f) {
                (((set_bundle_field_value(
                      bundle.*(f.ptr), value, offset,
                      ch::core::ch_width_v<
                          std::decay_t<decltype(bundle.*(f.ptr))>>)),
                  offset += ch::core::ch_width_v<
                      std::decay_t<decltype(bundle.*(f.ptr))>>),
                 ...);
            },
            fields);
    }

    template <typename BundleT>
        requires std::is_base_of_v<ch::core::bundle_base<BundleT>, BundleT>
    uint64_t get_bundle_value(const BundleT &bundle) const {
        CHDBG_FUNC();

        if (!initialized_) {
            CHABORT("Simulator not initialized when getting bundle value");
        }

        // 序列化Bundle字段为一个整数值
        uint64_t result = 0;
        const auto &fields = bundle.__bundle_fields();
        unsigned offset = 0;

        std::apply(
            [&](auto &&...f) {
                (((result |=
                   (get_bundle_field_value(bundle.*(f.ptr)) << offset)),
                  offset += ch::core::ch_width_v<
                      std::decay_t<decltype(bundle.*(f.ptr))>>),
                 ...);
            },
            fields);

        return result;
    }

    // 保持兼容性的旧接口
    template <typename T>
    const ch::core::sdata_type
    get_value(const ch::core::ch_out<T> &port) const {
        return get_port_value(port);
    }

    // 添加对ch_uint<N>类型的支持
    template <unsigned N>
    const ch::core::sdata_type
    get_value(const ch::core::ch_uint<N> &signal) const {
        return get_signal_value(signal);
    }

    // 添加对ch_bool类型的支持
    const ch::core::sdata_type
    get_value(const ch::core::ch_bool &signal) const {
        CHDBG_FUNC();

        if (!initialized_) {
            CHABORT("Simulator not initialized when getting ch_bool value");
        }

        auto *node = signal.impl();
        CHDBG("ch_bool impl pointer: %p", static_cast<void *>(node));

        if (!node) {
            CHABORT(
                "ch_bool implementation is null - ch_bool may not be properly "
                "initialized");
            return ch::core::constants::zero(1); // ch_bool width is always 1
        }

        uint32_t node_id = node->id();
        CHDBG("Getting value for node ID: %u", node_id);

        auto it = data_map_.find(node_id);
        if (it != data_map_.end()) {
            CHDBG("Found value for node ID: %u", node_id);
            return it->second;
        }

        CHWARN("Value not found for ch_bool node ID: %u", node_id);
        return ch::core::constants::zero(1); // ch_bool width is always 1
    }

    // 为输入端口添加 set_value 函数
    template <typename T, typename Dir>
    void set_value(const ch::core::port<T, Dir> &port, uint64_t value) {
        // 静态断言确保只能对输入端口设置值
        static_assert(ch::core::is_input_v<Dir>,
                      "set_value can only be used with input ports!");
        set_port_value(port, value);
    }

    // 为 ch_uint<N> 类型添加 set_value 函数
    template <unsigned N>
    void set_value(const ch::core::ch_uint<N> &signal, uint64_t value) {
        CHDBG_FUNC();

        if (!initialized_) {
            CHERROR("Simulator not initialized when setting signal value");
            return;
        }

        auto *node = signal.impl();
        if (!node) {
            CHERROR("Signal implementation is null");
            return;
        }

        uint32_t node_id = node->id();
        auto it = data_map_.find(node_id);
        if (it != data_map_.end()) {
            try {
                it->second = ch::core::sdata_type(value, it->second.bitwidth());
                CHDBG("Set signal value for node %u to %llu", node_id,
                      (unsigned long long)value);
            } catch (const std::exception &e) {
                CHERROR("Failed to set signal value: %s", e.what());
            }
        } else {
            CHERROR("Signal node ID not found: %u", node_id);
        }
    }

    // 为 ch_bool 类型添加 set_value 函数
    void set_value(const ch::core::ch_bool &signal, uint64_t value) {
        CHDBG_FUNC();

        if (!initialized_) {
            CHERROR("Simulator not initialized when setting ch_bool value");
            return;
        }

        auto *node = signal.impl();
        if (!node) {
            CHERROR("ch_bool implementation is null");
            return;
        }

        uint32_t node_id = node->id();
        auto it = data_map_.find(node_id);
        if (it != data_map_.end()) {
            try {
                // ch_bool 只需要1位，所以取value的最低位
                bool bool_value = (value & 1) != 0;
                it->second = ch::core::sdata_type(bool_value ? 1 : 0, 1);
                CHDBG("Set ch_bool value for node %u to %s", node_id,
                      bool_value ? "true" : "false");
            } catch (const std::exception &e) {
                CHERROR("Failed to set ch_bool value: %s", e.what());
            }
        } else {
            CHERROR("ch_bool node ID not found: %u", node_id);
        }
    }

    template <typename T>
    void set_input_value(const ch::core::ch_in<T> &port, uint64_t value) {
        set_port_value(port, value);
    }

    // 为CHLiteral类型添加重载支持
    template <typename T>
        requires ch::core::is_ch_literal_v<std::remove_cvref_t<T>>
    void set_input_value(const ch::core::ch_in<T> &port,
                         const T &literal_value) {
        set_port_value(port, literal_value.value());
    }

    const ch::core::sdata_type &
    get_value_by_name(const std::string &name) const;

    ch::core::context *context() const { return ctx_; }
    const ch::data_map_t &data_map() const { return data_map_; }

    // Disconnect the simulator from the context to prevent access during
    // destruction
    void disconnect();

    // 获取Bundle字段值的辅助函数
    template <typename FieldType>
    uint64_t get_bundle_field_value(const FieldType &field) const {
        // 对于端口类型字段
        if constexpr (requires { field.impl(); }) {
            // 检查是否有方向类型，这表明是端口类型
            if constexpr (requires { typename FieldType::direction; }) {
                auto value = get_port_value(field);
                return static_cast<uint64_t>(value);
            } else {
                return static_cast<uint64_t>(field);
            }
        }
        // 对于其他类型字段
        else {
            return static_cast<uint64_t>(field);
        }
    }

    void reinitialize();

    // 获取跟踪状态
    bool is_tracing_enabled() const { return trace_on_; }

    // 获取跟踪块（用于测试）
    const std::deque<TraceBlock *> &get_trace_blocks_for_testing() const {
        return trace_blocks_;
    }

    // 获取跟踪信号数量（用于测试）
    size_t get_traced_signals_count() const { return signals_.size(); }
    
    // 获取跟踪的信号列表（用于测试）
    const std::vector<std::pair<uint32_t, std::string>>& get_traced_signals() const { 
        return signals_; 
    }
    
    // 根据名称查找信号ID（用于跟踪）
    uint32_t get_signal_id_by_name(const std::string& name) const {
        for (const auto& signal : signals_) {
            if (signal.second == name) {
                return signal.first;
            }
        }
        return static_cast<uint32_t>(-1); // 返回无效ID
    }
    
    // 检查是否跟踪特定信号
    bool is_signal_traced(const std::string& name) const {
        return get_signal_id_by_name(name) != static_cast<uint32_t>(-1);
    }

    // 信号跟踪相关方法
    void enable_tracing(bool enable) { trace_on_ = enable; }
    void toVCD(const std::string &filename) const; // 预留VCD输出方法

    // 添加用于测试的getter方法
    size_t get_valid_mask_width() const { return signals_.size(); }

    // 添加write_to_trace_block函数的声明
    void write_to_trace_block(const std::string &data);

private:
    void initialize();
    void update_instruction_pointers();
    void collect_signals();                  // 收集需要跟踪的信号
    void trace();                            // 执行信号跟踪
    // 为Bundle字段设置值的辅助函数
    template <typename FieldType>
    void set_bundle_field_value(FieldType &field, uint64_t value,
                                unsigned offset, unsigned width) {
        // 提取对应位段的值
        uint64_t field_value = (value >> offset) & ((1ULL << width) - 1);

        // 对于端口类型字段
        if constexpr (requires { field.impl(); }) {
            // 检查是否有方向类型，这表明是端口类型
            if constexpr (requires { typename FieldType::direction; }) {
                set_port_value(field, field_value);
            } else {
                // 对于其他类型字段
                if constexpr (std::is_same_v<FieldType, ch::core::ch_bool>) {
                    field = ch::core::ch_bool(field_value != 0);
                } else {
                    field = FieldType(field_value);
                }
            }
        }
        // 对于其他类型字段
        else {
            if constexpr (std::is_same_v<FieldType, ch::core::ch_bool>) {
                field = ch::core::ch_bool(field_value != 0);
            } else {
                field = FieldType(field_value);
            }
        }
    }

    ch::core::context *ctx_;
    ch::core::context *ctx_curr_backup_;
    std::vector<ch::core::lnodeimpl *> eval_list_;
    std::unordered_map<uint32_t, ch::instr_base *> instr_map_;
    std::unordered_map<uint32_t, std::unique_ptr<ch::instr_base>> instr_cache_;
    ch::data_map_t data_map_;
    bool initialized_ = false;
    ch::core::sdata_type *default_clock_data_ = nullptr;

    // 按类别分类的指令列表，提高执行效率
    ch::instr_base *default_clock_instr_;
    std::vector<std::pair<uint32_t, ch::instr_base *>> other_clock_instr_list_;
    std::vector<std::pair<uint32_t, ch::instr_base *>> reset_instr_list_;
    std::vector<std::pair<uint32_t, ch::instr_base *>> input_instr_list_;
    std::vector<std::pair<uint32_t, ch::instr_base *>> sequential_instr_list_;
    std::vector<std::pair<uint32_t, ch::instr_base *>> combinational_instr_list_;

    // Add flag to track if we're in the destructor to prevent accessing
    // destroyed context
    bool disconnected_ = false;
    uint64_t ticks_{0};

    // 信号跟踪相关成员
    bool trace_on_ = false;
    std::vector<std::pair<uint32_t, std::string>> signals_; // 需要跟踪的信号列表
    std::vector<std::pair<const char *, size_t>>
        prev_values_;                       // 上次记录的值的位置信息
    ch::core::sdata_type valid_mask_;       // 有效位掩码
    size_t trace_width_ = 0;                // 跟踪数据的总宽度
    std::deque<TraceBlock *> trace_blocks_; // 跟踪数据块
    TraceBlock *current_trace_block_ = nullptr;      // 当前跟踪数据块
    size_t current_trace_block_size_ = 1024 * 1024;  // 默认1MB
    size_t current_trace_block_used_ = 0;

    // 跟踪配置
    std::map<std::string, TraceConfig> trace_configs_;

    // 追踪的节点ID集合
    std::set<uint32_t> traced_nodes_;

    // 内部方法
    void load_trace_config_from_file(const std::string &config_file);
    bool should_trace_node(uint32_t node_id, const std::string &node_name,
                           ch::core::lnodetype node_type);
    void allocate_new_trace_block(); // 替换allocate_trace方法
};

} // namespace ch

#endif // SIMULATOR_H