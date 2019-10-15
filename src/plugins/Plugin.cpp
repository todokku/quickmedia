#include "../../plugins/Plugin.hpp"
#include "../../include/Program.h"
#include <sstream>
#include <iomanip>
#include <array>

static int accumulate_string(char *data, int size, void *userdata) {
    std::string *str = (std::string*)userdata;
    str->append(data, size);
    return 0;
}

namespace QuickMedia {
    SearchResult Plugin::search(const std::string &text, BodyItems &result_items) {
        (void)text;
        (void)result_items;
        return SearchResult::OK;
    }

    SuggestionResult Plugin::update_search_suggestions(const std::string &text, BodyItems &result_items) {
        (void)text;
        (void)result_items;
        return SuggestionResult::OK;
    }

    BodyItems Plugin::get_related_media(const std::string &url) {
        (void)url;
        return {};
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

    struct HtmlEscapeSequence {
        std::string escape_sequence;
        std::string unescaped_str;
    };

    void string_replace_all(std::string &str, const std::string &old_str, const std::string &new_str) {
        size_t index = 0;
        while(true) {
            index = str.find(old_str, index);
            if(index == std::string::npos)
                return;
            str.replace(index, old_str.size(), new_str);
        }
    }

    void html_unescape_sequences(std::string &str) {
        const std::array<HtmlEscapeSequence, 5> escape_sequences = {
            HtmlEscapeSequence { "&quot;", "\"" },
            HtmlEscapeSequence { "&#39;", "'" },
            HtmlEscapeSequence { "&lt;", "<" },
            HtmlEscapeSequence { "&gt;", ">" },
            HtmlEscapeSequence { "&amp;", "&" } // This should be last, to not accidentally replace a new sequence caused by replacing this
        };

        for(const HtmlEscapeSequence &escape_sequence : escape_sequences) {
            string_replace_all(str, escape_sequence.escape_sequence, escape_sequence.unescaped_str);
        }
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

    DownloadResult Plugin::download_to_string(const std::string &url, std::string &result, const std::vector<CommandArg> &additional_args) {
        sf::Clock timer;
        std::vector<const char*> args;
        if(use_tor)
            args.push_back("torsocks");
        args.insert(args.end(), { "curl", "-f", "-H", "Accept-Language: en-US,en;q=0.5", "--compressed", "-s", "-L" });
        for(const CommandArg &arg : additional_args) {
            args.push_back(arg.option.c_str());
            args.push_back(arg.value.c_str());
        }
        args.push_back("--");
        args.push_back(url.c_str());
        args.push_back(nullptr);
        if(exec_program(args.data(), accumulate_string, &result) != 0)
            return DownloadResult::NET_ERR;
        fprintf(stderr, "Download duration for %s: %d ms\n", url.c_str(), timer.getElapsedTime().asMilliseconds());
        return DownloadResult::OK;
    }
}