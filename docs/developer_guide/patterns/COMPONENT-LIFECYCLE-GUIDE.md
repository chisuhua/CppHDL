# Component Lifecycle Guide

> Understanding `ch_device<T>` vs `ch::ch_module<T>` in CppHDL

## Overview

CppHDL provides two mechanisms for instantiating Components, each designed for a specific context. Using the wrong one causes **silent failure** followed by **SIGSEGV** at runtime.

## Quick Reference

| Pattern | Context | Parent Required? | Lifecycle |
|---------|---------|------------------|-----------|
| `ch::ch_device<T>` | Standalone tests, main() | No | Creates own context |
| `ch::ch_module<T>` | Inside `Component::describe()` | Yes | Managed by parent |

## ch_device: Standalone Mode

Use `ch_device<T>` when the component exists at the top level with no parent.

### How it Works

```cpp
ch::ch_device<HazardUnit> hazard;
```

Internally:
1. Creates a `ch::core::context` automatically
2. Constructs the component as the root of the hierarchy
3. Calls `build()` within its own context
4. Stores the component via `std::shared_ptr`

### Correct Usage in Tests

```cpp
TEST_CASE("HazardUnit: Forwarding", "[hazard]") {
    ch::ch_device<HazardUnit> hazard;  // ✅ Root-level, no parent needed
    ch::Simulator sim(hazard.context());
    
    sim.set_input_value(hazard.instance().io().rs1_data_raw, 100);
    sim.tick();
    
    auto rs1_data = sim.get_port_value(hazard.instance().io().rs1_data);
    REQUIRE(rs1_data == 100);
}
```

### Key Points

- Access the component via `.instance()`
- Access the context via `.context()` (returns `context*`, pass to Simulator)
- No hierarchical naming — component name is the instance name
- Can be instantiated anywhere: tests, main(), lambdas

## ch_module: Hierarchical Mode

Use `ch::ch_module<T>` **only** inside a `Component::describe()` method.

### How it Works

```cpp
class MyTop : public ch::Component {
    void describe() override {
        ch::ch_module<HazardUnit> hazard{"hazard"};
    }
};
```

Internally:
1. Calls `Component::current()` to find the parent component
2. If no parent found → **logs error and returns silently** (no exception!)
3. Creates a hierarchical name: `parent_name.child_name`
4. Uses `parent->add_child()` to transfer ownership
5. Builds the child within the **parent's context**
6. Stores the child via `std::shared_ptr` (owned by parent)

### Correct Usage in Components

```cpp
class Rv32iSoc : public ch::Component {
public:
    void describe() override {
        // ✅ All ch_module calls happen inside describe()
        ch::ch_module<Rv32iPipeline> pipeline{"pipeline"};
        ch::ch_module<AddressDecoder> addr_dec{"addr_dec"};
        ch::ch_module<InstrTCM<20, 32>> itcm{"itcm"};
        ch::ch_module<DataTCM<20, 32>> dtcm{"dtcm"};
        
        // Connect modules
        itcm.io().addr <<= pipeline.io().instr_addr;
        pipeline.io().instr_data <<= itcm.io().data;
    }
};
```

### ❌ Common Mistake: ch_module in Tests

```cpp
// WRONG: No parent Component exists in a test function
TEST_CASE("Wrong pattern", "[wrong]") {
    ch::ch_module<HazardUnit> hazard{"hazard"};  // ❌ Component::current() == nullptr
    
    hazard.io().rs1_data_raw <<= ch_uint<32>(0_d);  // SIGSEGV!
}
```

**Why it crashes:**
1. `Component::current()` returns `nullptr`
2. Constructor logs error and returns without creating the component
3. `child_component_sptr_` is empty (null)
4. Calling `hazard.io()` dereferences null → SIGSEGV
5. Error message: `"Child component has been destroyed unexpectedly in io()!"`

### Correct: ch_module in Tests with Parent Wrapper

```cpp
class TestTop : public ch::Component {
public:
    TestTop(ch::Component* parent = nullptr, const std::string& name = "top")
        : ch::Component(parent, name) {}
    void create_ports() override {}
    void describe() override {
        // ✅ Now Component::current() returns TestTop
        ch::ch_module<HazardUnit> hazard{"hazard"};
        hazard.io().rs1_data_raw <<= ch_uint<32>(0_d);
    }
};

TEST_CASE("Correct with wrapper", "[correct]") {
    context ctx("test");
    ctx_swap swap(&ctx);
    
    ch::ch_device<TestTop> top;  // Root device
    ch::Simulator sim(top.context());
    sim.tick();  // Top.describe() creates ch_module safely
}
```

