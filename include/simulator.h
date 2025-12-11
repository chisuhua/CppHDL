// include/simulator.h
#ifndef SIMULATOR_H
#define SIMULATOR_H

#include "ast/instr_base.h"
#include "core/bundle/bundle_base.h"
#include "core/context.h"
#include "core/io.h"
#include "core/reg.h"
#include "core/types.h"
#include "logger.h"
#include "utils/destruction_manager.h"
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

namespace ch {

class Simulator {
public:
    explicit Simulator(ch::core::context *ctx);
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

    template <typename T>
    void set_input_value(const ch::core::ch_in<T> &port, uint64_t value) {
        set_port_value(port, value);
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

private:
    void initialize();
    void update_instruction_pointers();

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
    std::vector<ch::core::lnodeimpl *> eval_list_;
    std::unordered_map<uint32_t, ch::instr_base *> instr_map_;
    std::unordered_map<uint32_t, std::unique_ptr<ch::instr_base>> instr_cache_;
    ch::data_map_t data_map_;
    bool initialized_ = false;
    ch::core::sdata_type *default_clock_data_ = nullptr;

    // 按类别分类的指令列表，提高执行效率
    std::vector<std::pair<uint32_t, ch::instr_base *>>
        default_clock_instr_list_;
    std::vector<std::pair<uint32_t, ch::instr_base *>> other_clock_instr_list_;
    std::vector<std::pair<uint32_t, ch::instr_base *>> reset_instr_list_;
    std::vector<std::pair<uint32_t, ch::instr_base *>> input_instr_list_;
    std::vector<std::pair<uint32_t, ch::instr_base *>> sequential_instr_list_;
    std::vector<std::pair<uint32_t, ch::instr_base *>>
        combinational_instr_list_;

    // Add flag to track if we're in the destructor to prevent accessing
    // destroyed context
    bool disconnected_ = false;
};

} // namespace ch

#endif // SIMULATOR_H