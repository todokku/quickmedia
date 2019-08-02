#include "../plugins/Manganelo.hpp"
#include <quickmedia/HtmlSearch.h>

namespace QuickMedia {
    SearchResult Manganelo::search(const std::string &text, std::vector<std::unique_ptr<BodyItem>> &result_items) {
        std::string url = "https://manganelo.com/search/";
        // TODO: Escape @text. Need to replace space with underscore for example.
        url += text;

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
                item_data->result_items.push_back(std::move(item));
            }, &item_data);
        if (result != 0)
            goto cleanup;

        result = quickmedia_html_find_nodes_xpath(&html_search, "//div[class=\"story_item\"]//img",
            [](QuickMediaHtmlNode *node, void *userdata) {
                ItemData *item_data = (ItemData*)userdata;
                const char *src = quickmedia_html_node_get_attribute_value(node, "src");
                if(item_data->item_index < item_data->result_items.size()) {
                    item_data->result_items[item_data->item_index]->cover_url = src;
                    ++item_data->item_index;
                }
            }, &item_data);

        cleanup:
        quickmedia_html_search_deinit(&html_search);
        return result == 0 ? SearchResult::OK : SearchResult::ERR;
    }
}