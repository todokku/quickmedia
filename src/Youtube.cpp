#include "../plugins/Youtube.hpp"
#include <quickmedia/HtmlSearch.h>

namespace QuickMedia {
    SearchResult Youtube::search(const std::string &text, std::vector<std::unique_ptr<BodyItem>> &result_items) {
        std::string url = "https://youtube.com/results?search_query=";
        // TODO: Escape @text
        url += text;

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
                printf("a href: %s, title: %s\n", href, title);

                auto item = std::make_unique<BodyItem>(title);
                result_items->push_back(std::move(item));
            }, &result_items);

        cleanup:
        quickmedia_html_search_deinit(&html_search);
        return result == 0 ? SearchResult::OK : SearchResult::ERR;
    }
}