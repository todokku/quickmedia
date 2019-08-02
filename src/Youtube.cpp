#include "../plugins/Youtube.hpp"
#include <quickmedia/HtmlSearch.h>
#include <json/reader.h>

namespace QuickMedia {
    SearchResult Youtube::search(const std::string &text, std::vector<std::unique_ptr<BodyItem>> &result_items) {
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
                auto item = std::make_unique<BodyItem>(title);
                result_items->push_back(std::move(item));
            }, &result_items);

        cleanup:
        quickmedia_html_search_deinit(&html_search);
        return result == 0 ? SearchResult::OK : SearchResult::ERR;
    }

    static void iterate_suggestion_result(const Json::Value &value, std::vector<std::unique_ptr<BodyItem>> &result_items, int &ignore_count) {
        if(value.isArray()) {
            for(const Json::Value &child : value) {
                iterate_suggestion_result(child, result_items, ignore_count);
            }
        } else if(value.isString()) {
            if(ignore_count > 1) {
                auto item = std::make_unique<BodyItem>(value.asString());
                result_items.push_back(std::move(item));
            }
            ++ignore_count;
        }
    }

    SuggestionResult Youtube::update_search_suggestions(const std::string &text, std::vector<std::unique_ptr<BodyItem>> &result_items) {
        if(text.empty())
            return SuggestionResult::OK;

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

        int ignore_count = 0;
        iterate_suggestion_result(json_root, result_items, ignore_count);
        return SuggestionResult::OK;
    }
}