// include/hdl/when.h
#ifndef WHEN_H
#define WHEN_H

namespace ch {

// 最简单的 when 实现 - 直接返回条件的布尔值
template<typename T>
bool when(const T& condition) {
    // 对于现在，简单地将条件转换为 bool
    // 在更完整的实现中，这里会生成硬件条件逻辑
    return static_cast<bool>(condition);
}

} // namespace ch

// 在更完整的实现中，when 语句会像这样使用：
// if (ch::when(valid && ready)) {
//     // 条件为真时执行的硬件逻辑
// }

#endif // WHEN_H
