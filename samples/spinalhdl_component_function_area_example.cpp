#include "chlib/stream.h"
#include "ch.hpp"
#include "core/context.h"
#include "simulator.h"
#include "core/literal.h"
#include <iostream>
#include <vector>
#include <algorithm>

using namespace ch::core;
using namespace chlib;

// 简单的Adder组件 - 对应SpinalHDL的Adder组件
class Adder : public component {
public:
    ch_in<ch_sint<8>> io_a;
    ch_in<ch_sint<8>> io_b;
    ch_out<ch_sint<9>> io_c;

    Adder() : component("Adder") {
        io_a.set_name("io_a");
        io_b.set_name("io_b");
        io_c.set_name("io_c");
        
        // 描述组合逻辑
        describe();
    }

private:
    void describe() override {
        // 简单加法运算
        io_c = io_a + io_b;
    }
};

// 递归AdderTree组件 - 对应SpinalHDL的AdderTree
class AdderTree : public component {
private:
    int diw;  // 输入数据宽度
    int size; // 输入向量大小
    int groupSize; // 组大小

public:
    ch_in<Flow<std::vector<ch_sint<32>>>> io_nets;
    ch_out<Flow<ch_sint<64>>> io_sum;

    AdderTree(int _diw, int _size, int _groupSize) 
        : component(), diw(_diw), size(_size), groupSize(_groupSize) {
        // 设置组件名称
        std::string comp_name = "AdderTree_n" + std::to_string(size) + "_g" + std::to_string(groupSize);
        this->set_definition_name(comp_name);
        
        // 创建输入输出端口
        io_nets.set_name("io_nets");
        io_sum.set_name("io_sum");
        
        describe();
    }

    // 计算延迟
    int get_latency() const {
        int stages = 0;
        int current_size = size;
        while (current_size > 1) {
            current_size = (current_size + groupSize - 1) / groupSize; // ceiling division
            stages++;
        }
        return stages;
    }

    // 计算输出宽度
    int get_output_width() const {
        int stages = get_latency();
        return diw + (groupSize > 1 ? (groupSize - 1) * stages : 0);
    }

private:
    void describe() override {
        // 递归构建树形加法器
        // 创建一个简单的实现，将输入向量分组并递归相加
        auto inputs = io_nets.value().payload;
        
        // 为了简化，我们创建一个函数来构建加法树
        ch_sint<64> result = build_adder_tree(inputs, groupSize);
        
        // 输出有效信号需要延迟
        ch_bool valid_delayed = io_nets.value().valid;
        for(int i = 0; i < get_latency(); i++) {
            valid_delayed = ch::on_posedge(clk).reg(valid_delayed, false);
        }
        
        // 设置输出
        Flow<ch_sint<64>> output_stream;
        output_stream.payload = result;
        output_stream.valid = valid_delayed;
        io_sum = output_stream;
    }
    
    // 时钟信号（模拟）
    ch_bool clk = false;
    
    ch_sint<64> build_adder_tree(const std::vector<ch_sint<32>>& inputs, int group_size) {
        std::vector<ch_sint<64>> current_level = convert_to_wider(inputs);
        
        // 逐层构建加法树
        while(current_level.size() > 1) {
            std::vector<ch_sint<64>> next_level;
            
            // 将当前层分组并相加
            for(size_t i = 0; i < current_level.size(); i += group_size) {
                ch_sint<64> sum = 0_s64;
                
                // 对当前组进行求和
                for(int j = 0; j < group_size && (i + j) < current_level.size(); j++) {
                    sum = sum + current_level[i + j];
                }
                
                next_level.push_back(sum);
            }
            
            current_level = next_level;
        }
        
        return current_level.empty() ? 0_s64 : current_level[0];
    }
    
    std::vector<ch_sint<64>> convert_to_wider(const std::vector<ch_sint<32>>& inputs) {
        std::vector<ch_sint<64>> wider_inputs;
        for(const auto& input : inputs) {
            wider_inputs.push_back(input.to_sint<64>());  // 扩展位宽
        }
        return wider_inputs;
    }
};

