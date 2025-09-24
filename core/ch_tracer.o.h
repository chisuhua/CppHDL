// core/ch_tracer.h
#pragma once

#include "min_cash.h"
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <string>
#include <vector>
#include <any>
#include <typeinfo>
#include <typeindex>
#include <bitset>
#include <iomanip>

namespace ch {
namespace core {

class ch_tracer {
private:
    ch_device_base* device_; // 指向被追踪的设备
    std::string vcd_filename_;
    std::ofstream vcd_file_;

    // 内部结构：存储信号的元数据和值历史
    struct SignalInfo {
        std::string name;
        int width;
        std::vector<std::string> value_history; // 每个时间点的值
        std::any* data_ptr; // 指向信号数据的指针 (ch_uint, ch_bool, ch_reg, ch_mem)
        std::type_index type; // 信号的类型
    };

    std::vector<SignalInfo> signals_;
    int current_time_ = 0;

    // 辅助函数：将任意信号转换为VCD格式的字符串
    std::string value_to_vcd_string(const std::any& data, const std::type_index& type, int width) {
        if (type == typeid(ch_bool)) {
            ch_bool* b = std::any_cast<ch_bool*>(data);
            return std::to_string(static_cast<int>(*b));
        } else if (type == typeid(ch_uint<1>)) {
            ch_uint<1>* u = std::any_cast<ch_uint<1>*>(data);
            return std::to_string(static_cast<unsigned>(*u));
        } else if (type.hash_code() == typeid(ch_uint<2>).hash_code() ||
                   type.hash_code() == typeid(ch_uint<4>).hash_code() ||
                   type.hash_code() == typeid(ch_uint<8>).hash_code() ||
                   type.hash_code() == typeid(ch_uint<16>).hash_code() ||
                   type.hash_code() == typeid(ch_uint<32>).hash_code()) {
            // 处理不同宽度的 ch_uint
            // 这里简化处理，实际项目中可以用模板特化或反射
            if (width <= 32) {
                unsigned val = 0;
                if (type == typeid(ch_uint<2>)) val = static_cast<unsigned>(*std::any_cast<ch_uint<2>*>(data));
                else if (type == typeid(ch_uint<4>)) val = static_cast<unsigned>(*std::any_cast<ch_uint<4>*>(data));
                else if (type == typeid(ch_uint<8>)) val = static_cast<unsigned>(*std::any_cast<ch_uint<8>*>(data));
                else if (type == typeid(ch_uint<16>)) val = static_cast<unsigned>(*std::any_cast<ch_uint<16>*>(data));
                else if (type == typeid(ch_uint<32>)) val = static_cast<unsigned>(*std::any_cast<ch_uint<32>*>(data));

                std::stringstream ss;
                ss << "b" << std::bitset<32>(val).to_string().substr(32 - width) << " ";
                return ss.str();
            }
        }
        return "x"; // 未知类型，返回x
    }

    // 辅助函数：生成唯一的信号ID (VCD要求)
    char get_next_id() {
        static int id = 0;
        return 'a' + (id++ % 26);
    }

public:
    // 构造函数：绑定到一个 ch_device
    template<typename T>
    ch_tracer(ch_device<T>& dev, const std::string& filename = "waveform.vcd")
        : device_(&dev), vcd_filename_(filename) {
        // 打开VCD文件
        vcd_file_.open(vcd_filename_, std::ios::out | std::ios::trunc);
        if (!vcd_file_.is_open()) {
            std::cerr << "  [ch_tracer] Failed to open VCD file: " << filename << std::endl;
            return;
        }

        // 写入VCD文件头
        vcd_file_ << "$date\n    " << __DATE__ << " " << __TIME__ << "\n$end\n";
        vcd_file_ << "$version\n    Mini-CaSH Tracer v0.1\n$end\n";
        vcd_file_ << "$timescale\n    1ns\n$end\n"; // 设置时间单位

        // $scope module ... $end
        vcd_file_ << "$scope module " << typeid(T).name() << " $end\n";

        // 注册信号 (这里简化，实际应扫描 device 的 I/O 和内部寄存器)
        // 由于架构限制，我们暂时手动注册，后续可改进
        // signals_.push_back(...);

        vcd_file_ << "$upscope $end\n";
        vcd_file_ << "$enddefinitions $end\n";
        vcd_file_ << "$dumpvars\n";

        // 初始值留空，将在第一次 tick 时填充
        vcd_file_ << "$end\n";
    }

    // 析构函数：关闭文件
    ~ch_tracer() {
        if (vcd_file_.is_open()) {
            vcd_file_ << "$dumpoff\n$end\n"; // 结束dump
            vcd_file_.close();
            std::cout << "  [ch_tracer] VCD file saved to: " << vcd_filename_ << std::endl;
        }
    }

    // 在每个仿真周期调用此函数
    void tick() {
        if (!vcd_file_.is_open()) return;

        // 写入时间戳
        vcd_file_ << "#" << current_time_ << "\n";

        // TODO: 遍历所有已注册的信号，记录其当前值
        // for (auto& sig : signals_) {
        //     std::string new_value = value_to_vcd_string(*sig.data_ptr, sig.type, sig.width);
        //     if (sig.value_history.empty() || new_value != sig.value_history.back()) {
        //         vcd_file_ << new_value << sig.id << "\n"; // VCD格式: 值 + ID
        //         sig.value_history.push_back(new_value);
        //     }
        // }

        current_time_ += 10; // 假设每个周期10ns
    }

    // 手动添加一个信号到追踪列表 (临时方案)
    template<typename SignalType>
    void add_signal(SignalType& signal, const std::string& name, int width) {
        SignalInfo info;
        info.name = name;
        info.width = width;
        info.data_ptr = &signal;
        info.type = typeid(SignalType);
        // info.id = get_next_id(); // 分配VCD ID
        signals_.push_back(info);

        // 在 $dumpvars 部分声明信号
        if (vcd_file_.is_open()) {
            if (width == 1) {
                vcd_file_ << "$var wire 1 " << get_next_id() << " " << name << " $end\n";
            } else {
                vcd_file_ << "$var wire " << width << " " << get_next_id() << " " << name << " $end\n";
            }
        }
    }
};

} // namespace core
} // namespace ch
