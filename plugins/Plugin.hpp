#pragma once

#include "../include/Page.hpp"
#include "../include/Body.hpp"
#include <string>
#include <vector>
#include <memory>

namespace QuickMedia {
    enum class PluginResult {
        OK,
        ERR,
        NET_ERR
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

    std::string strip(const std::string &str);
    void string_replace_all(std::string &str, const std::string &old_str, const std::string &new_str);
    void html_unescape_sequences(std::string &str);

    class Plugin {
    public:
        Plugin(const std::string &name) : name(name) {}
        virtual ~Plugin() = default;

        virtual PluginResult get_front_page(BodyItems &result_items) {
            (void)result_items; return PluginResult::OK;
        }
        virtual SearchResult search(const std::string &text, BodyItems &result_items);
        virtual SuggestionResult update_search_suggestions(const std::string &text, BodyItems &result_items);
        virtual BodyItems get_related_media(const std::string &url);
        virtual PluginResult get_content_list(const std::string &url, BodyItems &result_items) {
            (void)url;
            (void)result_items;
            return PluginResult::OK;
        }
        virtual PluginResult get_content_details(const std::string &list_url, const std::string &url, BodyItems &result_items) {
            (void)list_url;
            (void)url;
            (void)result_items;
            return PluginResult::OK;
        }
        virtual bool search_suggestions_has_thumbnails() const = 0;
        virtual bool search_results_has_thumbnails() const = 0;
        virtual int get_search_delay() const = 0;
        virtual bool search_suggestion_is_search() const { return false; }
        virtual Page get_page_after_search() const = 0;

        DownloadResult download_to_string(const std::string &url, std::string &result, const std::vector<CommandArg> &additional_args = {});

        const std::string name;
        bool use_tor = false;
    protected:
        std::string url_param_encode(const std::string &param) const;
    };
}