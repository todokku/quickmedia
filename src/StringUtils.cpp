#include "../include/StringUtils.hpp"
#include <string.h>

namespace QuickMedia {
    void string_split(const std::string &str, char delimiter, StringSplitCallback callback_func) {
        size_t index = 0;
        while(true) {
            size_t new_index = str.find(delimiter, index);
            if(new_index == std::string::npos)
                break;

            if(!callback_func(str.data() + index, new_index - index))
                break;

            index = new_index + 1;
        }
    }

    void string_replace_all(std::string &str, const std::string &old_str, const std::string &new_str) {
        size_t index = 0;
        while(true) {
            index = str.find(old_str, index);
            if(index == std::string::npos)
                return;
            str.replace(index, old_str.size(), new_str);
        }
    }

    static bool is_whitespace(char c) {
        return c == ' ' || c == '\n' || c == '\t' || c == '\v';
    }

    std::string strip(const std::string &str) {
        if(str.empty())
            return str;

        int start = 0;
        for(; start < (int)str.size(); ++start) {
            if(!is_whitespace(str[start]))
                break;
        }

        int end = str.size() - 1;
        for(; end >= start; --end) {
            if(!is_whitespace(str[end]))
                break;
        }

        return str.substr(start, end - start + 1);
    }

    bool string_ends_with(const std::string &str, const std::string &ends_with_str) {
        size_t ends_len = ends_with_str.size();
        return ends_len == 0 || (str.size() >= ends_len && memcmp(&str[str.size() - ends_len], ends_with_str.data(), ends_len) == 0);
    }
}