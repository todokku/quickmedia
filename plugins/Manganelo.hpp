#pragma once

#include "Plugin.hpp"

namespace QuickMedia {
    class Manganelo : public Plugin {
    public:
        SearchResult search(const std::string &url, BodyItems &result_items) override;
        SuggestionResult update_search_suggestions(const std::string &text, BodyItems &result_items) override;
        ImageResult get_image_by_index(const std::string &url, int index, std::string &image_data);
        ImageResult get_number_of_images(const std::string &url, int &num_images);
        bool search_suggestions_has_thumbnails() const override { return true; }
        bool search_results_has_thumbnails() const override { return false; }
        int get_search_delay() const override { return 150; }
        Page get_page_after_search() const override { return Page::EPISODE_LIST; }
    private:
        // Caches url. If the same url is requested multiple times then the cache is used
        ImageResult get_image_urls_for_chapter(const std::string &url);
    private:
        std::string last_chapter_url;
        std::vector<std::string> last_chapter_image_urls;
    };
}