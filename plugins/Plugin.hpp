#pragma once

#include "../include/Page.hpp"
#include <string>
#include <vector>
#include <memory>

namespace QuickMedia {
    class BodyItem {
    public:
        BodyItem(const std::string &_title): title(_title), visible(true) {

        }

        std::string title;
        std::string url;
        std::string thumbnail_url;
        bool visible;
    };

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

    class Plugin {
    public:
        virtual ~Plugin() = default;

        virtual SearchResult search(const std::string &text, std::vector<std::unique_ptr<BodyItem>> &result_items, Page &next_page) = 0;
        virtual SuggestionResult update_search_suggestions(const std::string &text, std::vector<std::unique_ptr<BodyItem>> &result_items);
        virtual std::vector<std::unique_ptr<BodyItem>> get_related_media(const std::string &url);
    protected:
        DownloadResult download_to_string(const std::string &url, std::string &result, const std::vector<CommandArg> &additional_args = {});
        std::string url_param_encode(const std::string &param) const;
    };
}