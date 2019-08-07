#include "../../plugins/Youtube.hpp"
#include <quickmedia/HtmlSearch.h>
#include <json/reader.h>
#include <string.h>

namespace QuickMedia {
    static bool begins_with(const char *str, const char *begin_with) {
        return strncmp(str, begin_with, strlen(begin_with)) == 0;
    }

    static bool contains(const char *str, const char *substr) {
        return strstr(str, substr);
    }

    SearchResult Youtube::search(const std::string &text, std::vector<std::unique_ptr<BodyItem>> &result_items, Page &next_page) {
        next_page = Page::SEARCH_RESULT;
        std::string url = "https://youtube.com/results?search_query=";
        url += url_param_encode(text);

        std::string website_data;
        if(download_to_string(url, website_data) != DownloadResult::OK)
            return SearchResult::NET_ERR;

        struct ItemData {
            std::vector<std::unique_ptr<BodyItem>> *result_items;
            size_t index;
        };
        ItemData item_data = { &result_items, 0 };

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
                    auto item = std::make_unique<BodyItem>(strip(title));
                    item->url = std::string("https://www.youtube.com") + href;
                    result_items->push_back(std::move(item));
                }
            }, &result_items);
        if(result != 0)
            goto cleanup;

        result = quickmedia_html_find_nodes_xpath(&html_search, "//span[class=\"yt-thumb-simple\"]//img",
            [](QuickMediaHtmlNode *node, void *userdata) {
                ItemData *item_data = (ItemData*)userdata;
                if(item_data->index >= item_data->result_items->size())
                    return;

                const char *src = quickmedia_html_node_get_attribute_value(node, "src");
                const char *data_thumb = quickmedia_html_node_get_attribute_value(node, "data-thumb");

                if(src && contains(src, "i.ytimg.com/")) {
                    (*item_data->result_items)[item_data->index]->thumbnail_url = src;
                    ++item_data->index;
                } else if(data_thumb && contains(data_thumb, "i.ytimg.com/")) {
                    (*item_data->result_items)[item_data->index]->thumbnail_url = data_thumb;
                    ++item_data->index;
                }
            }, &item_data);

        cleanup:
        quickmedia_html_search_deinit(&html_search);
        return result == 0 ? SearchResult::OK : SearchResult::ERR;
    }

    static void iterate_suggestion_result(const Json::Value &value, std::vector<std::unique_ptr<BodyItem>> &result_items, int &iterate_count) {
        ++iterate_count;
        if(value.isArray()) {
            for(const Json::Value &child : value) {
                iterate_suggestion_result(child, result_items, iterate_count);
            }
        } else if(value.isString() && iterate_count > 2) {
            std::string title = value.asString();
            auto item = std::make_unique<BodyItem>(title);
            result_items.push_back(std::move(item));
        }
    }

    SuggestionResult Youtube::update_search_suggestions(const std::string &text, std::vector<std::unique_ptr<BodyItem>> &result_items) {
        std::string url = "https://clients1.google.com/complete/search?client=youtube&hl=en&gs_rn=64&gs_ri=youtube&ds=yt&cp=7&gs_id=x&q=";
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

        int iterate_count = 0;
        iterate_suggestion_result(json_root, result_items, iterate_count);
        bool found_search_text = false;
        for(auto &item : result_items) {
            if(item->title == text) {
                found_search_text = true;
                break;
            }
        }
        if(!found_search_text)
            result_items.insert(result_items.begin(), std::make_unique<BodyItem>(text));
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