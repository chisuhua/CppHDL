// include/core/direction.h
#pragma once

#include <type_traits>

namespace ch { namespace core {

struct input_direction {};
struct output_direction {};

template<typename Dir>
constexpr bool is_input_v = std::is_same_v<Dir, input_direction>;

template<typename Dir>
constexpr bool is_output_v = std::is_same_v<Dir, output_direction>;

}} // namespace ch::core

