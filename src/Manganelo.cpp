#include "../plugins/Manganelo.hpp"
#include <quickmedia/HtmlSearch.h>
#include <json/reader.h>

namespace QuickMedia {
    SearchResult Manganelo::search(const std::string &text, std::vector<std::unique_ptr<BodyItem>> &result_items) {
        std::string url = "https://manganelo.com/search/";
        url += url_param_encode(text);

        std::string website_data;
        if(download_to_string(url, website_data) != DownloadResult::OK)
            return SearchResult::NET_ERR;

        struct ItemData {
            std::vector<std::unique_ptr<BodyItem>> &result_items;
            size_t item_index;
        };

        ItemData item_data = { result_items, 0 };

        QuickMediaHtmlSearch html_search;
        int result = quickmedia_html_search_init(&html_search, website_data.c_str());
        if(result != 0)
            goto cleanup;

        result = quickmedia_html_find_nodes_xpath(&html_search, "//h3[class=\"story_name\"]/a",
            [](QuickMediaHtmlNode *node, void *userdata) {
                ItemData *item_data = (ItemData*)userdata;
                const char *href = quickmedia_html_node_get_attribute_value(node, "href");
                const char *text = quickmedia_html_node_get_text(node);
                auto item = std::make_unique<BodyItem>(text);
                item->url = href;
                item_data->result_items.push_back(std::move(item));
            }, &item_data);
        if (result != 0)
            goto cleanup;

        result = quickmedia_html_find_nodes_xpath(&html_search, "//div[class=\"story_item\"]//img",
            [](QuickMediaHtmlNode *node, void *userdata) {
                ItemData *item_data = (ItemData*)userdata;
                const char *src = quickmedia_html_node_get_attribute_value(node, "src");
                if(item_data->item_index < item_data->result_items.size()) {
                    item_data->result_items[item_data->item_index]->thumbnail_url = src;
                    ++item_data->item_index;
                }
            }, &item_data);

        cleanup:
        quickmedia_html_search_deinit(&html_search);
        return result == 0 ? SearchResult::OK : SearchResult::ERR;
    }

    // Returns true if changed
    static bool remove_html_span(std::string &str) {
        size_t open_tag_start = str.find("<span");
        if(open_tag_start == std::string::npos)
            return false;

        size_t open_tag_end = str.find('>', open_tag_start + 5);
        if(open_tag_end == std::string::npos)
            return false;

        str.erase(open_tag_start, open_tag_end - open_tag_start + 1);

        size_t close_tag = str.find("</span>");
        if(close_tag == std::string::npos)
            return true;

        str.erase(close_tag, 7);
        return true;
    }

    SuggestionResult Manganelo::update_search_suggestions(const std::string &text, std::vector<std::unique_ptr<BodyItem>> &result_items) {
        std::string url = "https://manganelo.com/home_json_search";
        std::string search_term = "searchword=";
        search_term += url_param_encode(text);
        CommandArg data_arg = { "--data", std::move(search_term) };

        std::string server_response;
        if(download_to_string(url, server_response, {data_arg}) != DownloadResult::OK)
            return SuggestionResult::NET_ERR;

        if(server_response.empty())
            return SuggestionResult::OK;

        Json::Value json_root;
        Json::CharReaderBuilder json_builder;
        std::unique_ptr<Json::CharReader> json_reader(json_builder.newCharReader());
        std::string json_errors;
        if(json_reader->parse(&server_response.front(), &server_response.back(), &json_root, &json_errors)) {
            fprintf(stderr, "Manganelo suggestions json error: %s\n", json_errors.c_str());
            return SuggestionResult::ERR;
        }

        if(json_root.isArray()) {
            for(const Json::Value &child : json_root) {
                if(child.isObject()) {
                    Json::Value name = child.get("name", "");
                    if(name.isString() && name.asCString()[0] != '\0') {
                        std::string name_str = name.asString();
                        while(remove_html_span(name_str)) {}
                        if(name_str != text) {
                            auto item = std::make_unique<BodyItem>(name_str);
                            result_items.push_back(std::move(item));
                        }
                    }
                }
            }
        }
        return SuggestionResult::OK;
    }
}