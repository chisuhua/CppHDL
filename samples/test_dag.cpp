#include "ch.hpp"
#include "codegen_dag.h"
#include <iostream>

int main() {
    // 创建一个空的上下文并生成空的DAG图
    ch::core::context ctx;
    ch::toDAG("empty_test.dot", &ctx);
    std::cout << "Generated empty_test.dot" << std::endl;

    return 0;
}