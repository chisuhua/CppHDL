// include/core/bundle.h
#ifndef CH_CORE_BUNDLE_H
#define CH_CORE_BUNDLE_H

#include <memory>

namespace ch::core {

class bundle {
public:
    virtual ~bundle() = default;
    virtual std::unique_ptr<bundle> flip() const = 0;
    virtual bool is_valid() const = 0;
};

} // namespace ch::core

#endif // CH_CORE_BUNDLE_H
