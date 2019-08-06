#pragma once

#include "Plugin.hpp"

namespace QuickMedia {
    class Youtube : public Plugin {
    public:
        SearchResult search(const std::string &text, std::vector<std::unique_ptr<BodyItem>> &result_items, Page &next_page) override;
        SuggestionResult update_search_suggestions(const std::string &text, std::vector<std::unique_ptr<BodyItem>> &result_items) override;
        std::vector<std::unique_ptr<BodyItem>> get_related_media(const std::string &url) override;
        bool search_suggestions_has_thumbnails() const override { return false; }
        bool search_results_has_thumbnails() const override { return false; }
    };
}