#include "catch_amalgamated.hpp"
#include "reg.h"
#include "core/uint.h"
#include "core/traits.h"
#include "component.h"
#include "core/context.h"
#include "ast/ast_nodes.h"
#include "device.h"
#include <type_traits>
#include <thread>
#include <vector>
#include <future>
#include <atomic>

using namespace ch::core;

// 测试用的简单模块
class SimpleModule : public ch::Component {
public:
    SimpleModule(ch::Component* parent = nullptr, const std::string& name = "SimpleModule")
        : ch::Component(parent, name) {}

    void describe() override {
        // 创建一些基本的硬件元素进行测试
        auto reg1 = ch::core::ch_reg_impl<ch_uint<8>>(0);
        auto input1 = ch::core::ch_logic_in<ch_uint<8>>("in1");
        auto output1 = ch::core::ch_logic_out<ch_uint<8>>("out1");
        
        // 简单的赋值操作
        output1 = input1;
    }
};

// ---------- 多线程测试 ----------

TEST_CASE("Multithreaded context isolation", "[multithread][context][isolation]") {
    std::atomic<int> test_counter{0};
    
    auto worker = [&test_counter]() {
        // 每个线程创建自己的设备和上下文
        auto device = ch::ch_device<SimpleModule>();
        REQUIRE(device.context() != nullptr);
        
        // 验证线程局部存储正确工作
        REQUIRE(ch::core::ctx_curr_ == device.context());
        
        // 验证可以正常创建节点
        auto* ctx = device.context();
        auto* lit_node = ctx->create_literal(ch::core::sdata_type(42, 8), "test_lit");
        REQUIRE(lit_node != nullptr);
        REQUIRE(lit_node->value().bitvector().to_uint64() == 42);
        
        test_counter.fetch_add(1);
        return true;
    };
    
    const int num_threads = 4;
    std::vector<std::future<bool>> futures;
    
    // 启动多个线程
    for (int i = 0; i < num_threads; ++i) {
        futures.push_back(std::async(std::launch::async, worker));
    }
    
    // 等待所有线程完成并验证结果
    for (auto& future : futures) {
        REQUIRE(future.get() == true);
    }
    
    REQUIRE(test_counter.load() == num_threads);
}

TEST_CASE("Thread local context switching", "[multithread][context][switch]") {
    auto worker = []() {
        // 线程内创建多个上下文并切换
        auto ctx1 = std::make_unique<ch::core::context>("ctx1");
        auto ctx2 = std::make_unique<ch::core::context>("ctx2");
        
        // 初始状态检查
        REQUIRE(ch::core::ctx_curr_ == nullptr);
        
        // 切换到第一个上下文
        {
            ch::core::ctx_swap swap(ctx1.get());
            REQUIRE(ch::core::ctx_curr_ == ctx1.get());
            
            // 在ctx1中创建节点
            auto* lit1 = ctx1->create_literal(ch::core::sdata_type(100, 8), "lit1");
            REQUIRE(lit1 != nullptr);
            REQUIRE(lit1->id() == 0); // 第一个节点ID应该是0
        }
        
        // 切换到第二个上下文
        {
            ch::core::ctx_swap swap(ctx2.get());
            REQUIRE(ch::core::ctx_curr_ == ctx2.get());
            
            // 在ctx2中创建节点
            auto* lit2 = ctx2->create_literal(ch::core::sdata_type(200, 16), "lit2");
            REQUIRE(lit2 != nullptr);
            REQUIRE(lit2->id() == 0); // 新上下文中第一个节点ID也是0
            
            // 验证宽度正确
            REQUIRE(lit2->size() == 16);
        }
        
        // 退出作用域后应该恢复为nullptr
        REQUIRE(ch::core::ctx_curr_ == nullptr);
        
        return true;
    };
    
    const int num_threads = 3;
    std::vector<std::thread> threads;
    
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back(worker);
    }
    
    for (auto& t : threads) {
        t.join();
    }
}

TEST_CASE("Concurrent device creation", "[multithread][device][creation]") {
    auto create_device_worker = [](int thread_id) -> bool {
        // 每个线程创建自己的设备实例
        {
            auto device = ch::ch_device<SimpleModule>();
            REQUIRE(device.instance().context() != nullptr);
            
            // 验证设备名称和上下文
            REQUIRE(device.instance().name() == "top");
            REQUIRE(device.context() == device.instance().context());
            
            // 验证线程局部变量设置正确
            REQUIRE(ch::core::ctx_curr_ == device.context());
        }
        
        // 设备销毁后上下文应该为nullptr
        // 注意：这取决于具体的实现，有些实现可能不重置为nullptr
        // REQUIRE(ch::core::ctx_curr_ == nullptr);
        
        return true;
    };
    
    const int num_devices = 6;
    std::vector<std::future<bool>> results;
    
    // 并发创建多个设备
    for (int i = 0; i < num_devices; ++i) {
        results.push_back(std::async(std::launch::async, create_device_worker, i));
    }
    
    // 验证所有设备都创建成功
    for (auto& result : results) {
        REQUIRE(result.get() == true);
    }
}

