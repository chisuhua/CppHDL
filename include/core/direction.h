// include/core/direction.h
#ifndef DIRECTION_H
#define DIRECTION_H

#include <type_traits>

namespace ch { namespace core {

struct input_direction {};
struct output_direction {};
struct internal_direction {};

template<typename Dir>
constexpr bool is_input_v = std::is_same_v<Dir, input_direction>;

template<typename Dir>
constexpr bool is_output_v = std::is_same_v<Dir, output_direction>;

// 添加一些有用的类型 trait
template<typename Dir>
constexpr bool is_internal_v = std::is_same_v<Dir, internal_direction>;

}} // namespace ch::core

#endif // DIRECTION_H
