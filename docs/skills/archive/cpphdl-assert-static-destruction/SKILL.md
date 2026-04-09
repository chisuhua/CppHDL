# CppHDL 断言宏静态析构检查技能

## 触发条件

当 CppHDL 项目编译或运行时出现以下问题时激活：
- 断言宏在程序退出时报错或崩溃
- 日志系统在静态析构期间访问已销毁的资源
- 编译错误提到 `in_static_destruction()` 相关宏
- 需要在析构期间跳过某些检查或日志

## 问题根因

### 核心问题

在 C++ 程序退出阶段，静态对象按**未定义顺序**销毁。如果日志系统或断言宏在静态析构期间被调用，可能访问已销毁的资源（如 `std::cout`、文件句柄、单例对象），导致：
- 段错误 (Segmentation Fault)
- 未定义行为
- 程序崩溃

### 典型场景

```cpp
// ❌ 错误模式：静态析构期间调用日志
static SomeObject obj;  // 静态对象

~SomeObject() {
    CHREQUIRE(valid_, "Object must be valid");  // 可能在 std::cout 销毁后调用
}

// 程序退出时：
// 1. std::cout 被销毁（顺序未定义）
// 2. obj 析构，调用 CHREQUIRE
// 3. CHREQUIRE 尝试使用 std::cout → 崩溃
```

## 修复方案

### 方案 1: 添加静态析构检查（标准模式）

所有日志和断言宏必须检查 `in_static_destruction()`：

```cpp
// ✅ 正确模式：CHREQUIRE 宏实现
#define CHREQUIRE(condition, fmt, ...)                                         \
    do {                                                                       \
        if (!(condition) && !ch::detail::in_static_destruction()) {            \
            auto loc = std::source_location::current();                        \
            std::string msg = ch::detail::printf_format(                       \
                "Requirement failed [%s]: " fmt, #condition, ##__VA_ARGS__);   \
            ch::detail::log_message(ch::log_level::error, msg, loc);           \
        }                                                                      \
    } while (0)
```

**关键点**:
- `!ch::detail::in_static_destruction()` 检查必须在日志输出之前
- 如果处于静态析构期，直接 `break` 跳过日志

### 方案 2: 自定义断言添加检查

如果你需要创建自定义断言宏：

```cpp
// ✅ 正确模式：自定义断言
#define MY_ASSERT(condition, msg)                                              \
    do {                                                                       \
        if (!(condition) && !ch::detail::in_static_destruction()) {            \
            std::cerr << "[MY_ASSERT] " << msg << std::endl;                   \
        }                                                                      \
    } while (0)
```

### 方案 3: 条件编译禁用（调试模式）

在调试模式下可以禁用静态析构检查：

```cpp
#ifdef CH_DEBUG_STATIC_DESTRUCTION
    // 调试模式：允许静态析构期间日志
    #define CHREQUIRE_DEBUG(condition, fmt, ...)                               \
        do {                                                                   \
            if (!(condition)) {                                                \
                /* 记录错误 */                                                 \
            }                                                                  \
        } while (0)
#else
    // 发布模式：跳过静态析构期间日志
    #define CHREQUIRE_DEBUG(condition, fmt, ...)                               \
        do {                                                                   \
            if (!(condition) && !ch::detail::in_static_destruction()) {        \
                /* 记录错误 */                                                 \
            }                                                                  \
        } while (0)
#endif
```

## 执行流程

### 1. 定位问题宏

```bash
cd /workspace/CppHDL

# 查找所有断言/日志宏使用
grep -rn "CHREQUIRE\|CHCHECK\|CHINFO\|CHERROR" include/ src/ | head -30

# 查找未添加静态析构检查的宏
grep -rn "if.*condition" include/utils/logger.h
```

### 2. 检查 destruction_manager 实现

```bash
cat /workspace/CppHDL/include/utils/destruction_manager.h
cat /workspace/CppHDL/include/utils/logger.h | grep -A 5 "in_static_destruction"
```

### 3. 应用修复

根据宏类型添加检查：

| 宏类型 | 检查模式 |
|--------|---------|
| `CHLOG` | `if (ch::detail::in_static_destruction()) break;` |
| `CHREQUIRE` | `if (!(condition) && !ch::detail::in_static_destruction())` |
| `CHDBG_FUNC` | `if (!ch::detail::in_static_destruction()) { ... }` |
| 自定义宏 | 参考上述模式 |

### 4. 验证修复

