#pragma once

#include "Plugin.hpp"

namespace QuickMedia {
    class Youtube : public Plugin {
    public:
        SuggestionResult update_search_suggestions(const std::string &text, BodyItems &result_items) override;
        BodyItems get_related_media(const std::string &url) override;
        bool search_suggestions_has_thumbnails() const override { return true; }
        bool search_results_has_thumbnails() const override { return false; }
        int get_search_delay() const override { return 150; }
        bool search_suggestion_is_search() const override { return true; }
        Page get_page_after_search() const override { return Page::VIDEO_CONTENT; }
    };
}