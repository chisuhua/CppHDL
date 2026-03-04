# StreamBuilder Implementation Learnings

## Summary
Implemented a fluent API builder `StreamBuilder<T>` for chainable stream operations in CppHDL.

## Files Created
- `include/chlib/stream_builder.h` - Header file with StreamBuilder class
- `tests/test_stream_builder.cpp` - Test file

## Design Decisions

### Template vs Runtime Parameters
- `queue(depth)` requires compile-time constant depth, so implemented as `queue<DEPTH>()` template method
- Other methods like `haltWhen`, `map`, `throwWhen`, `takeWhen` accept runtime parameters

### Method Chaining
- All methods return `StreamBuilder<T>&` for fluent chaining
- `build()` returns the final `ch_stream<T>`

### Generic Callbacks
- Used template parameters for `map(Func)` and `filter(Pred)` instead of `std::function` to support lambdas directly

## Key Learnings

1. **Hardware Description Constraints**: Unlike software, many stream operations require valid hardware signals. Empty `ch_stream` objects created without proper context can cause runtime errors.

2. **Template Parameters**: FIFO depth must be compile-time, so using template method `queue<DEPTH>()` is appropriate.

3. **Stream API Patterns**: The existing stream functions (`stream_fifo`, `stream_halt_when`, etc.) return `ch_stream<T>` making them perfect for chaining.

4. **Test Limitations**: Some operations (like queue) require fully initialized hardware context and can't be tested with simple unit tests. The tests verify compilation and basic API structure.

## Operations Supported
- `.queue<DEPTH>()` - Add FIFO buffer
- `.haltWhen(ch_bool)` - Pause stream
- `.continueWhen(ch_bool)` - Continue stream
- `.map(func)` - Transform payload
- `.throwWhen(ch_bool)` - Drop data
- `.takeWhen(ch_bool)` - Pass data conditionally
- `.build()` - Finalize and return stream

## Notes
- Filter operation was initially planned but removed because `stream_take_when` requires `ch_bool` (hardware signal), not a runtime lambda predicate. This is a fundamental hardware vs software difference.
- The helper function `make_stream_builder(source)` provides a convenient way to create builders.
