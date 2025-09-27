#pragma once

#include <iostream>
#include <functional>
#include <type_traits>
#include <stack>
#include <vector>
#include <map>
#include <unordered_map>
#include <thread>
#include <array>
#include <utility>

namespace ch {
namespace core {

// ============ 前向声明 ============
class ch_reg_base;
class Component; // ✅ 前向声明 Component

// ============ 基础数据类型 ============
template<int N>
class ch_uint {
private:
    unsigned data_;

public:
    ch_uint(unsigned d = 0) : data_(d & ((1U << N) - 1)) {}
    operator unsigned() const { return data_; }

    template<int M>
    auto operator+(const ch_uint<M>& other) const {
        constexpr int ResultWidth = (N > M) ? N + 1 : M + 1;
        return ch_uint<ResultWidth>(data_ + other.data_);
    }

    ch_uint& operator=(unsigned val) {
        data_ = val & ((1U << N) - 1);
        return *this;
    }
};

// ============ 布尔类型 ============
class ch_bool {
private:
    bool data_;
    bool prev_data_;

public:
    ch_bool(bool d = false) : data_(d), prev_data_(d) {}
    ch_bool(int d) : data_(d != 0), prev_data_(d != 0) {}

    explicit operator bool() const { return data_; }
    operator unsigned() const { return data_ ? 1U : 0U; }

    ch_bool operator!() const { return !data_; }
    ch_bool operator&&(const ch_bool& other) const { return data_ && other.data_; }
    ch_bool operator||(const ch_bool& other) const { return data_ || other.data_; }

    ch_bool operator~() const { return !data_; }
    ch_bool operator&(const ch_bool& other) const { return data_ && other.data_; }
    ch_bool operator|(const ch_bool& other) const { return data_ || other.data_; }
    ch_bool operator^(const ch_bool& other) const { return data_ != other.data_; }

    ch_bool& operator=(bool val) {
        prev_data_ = data_;
        data_ = val;
        return *this;
    }

    ch_bool& operator=(int val) {
        prev_data_ = data_;
        data_ = (val != 0);
        return *this;
    }

    bool operator==(int val) const { return data_ == (val != 0); }
    bool operator!=(int val) const { return data_ != (val != 0); }
    bool operator==(bool val) const { return data_ == val; }
    bool operator!=(bool val) const { return data_ != val; }
    bool operator==(const ch_bool& other) const { return data_ == other.data_; }
    bool operator!=(const ch_bool& other) const { return data_ != other.data_; }

    void update_prev() {
        prev_data_ = data_;
    }

    bool prev_value() const {
        return prev_data_;
    }
};
// ============ 全局时钟和复位信号 ============
// 整个设计使用唯一的一对 clk/rst 信号
static ch_bool global_clk;
static ch_bool global_rst; 
// ============ 端口方向修饰宏 ============
#define __in(x) x
#define __out(x) x

// ============ __io 宏 ============
#define __io(...) \
    struct io_type { \
        __VA_ARGS__ \
    } io;


// ============ ch_reg_base 基类 ============
class ch_reg_base {
public:
    virtual ~ch_reg_base() = default;
    virtual void tick() = 0;
    virtual void end_of_cycle() = 0;
};

// ============ 寄存器模板 ============
template<typename T>
class ch_reg : public ch_reg_base {
private:
    T current_value_;
    T next_value_;
    std::string path_name_;

public:
    // ✅ 修正构造函数签名
    explicit ch_reg(Component* parent, const std::string& name, const T& init);

    const T& operator*() const { return current_value_; }
    T& operator*() { return current_value_; }
    const T& value() const { return current_value_; }
    T& value() { return current_value_; }
    const T* operator->() const { return &current_value_; }
    T* operator->() { return &current_value_; }

    struct next_proxy {
        ch_reg* self;
        next_proxy& operator=(const T& val) {
            self->next_value_ = val;
            return *this;
        }

        template<typename IndexType>
        decltype(auto) operator[](const IndexType& index) & {
            return self->next_value_[index];
        }

        template<typename IndexType>
        decltype(auto) operator[](const IndexType& index) const & {
            return self->next_value_[index];
        }
    };

