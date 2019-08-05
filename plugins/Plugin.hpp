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

    class Plugin {
    public:
        virtual ~Plugin() = default;

        virtual SearchResult search(const std::string &text, std::vector<std::unique_ptr<BodyItem>> &result_items, Page &next_page) = 0;
        virtual SuggestionResult update_search_suggestions(const std::string &text, std::vector<std::unique_ptr<BodyItem>> &result_items);
        virtual std::vector<std::unique_ptr<BodyItem>> get_related_media(const std::string &url);
    protected:
        std::string url_param_encode(const std::string &param) const;
    };
}