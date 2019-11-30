#pragma once

#include "ImageBoard.hpp"

namespace QuickMedia {
    class Fourchan : public ImageBoard {
    public:
        Fourchan() : ImageBoard("4chan") {}
        PluginResult get_front_page(BodyItems &result_items) override;
        SearchResult search(const std::string &url, BodyItems &result_items) override;
        SuggestionResult update_search_suggestions(const std::string &text, BodyItems &result_items) override;
        PluginResult get_threads(const std::string &url, BodyItems &result_items) override;
        PluginResult get_thread_comments(const std::string &list_url, const std::string &url, BodyItems &result_items) override;
        bool search_suggestions_has_thumbnails() const override { return false; }
        bool search_results_has_thumbnails() const override { return false; }
        int get_search_delay() const override { return 150; }
        Page get_page_after_search() const override { return Page::IMAGE_BOARD_THREAD_LIST; }
    };
}