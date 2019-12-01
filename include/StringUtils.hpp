#pragma once

#include <string>
#include <functional>

namespace QuickMedia {
    // Return false to stop iterating
    using StringSplitCallback = std::function<bool(const char *str, size_t size)>;

    void string_split(const std::string &str, char delimiter, StringSplitCallback callback_func);
    void string_replace_all(std::string &str, const std::string &old_str, const std::string &new_str);
    std::string strip(const std::string &str);
}