    next_proxy next() {
        return {this};
    }

    bool should_tick() const {
        bool current_clk = static_cast<bool>(global_clk);
        bool prev_clk = static_cast<bool>(global_clk.prev_value());
        bool is_reset = static_cast<bool>(global_rst);

        bool clk_edge = true ? (current_clk && !prev_clk) : (!current_clk && prev_clk);

        std::cout << "  [TICK] " << path_name_
                << "  [ch_reg] clk_edge=" << clk_edge
                << " is_reset=" << is_reset
                << " clk=" << current_clk
                << " prev_clk=" << prev_clk
                << std::endl;

        return clk_edge && !is_reset;
    }

    void tick() override {
        if (should_tick()) {
            std::cout << "  [TICK] " << path_name_ 
                      << ": " << current_value_ 
                      << " -> " << next_value_
                      << std::endl;
            current_value_ = next_value_;
        }
    }

    void end_of_cycle() override {}

};

// ============ 内存模板 ============
template<typename T, int N>
class ch_mem : public ch_reg_base {
private:
    std::array<T, N> current_storage_;
    std::array<T, N> next_storage_;

public:
    explicit ch_mem(Component* parent);

    struct mem_proxy {
        ch_mem* self;
        int addr;

        mem_proxy& operator=(const T& val) {
            if (addr >= 0 && addr < N) {
                self->next_storage_[addr] = val;
            } else {
                std::cerr << "  [ch_mem] Address " << addr << " out of range: " << N << std::endl;
            }
            return *this;
        }

        T operator*() const {
            if (addr >= 0 && addr < N) {
                return self->current_storage_[addr];
            } else {
                std::cerr << "  [ch_mem] Address " << addr << " out of range: " << N << std::endl;
                return T();
            }
        }
    };

    bool should_tick() const {
        bool current_clk = static_cast<bool>(global_clk);
        bool prev_clk = static_cast<bool>(global_clk.prev_value());
        bool is_reset = static_cast<bool>(global_rst);

        bool clk_edge = true ? (current_clk && !prev_clk) : (!current_clk && prev_clk);

        std::cout << "  [ch_mem] "
                << " clk_edge=" << clk_edge
                << " is_reset=" << is_reset
                << " clk=" << current_clk
                << " prev_clk=" << prev_clk
                << std::endl;

        return clk_edge && !is_reset;
    }

    mem_proxy operator[](int addr) {
        return {this, addr};
    }

    void tick() override {
        if (should_tick()) {
            current_storage_ = next_storage_;
            std::cout << "  [ch_mem] Ticked in  " << std::endl;
        }
    }

    void end_of_cycle() override {}

};


// ============ ch_device_base 基类 ============
class ch_device_base {
public:
    virtual ~ch_device_base() = default;
    virtual void describe() = 0;
    virtual void tick() = 0;
};

// ============ ch_device 模板 ============
// ✅ 简化: ch_device 不再管理寄存器列表，它只是一个调度器
template<typename T>
class ch_device : public ch_device_base {
private:
    T instance_;

public:
    template<typename... Args>
    explicit ch_device(Args&&... args) 
        : instance_(std::forward<Args>(args)...) {
        std::cout << "[ch_device] Constructing module: " << typeid(T).name() << std::endl;
    }

    T& instance() {
        return instance_;
    }

    void describe() override {
        instance_.describe();
    }

    void tick() override {
        std::cout << "  [ch_device] Tick: Updating registers by clock domain" << std::endl;

        const auto& all_regs = instance_.get_all_regs();

        for (auto* reg : all_regs) {
            reg->tick();
        }
        // 结束周期
        for (auto* reg : all_regs) {
            reg->end_of_cycle();
        }
    }
};


// ============ 向量类型 (ch_vec) ============
// 用于创建一组相同类型的硬件元素
template<typename T, int N>
class ch_vec : public std::array<T, N> {
public:
    using std::array<T, N>::array; // 继承构造函数

    // 重载 [] 操作符，使其行为与 ch_mem 类似，返回一个代理对象
    // 这样就可以使用 vec[i] = value; 语法
    struct vec_proxy {
        ch_vec* self;
        int index;