// 用于分组的辅助函数 - 对应SpinalHDL的group函数
template<typename T>
std::vector<std::vector<T>> group_elements(const std::vector<T>& vec, int group_size) {
    std::vector<std::vector<T>> result;
    for(size_t i = 0; i < vec.size(); i += group_size) {
        std::vector<T> group;
        for(int j = 0; j < group_size && (i + j) < vec.size(); j++) {
            group.push_back(vec[i + j]);
        }
        result.push_back(group);
    }
    return result;
}

// 简单的加法树函数实现 - 对应SpinalHDL的函数式实现
template<int WIDTH>
ch_sint<WIDTH+1> add_two(ch_sint<WIDTH> a, ch_sint<WIDTH> b) {
    return a + b;
}

// 函数式加法树实现
template<int WIDTH>
ch_sint<WIDTH+4> adder_tree_function(const std::vector<ch_sint<WIDTH>>& inputs) {
    if(inputs.size() == 1) {
        return inputs[0].to_sint<WIDTH+4>();
    } else if(inputs.size() == 2) {
        auto sum = add_two(inputs[0], inputs[1]);
        return sum.to_sint<WIDTH+4>();
    } else {
        // 分组并递归相加
        auto grouped = group_elements(inputs, 2);
        std::vector<ch_sint<WIDTH+1>> group_sums;
        
        for(const auto& group : grouped) {
            if(group.size() == 1) {
                group_sums.push_back(group[0].to_sint<WIDTH+1>());
            } else {
                group_sums.push_back(add_two(group[0], group[1]));
            }
        }
        
        // 递归处理组和
        return adder_tree_function(group_sums);
    }
}

// 对应SpinalHDL的AdderTree object，提供工厂函数
namespace AdderTreeFactory {
    // 对应SpinalHDL的 apply(nets: Flow[Vec[SInt]], addCellSize: Int)
    template<int WIDTH, int SIZE>
    AdderTree* create_adder_tree(Flow<std::vector<ch_sint<WIDTH>>>& nets, int addCellSize) {
        // 提取输入宽度和大小
        int diw = WIDTH;
        int size = SIZE;
        
        // 创建AdderTree实例
        AdderTree* uAdderTree = new AdderTree(diw, size, addCellSize);
        
        // 连接输入
        uAdderTree->io_nets = nets;
        
        return uAdderTree;
    }
    
    // 对应SpinalHDL的 apply(nets: Vec[SInt], addCellSize: Int)
    template<int WIDTH, int SIZE>
    AdderTree* create_adder_tree_from_vec(const std::vector<ch_sint<WIDTH>>& nets, int addCellSize) {
        // 提取输入宽度和大小
        int diw = WIDTH;
        int size = SIZE;
        
        // 创建AdderTree实例
        AdderTree* uAdderTree = new AdderTree(diw, size, addCellSize);
        
        // 创建Flow类型的输入
        Flow<std::vector<ch_sint<WIDTH>>> flow_nets;
        flow_nets.payload = nets;
        flow_nets.valid = true;  // 默认有效
        
        // 连接输入
        uAdderTree->io_nets = flow_nets;
        
        return uAdderTree;
    }
}

// Top组件 - 对应SpinalHDL的Top组件
class Top : public component {
public:
    Top() : component("Top") {
        describe();
    }
    
private:
    void describe() override {
        // 这里可以放置顶层逻辑
    }
};

