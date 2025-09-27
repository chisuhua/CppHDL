// include/module.h
#ifndef MODULE_H
#define MODULE_H

#include "component.h"

namespace ch {

template <typename T, typename... Args>
class ch_module : public T {
public:
    ch_module(Args&&... args)
        : T(Component::current(), std::forward<Args>(args)...)
    {
        this->build();
    }
};

} // namespace ch

#endif // MODULE_H
