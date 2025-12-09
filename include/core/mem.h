#ifndef CH_CORE_CH_MEM_H
#define CH_CORE_CH_MEM_H

#include "ast/ast_nodes.h"
#include "ast/mem_port_impl.h"
#include "ast/memimpl.h"
#include "core/bool.h"
#include "core/context.h"
#include "core/lnode.h"
#include "core/traits.h"
#include "core/uint.h"
#include <array>
#include <source_location>
#include <string>
#include <vector>

namespace ch {
namespace core {

template <typename T, unsigned N> class ch_mem {
public:
    using value_type = T;
    static constexpr unsigned num_entries = N;
    static constexpr unsigned data_width = ch_width_v<T>;
    static constexpr unsigned addr_width =
        (N > 1) ? (32 - __builtin_clz(N - 1)) : 1;

    // 读端口类型 - 直接继承端口节点功能
    class read_port {
    private:
        mem_read_port_impl *port_impl_;

    public:
        read_port(mem_read_port_impl *impl) : port_impl_(impl) {}

        // 转换为lnode，用于连接和运算
        operator lnode<T>() const { return lnode<T>(port_impl_); }

        // 获取底层实现
        mem_read_port_impl *impl() const { return port_impl_; }

        // 端口信息
        uint32_t port_id() const { return port_impl_->port_id(); }
    };

    // 写端口类型
    class write_port {
    private:
        mem_write_port_impl *port_impl_;

    public:
        write_port(mem_write_port_impl *impl) : port_impl_(impl) {}

        // 获取底层实现
        mem_write_port_impl *impl() const { return port_impl_; }

        // 端口信息
        uint32_t port_id() const { return port_impl_->port_id(); }
    };

private:
    memimpl *mem_node_;

public:
    // 构造函数 - 无初始值
    explicit ch_mem(
        const std::string &name = "mem",
        const std::source_location &sloc = std::source_location::current()) {
        init_memory(name, sloc, false, std::vector<sdata_type>());
    }

    // 构造函数 - 带初始值（从vector）
    template <typename U>
    ch_mem(const std::vector<U> &init_data, const std::string &name = "mem",
           const std::source_location &sloc = std::source_location::current()) {
        static_assert(std::is_integral_v<U>,
                      "Initial data must be integral type");
        std::vector<sdata_type> init_sdata = create_init_data(init_data);
        init_memory(name, sloc, false, init_sdata);
    }

    // 构造函数 - 带初始值（从数组）
    template <typename U, std::size_t M>
    ch_mem(const std::array<U, M> &init_data, const std::string &name = "mem",
           const std::source_location &sloc = std::source_location::current()) {
        static_assert(std::is_integral_v<U>,
                      "Initial data must be integral type");
        std::vector<sdata_type> init_sdata = create_init_data(init_data);
        init_memory(name, sloc, false, init_sdata);
    }

    // ROM工厂方法
    template <typename Container>
    static ch_mem<T, N> make_rom(
        const Container &init_data, const std::string &name = "rom",
        const std::source_location &sloc = std::source_location::current()) {
        ch_mem<T, N> rom;
        std::vector<sdata_type> init_sdata = rom.create_init_data(init_data);
        rom.init_memory(name, sloc, true, init_sdata);
        return rom;
    }

    // 创建异步读端口
    read_port aread(const ch_uint<addr_width> &addr,
                    const std::string &name = "mem_aread",
                    const std::source_location &sloc =
                        std::source_location::current()) const {
        auto addr_lnode = get_lnode(addr);

        // 创建数据输出节点
        auto *ctx = mem_node_->ctx();
        auto *data_output =
            ctx->create_output(data_width, name + "_data", sloc);

        // 创建异步读端口节点
        auto *port_impl = ctx->create_mem_read_port(
            mem_node_, mem_node_->next_port_id(), data_width, nullptr,
            addr_lnode.impl(), nullptr, data_output, name, sloc);

        return read_port(port_impl);
    }

    // 创建同步读端口
    read_port sread(const ch_uint<addr_width> &addr,
                    const ch_bool &enable = ch_bool(true),
                    const std::string &name = "mem_sread",
                    const std::source_location &sloc =
                        std::source_location::current()) const {
        auto addr_lnode = get_lnode(addr);
        auto enable_lnode = get_lnode(enable);

        // 获取当前时钟域
        auto *ctx = mem_node_->ctx();
        auto *cd = ctx->current_clock(sloc);

        // 创建数据输出节点
        auto *data_output =
            ctx->create_output(data_width, name + "_data", sloc);

        // 创建同步读端口节点
        auto enable_impl =
            is_litimpl_one(enable_lnode.impl()) ? nullptr : enable_lnode.impl();
        auto *port_impl = ctx->create_mem_read_port(
            mem_node_, mem_node_->next_port_id(), data_width, cd,
            addr_lnode.impl(), enable_impl, data_output, name, sloc);

        return read_port(port_impl);
    }

    // 写操作
    template <typename U, typename E = ch_bool>
    write_port
    write(const ch_uint<addr_width> &addr, const U &data,
          const E &enable = ch_bool(true),
          const std::string &name = "mem_write",
          const std::source_location &sloc = std::source_location::current()) {
        static_assert(!std::is_const_v<ch_mem>, "Cannot write to ROM");

        auto addr_lnode = get_lnode(addr);
        auto data_lnode = get_lnode(data);
        auto enable_lnode = get_lnode(enable);

        // 获取当前时钟域
        auto *ctx = mem_node_->ctx();
        auto *cd = ctx->current_clock(sloc);

        // 创建写端口节点
        auto enable_impl =
            is_litimpl_one(enable_lnode.impl()) ? nullptr : enable_lnode.impl();
        auto *port_impl = ctx->create_mem_write_port(
            mem_node_, mem_node_->next_port_id(), data_width, cd,
            addr_lnode.impl(), data_lnode.impl(), enable_impl, name, sloc);

        return write_port(port_impl);
    }

    // 获取底层节点
    memimpl *impl() const { return mem_node_; }

    // 转换为lnode
    operator lnodeimpl *() const { return mem_node_; }

private:
    void init_memory(const std::string &name, const std::source_location &sloc,
                     bool is_rom,
                     const std::vector<sdata_type> &init_data) { // 接收vector
        auto *ctx = ctx_curr_;
        if (!ctx) {
            throw std::runtime_error("No active context for memory creation");
        }

        mem_node_ =
            ctx->create_memory(addr_width, data_width, N, 1, true, is_rom,
                               init_data, name, sloc); // 传递vector
    }

    template <typename Container>
    std::vector<sdata_type> create_init_data(const Container &data) {
        using value_type = typename Container::value_type;
        static_assert(std::is_integral_v<value_type>,
                      "Container value type must be integral");

        std::vector<sdata_type> result;
        result.reserve(data.size());

        for (const auto &item : data) {
            // 每个元素创建独立的sdata_type
            result.emplace_back(static_cast<uint64_t>(item), data_width);
        }
        return result;
    }
};

// ROM特化
template <typename T, unsigned N> using ch_rom = ch_mem<T, N>;

} // namespace core
} // namespace ch

// 宽度特质特化
namespace ch {
namespace core {
template <typename T, unsigned N> struct ch_width_impl<ch_mem<T, N>, void> {
    static constexpr unsigned value = ch_width_v<T>;
};

template <typename T, unsigned N>
struct ch_width_impl<const ch_mem<T, N>, void> {
    static constexpr unsigned value = ch_width_v<T>;
};
} // namespace core
} // namespace ch

#endif // CH_CORE_CH_MEM_H
