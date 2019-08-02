#pragma once

#include "Plugin.hpp"

namespace QuickMedia {
    class Manganelo : public Plugin {
    public:
        SearchResult search(const std::string &text, std::vector<std::unique_ptr<BodyItem>> &result_items) override;
        SuggestionResult update_search_suggestions(const std::string &text, std::vector<std::unique_ptr<BodyItem>> &result_items) override;
    };
}