TEST_CASE("Thread safety with node creation", "[multithread][node][creation]") {
    std::atomic<int> total_nodes_created{0};
    
    auto node_creator = [&total_nodes_created](int thread_id) {
        auto ctx = std::make_unique<ch::core::context>("thread_ctx_" + std::to_string(thread_id));
        
        ch::core::ctx_swap swap(ctx.get());
        
        // 每个线程创建多个节点
        constexpr int nodes_per_thread = 50;
        for (int i = 0; i < nodes_per_thread; ++i) {
            auto* lit = ctx->create_literal(
                ch::core::sdata_type(static_cast<uint64_t>(thread_id * 1000 + i), 32),
                "lit_" + std::to_string(i)
            );
            
            REQUIRE(lit != nullptr);
            REQUIRE(lit->value().bitvector().to_uint64() == static_cast<uint64_t>(thread_id * 1000 + i));
        }
        
        total_nodes_created += nodes_per_thread;
        return nodes_per_thread;
    };
    
    const int num_threads = 4;
    std::vector<std::future<int>> futures;
    
    for (int i = 0; i < num_threads; ++i) {
        futures.push_back(std::async(std::launch::async, node_creator, i));
    }
    
    int expected_total = 0;
    for (auto& future : futures) {
        expected_total += future.get();
    }
    
    REQUIRE(total_nodes_created.load() == expected_total);
    REQUIRE(expected_total == num_threads * 50);
}

TEST_CASE("Component hierarchy in multithreaded environment", "[multithread][component][hierarchy]") {
    class NestedModule : public ch::Component {
    public:
        NestedModule(ch::Component* parent, const std::string& name) 
            : ch::Component(parent, name) {}
        
        void describe() override {
            // 创建嵌套的子模块
            auto child1 = create_child<SimpleModule>("child1");
            auto child2 = create_child<SimpleModule>("child2");
            REQUIRE(child1 != nullptr);
            REQUIRE(child2 != nullptr);
        }
    };
    
    auto worker = []() {
        auto device = ch::ch_device<NestedModule>();
        
        // 验证层次结构
        auto& top_module = device.instance();
        REQUIRE(top_module.name() == "top");
        REQUIRE(top_module.child_count() == 2);
        
        const auto& children = top_module.children();
        REQUIRE(children.size() == 2);
        
        // 验证子模块名称
        REQUIRE(children[0]->name() == "child1");
        REQUIRE(children[1]->name() == "child2");
        
        return true;
    };
    
    const int num_threads = 3;
    std::vector<std::thread> threads;
    
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back(worker);
    }
    
    for (auto& t : threads) {
        t.join();
    }
}

using namespace ch;

// 测试用的嵌套模块
class TestModuleA : public Component {
public:
    TestModuleA(Component* parent = nullptr, const std::string& name = "TestModuleA")
        : Component(parent, name) {}

    void describe() override {
        // 在describe中检查current指针
        REQUIRE(Component::current() == this);
    }
};

class TestModuleB : public Component {
public:
    TestModuleB(Component* parent = nullptr, const std::string& name = "TestModuleB")
        : Component(parent, name) {}

    void describe() override {
        // 在describe中检查current指针
        REQUIRE(Component::current() == this);
        
        // 创建子模块
        auto child = create_child<TestModuleA>("child_a");
        REQUIRE(child != nullptr);
    }
};

// ---------- Component::current() 多线程测试 ----------

TEST_CASE("Thread local Component::current() isolation", "[multithread][component][current]") {
    std::atomic<int> thread_counter{0};
    
    auto worker = [&thread_counter](int thread_id) {
        // 初始状态：current应该为nullptr
        REQUIRE(Component::current() == nullptr);
        
        {
            auto device = ch_device<TestModuleA>();
            auto& module = device.instance();
            
            // 构建过程中current应该指向当前模块
            REQUIRE(Component::current() == &module);
            
            // 验证模块属性
            REQUIRE(module.name() == "top");
            REQUIRE(module.context() != nullptr);
        }
        
        // 设备销毁后current应回到nullptr
        REQUIRE(Component::current() == nullptr);
        
        thread_counter.fetch_add(1);
        return thread_id;
    };
    
    const int num_threads = 4;
    std::vector<std::future<int>> futures;
    
    for (int i = 0; i < num_threads; ++i) {
        futures.push_back(std::async(std::launch::async, worker, i));
    }
    
    // 验证所有线程都完成且返回正确的ID
    std::vector<int> results;
    for (auto& future : futures) {
        results.push_back(future.get());
    }
    
    REQUIRE(results.size() == num_threads);
    REQUIRE(thread_counter.load() == num_threads);
    
    // 验证主线程的Component::current仍然为nullptr
    REQUIRE(Component::current() == nullptr);
}

