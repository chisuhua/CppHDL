---
name: cpphdl-component-lifecycle
description: "CppHDL component lifecycle: ch_device vs ch_module patterns. Use when writing tests, creating Components, or instantiating modules. Covers ch_device (standalone) vs ch_module (hierarchical), correct usage in tests vs Component::describe(), and troubleshooting SIGSEGV from missing parent context."
---

# CppHDL Component Lifecycle

## When to Use This Skill

- Writing Catch2 tests for CppHDL components
- Instantiating components in main() or test functions
- Creating hierarchical designs with sub-components
- Debugging `"Child component has been destroyed unexpectedly in io()!"` errors
- Debugging SIGSEGV on Simulator tick

## Quick Decision

| Where are you? | Use |
|----------------|-----|
| **Test function** (`TEST_CASE`) | `ch::ch_device<T>` |
| **main()** | `ch::ch_device<T>` |
| **Component::describe()** | `ch::ch_module<T>` |
| **Connecting multiple components** | Wrapper Component + `ch::ch_device<Wrapper>` |

## Pattern 1: ch_device (Standalone)

Use in tests, main(), any top-level context.

```cpp
TEST_CASE("My component test", "[my-comp]") {
    ch::ch_device<MyComponent> comp;
    ch::Simulator sim(comp.context());
    
    sim.set_input_value(comp.instance().io().my_input, 42);
    sim.tick();
    
    auto output = sim.get_port_value(comp.instance().io().my_output);
    REQUIRE(output == expected);
}
```

## Pattern 2: ch_module (Hierarchical)

Use ONLY inside `Component::describe()`.

```cpp
class MyTop : public ch::Component {
public:
    MyTop(ch::Component* p = nullptr, const std::string& n = "top")
        : ch::Component(p, n) {}
    void create_ports() override {}
    
    void describe() override {
        ch::ch_module<SubComponentA> mod_a{"mod_a"};
        ch::ch_module<SubComponentB> mod_b{"mod_b"};
        
        mod_b.io().input <<= mod_a.io().output;
    }
};
```

## Pattern 3: Multi-Component Test with Wrapper

When tests need to connect multiple components:

```cpp
// 1. Define wrapper Component
class TestTop : public ch::Component {
public:
    TestTop(ch::Component* p = nullptr, const std::string& n = "top")
        : ch::Component(p, n) {}
    void create_ports() override {}
    
    void describe() override {
        ch::ch_module<CompA> a{"a"};
        ch::ch_module<CompB> b{"b"};
        b.io().input <<= a.io().output;
    }
};

// 2. Use ch_device to wrap the test
TEST_CASE("Multi-component test", "[multi]") {
    context ctx("test");
    ctx_swap swap(&ctx);
    
    ch::ch_device<TestTop> top;
    ch::Simulator sim(top.context());
    sim.tick();  // Calls top.describe() which creates ch_modules
}
```

## ❌ Common Anti-Patterns

### Anti-Pattern 1: ch_module in test function

```cpp
// WRONG - causes SIGSEGV
TEST_CASE("Bad test", "[bad]") {
    ch::ch_module<HazardUnit> hazard{"hazard"};  // ❌ No parent Component!
    hazard.io().input <<= ch_uint<32>(0_d);       // 💥 SIGSEGV
}
```

**Fix:** Use `ch_device`:
```cpp
ch::ch_device<HazardUnit> hazard;
ch::Simulator sim(hazard.context());
```

### Anti-Pattern 2: Direct IO assignment

```cpp
// WRONG - compiles but does nothing
device.io().my_port = value;
```

**Fix:** Use `sim.set_input_value`:
```cpp
sim.set_input_value(device.instance().io().my_port, value);
```

### Anti-Pattern 3: Missing ctx_swap

```cpp
// WRONG - register operations segfault
context ctx("test");
ch::ch_device<MyComp> comp;
// Missing ctx_swap!
Simulator sim(comp.context());
sim.tick();  // 💥 SIGSEGV
```

**Fix:**
```cpp
context ctx("test");
ctx_swap swap(&ctx);  // ← Required!
```

## Troubleshooting

| Error | Cause | Fix |
|-------|-------|-----|
| `"Child component has been destroyed unexpectedly in io()!"` | `ch_module` used without parent | Change to `ch_device` |
| `"Error: No active parent Component found"` | Same as above | Same as above |
| SIGSEGV on first `io()` after `ch_module` | Constructor failed silently | Use `ch_device` |
| Components don't share context | Separate `ch_device` for each | Use wrapper Component |

## API Reference

### ch_device<T>

```cpp
ch::ch_device<T> device;
device.context();        // → ch::core::context*
device.instance();       // → T&
device.instance().io();  // → T::io_type&
```

### ch::ch_module<T>

```cpp
// Inside Component::describe():
ch::ch_module<T> mod{"name"};
mod.io();        // → T::io_type&
mod.instance();  // → T&
```

### Simulator

```cpp
ch::Simulator sim(context_ptr);
sim.set_input_value(port, value);   // Set input port
sim.tick();                          // Advance one clock cycle
sim.get_port_value(port);            // Read output port (returns sdata_type)
```

## See Also

- Full guide: `docs/COMPONENT-LIFECYCLE-GUIDE.md`
- Source: `include/module.h`, `include/component.h`, `include/device.h`
