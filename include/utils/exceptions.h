// include/core/exceptions.h
#ifndef EXCEPTIONS_H
#define EXCEPTIONS_H

#include <exception>
#include <string>
#include <source_location>
#include "../macros.h"

namespace ch { namespace core {

class ch_exception : public std::exception {
public:
    explicit ch_exception(const std::string& msg, 
                         const std::source_location& loc = std::source_location::current())
        : message_(msg), location_(loc) {}
    
    const char* what() const noexcept override {
        return message_.c_str();
    }
    
    const std::source_location& location() const { return location_; }
    const std::string& message() const { return message_; }

private:
    std::string message_;
    std::source_location location_;
};

class context_exception : public ch_exception {
public:
    explicit context_exception(const std::string& msg,
                              const std::source_location& loc = std::source_location::current())
        : ch_exception("Context Error: " + msg, loc) {}
};

class node_exception : public ch_exception {
public:
    explicit node_exception(const std::string& msg,
                           const std::source_location& loc = std::source_location::current())
        : ch_exception("Node Error: " + msg, loc) {}
};

}} // namespace ch::core

#endif // EXCEPTIONS_H
