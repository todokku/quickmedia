#include "../../plugins/Plugin.hpp"
#include <sstream>
#include <iomanip>
#include <array>

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

    struct HtmlEscapeSequence {
        std::string escape_sequence;
        std::string unescaped_str;
    };

    void html_unescape_sequences(std::string &str) {
        const std::array<HtmlEscapeSequence, 6> escape_sequences = {
            HtmlEscapeSequence { "&quot;", "\"" },
            HtmlEscapeSequence { "&#039;", "'" },
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
}