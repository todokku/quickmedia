#pragma once

#include "Plugin.hpp"

namespace QuickMedia {
    class Manganelo : public Plugin {
    public:
        SearchResult search(const std::string &url, std::vector<std::unique_ptr<BodyItem>> &result_items, Page &next_page) override;
        SuggestionResult update_search_suggestions(const std::string &text, std::vector<std::unique_ptr<BodyItem>> &result_items) override;
        ImageResult get_image_by_index(const std::string &url, int index, std::string &image_data);
    private:
        ImageResult get_image_urls_for_chapter(const std::string &url, std::vector<std::string> &urls);
    private:
        std::string last_chapter_url;
        std::vector<std::string> last_chapter_image_urls;
    };
}