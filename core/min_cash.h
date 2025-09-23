#pragma once

#include <iostream>
#include <functional>
#include <type_traits>
#include <stack>
#include <vector>
#include <map> // ⚠️ 必须添加，用于 std::map
#include <thread>

namespace ch {
namespace core {

// ============ 前向声明 ============
// ⚠️ 必须在 ch_reg 之前声明，因为 ch_reg 依赖它
class ClockDomainManager;
class ch_reg_base;

// ============ 基础数据类型 ============
template<int N>
class ch_uint {
private:
    unsigned data_;

public:
    // 构造函数
    ch_uint(unsigned d = 0) : data_(d & ((1U << N) - 1)) {}

    // 转换为 unsigned 以便打印
    operator unsigned() const { return data_; }

    // 加法运算符 (返回 ch_uint<N+1> 以避免溢出)
    template<int M>
    auto operator+(const ch_uint<M>& other) const {
        constexpr int ResultWidth = (N > M) ? N + 1 : M + 1;
        return ch_uint<ResultWidth>(data_ + other.data_);
    }

    // 赋值运算符
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
    // 构造函数
    ch_bool(bool d = false) : data_(d), prev_data_(d) {}
    // 从 bool 隐式构造
    ch_bool(int d) : data_(d != 0), prev_data_(d != 0) {}

    // 转换为 bool 以便在 if 语句中使用
    explicit operator bool() const { return data_; }

    // 转换为 unsigned 以便打印
    operator unsigned() const { return data_ ? 1U : 0U; }

    // 逻辑运算符
    ch_bool operator!() const { return !data_; }
    ch_bool operator&&(const ch_bool& other) const { return data_ && other.data_; }
    ch_bool operator||(const ch_bool& other) const { return data_ || other.data_; }

    // 位运算符（为了与 ch_uint 一致）
    ch_bool operator~() const { return !data_; }
    ch_bool operator&(const ch_bool& other) const { return data_ && other.data_; }
    ch_bool operator|(const ch_bool& other) const { return data_ || other.data_; }
    ch_bool operator^(const ch_bool& other) const { return data_ != other.data_; }

    // 赋值运算符
    ch_bool& operator=(bool val) {
        prev_data_ = data_; // 保存旧值
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

    // 获取上一周期的值（用于边沿检测）
    bool prev_value() const {
        return prev_data_;
    }
};

// ============ 端口方向修饰宏 ============
#define __in(x) x
#define __out(x) x

// ============ __io 宏 ============
#define __io(...) \
    struct io_type { \
        __VA_ARGS__ \
    } io;

// ============ 时钟域定义 ============
struct ClockDomain {
    ch_bool& clk_signal;    // 时钟信号指针
    ch_bool& rst_signal;    // 复位信号指针
    bool     posedge;       // 是否上升沿触发
    int      id;            // 时钟域唯一ID（用于寄存器分组）

    ClockDomain(ch_bool& clk, ch_bool& rst, bool pe, int domain_id)
        : clk_signal(clk), rst_signal(rst), posedge(pe), id(domain_id) {}
};

// ============ ch_reg_base 基类 ============
class ch_reg_base {
public:
    virtual ~ch_reg_base() = default;
    virtual void tick() = 0;
    virtual void end_of_cycle() = 0;
    virtual int domain_id() const = 0;
};

// ============ 寄存器模板 ============
template<typename T>
class ch_reg : public ch_reg_base {
private:
    T current_value_;
    T next_value_;
    ClockDomain domain_; // 记录构造时的时钟域
    bool has_ticked_in_cycle_ = false; // 防止同一周期多次 tick

public:
    // 构造函数：绑定到当前时钟域
    explicit ch_reg(const T& init);

    // 解引用操作符：获取当前值
    const T& operator*() const { return current_value_; }
    T& operator*() { return current_value_; }

    const T& value() const { return current_value_; }
    T& value() { return current_value_; }

    // -> 操作符：用于访问成员（如果 T 是结构体）
    const T* operator->() const { return &current_value_; }
    T* operator->() { return &current_value_; }

    // 内部类：用于设置 next_value
    struct next_proxy {
        ch_reg* self;
        next_proxy& operator=(const T& val) {
            self->next_value_ = val;
            return *this;
        }
    };

    // 返回 next_proxy 对象，用于 `reg.next() = value;` 语法
    next_proxy next() {
        return {this};
    }

