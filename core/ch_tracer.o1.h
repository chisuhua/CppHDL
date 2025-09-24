// core/ch_tracer.h
#pragma once

#include "min_cash.h"
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <string>
#include <vector>
#include <functional> // ✅ 添加，用于 std::function
#include <bitset>     // ✅ 添加，用于 std::bitset
#include <typeindex>

namespace ch {
namespace core {

class ch_tracer {
private:
    ch_device_base* device_;
    std::string vcd_filename_;
    std::ofstream vcd_file_;

    // ✅ 重构 SignalInfo: 使用 std::function 封装值获取逻辑
    struct SignalInfo {
        std::string name;
        int width;
        char id; // VCD 信号ID
        std::function<std::string()> get_value_func; // 函数对象，用于获取当前值的VCD字符串
    };

    std::vector<SignalInfo> signals_;
    int current_time_ = 0;

    // ✅ 辅助函数：生成唯一的信号ID
    char get_next_id() {
        static int id = 0;
        return 'a' + (id++ % 26);
    }

public:
    // 构造函数
    template<typename T>
    ch_tracer(ch_device<T>& dev, const std::string& filename = "waveform.vcd")
        : device_(&dev), vcd_filename_(filename) {

        vcd_file_.open(vcd_filename_, std::ios::out | std::ios::trunc);
        if (!vcd_file_.is_open()) {
            std::cerr << "  [ch_tracer] Failed to open VCD file: " << filename << std::endl;
            return;
        }

        // 写入VCD文件头
        vcd_file_ << "$date\n    " << __DATE__ << " " << __TIME__ << "\n$end\n";
        vcd_file_ << "$version\n    Mini-CaSH Tracer v0.1\n$end\n";
        vcd_file_ << "$timescale\n    1ns\n$end\n";
        vcd_file_ << "$scope module " << typeid(T).name() << " $end\n";

        // $dumpvars 部分将在 add_signal 时填充
        vcd_file_ << "$upscope $end\n";
        vcd_file_ << "$enddefinitions $end\n";
        vcd_file_ << "$dumpvars\n";
    }

    // 析构函数
    ~ch_tracer() {
        if (vcd_file_.is_open()) {
            vcd_file_ << "$end\n"; // 结束 $dumpvars
            vcd_file_ << "$dumpoff\n$end\n";
            vcd_file_.close();
            std::cout << "  [ch_tracer] VCD file saved to: " << vcd_filename_ << std::endl;
        }
    }

    // ✅ 重构：模板化的 add_signal
    template<typename SignalType>
    void add_signal(SignalType& signal, const std::string& name, int width) {
        SignalInfo info;
        info.name = name;
        info.width = width;
        info.id = get_next_id();

        // ✅ 核心：在编译时生成获取值的 lambda
        info.get_value_func = [&signal, width]() -> std::string {
            if constexpr (std::is_same_v<SignalType, ch_bool>) {
                return std::to_string(static_cast<int>(signal));
            } else if constexpr (IsChUintV<SignalType>) { // 我们需要一个类型萃取器
                unsigned val = static_cast<unsigned>(signal);
                if (width == 1) {
                    return std::to_string(val);
                } else {
                    std::stringstream ss;
                    ss << "b" << std::bitset<64>(val).to_string().substr(64 - width) << " ";
                    return ss.str();
                }
            }
            return "x"; // 默认返回未知值
        };

        signals_.push_back(info);

        // 在 $dumpvars 部分声明信号
        if (vcd_file_.is_open()) {
            if (width == 1) {
                vcd_file_ << "b0 " << info.id << "\n"; // 初始值设为0
                vcd_file_ << "$var wire 1 " << info.id << " " << name << " $end\n";
            } else {
                vcd_file_ << "b" << std::string(width, '0') << " " << info.id << "\n"; // 初始值设为0
                vcd_file_ << "$var wire " << width << " " << info.id << " " << name << " $end\n";
            }
        }
    }

    // ✅ 辅助类型萃取器：判断是否为 ch_uint<N>
    template<typename T>
    static constexpr bool IsChUintV = false;

    template<int N>
    static constexpr bool IsChUintV<ch_uint<N>> = true;

    // 在每个仿真周期调用此函数
    void tick() {
        if (!vcd_file_.is_open()) return;

        vcd_file_ << "#" << current_time_ << "\n";

        for (auto& sig : signals_) {
            std::string new_value = sig.get_value_func();
            // 简化：每次都写入，不判断是否变化
            if (sig.width == 1) {
                vcd_file_ << new_value << sig.id << "\n";
            } else {
                vcd_file_ << new_value << sig.id << "\n";
            }
        }

        current_time_ += 10; // 假设每个周期10ns
    }
};

} // namespace core
} // namespace ch
