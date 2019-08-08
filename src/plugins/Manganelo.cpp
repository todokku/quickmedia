#include "../../plugins/Manganelo.hpp"
#include <quickmedia/HtmlSearch.h>
#include <json/reader.h>

namespace QuickMedia {
    SearchResult Manganelo::search(const std::string &url, std::vector<std::unique_ptr<BodyItem>> &result_items) {
        std::string website_data;
        if(download_to_string(url, website_data) != DownloadResult::OK)
            return SearchResult::NET_ERR;

        QuickMediaHtmlSearch html_search;
        int result = quickmedia_html_search_init(&html_search, website_data.c_str());
        if(result != 0)
            goto cleanup;

        result = quickmedia_html_find_nodes_xpath(&html_search, "//div[class='chapter-list']/div[class='row']//a",
            [](QuickMediaHtmlNode *node, void *userdata) {
                auto *item_data = (std::vector<std::unique_ptr<BodyItem>>*)userdata;
                const char *href = quickmedia_html_node_get_attribute_value(node, "href");
                const char *text = quickmedia_html_node_get_text(node);
                if(href && text) {
                    auto item = std::make_unique<BodyItem>(strip(text));
                    item->url = href;
                    item_data->push_back(std::move(item));
                }
            }, &result_items);

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
        if(!json_reader->parse(&server_response[0], &server_response[server_response.size()], &json_root, &json_errors)) {
            fprintf(stderr, "Manganelo suggestions json error: %s\n", json_errors.c_str());
            return SuggestionResult::ERR;
        }

        if(json_root.isArray()) {
            for(const Json::Value &child : json_root) {
                if(child.isObject()) {
                    Json::Value name = child.get("name", "");
                    Json::Value nameunsigned = child.get("nameunsigned", "");
                    if(name.isString() && name.asCString()[0] != '\0' && nameunsigned.isString() && nameunsigned.asCString()[0] != '\0') {
                        std::string name_str = name.asString();
                        while(remove_html_span(name_str)) {}
                        auto item = std::make_unique<BodyItem>(strip(name_str));
                        item->url = "https://manganelo.com/manga/" + url_param_encode(nameunsigned.asString());
                        Json::Value image = child.get("image", "");
                        if(image.isString() && image.asCString()[0] != '\0')
                            item->thumbnail_url = image.asString();
                        result_items.push_back(std::move(item));
                    }
                }
            }
        }
        return SuggestionResult::OK;
    }

    ImageResult Manganelo::get_image_by_index(const std::string &url, int index, std::string &image_data) {
        ImageResult image_result = get_image_urls_for_chapter(url);
        if(image_result != ImageResult::OK)
            return image_result;

        int num_images = last_chapter_image_urls.size();
        if(index < 0 || index >= num_images)
            return ImageResult::END;
        
        // TODO: Cache image in file/memory
        switch(download_to_string(last_chapter_image_urls[index], image_data)) {
            case DownloadResult::OK:
                return ImageResult::OK;
            case DownloadResult::ERR:
                return ImageResult::ERR;
            case DownloadResult::NET_ERR:
                return ImageResult::NET_ERR;
            default:
                return ImageResult::ERR;
        }
    }

    ImageResult Manganelo::get_number_of_images(const std::string &url, int &num_images) {
        num_images = 0;
        ImageResult image_result = get_image_urls_for_chapter(url);
        if(image_result != ImageResult::OK)
            return image_result;

        num_images = last_chapter_image_urls.size();
        return ImageResult::OK;
    }

    ImageResult Manganelo::get_image_urls_for_chapter(const std::string &url) {
        if(url == last_chapter_url)
            return ImageResult::OK;

        last_chapter_image_urls.clear();

        std::string website_data;
        if(download_to_string(url, website_data) != DownloadResult::OK)
            return ImageResult::NET_ERR;

        QuickMediaHtmlSearch html_search;
        int result = quickmedia_html_search_init(&html_search, website_data.c_str());
        if(result != 0)
            goto cleanup;

        result = quickmedia_html_find_nodes_xpath(&html_search, "//div[id='vungdoc']/img",
            [](QuickMediaHtmlNode *node, void *userdata) {
                auto *urls = (std::vector<std::string>*)userdata;
                const char *src = quickmedia_html_node_get_attribute_value(node, "src");
                if(src)
                    urls->push_back(src);
            }, &last_chapter_image_urls);

        cleanup:
        quickmedia_html_search_deinit(&html_search);
        if(result == 0)
            last_chapter_url = url;
        return result == 0 ? ImageResult::OK : ImageResult::ERR;
    }
}