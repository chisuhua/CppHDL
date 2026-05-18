## Why

`using namespace ch::core;` 出现在 `namespace ch { ... }` 内部，会将 `ch::core` 中约 50+ 个类型/函数/概念直接注入 `ch` 命名空间，造成命名空间污染。这会导致：

1. **命名冲突风险**：第三方库或用户代码中与 `ch::core` 同名的符号可能产生隐式碰撞
2. **ADL 异常行为**：Argument-Dependent Lookup 可能指向错误的命名空间
3. **代码可读性下降**：无法从代码中判断 `ch_bool` 是来自 `ch` 还是 `ch::core`

ADR-033 记录了 8 个受影响文件，暂定"记录现状，暂不修复"，但项目实际需要真正修复以保证代码质量。

## What

修复 8 个文件中的 `using namespace ch::core;` 命名空间污染问题：

| 文件 | 说明 |
|------|------|
| `include/ast/instr_mem.h` | 内存指令实现 |
| `include/bundle/axi_bundle.h` | AXI bundle 定义 |
| `include/bundle/axi_lite_bundle.h` | AXI-Lite bundle 定义 |
| `include/bundle/common_bundles.h` | 通用 bundle 类型 |
| `include/bundle/flow_bundle.h` | Flow bundle 定义 |
| `include/bundle/fragment.h` | Fragment bundle 定义 |
| `include/bundle/stream_bundle.h` | Stream bundle 定义 |
| `include/chlib/assert.h` | 断言组件 |

**修复方式**：将 `using namespace ch::core;` 替换为按需引入：
```cpp
namespace ch {
using ch::core::ch_bool;
using ch::core::ch_uint;
// ... 其他实际使用的类型
}
```

或使用完全限定名：`ch::core::ch_bool`, `ch::core::ch_uint` 等。

## Capabilities

**Modified Capabilities**：
- 无 spec 级别行为变更，仅内部实现重构

## Impact

- **影响范围**：8 个头文件
- **风险等级**：低（仅影响命名空间，不改变功能行为）
- **构建影响**：需验证编译通过
- **测试影响**：需运行现有测试确保无回归

## Non-Goals

- 不修复 `chlib/if_stmt.h`（已正确修复，参考实现）
- 不修改 `ch::core` 命名空间本身
- 不添加新功能