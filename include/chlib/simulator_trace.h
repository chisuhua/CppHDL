#pragma once

#include <cstdint>
#include <sstream>
#include <string>
#include <vector>

namespace ch::chlib {

/**
 * @brief UART控制台捕获工具类
 * 用于捕获CppHDL仿真过程中的UART TX输出
 */
class UartCapture {
public:
    UartCapture() = default;

    /**
     * @brief 每个tick调用以捕获UART输出
     * @param char_code UART字符码 (ASCII值)
     * @param tx_valid TX有效信号
     */
    void capture_tick(uint64_t char_code, uint64_t tx_valid) {
        if (tx_valid && char_code != 0) {
            char c = static_cast<char>(char_code & 0xFF);
            if (c == '\n') {
                lines_.push_back(buf_.str());
                buf_.str("");
                buf_.clear();
            } else if (c != '\r') {
                buf_ << c;
            }
        }
    }

    /**
     * @brief 获取捕获的输出字符串
     */
    std::string output() const {
        std::string result;
        for (const auto& line : lines_) {
            result += line;
            result += '\n';
        }
        result += buf_.str();
        return result;
    }

    /**
     * @brief 获取行数
     */
    size_t line_count() const {
        return lines_.size();
    }

    /**
     * @brief 获取指定行
     * @param index 行索引
     */
    std::string line(size_t index) const {
        if (index < lines_.size()) {
            return lines_[index];
        }
        return "";
    }

    /**
     * @brief 清空捕获缓冲区
     */
    void clear() {
        buf_.str("");
        buf_.clear();
        lines_.clear();
    }

private:
    std::ostringstream buf_;
    std::vector<std::string> lines_;
};

} // namespace ch::chlib