        vec_proxy& operator=(const T& val) {
            std::cout << "  [DEBUG] vec_proxy::operator= called for index: " << index << std::endl; // ✅ 调试打印
            if (index >= 0 && index < N) {
                static_cast<std::array<T, N>&>(*self)[index] = val;
                std::cout << "MYDBG" << static_cast<std::array<T, N>&>(*self)[index] << "=" << val << std::endl;
            } else {
                std::cerr << "  [ch_vec] Index out of range: " << index << std::endl;
            }
            return *this;
        }

        operator const T&() const {
            if (index >= 0 && index < N) {
                return static_cast<const std::array<T, N>&>(*self)[index];
            } else {
                static T dummy;
                return dummy;
            }
        }

        const T& operator*() const {
            if (index >= 0 && index < N) {
                return static_cast<const std::array<T, N>&>(*self)[index];
            } else {
                static T dummy;
                return dummy;
            }
        }

        T& operator*() {
            if (index >= 0 && index < N) {
                return static_cast<std::array<T, N>&>(*self)[index];
            } else {
                static T dummy;
                return dummy;
            }
        }
        friend std::ostream& operator<<(std::ostream& os, const vec_proxy& proxy) {
            os << *proxy; // 调用 operator*() 获取值
            return os;
        }
    };

    vec_proxy operator[](int index) & {
        return {this, index};
    }

    const T& operator[](int index) const & {
        return std::array<T, N>::operator[](index);
    }

    T operator[](int index) && {
        return std::array<T, N>::operator[](index);
    }

    friend std::ostream& operator<<(std::ostream& os, const ch_vec<T, N>& vec) {
        os << "[";
        for (int i = 0; i < N; ++i) {
            if (i > 0) os << ", ";
            os << vec[i]; // 依赖 T 的 operator<<
        }
        os << "]";
        return os;
    }
};


// 辅助类型萃取器：获取 ch_uint, ch_bool, ch_vec 的位宽
template<typename T>
struct WidthTrait {
    static constexpr int value = 1; // 默认宽度为1
};

template<int N>
struct WidthTrait<ch_uint<N>> {
    static constexpr int value = N;
};

template<>
struct WidthTrait<ch_bool> {
    static constexpr int value = 1;
};

template<typename T, int N>
struct WidthTrait<ch_vec<T, N>> {
    static constexpr int value = N * WidthTrait<T>::value;
};

template<typename T>
constexpr int WidthOf() {
    return WidthTrait<T>::value;
}

// 辅助函数：位拼接 (Concatenation)
// 例如: Cat(vec[0], vec[1], vec[2])
template<typename... Args>
auto Cat(Args&&... args) {
    // 计算总位宽
    constexpr int total_width = (WidthOf<Args>() + ... + 0);

    // 创建一个足够宽的 ch_uint 来存放结果
    ch_uint<total_width> result(0);

    // 从左到右拼接 (args... 中的第一个参数是最高位)
    int shift = total_width;
    ([&](auto&& arg) {
        constexpr int arg_width = WidthOf<std::decay_t<decltype(arg)>>();
        shift -= arg_width;
        result = result | (ch_uint<total_width>(static_cast<unsigned>(arg)) << shift);
    }(args), ...);

    return result;
}

// ============ 便捷函数 ============
template<typename T>
T ch_next(const T& data) {
    return data;
}

template<typename T, typename U>
T ch_nextEn(const T& data, const ch_bool& enable, const U& init) {
    return enable ? data : T(init);
}

// ============ 格雷码转换函数 ============
// 二进制转格雷码
template<int N>
ch_uint<N> bin_to_gray(const ch_uint<N>& bin) {
    return bin ^ (bin >> 1);
}

// 格雷码转二进制
template<int N>
ch_uint<N> gray_to_bin(const ch_uint<N>& gray) {
    ch_uint<N> bin = gray;
    for (int i = 1; i < N; ++i) {
        bin = bin ^ (gray >> i);
    }
    return bin;
}

} // namespace core
} // namespace ch
