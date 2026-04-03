// // // tests/test_memory_leak.cpp
// // // Memory leak detection tests using AddressSanitizer
// // 
// // #include <catch2/catch_test_macros.hpp>
// // #include <memory>
// // #include "component.h"
// // #include "core/context.h"
// // 
// // using namespace ch;
// // 
// // // Test 1: Component creation and destruction without leaks
// // TEST_CASE("Component lifecycle - no memory leaks", "[memory][component]") {
// //     // Create and destroy a single component
// //     {
// //         auto comp = std::make_shared<TestComponent>(nullptr, "test");
// //         REQUIRE(comp != nullptr);
// //     }
// //     // Component should be fully destroyed here with no leaks
// // }
// // 
// // Test 2: Parent-child relationship cleanup
// TEST_CASE("Parent-child cleanup - no circular reference leaks", "[memory][component]") {
//     {
//         auto parent = std::make_shared<TestComponent>(nullptr, "parent");
//         auto child = parent->create_child<TestComponent>("child");
//         
//         REQUIRE(parent != nullptr);
//         REQUIRE(child != nullptr);
//         REQUIRE(child->parent() == parent);
//     }
//     // Both parent and child should be destroyed without leaks
// }
// 
// // Test 3: Deep hierarchy cleanup
// TEST_CASE("Deep hierarchy cleanup - no memory leaks", "[memory][component]") {
//     {
//         auto root = std::make_shared<TestComponent>(nullptr, "root");
//         auto level1 = root->create_child<TestComponent>("level1");
//         auto level2 = level1->create_child<TestComponent>("level2");
//         auto level3 = level2->create_child<TestComponent>("level3");
//         
//         REQUIRE(root->child_count() == 1);
//         REQUIRE(level1->child_count() == 1);
//         REQUIRE(level2->child_count() == 1);
//     }
//     // All components should be destroyed without leaks
// }
// 
// // Test 4: Context creation and destruction
// TEST_CASE("Context lifecycle - no memory leaks", "[memory][context]") {
//     {
//         auto ctx = std::make_unique<ch::core::context>();
//         REQUIRE(ctx != nullptr);
//     }
//     // Context should be fully destroyed here with no leaks
// }
// 
// // Test 5: Component with context ownership
// TEST_CASE("Component with context ownership - no memory leaks", "[memory][component][context]") {
//     {
//         auto comp = std::make_shared<TestComponent>(nullptr, "test");
//         comp->build();  // This creates and owns a context
//         REQUIRE(comp->context() != nullptr);
//     }
//     // Component and its context should be destroyed without leaks
// }
// 
// // Test helper class
// class TestComponent : public Component {
// public:
//     TestComponent(std::shared_ptr<Component> parent, const std::string& name)
//         : Component(parent, name) {}
//     
//     void describe() override {
//         // Minimal implementation for testing
//     }
// };
