#pragma once

#include <string>
#include <vector>
#include <memory>

namespace QuickMedia {
    class BodyItem {
    public:
        BodyItem(const std::string &_title): title(_title) {

        }

        std::string title;
        std::string cover_url;
    };

    enum class SearchResult {
        OK,
        ERR,
        NET_ERR
    };

    enum class DownloadResult {
        OK,
        ERR,
        NET_ERR
    };

    class Plugin {
    public:
        virtual ~Plugin() = default;

        virtual SearchResult search(const std::string &text, std::vector<std::unique_ptr<BodyItem>> &result_items) = 0;
    protected:
        DownloadResult download_to_string(const std::string &url, std::string &result);
    };
}