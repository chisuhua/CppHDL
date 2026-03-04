# Draft: SpinalHDL-like Stream Operator Implementation

## Requirements (from comparison document)

**Missing Features to Implement (excluding cross-clock domain):**

1. **Connection Operators** (8 operators):
   - Direct connection: `<<`, `>>` (combinational, 0-cycle delay)
   - Master-to-slave pipeline: `<-<`, `>->` (m2sPipe, 1-cycle delay on valid/payload)
   - Slave-to-master pipeline: `</<`, `>/>` (s2mPipe, 0-cycle data delay, register on ready)
   - Full pipeline: `<-/<`, `>/->` (m2sPipe + s2mPipe, 1-cycle delay all signals)

2. **Pipeline Methods** (member functions):
   - `.m2sPipe()` - returns stream with master-to-slave pipeline
   - `.stage()` - alias for `.m2sPipe()`
   - `.s2mPipe()` - returns stream with slave-to-master pipeline
   - `.halfPipe()` - half-bandwidth pipeline (all signals registered, bandwidth halved)

3. **Additional Features**:
   - More arbitration strategies (lock, sequence, etc.)
   - Chainable fluent API (`.queue(4).haltWhen(busy).map(transform)`)
   - Width adapters (but cross-clock domain excluded per user request)
   - `.filter()` method (could be via `takeWhen`)

## Technical Decisions Needed

### 1. Operator Overloading Approach
**Options:**
- Overload `operator<<` for connection (but `<<` is bit-shift in C++)
- Use `operator=` with special proxy objects?
- Create new operators? (C++ doesn't allow new operators)
- Use named functions like `connect(source, sink)`?

**Recommendation:** Overload `operator<<=` (compound assignment) or `operator>>=`:
```cpp
sink <<= source;  // Direct connection
sink <<= m2sPipe(source);  // Pipeline connection
```

### 2. Pipeline Implementation
**Options:**
- Return new stream objects (value semantics)
- Modify in-place (reference semantics)
- Use intermediate proxy objects

**Recommendation:** Return new stream objects (like existing `stream_fifo`, `stream_halt_when`).

### 3. Fluent API Design
**Options:**
- Method chaining on `ch_stream` object
- Free functions that take stream and return stream
- Builder pattern

**Recommendation:** Method chaining with intermediate pipeline objects.

## Current Architecture Analysis

### `ch_stream<T>` (from stream_bundle.h)
- Bundle with `payload`, `valid`, `ready`
- Methods: `fire()`, `isStall()`
- Direction control: `as_master_direction()`, `as_slave_direction()`

### Existing Stream Functions (from stream.h)
- `stream_fifo<T, DEPTH>(input)` -> returns `StreamFifoResult`
- `stream_halt_when<T>(input, condition)` -> returns `ch_stream<T>`
- `stream_throw_when`, `stream_take_when`, `stream_continue_when`
- `stream_fork`, `stream_join`, `stream_arbiter_*`, `stream_mux`, `stream_demux`
- `stream_translate_with`, `stream_map`, `stream_transpose`
- `stream_combine_with`

## Implementation Strategy

### Phase 1: Core Pipeline Primitives
1. Implement `stream_m2s_pipe<T>(input)` function
2. Implement `stream_s2m_pipe<T>(input)` function  
3. Implement `stream_half_pipe<T>(input)` function
4. Add corresponding member functions: `.m2sPipe()`, `.s2mPipe()`, `.halfPipe()`

### Phase 2: Operator Overloading
1. Define connection operators (`<<=`, `>>=`, etc.)
2. Implement operator overloads for different pipeline types
3. Ensure type safety and proper signal connections

### Phase 3: Fluent API
1. Design pipeline builder/chainable interface
2. Implement `.queue(depth)`, `.haltWhen(cond)`, etc.
3. Ensure compatibility with existing functions

### Phase 4: Additional Features
1. More arbitration strategies
2. Width adapters
3. `.filter()` method

## Open Questions
1. Should operators modify left-hand side or create new stream?
2. How to handle different pipeline semantics (0-cycle vs 1-cycle delays)?
3. How to integrate with existing stream functions?
4. What about backward compatibility?