#ifndef STRING_UTIL_HPP
#define STRING_UTIL_HPP

#include "string_util.h"

bool ends_with(std::string &input, const std::string &suffix) {
    if (suffix.length() > input.length()) {
        return false;
    }
    else {
        return (input.substr(input.length() - suffix.length()) == suffix);
    }
}

#endif //STRING_UTIL_HPP
