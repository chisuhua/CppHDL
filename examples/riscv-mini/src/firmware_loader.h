#pragma once

#include <cstdint>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace ch::chlib {

/**
 * @brief Intel HEX 格式固件加载器
 * 解析 Intel HEX 文件并将代码/数据加载到内存中
 */
struct HexRecord {
    uint8_t byte_count;        // 数据字节数
    uint32_t address;          // 16位起始地址
    uint8_t record_type;       // 记录类型: 00=Data, 01=EOF, 02=ExtSeg, 04=ExtLin
    std::vector<uint8_t> data; // 数据字节
    uint8_t checksum;          // 校验和
};

class FirmwareLoader {
public:
    /**
     * @brief 从文件加载 Intel HEX 到内存缓冲区
     * @param hex_file_path HEX 文件路径
     * @param memory 目标内存缓冲区 (向量，按地址写入)
     * @param base_offset 可选的地址偏移
     * @return 加载成功返回 true，失败返回 false
     */
    static bool loadHex(const std::string& hex_file_path,
                        std::vector<uint8_t>& memory,
                        uint32_t base_offset = 0);

    /**
     * @brief 加载 32位字到内存 (用于指令)
     * @param hex_file_path HEX 文件路径
     * @param words 目标字缓冲区
     * @param base_addr 起始地址 (按字对齐)
     * @return 加载成功返回 true，失败返回 false
     */
    static bool loadHexWords(const std::string& hex_file_path,
                             std::vector<uint32_t>& words,
                             uint32_t base_addr = 0);

private:
    /**
     * @brief 解析单行 Intel HEX 记录
     * @param line HEX 行字符串
     * @return 解析后的 HexRecord，失败时 byte_count 为 0
     */
    static HexRecord parseLine(const std::string& line);

    /**
     * @brief 验证记录校验和
     * @param record HEX 记录
     * @return 校验和正确返回 true
     */
    static bool verifyChecksum(const HexRecord& record);

    /**
     * @brief 将两个十六进制字符转换为字节
     * @param high 高四位字符
     * @param low 低四位字符
     * @return 转换后的字节值
     */
    static uint8_t hexToByte(char high, char low);

    /**
     * @brief 将十六进制字符串转换为字节向量
     * @param hex 十六进制字符串
     * @return 转换后的字节向量
     */
    static std::vector<uint8_t> hexToBytes(const std::string& hex);
};

// 实现

inline uint8_t FirmwareLoader::hexToByte(char high, char low) {
    auto hexCharToNibble = [](char c) -> uint8_t {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        return 0;
    };
    return (hexCharToNibble(high) << 4) | hexCharToNibble(low);
}

inline std::vector<uint8_t> FirmwareLoader::hexToBytes(const std::string& hex) {
    std::vector<uint8_t> bytes;
    bytes.reserve(hex.length() / 2);
    for (size_t i = 0; i + 1 < hex.length(); i += 2) {
        bytes.push_back(hexToByte(hex[i], hex[i + 1]));
    }
    return bytes;
}

inline HexRecord FirmwareLoader::parseLine(const std::string& line) {
    HexRecord record{};
    record.byte_count = 0;
    record.address = 0;
    record.record_type = 0;
    record.checksum = 0;

    // 跳过空行和注释行
    if (line.empty() || line[0] != ':') {
        return record;
    }

    // 去除前后空白
    std::string trimmed = line;
    while (!trimmed.empty() && (trimmed.back() == '\n' || trimmed.back() == '\r')) {
        trimmed.pop_back();
    }
    if (trimmed.empty() || trimmed[0] != ':') {
        return record;
    }

    // 去掉开头的 ':'
    std::string hex = trimmed.substr(1);
    if (hex.length() < 9) {
        return record;  // 最小记录: :BBAAAATTCC (10字符)
    }

    try {
        // 解析字节数
        record.byte_count = hexToByte(hex[0], hex[1]);

        // 解析地址
        record.address = (static_cast<uint32_t>(hexToByte(hex[2], hex[3])) << 8) |
                         static_cast<uint32_t>(hexToByte(hex[4], hex[5]));

        // 解析记录类型
        record.record_type = hexToByte(hex[6], hex[7]);

        // 计算数据部分的起始位置和长度
        size_t dataStart = 8;
        size_t dataLen = static_cast<size_t>(record.byte_count) * 2;

        // 验证字符串长度足够 (数据 + 校验和 2 字符)
        if (hex.length() < dataStart + dataLen + 2) {
            return record;
        }

        // 解析数据
        std::string dataHex = hex.substr(dataStart, dataLen);
        record.data = hexToBytes(dataHex);

        // 解析校验和
        std::string checksumHex = hex.substr(dataStart + dataLen, 2);
        record.checksum = hexToByte(checksumHex[0], checksumHex[1]);

    } catch (...) {
        return record;  // 解析失败，返回空记录
    }

    return record;
}

