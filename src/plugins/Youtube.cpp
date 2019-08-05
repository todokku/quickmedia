#include "../../plugins/Youtube.hpp"
#include <quickmedia/HtmlSearch.h>
#include <json/reader.h>
#include <string.h>

namespace QuickMedia {
    static bool begins_with(const char *str, const char *begin_with) {
        return strncmp(str, begin_with, strlen(begin_with)) == 0;
    }

    SearchResult Youtube::search(const std::string &text, std::vector<std::unique_ptr<BodyItem>> &result_items, Page &next_page) {
        next_page = Page::SEARCH_RESULT;
        std::string url = "https://youtube.com/results?search_query=";
        url += url_param_encode(text);

        std::string website_data;
        if(download_to_string(url, website_data) != DownloadResult::OK)
            return SearchResult::NET_ERR;

        QuickMediaHtmlSearch html_search;
        int result = quickmedia_html_search_init(&html_search, website_data.c_str());
        if(result != 0)
            goto cleanup;

        result = quickmedia_html_find_nodes_xpath(&html_search, "//h3[class=\"yt-lockup-title\"]/a",
            [](QuickMediaHtmlNode *node, void *userdata) {
                auto *result_items = (std::vector<std::unique_ptr<BodyItem>>*)userdata;
                const char *href = quickmedia_html_node_get_attribute_value(node, "href");
                const char *title = quickmedia_html_node_get_attribute_value(node, "title");
                // Checking for watch?v helps skipping ads
                if(href && title && begins_with(href, "/watch?v=")) {
                    auto item = std::make_unique<BodyItem>(title);
                    item->url = std::string("https://www.youtube.com") + href;
                    result_items->push_back(std::move(item));
                }
            }, &result_items);

        cleanup:
        quickmedia_html_search_deinit(&html_search);
        return result == 0 ? SearchResult::OK : SearchResult::ERR;
    }

    static void iterate_suggestion_result(const Json::Value &value, const std::string &search_text, std::vector<std::unique_ptr<BodyItem>> &result_items) {
        if(value.isArray()) {
            for(const Json::Value &child : value) {
                iterate_suggestion_result(child, search_text, result_items);
            }
        } else if(value.isString()) {
            std::string title = value.asString();
            if(title != search_text) {
                auto item = std::make_unique<BodyItem>(title);
                result_items.push_back(std::move(item));
            }
        }
    }

    SuggestionResult Youtube::update_search_suggestions(const std::string &text, std::vector<std::unique_ptr<BodyItem>> &result_items) {
        result_items.push_back(std::make_unique<BodyItem>(text));
        std::string url = "https://clients1.google.com/complete/search?client=youtube&hl=en&gl=us&q=";
        url += url_param_encode(text);

        std::string server_response;
        if(download_to_string(url, server_response) != DownloadResult::OK)
            return SuggestionResult::NET_ERR;

        size_t json_start = server_response.find_first_of('(');
        if(json_start == std::string::npos)
            return SuggestionResult::ERR;
        ++json_start;

        size_t json_end = server_response.find_last_of(')');
        if(json_end == std::string::npos)
            return SuggestionResult::ERR;

        if(json_end == 0 || json_start >= json_end)
            return SuggestionResult::ERR;
        --json_end;

        Json::Value json_root;
        Json::CharReaderBuilder json_builder;
        std::unique_ptr<Json::CharReader> json_reader(json_builder.newCharReader());
        std::string json_errors;
        if(json_reader->parse(&server_response[json_start], &server_response[json_end], &json_root, &json_errors)) {
            fprintf(stderr, "Youtube suggestions json error: %s\n", json_errors.c_str());
            return SuggestionResult::ERR;
        }

        iterate_suggestion_result(json_root, text, result_items);
        return SuggestionResult::OK;
    }

    std::vector<std::unique_ptr<BodyItem>> Youtube::get_related_media(const std::string &url) {
        std::vector<std::unique_ptr<BodyItem>> result_items;

        std::string website_data;
        if(download_to_string(url, website_data) != DownloadResult::OK)
            return result_items;

        QuickMediaHtmlSearch html_search;
        int result = quickmedia_html_search_init(&html_search, website_data.c_str());
        if(result != 0)
            goto cleanup;

        result = quickmedia_html_find_nodes_xpath(&html_search, "//ul[class=\"video-list\"]//div[class=\"content-wrapper\"]/a",
            [](QuickMediaHtmlNode *node, void *userdata) {
                auto *result_items = (std::vector<std::unique_ptr<BodyItem>>*)userdata;
                const char *href = quickmedia_html_node_get_attribute_value(node, "href");
                // TODO: Also add title for related media
                if(href && begins_with(href, "/watch?v=")) {
                    auto item = std::make_unique<BodyItem>("");
                    item->url = std::string("https://www.youtube.com") + href;
                    result_items->push_back(std::move(item));
                }
            }, &result_items);

        cleanup:
        quickmedia_html_search_deinit(&html_search);
        return result_items;
    }
}