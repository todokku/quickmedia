#include "../../plugins/Plugin.hpp"
#include "../../include/Program.h"
#include <sstream>
#include <iomanip>

static int accumulate_string(char *data, int size, void *userdata) {
    std::string *str = (std::string*)userdata;
    str->append(data, size);
    return 0;
}

namespace QuickMedia {
    SearchResult Plugin::search(const std::string &text, std::vector<std::unique_ptr<BodyItem>> &result_items) {
        (void)text;
        (void)result_items;
        return SearchResult::OK;
    }

    SuggestionResult Plugin::update_search_suggestions(const std::string &text, std::vector<std::unique_ptr<BodyItem>> &result_items) {
        (void)text;
        (void)result_items;
        return SuggestionResult::OK;
    }

    std::vector<std::unique_ptr<BodyItem>> Plugin::get_related_media(const std::string &url) {
        (void)url;
        return {};
    }

    DownloadResult download_to_string(const std::string &url, std::string &result, const std::vector<CommandArg> &additional_args) {
        std::vector<const char*> args = { "curl", "-H", "Accept-Language: en-US,en;q=0.5", "--compressed", "-s", "-L" };
        for(const CommandArg &arg : additional_args) {
            args.push_back(arg.option.c_str());
            args.push_back(arg.value.c_str());
        }
        args.push_back("--");
        args.push_back(url.c_str());
        args.push_back(nullptr);
        if(exec_program(args.data(), accumulate_string, &result) != 0)
            return DownloadResult::NET_ERR;
        return DownloadResult::OK;
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

    std::string Plugin::url_param_encode(const std::string &param) const {
        std::ostringstream result;
        result.fill('0');
        result << std::hex;

        for(char c : param) {
            if(isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
                result << c;
            } else {
                result << std::uppercase;
                result << "%" << std::setw(2) << (int)(unsigned char)(c);
            }
        }

        return result.str();
    }
}