## Architecture: Why Two Patterns?

### ch_device = Root Component

```
ch::ch_device<MySoc> soc;
└── soc (root, owns its context)
    ├── soc.pipeline
    ├── soc.addr_dec
    └── soc.itcm
```

### ch_module = Child Component

```
class MySoc : public ch::Component {
    void describe() {
        ch::ch_module<Pipeline> pipeline{"pipeline"};
    }
};
// Results in:
ch::ch_device<MySoc> soc;
└── soc (root)
    └── soc.pipeline (child, shares parent's context)
```

## Module Implementation Details

### Ownership Model

```cpp
// include/module.h
template <typename T>
class ch_module {
private:
    std::shared_ptr<T> child_component_sptr_;  // ← shared_ptr, NOT weak_ptr
};
```

The parent Component owns all children via `std::vector<std::shared_ptr<Component>>`. When the parent is destroyed, all children are destroyed too.

### ch_module Constructor Flow

```
ch::ch_module<HazardUnit> hazard{"hazard"};
│
├─ 1. Component::current() → parent (must be non-null!)
├─ 2. parent->context() → parent_context
├─ 3. build_hierarchical_name(parent, "hazard") → "parent.hazard"
├─ 4. std::make_unique<HazardUnit>(parent, "parent.hazard")
├─ 5. parent->add_child(std::move(unique_ptr)) → shared_ptr
├─ 6. child->build(parent_context)  ← uses parent's context, not its own
└─ 7. child_component_sptr_ = shared_ptr
```

## Decision Tree

```
Need to instantiate a Component?
│
├─ Is it in a test function or main()?
│   └─ YES → Use ch::ch_device<T>
│
├─ Is it inside Component::describe()?
│   └─ YES → Use ch::ch_module<T>
│
└─ Is it a top-level design in main()?
    └─ YES → Use ch::ch_device<T>
```

## Migration Examples

### Before (WRONG)

```cpp
TEST_CASE("Pipeline test", "[pipeline]") {
    context ctx("test");
    ctx_swap swap(&ctx);
    
    ch::ch_module<Rv32iPipeline> pipeline{"pipeline"};  // ❌ No parent
    ch::ch_module<InstrTCM<20, 32>> itcm{"itcm"};       // ❌ No parent
    
    // Wire them together
    itcm.io().addr <<= pipeline.io().instr_addr;
    // ...
    
    Simulator sim(&ctx);
    sim.tick();  // SIGSEGV
}
```

### After (CORRECT)

```cpp
// Option A: Use ch_device for each component
TEST_CASE("Pipeline test", "[pipeline]") {
    ch::ch_device<Rv32iPipeline> pipeline;
    ch::ch_device<InstrTCM<20, 32>> itcm;
    
    // Each has its own context - cannot connect IO ports directly
    // This only works for testing individual components
    ch::Simulator sim(pipeline.context());
    // ...
}

// Option B: Use a wrapper Component for multi-component tests
class PipelineTestTop : public ch::Component {
public:
    PipelineTestTop(ch::Component* p = nullptr, const std::string& n = "top")
        : ch::Component(p, n) {}
    void create_ports() override {}
    void describe() override {
        // ✅ ch_module works here because we have a parent (PipelineTestTop)
        ch::ch_module<Rv32iPipeline> pipeline{"pipeline"};
        ch::ch_module<InstrTCM<20, 32>> itcm{"itcm"};
        
        itcm.io().addr <<= pipeline.io().instr_addr;
        pipeline.io().instr_data <<= itcm.io().data;
    }
};

TEST_CASE("Pipeline test with wrapper", "[pipeline]") {
    context ctx("test");
    ctx_swap swap(&ctx);
    
    ch::ch_device<PipelineTestTop> top;
    ch::Simulator sim(top.context());
    sim.tick();  // Works!
}
```

## Troubleshooting

| Symptom | Cause | Fix |
|---------|-------|-----|
| `"Child component has been destroyed unexpectedly in io()!"` | `ch_module` used without parent | Change to `ch_device` or wrap in Component |
| `"Error: No active parent Component found when creating ch_module!"` | Same as above | Same as above |
| SIGSEGV on first `io()` call after `ch_module` | Constructor failed silently | Check logs for parent error message |
| Components don't share context | Used separate `ch_device` for each | Use wrapper Component with `ch_module` |

## Related Files

- `include/module.h` — `ch_module` and `ch_device` implementation
- `include/component.h` — `Component` base class, `add_child()`, `current()`
- `include/core/context.h` — Context management
- `include/device.h` — `ch_device` wrapper