```bash
cd /workspace/CppHDL/build

# 重新编译
cmake .. && make -j$(nproc)

# 运行测试（观察退出时是否崩溃）
ctest --output-on-failure

# 运行示例（观察程序退出日志）
./examples/riscv/rv32i_test
echo "Exit code: $?"
```

## 检查清单

修复后必须验证：
- [ ] 所有日志宏添加 `in_static_destruction()` 检查
- [ ] 所有断言宏添加 `in_static_destruction()` 检查
- [ ] 程序退出时无段错误
- [ ] 程序退出时无未定义行为警告
- [ ] Valgrind/ASan 检查通过

## 实现细节

### destruction_manager 工作原理

```cpp
// include/utils/destruction_manager.h

class destruction_manager {
public:
    // 获取单例
    static destruction_manager& instance();
    
    // 设置静态析构标志
    void mark_static_destruction() {
        in_static_destruction_flag() = true;
    }
    
    // 检查是否在静态析构期
    bool is_in_static_destruction() const {
        return in_static_destruction_flag();
    }

private:
    // 静态析构标志
    std::atomic<bool>& in_static_destruction_flag() {
        static std::atomic<bool> flag{false};
        return flag;
    }
};

// 静态对象析构时设置标志
static struct static_destruction_guard {
    ~static_destruction_guard() {
        destruction_manager::instance().mark_static_destruction();
    }
} _static_destruction_guard;

// 便捷函数
inline bool in_static_destruction() {
    return destruction_manager::instance().is_in_static_destruction();
}
```

### 日志宏标准实现

```cpp
// include/utils/logger.h

// 基础日志宏
#define CHLOG(level, fmt, ...)                                                 \
    do {                                                                       \
        if (ch::detail::in_static_destruction())                               \
            break;  /* 静态析构期间跳过 */                                     \
        auto loc = std::source_location::current();                            \
        std::string msg = ch::detail::printf_format(fmt, ##__VA_ARGS__);       \
        ch::detail::log_message(level, msg, loc);                              \
    } while (0)

// 检查宏
#define CHREQUIRE(condition, fmt, ...)                                         \
    do {                                                                       \
        if (!(condition) && !ch::detail::in_static_destruction()) {            \
            auto loc = std::source_location::current();                        \
            std::string msg = ch::detail::printf_format(                       \
                "Requirement failed [%s]: " fmt, #condition, ##__VA_ARGS__);   \
            ch::detail::log_message(ch::log_level::error, msg, loc);           \
        }                                                                      \
    } while (0)
```

## 相关技能

- `cpphdl-shift-fix`: 位移操作 UB 修复
- `cpphdl-fifo-timing-fix`: FIFO 时序逻辑修复
- `cpphdl-raii-memory`: RAII 内存安全模式

## 历史案例

### 2026-04-01: context.tpp 断言宏问题

**问题现象**:
```
!(condition) && !ch::detail::in_static_destruction()) { \
```
编译错误提到断言宏在静态析构期间行为异常

**根本原因**:
- `CHREQUIRE` 宏已正确添加 `in_static_destruction()` 检查
- 但某些自定义断言可能未添加检查
- 或者日志系统在析构期间被调用

**修复方案**:
- 检查所有自定义断言宏
- 确保添加 `!ch::detail::in_static_destruction()` 检查
- 参考 `logger.h` 中的标准实现

**验证结果**: ✅ 编译通过，运行时无崩溃

---

## 最佳实践

### 创建新宏时的检查清单

```markdown
## 新宏安全检查

- [ ] 是否添加了 `in_static_destruction()` 检查？
- [ ] 检查位置是否在日志输出之前？
- [ ] 是否使用了 `do { ... } while(0)` 包裹？
- [ ] 是否支持 printf 风格格式化？
- [ ] 是否包含 `std::source_location` 信息？
```

### 代码审查要点

在审查代码时，检查所有日志/断言宏：
```cpp
// ✅ 好的：有静态析构检查
if (!(condition) && !ch::detail::in_static_destruction()) {
    // 记录错误
}

// ❌ 坏的：缺少静态析构检查
if (!(condition)) {
    // 可能在静态析构期间崩溃
}
```

---

**技能版本**: v1.0  
**创建时间**: 2026-04-01  
**适用项目**: CppHDL, 及其他使用 CppHDL 日志/断言系统的项目  
**参考文件**: `include/utils/logger.h`, `include/utils/destruction_manager.h`
