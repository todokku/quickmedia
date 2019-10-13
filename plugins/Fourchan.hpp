#pragma once

#include "Plugin.hpp"

namespace QuickMedia {
    class Fourchan : public Plugin {
    public:
        Fourchan() : Plugin("4chan") {}
        PluginResult get_front_page(BodyItems &result_items) override;
        SearchResult search(const std::string &url, BodyItems &result_items) override;
        SuggestionResult update_search_suggestions(const std::string &text, BodyItems &result_items) override;
        PluginResult get_content_list(const std::string &url, BodyItems &result_items) override;
        PluginResult get_content_details(const std::string &list_url, const std::string &url, BodyItems &result_items) override;
        bool search_suggestions_has_thumbnails() const override { return false; }
        bool search_results_has_thumbnails() const override { return false; }
        int get_search_delay() const override { return 150; }
        Page get_page_after_search() const override { return Page::CONTENT_LIST; }
    };
}