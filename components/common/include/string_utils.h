#pragma once
#include <cstddef>

namespace smart_device {

inline void copy_cstr(char* destination, std::size_t capacity, const char* source) {
    if (!destination || capacity == 0) return;
    if (!source) {
        destination[0] = '\0';
        return;
    }

    std::size_t length = 0;
    while (length + 1 < capacity && source[length] != '\0') {
        destination[length] = source[length];
        ++length;
    }
    destination[length] = '\0';
}

template<std::size_t N>
inline void copy_cstr(char (&destination)[N], const char* source) {
    copy_cstr(destination, N, source);
}

} // namespace smart_device
