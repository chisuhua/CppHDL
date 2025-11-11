// samples/bundle_simulator_demo.cpp
#include "../include/ch.hpp"
#include "../include/component.h"
#include "../include/module.h"
#include "../include/simulator.h"
#include "../include/codegen.h"
#include "../include/io/common_bundles.h"
#include "../include/core/bundle/bundle_utils.h"
#include <iostream>
#include <bitset>

using namespace ch::core;

int main() {
    std::cout << "Bundle Simulator Demo - demonstrating POD-driven Bundle interfaces" << std::endl;
    
    std::cout << "\n=== 测试1: 位操作演示 ===" << std::endl;
    
    // 演示如何手动处理位操作
    uint64_t data = 10;     // 4位数据: 1010
    uint64_t valid = 1;     // 1位有效: 1
    uint64_t ready = 0;     // 1位就绪: 0
    
    // 手动拼接: [ready(1位) | valid(1位) | data(4位)] = [0|1|1010] = 0x1A
    uint64_t serialized = (ready << 5) | (valid << 4) | data;
    
    std::cout << "Manual serialized value: 0x" << std::hex << serialized
              << " (binary: " << std::bitset<6>(serialized) << ")" << std::dec << std::endl;
    
    // 手动解包
    uint64_t unpacked_data = serialized & 0xF;         // 提取低4位
    uint64_t unpacked_valid = (serialized >> 4) & 0x1; // 提取第4位
    uint64_t unpacked_ready = (serialized >> 5) & 0x1; // 提取第5位
    
    std::cout << "Manual unpacked values - data: " << unpacked_data
              << ", valid: " << unpacked_valid 
              << ", ready: " << unpacked_ready << std::endl;
    
    std::cout << "\nDemo completed successfully!" << std::endl;
    std::cout << "This demonstrates the basic principles of serializing/deserializing Bundle interfaces." << std::endl;
    std::cout << "A full implementation would require more sophisticated template metaprogramming" << std::endl;
    std::cout << "and integration with the hardware node system." << std::endl;
    
    return 0;
}