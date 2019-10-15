#include "../include/StringUtils.hpp"

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
}