inline bool FirmwareLoader::verifyChecksum(const HexRecord& record) {
    // 计算校验和: byte_count + address_high + address_low + record_type + data bytes
    uint8_t sum = 0;
    sum += record.byte_count;
    sum += static_cast<uint8_t>((record.address >> 8) & 0xFF);
    sum += static_cast<uint8_t>(record.address & 0xFF);
    sum += record.record_type;

    for (uint8_t byte : record.data) {
        sum += byte;
    }

    // 校验和 = 0 表示正确 (sum + checksum = 0 mod 256)
    return (sum + record.checksum) == 0;
}

inline bool FirmwareLoader::loadHex(const std::string& hex_file_path,
                                    std::vector<uint8_t>& memory,
                                    uint32_t base_offset) {
    std::ifstream file(hex_file_path);
    if (!file.is_open()) {
        return false;
    }

    uint32_t extended_linear_address = 0;  // 用于记录类型 04
    std::string line;

    while (std::getline(file, line)) {
        HexRecord record = parseLine(line);

        // 跳过无效记录
        if (record.byte_count == 0 && record.data.empty()) {
            continue;
        }

        // 验证校验和
        if (!verifyChecksum(record)) {
            return false;
        }

        switch (record.record_type) {
            case 0x00: {  // Data record
                uint32_t start_addr = extended_linear_address + record.address + base_offset;
                // 确保内存缓冲区足够大
                size_t end_addr = start_addr + record.data.size();
                if (memory.size() < end_addr) {
                    memory.resize(end_addr, 0);
                }
                // 写入数据到内存
                for (size_t i = 0; i < record.data.size(); ++i) {
                    memory[start_addr + i] = record.data[i];
                }
                break;
            }
            case 0x01:  // End Of File
                return true;

            case 0x02:  // Extended Segment Address
                // 段地址 (将被左移4位后使用)
                if (record.data.size() >= 2) {
                    uint16_t segment = (static_cast<uint16_t>(record.data[0]) << 8) |
                                       static_cast<uint16_t>(record.data[1]);
                    // 在数据记录中不使用段地址，而是使用线性地址
                    (void)segment;
                }
                break;

            case 0x04: {  // Extended Linear Address
                // 高16位地址 (用于32位寻址)
                if (record.data.size() >= 2) {
                    extended_linear_address = (static_cast<uint32_t>(record.data[0]) << 24) |
                                              (static_cast<uint32_t>(record.data[1]) << 16);
                }
                break;
            }
            case 0x03:  // Start Segment Address (忽略)
            case 0x05:  // Start Linear Address (忽略)
            default:
                // 未知记录类型，跳过
                break;
        }
    }

    return true;
}

inline bool FirmwareLoader::loadHexWords(const std::string& hex_file_path,
                                          std::vector<uint32_t>& words,
                                          uint32_t base_addr) {
    // 首先加载为字节
    std::vector<uint8_t> memory;
    if (!loadHex(hex_file_path, memory, 0)) {
        return false;
    }

    // 计算有多少个32位字
    // 起始地址需要按4字节对齐
    size_t byte_offset = (base_addr % 4);
    size_t num_words = (memory.size() + byte_offset) / 4;

    if (num_words == 0) {
        return true;  // 没有数据
    }

    words.resize(num_words, 0);

    // 将字节转换为32位字 (小端序)
    for (size_t i = 0; i < memory.size(); ++i) {
        size_t word_idx = (byte_offset + i) / 4;
        size_t byte_idx = (byte_offset + i) % 4;
        if (word_idx < words.size()) {
            words[word_idx] |= (static_cast<uint32_t>(memory[i]) << (byte_idx * 8));
        }
    }

    return true;
}

}  // namespace ch::chlib