    // 判断是否应该更新
    bool should_tick() const {
        bool current_clk = static_cast<bool>(domain_.clk_signal);
        bool prev_clk = static_cast<bool>(domain_.clk_signal.prev_value());
        bool is_reset = static_cast<bool>(domain_.rst_signal);

        bool clk_edge = domain_.posedge ?
            (current_clk && !prev_clk) : // 上升沿
            (!current_clk && prev_clk);  // 下降沿

        std::cout << "  [ch_reg] posedge=" << domain_.posedge
                << " clk_edge=" << clk_edge
                << " is_reset=" << is_reset
                << " clk=" << current_clk
                << " prev_clk=" << prev_clk
                << std::endl;

        // 如果是复位状态，或者没有时钟边沿，则不应更新
        return clk_edge && !is_reset;
    }

    // 更新寄存器值
    void tick() override {
        if (should_tick()) {
            current_value_ = next_value_;
            has_ticked_in_cycle_ = true;
            std::cout << "  [ch_reg] Ticked in domain " << domain_.id << std::endl;
        }
    }

    // 在周期结束时重置标记
    void end_of_cycle() override {
        has_ticked_in_cycle_ = false;
    }

    // 实现基类纯虚函数
    int domain_id() const override {
        return domain_.id;
    }
};

namespace {
    ch_bool global_default_clk(false);
    ch_bool global_default_rst(true);
    ClockDomain global_default_domain(global_default_clk, global_default_rst, true, 0);
}

// ============ 时钟域管理器 ============
class ClockDomainManager {
private:
    // 使用栈管理时钟域嵌套
    std::stack<ClockDomain> domain_stack_;
    // 全局时钟域ID计数器
    static int global_domain_id_;
    // 当前活跃的时钟域ID
    int current_domain_id_ = 0;

public:
    // 推入新时钟域
    void push(ch_bool& clk, ch_bool& rst, bool posedge) {
        int new_id = ++global_domain_id_;
        domain_stack_.push(ClockDomain(clk, rst, posedge, new_id));
        current_domain_id_ = new_id;
    }

    // 弹出当前时钟域
    void pop() {
        if (!domain_stack_.empty()) {
            domain_stack_.pop();
            current_domain_id_ = domain_stack_.empty() ? 0 : domain_stack_.top().id;
        }
    }

    // 获取当前时钟域ID
    int current_domain_id() const {
        return current_domain_id_;
    }

    // 获取当前时钟域（用于 ch_reg 绑定）
    const ClockDomain current_domain() const {
        if (domain_stack_.empty()) {
            return global_default_domain;
        }
        return domain_stack_.top();
    }

    // 单例模式：确保全局唯一
    static ClockDomainManager& instance() {
        static thread_local ClockDomainManager mgr;
        return mgr;
    }

private:
    ClockDomainManager() = default; // 私有构造函数
};

// 初始化静态成员
int ClockDomainManager::global_domain_id_ = 0;

// ============ 全局函数 ch_pushcd 和 ch_popcd ============
inline void ch_pushcd(ch_bool& clk, ch_bool& rst, bool posedge) {
    ClockDomainManager::instance().push(clk, rst, posedge);
}

inline void ch_popcd() {
    ClockDomainManager::instance().pop();
}

template<typename T>
ch_reg<T>::ch_reg(const T& init)
    : current_value_(init), next_value_(init),
      domain_(ClockDomainManager::instance().current_domain()) {
    std::cout << "  [ch_reg] Created in ClockDomain " << domain_.id << std::endl;
    register_with_device(this);
}

std::vector<ch_reg_base*> global_reg_list;

// ✅ 全局注册函数
inline void register_with_device(ch_reg_base* reg) {
    global_reg_list.push_back(reg);
}
// ============ ch_device 模板 ============
template<typename T>
class ch_device {
private:
    T instance_;
    // HACK: 使用全局向量收集所有寄存器（仅用于演示）
    static std::vector<ch_reg_base*> all_regs_;

public:
    ch_device() {
        std::cout << "[ch_device] Constructing module: " << typeid(T).name() << std::endl;
    }

    T& instance() {
        return instance_;
    }

    void describe() {
        instance_.describe();
    }

    // tick 方法，按不同时钟域更新寄存器
    void tick() {
        std::cout << "  [ch_device] Tick: Updating registers by clock domain" << std::endl;

        // 按时钟域分组
        std::map<int, std::vector<ch_reg_base*>> domain_map;
        //for (auto* reg : all_regs_) {
        for (auto* reg : global_reg_list) {
            domain_map[reg->domain_id()].push_back(reg);
        }

        // 按域更新
        for (auto& [domain_id, regs] : domain_map) {
            std::cout << "    Updating domain " << domain_id << " with " << regs.size() << " registers" << std::endl;
            for (auto* reg : regs) {
                reg->tick();
            }
        }

        // 结束周期，重置标记
        for (auto* reg : all_regs_) {
            reg->end_of_cycle();
        }
    }
};

// 初始化静态成员（必须在类外定义）
template<typename T>
std::vector<ch_reg_base*> ch_device<T>::all_regs_;

} // namespace core
} // namespace ch
