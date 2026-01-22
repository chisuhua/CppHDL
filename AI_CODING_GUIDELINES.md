AI_CODING_GUIDELINES.md
# CppHDL AI Coding Guidelines

This document outlines the coding guidelines for the CppHDL project, designed to ensure consistency and maintainability when working with AI-assisted development.

## 1. Bundle System Guidelines

### 1.1 Bundle Definition
- Use `CH_BUNDLE_FIELDS_T(...)` macro for defining bundle fields
- Define bundles as structs inheriting from `bundle_base<Derived>`
- Bundle fields should use `ch_uint<N>` and `ch_bool` types

Example:
```cpp
// Non-template bundle
struct my_bundle : public bundle_base<my_bundle> {
    using Self = my_bundle;
    ch_uint<8> data;
    ch_bool valid;
    
    CH_BUNDLE_FIELDS_T(data, valid)
    
    void as_master_direction() {
        this->make_output(data, valid);
    }
    
    void as_slave_direction() {
        this->make_input(data, valid);
    }
};

// Template bundle
template<typename T>
struct my_template_bundle : public bundle_base<my_template_bundle<T>> {
    using Self = my_template_bundle<T>;
    T data;
    ch_bool valid;
    
    CH_BUNDLE_FIELDS_T(data, valid)
    
    void as_master_direction() {
        this->make_output(data, valid);
    }
    
    void as_slave_direction() {
        this->make_input(data, valid);
    }
};
```

### 1.2 Bundle Direction Control
- Implement `as_master_direction()` and `as_slave_direction()` methods instead of `as_master()` and `as_slave()`
- Use `make_output()` and `make_input()` helper methods to set field directions
- Do not use `override` keyword when implementing these methods
- The parent `bundle_base` class will handle the direction setting via `as_master()` and `as_slave()` methods

Example:
```cpp
struct TestBundle : public bundle_base<TestBundle> {
    using Self = TestBundle;
    ch_uint<8> data;
    ch_bool valid;

    CH_BUNDLE_FIELDS_T(data, valid)

    void as_master_direction() {
        this->make_output(data, valid);
    }

    void as_slave_direction() {
        this->make_input(data, valid);
    }
};
```

## 2. Template Guidelines

### 2.1 Template Parameter Naming
- Use descriptive names for template parameters
- Use `typename` consistently over `class` for template parameters

## 3. Memory Management Guidelines

### 3.1 Smart Pointer Usage
- Use `std::unique_ptr` for exclusive ownership
- Use `std::shared_ptr` for shared ownership with reference counting
- Avoid raw pointers for object ownership

## 4. Error Handling Guidelines

### 4.1 Exception Usage
- Use exceptions for error conditions that prevent normal execution
- Create specific exception types for domain-specific errors
- Document exception behavior in function comments

## 5. Naming Conventions

### 5.1 Class Names
- Use PascalCase for class and struct names
- Use descriptive names that clearly indicate the purpose

### 5.2 Function Names
- Use camelCase for function names
- Use descriptive names that clearly indicate the function's purpose

### 5.3 Variable Names
- Use camelCase for variable names
- Use descriptive names that clearly indicate the variable's purpose

## 6. Documentation Guidelines

### 6.1 Comments
- Use clear, concise comments that explain the "why" not just the "what"
- Document non-obvious design decisions
- Update comments when code is modified

## 7. Testing Guidelines

### 7.1 Unit Tests
- Write comprehensive unit tests for all functionality
- Use descriptive test names that clearly indicate what is being tested
- Test both normal operation and edge cases
- Use RAII for resource management in tests

Example test:
```cpp
// tests/test_example.cpp
#define CATCH_CONFIG_MAIN
#include "catch_amalgamated.hpp"
#include "ch.hpp"
#include "core/bundle/bundle_base.h"
#include "core/bundle/bundle_meta.h"
#include "core/context.h"
#include <memory>

using namespace ch;
using namespace ch::core;

// 定义一个简单的Bundle用于测试
struct TestBundle : public bundle_base<TestBundle> {
    using Self = TestBundle;
    ch_uint<8> data;
    ch_bool valid;

    TestBundle() = default;

    CH_BUNDLE_FIELDS_T(data, valid)

    void as_master_direction() {
        this->make_output(data, valid);
    }

    void as_slave_direction() {
        this->make_input(data, valid);
    }
};

TEST_CASE("Example - Basic Bundle Creation", "[bundle][example]") {
    auto ctx = std::make_unique<ch::core::context>("test_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());

    TestBundle bundle;
    bundle.data = 0x55_d;
    bundle.valid = true;

    // 设置方向
    bundle.as_master();
    REQUIRE(bundle.get_role() == bundle_role::master);

    bundle.as_slave();
    REQUIRE(bundle.get_role() == bundle_role::slave);

    REQUIRE(bundle.is_valid());
}
```

## 8. Bundle Connection Guidelines

### 8.1 Bundle Connection Pattern
- Use `<<=` operator for connecting bundles
- Ensure bundles have correct roles (master/slave) before connection
- Check bundle validity after connection

Example:
```cpp
// 创建bundle实例
TestBundle master, slave;

// 设置方向
master.as_master();
slave.as_slave();

// 连接bundle
slave <<= master;

// 验证连接
REQUIRE(master.is_valid());
REQUIRE(slave.is_valid());
```

## 9. Bundle Field Type Guidelines

### 9.1 Supported Types
- Use `ch_uint<N>` for unsigned integer fields
- Use `ch_bool` for boolean fields
- Bundle fields can contain other bundles (nested bundles)

### 9.2 Direction Control for Different Types
- For basic types (`ch_uint<N>`, `ch_bool`), implement `as_input()` and `as_output()` methods
- For bundle types, the direction control is handled through the bundle's own `as_master_direction()` and `as_slave_direction()` methods

## 10. Bundle Role Management

### 10.1 Role Types
- `bundle_role::unknown`: Internal connections, can be both input and output
- `bundle_role::master`: Output signals to other bundles
- `bundle_role::slave`: Input signals from other bundles

### 10.2 Role Setting
- Use `as_master()` and `as_slave()` methods to set roles
- These methods internally call the corresponding `_direction` methods
- Check role using `get_role()` method

These guidelines are intended to be used in conjunction with AI-assisted development tools to ensure consistent and maintainable code in the CppHDL project.
