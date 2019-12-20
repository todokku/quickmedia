#include "../../plugins/Pornhub.hpp"
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

    static void iterate_suggestion_result(const Json::Value &value, BodyItems &result_items, int &iterate_count) {
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

    // TODO: Speed this up by using string.find instead of parsing html
    SuggestionResult Pornhub::update_search_suggestions(const std::string &text, BodyItems &result_items) {
        std::string url = "https://www.pornhub.com/video/search?search=";
        url += url_param_encode(text);

        std::string website_data;
        if(download_to_string(url, website_data, {}, use_tor) != DownloadResult::OK)
            return SuggestionResult::NET_ERR;

        struct ItemData {
            BodyItems *result_items;
            size_t index;
        };
        ItemData item_data = { &result_items, 0 };

        QuickMediaHtmlSearch html_search;
        int result = quickmedia_html_search_init(&html_search, website_data.c_str());
        if(result != 0)
            goto cleanup;

        result = quickmedia_html_find_nodes_xpath(&html_search, "//div[class='phimage']//a",
            [](QuickMediaHtmlNode *node, void *userdata) {
                auto *result_items = (BodyItems*)userdata;
                const char *href = quickmedia_html_node_get_attribute_value(node, "href");
                const char *title = quickmedia_html_node_get_attribute_value(node, "title");
                if(href && title && begins_with(href, "/view_video.php?viewkey")) {
                    auto item = std::make_unique<BodyItem>(strip(title));
                    item->url = std::string("https://www.pornhub.com") + href;
                    result_items->push_back(std::move(item));
                }
            }, &result_items);
        if(result != 0)
            goto cleanup;

        result = quickmedia_html_find_nodes_xpath(&html_search, "//div[class='phimage']//img",
            [](QuickMediaHtmlNode *node, void *userdata) {
                ItemData *item_data = (ItemData*)userdata;
                if(item_data->index >= item_data->result_items->size())
                    return;

                const char *data_src = quickmedia_html_node_get_attribute_value(node, "data-src");
                if(data_src && contains(data_src, "phncdn.com/videos")) {
                    (*item_data->result_items)[item_data->index]->thumbnail_url = data_src;
                    ++item_data->index;
                }
            }, &item_data);

        // Attempt to skip promoted videos (that are not related to the search term)
        if(result_items.size() >= 4) {
            result_items.erase(result_items.begin(), result_items.begin() + 4);
        }

        cleanup:
        quickmedia_html_search_deinit(&html_search);
        return result == 0 ? SuggestionResult::OK : SuggestionResult::ERR;
    }

    BodyItems Pornhub::get_related_media(const std::string &url) {
        BodyItems result_items;

        std::string website_data;
        if(download_to_string(url, website_data, {}, use_tor) != DownloadResult::OK)
            return result_items;

        struct ItemData {
            BodyItems *result_items;
            size_t index;
        };
        ItemData item_data = { &result_items, 0 };

        QuickMediaHtmlSearch html_search;
        int result = quickmedia_html_search_init(&html_search, website_data.c_str());
        if(result != 0)
            goto cleanup;

        result = quickmedia_html_find_nodes_xpath(&html_search, "//div[class='phimage']//a",
            [](QuickMediaHtmlNode *node, void *userdata) {
                auto *result_items = (BodyItems*)userdata;
                const char *href = quickmedia_html_node_get_attribute_value(node, "href");
                const char *title = quickmedia_html_node_get_attribute_value(node, "title");
                if(href && title && begins_with(href, "/view_video.php?viewkey")) {
                    auto item = std::make_unique<BodyItem>(strip(title));
                    item->url = std::string("https://www.pornhub.com") + href;
                    result_items->push_back(std::move(item));
                }
            }, &result_items);
        if(result != 0)
            goto cleanup;

        result = quickmedia_html_find_nodes_xpath(&html_search, "//div[class='phimage']//img",
            [](QuickMediaHtmlNode *node, void *userdata) {
                ItemData *item_data = (ItemData*)userdata;
                if(item_data->index >= item_data->result_items->size())
                    return;

                const char *src = quickmedia_html_node_get_attribute_value(node, "src");
                if(src && contains(src, "phncdn.com/videos")) {
                    (*item_data->result_items)[item_data->index]->thumbnail_url = src;
                    ++item_data->index;
                }
            }, &item_data);

        // Attempt to skip promoted videos (that are not related to the search term)
        if(result_items.size() >= 4) {
            result_items.erase(result_items.begin(), result_items.begin() + 4);
        }

        cleanup:
        quickmedia_html_search_deinit(&html_search);
        return result_items;
    }
}