// include/direction.h
#ifndef DIRECTION_H
#define DIRECTION_H

namespace ch::core {

struct input_direction {};
struct output_direction {};
struct internal_direction {};

template<typename Dir>
constexpr bool is_input_v  = std::is_same_v<Dir, input_direction>;

template<typename Dir>
constexpr bool is_output_v = std::is_same_v<Dir, output_direction>;

} // namespace ch::core

#endif // DIRECTION_H