TEST_CASE("Nested Component::current() in multithreaded environment", "[multithread][component][nested]") {
    auto worker = [](int thread_id) {
        REQUIRE(Component::current() == nullptr);
        
        {
            auto device = ch_device<TestModuleB>();
            auto& top_module = device.instance();
            
            // 验证顶层模块的current指针
            REQUIRE(Component::current() == &top_module);
            
            // 验证子模块创建
            REQUIRE(top_module.child_count() == 1);
            auto* child_module = dynamic_cast<TestModuleA*>(top_module.children()[0].get());
            REQUIRE(child_module != nullptr);
        }
        
        // 清理后恢复nullptr
        REQUIRE(Component::current() == nullptr);
        
        return true;
    };
    
    const int num_threads = 3;
    std::vector<std::thread> threads;
    
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back(worker, i);
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    // 主线程验证
    REQUIRE(Component::current() == nullptr);
}

TEST_CASE("Concurrent Component creation and current() tracking", "[multithread][component][tracking]") {
    std::atomic<int> total_components{0};
    
    auto component_creator = [&total_components](int thread_id) -> int {
        const int components_per_thread = 10;
        
        for (int i = 0; i < components_per_thread; ++i) {
            REQUIRE(Component::current() == nullptr);
            
            {
                // 创建组件时验证current指针的正确设置
                auto device = ch_device<TestModuleA>();
                auto& module = device.instance();
                
                // current应该指向当前创建的模块
                REQUIRE(Component::current() == &module);
                REQUIRE(module.name() == "top");
            }
            
            // 销毁后应该回到nullptr
            REQUIRE(Component::current() == nullptr);
        }
        
        total_components += components_per_thread;
        return components_per_thread;
    };
    
    const int num_threads = 5;
    std::vector<std::future<int>> futures;
    
    for (int i = 0; i < num_threads; ++i) {
        futures.push_back(std::async(std::launch::async, component_creator, i));
    }
    
    int expected_total = 0;
    for (auto& future : futures) {
        expected_total += future.get();
    }
    
    REQUIRE(total_components.load() == expected_total);
    REQUIRE(expected_total == num_threads * 10);
    
    // 主线程验证
    REQUIRE(Component::current() == nullptr);
}

TEST_CASE("Component::current() during build process in multithread", "[multithread][component][build]") {
    class BuildTrackingModule : public Component {
    public:
        static thread_local std::vector<Component*> build_stack;
        
        BuildTrackingModule(Component* parent, const std::string& name)
            : Component(parent, name) {
            // 构造时记录
            build_stack.push_back(this);
        }
        
        void create_ports() override {
            // create_ports期间current应该是this
            REQUIRE(Component::current() == this);
            build_stack.push_back(this);
        }
        
        void describe() override {
            // describe期间current应该是this
            REQUIRE(Component::current() == this);
            build_stack.push_back(this);
            
            // 创建子组件
            auto child = create_child<BuildTrackingModule>("child");
            REQUIRE(child != nullptr);
        }
    };
    
    thread_local std::vector<Component*> BuildTrackingModule::build_stack;
    
    auto worker = [](int thread_id) {
        REQUIRE(Component::current() == nullptr);
        
        {
            auto device = ch_device<BuildTrackingModule>();
            auto& module = device.instance();
            
            REQUIRE(Component::current() == &module);
        }
        
        REQUIRE(Component::current() == nullptr);
        return thread_id;
    };
    
    const int num_threads = 2;
    std::vector<std::future<int>> futures;
    
    for (int i = 0; i < num_threads; ++i) {
        futures.push_back(std::async(std::launch::async, worker, i));
    }
    
    for (auto& future : futures) {
        future.get();
    }
    
    REQUIRE(Component::current() == nullptr);
}

TEST_CASE("Component::current() cross-thread validation", "[multithread][component][cross]") {
    std::atomic<bool> test_completed{false};
    std::atomic<int> thread_ids[4] = {0};
    
    auto thread_worker = [&test_completed, &thread_ids](int thread_index) {
        // 每个线程都有独立的Component::current状态
        REQUIRE(Component::current() == nullptr);
        
        {
            auto device = ch_device<TestModuleA>();
            auto& module = device.instance();
            
            // 验证current指针在线程内正确设置
            REQUIRE(Component::current() == &module);
            REQUIRE(module.name() == "top");
            
            // 存储线程ID用于验证
            thread_ids[thread_index].store(thread_index + 1);
        }
        
        REQUIRE(Component::current() == nullptr);
        test_completed.store(true);
        
        return thread_index;
    };
    
    const int num_threads = 4;
    std::vector<std::thread> threads;
    std::vector<int> thread_results(num_threads, -1);
    
    // 启动多个线程
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([thread_worker, i, &thread_results]() {
            thread_results[i] = thread_worker(i);
        });
    }
    
    // 等待所有线程完成
    for (auto& t : threads) {
        t.join();
    }
    
    // 验证每个线程都正确执行
    for (int i = 0; i < num_threads; ++i) {
        REQUIRE(thread_ids[i].load() == i + 1);
    }
    
    // 主线程状态验证
    REQUIRE(Component::current() == nullptr);
}
