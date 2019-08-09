#pragma once

#include "../include/Page.hpp"
#include "../include/Body.hpp"
#include <string>
#include <vector>
#include <memory>

namespace QuickMedia {
    enum class SearchResult {
        OK,
        ERR,
        NET_ERR
    };

    enum class SuggestionResult {
        OK,
        ERR,
        NET_ERR
    };

    enum class DownloadResult {
        OK,
        ERR,
        NET_ERR
    };

    enum class ImageResult {
        OK,
        END,
        ERR,
        NET_ERR
    };

    struct CommandArg {
        std::string option;
        std::string value;
    };

    DownloadResult download_to_string(const std::string &url, std::string &result, const std::vector<CommandArg> &additional_args = {});
    std::string strip(const std::string &str);

    class Plugin {
    public:
        virtual ~Plugin() = default;

        virtual SearchResult search(const std::string &text, BodyItems &result_items);
        virtual SuggestionResult update_search_suggestions(const std::string &text, BodyItems &result_items);
        virtual BodyItems get_related_media(const std::string &url);
        virtual bool search_suggestions_has_thumbnails() const = 0;
        virtual bool search_results_has_thumbnails() const = 0;
        virtual int get_search_delay() const = 0;
        virtual bool search_suggestion_is_search() const { return false; }
        virtual Page get_page_after_search() const = 0;
    protected:
        std::string url_param_encode(const std::string &param) const;
    };
}