// samples/bundle_pod_generation.cpp
#include "../include/ch.hpp"
#include "../include/component.h"
#include "../include/module.h"
#include "../include/simulator.h"
#include "../include/core/bundle/bundle_base.h"
#include "../include/core/bundle/bundle_pod_traits.h"
#include <iostream>
#include <type_traits>

using namespace ch::core;

// 定义一个Bundle类型
struct example_bundle : public bundle_base<example_bundle> {
    ch_uint<32> data;
    ch_uint<16> addr;
    ch_bool     valid;
    ch_bool     ready;

    // 注意：实际实现中需要更复杂的宏来处理字段映射
    static constexpr auto __bundle_fields() {
        return std::make_tuple(
            bundle_field<example_bundle, ch_uint<32>>{"data", &example_bundle::data},
            bundle_field<example_bundle, ch_uint<16>>{"addr", &example_bundle::addr},
            bundle_field<example_bundle, ch_bool>{"valid", &example_bundle::valid},
            bundle_field<example_bundle, ch_bool>{"ready", &example_bundle::ready}
        );
    }

    void as_master() override {
        this->make_output(data, addr, valid);
        this->make_input(ready);
    }

    void as_slave() override {
        this->make_input(data, addr, valid);
        this->make_output(ready);
    }
};

// 手动定义对应的POD类型，用于对比
struct example_pod {
    uint32_t data;
    uint16_t addr;
    bool     valid;
    bool     ready;
};

// 验证POD类型
static_assert(std::is_standard_layout_v<example_pod>);
static_assert(std::is_trivial_v<example_pod>);

// 演示从Bundle类型生成POD类型的概念（伪代码）
using generated_pod = bundle_to_pod_t<example_bundle>;

// 演示从POD类型生成Bundle类型的概念（伪代码）
using generated_bundle = pod_to_bundle_t<example_pod>;

int main() {
    std::cout << "Bundle to POD Generation Demo" << std::endl;
    std::cout << "============================" << std::endl;

    // 显示Bundle信息
    std::cout << "\nBundle Analysis:" << std::endl;
    std::cout << "Bundle width: " << get_bundle_width<example_bundle>() << " bits" << std::endl;
    std::cout << "Bundle field count: " << bundle_field_count_v<example_bundle> << std::endl;

    // 显示手动定义的POD信息
    std::cout << "\nManual POD Analysis:" << std::endl;
    std::cout << "POD size: " << sizeof(example_pod) << " bytes" << std::endl;
    std::cout << "POD is standard layout: " << std::is_standard_layout_v<example_pod> << std::endl;
    std::cout << "POD is trivial: " << std::is_trivial_v<example_pod> << std::endl;

    // 演示从Bundle生成POD类型的思路（概念验证）
    std::cout << "\nConcept Demonstration:" << std::endl;
    std::cout << "In a full implementation, we would be able to generate:" << std::endl;
    std::cout << "- POD type from Bundle definition" << std::endl;
    std::cout << "- Bundle type from POD definition" << std::endl;
    std::cout << "- Automatic serialization/deserialization between them" << std::endl;

    // 显示字段映射关系
    std::cout << "\nField Mapping:" << std::endl;
    std::cout << "Bundle Field\t\tC++ Type\t\tPOD Equivalent" << std::endl;
    std::cout << "data\t\t\tch_uint<32>\t\tuint32_t" << std::endl;
    std::cout << "addr\t\t\tch_uint<16>\t\tuint16_t" << std::endl;
    std::cout << "valid\t\t\tch_bool\t\t\tbool" << std::endl;
    std::cout << "ready\t\t\tch_bool\t\t\tbool" << std::endl;

    std::cout << "\nDemo completed successfully!" << std::endl;
    std::cout << "This demonstrates the concept of generating POD types from Bundle definitions." << std::endl;
    std::cout << "A complete implementation would use advanced template metaprogramming" << std::endl;
    std::cout << "and possibly reflection (C++23) to automatically generate the mappings." << std::endl;

    return 0;
}