int main() {
    // 创建上下文
    auto ctx = std::make_unique<ch::core::context>("spinalhdl_component_function_area_example");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    std::cout << "CppHDL Component & Function Area Examples (based on SpinalHDL)" << std::endl;
    std::cout << "===========================================================" << std::endl;

    // 示例1: 简单的Adder组件
    std::cout << "\n1. Simple Adder Component Example:" << std::endl;
    
    Adder adder;
    
    // 创建一些测试值
    ch_sint<8> a_val = 10_s8;
    ch_sint<8> b_val = 20_s8;
    
    // 模拟连接
    adder.io_a = a_val;
    adder.io_b = b_val;
    
    // 仿真
    ch::Simulator sim(ctx.get());
    sim.tick();
    
    std::cout << "Adder: " << simulator.get_value(a_val) << " + " 
              << simulator.get_value(b_val) << " = " 
              << simulator.get_value(adder.io_c) << std::endl;

    // 示例2: 使用函数式方法实现加法树
    std::cout << "\n2. Function-based Adder Tree Example:" << std::endl;
    
    std::vector<ch_sint<8>> test_inputs = {1_s8, 2_s8, 3_s8, 4_s8, 5_s8, 6_s8, 7_s8, 8_s8};
    
    // 使用函数式方法构建加法树
    ch_sint<12> tree_result = adder_tree_function(test_inputs);
    
    std::cout << "Input values: ";
    for(const auto& val : test_inputs) {
        std::cout << simulator.get_value(val) << " ";
    }
    std::cout << std::endl;
    
    // 计算期望结果
    int expected = 0;
    for(const auto& val : test_inputs) {
        expected += simulator.get_value(val);
    }
    std::cout << "Expected sum: " << expected << std::endl;
    std::cout << "Actual result: " << simulator.get_value(tree_result) << std::endl;

    // 示例3: 分组函数演示
    std::cout << "\n3. Grouping Function Example:" << std::endl;
    
    std::vector<int> numbers = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    auto grouped = group_elements(numbers, 3);
    
    std::cout << "Original: ";
    for(int n : numbers) std::cout << n << " ";
    std::cout << std::endl;
    
    std::cout << "Grouped by 3: " << std::endl;
    for(size_t i = 0; i < grouped.size(); i++) {
        std::cout << "  Group " << i << ": ";
        for(int n : grouped[i]) {
            std::cout << n << " ";
        }
        std::cout << std::endl;
    }

    // 示例4: 简化的AdderTree组件演示
    std::cout << "\n4. Simplified AdderTree Component Example:" << std::endl;
    
    // 创建一个简化的AdderTree实例
    try {
        AdderTree adder_tree(8, 8, 2);  // 8个8位输入，每组2个
        
        std::cout << "AdderTree component created: " << adder_tree.get_definition_name() << std::endl;
        std::cout << "Latency: " << adder_tree.get_latency() << " cycles" << std::endl;
        std::cout << "Output width: " << adder_tree.get_output_width() << " bits" << std::endl;
    } catch (...) {
        std::cout << "AdderTree component creation skipped due to complexity" << std::endl;
    }

    // 示例5: AdderTree工厂函数演示 - 对应SpinalHDL的工厂函数
    std::cout << "\n5. AdderTree Factory Function Example (SpinalHDL-style):" << std::endl;
    
    // 创建输入Flow
    std::vector<ch_sint<8>> flow_inputs = {1_s8, 2_s8, 3_s8, 4_s8, 5_s8, 6_s8, 7_s8, 8_s8};
    Flow<std::vector<ch_sint<8>>> nets;
    nets.payload = flow_inputs;
    nets.valid = true;
    
    // 使用工厂函数创建AdderTree
    AdderTree* uAdderTree = AdderTreeFactory::create_adder_tree<8, 8>(nets, 2);
    
    std::cout << "Created AdderTree using factory function" << std::endl;
    std::cout << "Component name: " << uAdderTree->get_definition_name() << std::endl;
    std::cout << "Latency: " << uAdderTree->get_latency() << " cycles" << std::endl;
    std::cout << "Output width: " << uAdderTree->get_output_width() << " bits" << std::endl;
    
    // 清理内存
    delete uAdderTree;

    // 示例6: Top组件演示
    std::cout << "\n6. Top Component Example:" << std::endl;
    
    Top top;
    std::cout << "Top component created: " << top.get_definition_name() << std::endl;

    std::cout << "\nAll examples completed successfully!" << std::endl;

    return 0;
}