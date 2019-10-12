#include "../../plugins/Fourchan.hpp"
#include <json/reader.h>
#include <string.h>

// API documentation: https://github.com/4chan/4chan-API

static const std::string fourchan_url = "https://a.4cdn.org/";
static const std::string fourchan_image_url = "https://i.4cdn.org/";

// Legacy recaptcha command: curl 'https://www.google.com/recaptcha/api/fallback?k=6Ldp2bsSAAAAAAJ5uyx_lx34lJeEpTLVkP5k04qc' -H 'Referer: https://boards.4channel.org/' -H 'Cookie: CONSENT=YES'

/*
Answering recaptcha:
curl 'https://www.google.com/recaptcha/api/fallback?k=6Ldp2bsSAAAAAAJ5uyx_lx34lJeEpTLVkP5k04qc'
-H 'User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:68.0) Gecko/20100101 Firefox/68.0'
-H 'Referer: https://www.google.com/recaptcha/api/fallback?k=6Ldp2bsSAAAAAAJ5uyx_lx34lJeEpTLVkP5k04qc'
-H 'Content-Type: application/x-www-form-urlencoded'
--data 'c=03AOLTBLQ66PjSi9s8S-R1vUS2Jgm-Z_ghEejvvjAaeF3FoR9MiM0zHhCxuertrCo7MAcFUEqcIg4l2WJzVtrJhJVLkncF12OzCaeIvbm46hgDZDZjLD89-LMn1Zs0TP37P-Hd4cuRG8nHuEBXc2ZBD8CVX-6HAs9VBgSmsgQeKF1PWm1tAMBccJhlh4rAOkpjzaEXMMGOe17N0XViwDYZxLGhe4H8IAG2KNB1fb4rz4YKJTPbL30_FvHw7zkdFtojjWiqVW0yCN6N192dhfd9oKz2r9pGRrR6N4AkkX-L0DsBD4yNK3QRsQn3dB1fs3JRZPAh1yqUqTQYhOaqdggyc1EwL8FZHouGRkHTOcCmLQjyv6zuhi6CJbg&response=1&response=4&response=5&response=7'
*/

/* Posting message:
curl 'https://sys.4chan.org/bant/post'
-H 'User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:68.0) Gecko/20100101 Firefox/68.0'
-H 'Referer: https://boards.4chan.org/'
-H 'Content-Type: multipart/form-data; boundary=---------------------------119561554312148213571335532670'
-H 'Origin: https://boards.4chan.org'
-H 'Cookie: __cfduid=d4bd4932e46bc3272fae4ce7a4e2aac511546800687; 4chan_pass=_SsBuZaATt3dIqfVEWlpemhU5XLQ6i9RC'
--data-binary $'-----------------------------119561554312148213571335532670\r\nContent-Disposition: form-data; name="resto"\r\n\r\n8640736\r\n-----------------------------119561554312148213571335532670\r\nContent-Disposition: form-data; name="com"\r\n\r\n>>8640771\r\nShe looks finnish\r\n-----------------------------119561554312148213571335532670\r\nContent-Disposition: form-data; name="mode"\r\n\r\nregist\r\n-----------------------------119561554312148213571335532670\r\nContent-Disposition: form-data; name="pwd"\r\n\r\n_SsBuZaATt3dIqfVEWlpemhU5XLQ6i9RC\r\n-----------------------------119561554312148213571335532670\r\nContent-Disposition: form-data; name="g-recaptcha-response"\r\n\r\n03AOLTBLS5lshp5aPj5pG6xdVMQ0pHuHxAtJoCEYuPLNKYlsRWNCPQegjB9zgL-vwdGMzjcT-L9iW4bnQ5W3TqUWHOVqtsfnx9GipLUL9o2XbC6r9zy-EEiPde7l6J0WcZbr9nh_MGcUpKl6RGaZoYB3WwXaDq74N5hkmEAbqM_CBtbAVVlQyPmemI2HhO2J6K0yFVKBrBingtIZ6-oXBXZ4jC4rT0PeOuVaH_gf_EBjTpb55ueaPmTbeLGkBxD4-wL1qA8F8h0D8c\r\n-----------------------------119561554312148213571335532670--\r\n'
*/

namespace QuickMedia {
    PluginResult Fourchan::get_front_page(BodyItems &result_items) {
        std::string server_response;
        if(download_to_string(fourchan_url + "boards.json", server_response) != DownloadResult::OK)
            return PluginResult::NET_ERR;

        Json::Value json_root;
        Json::CharReaderBuilder json_builder;
        std::unique_ptr<Json::CharReader> json_reader(json_builder.newCharReader());
        std::string json_errors;
        if(!json_reader->parse(server_response.data(), server_response.data() + server_response.size(), &json_root, &json_errors)) {
            fprintf(stderr, "4chan front page json error: %s\n", json_errors.c_str());
            return PluginResult::ERR;
        }

        const Json::Value &boards = json_root["boards"];
        if(boards.isArray()) {
            for(const Json::Value &board : boards) {
                const Json::Value &board_id = board["board"]; // /g/, /a/, /b/ etc
                const Json::Value &board_title = board["title"];
                const Json::Value &board_description = board["meta_description"];
                if(board_id.isString() && board_title.isString() && board_description.isString()) {
                    std::string board_description_str = board_description.asString();
                    html_unescape_sequences(board_description_str);
                    auto body_item = std::make_unique<BodyItem>(board_title.asString());
                    body_item->url = board_id.asString();
                    result_items.emplace_back(std::move(body_item));
                }
            }
        }

        return PluginResult::OK;
    }

    SearchResult Fourchan::search(const std::string &url, BodyItems &result_items) {
        return SearchResult::OK;
    }

    SuggestionResult Fourchan::update_search_suggestions(const std::string &text, BodyItems &result_items) {
        return SuggestionResult::OK;
    }

    static bool string_ends_with(const std::string &str, const std::string &ends_with_str) {
        size_t len = ends_with_str.size();
        return len == 0 || (str.size() >= len && memcmp(&str[str.size() - len], ends_with_str.data(), len) == 0);
    }

    PluginResult Fourchan::get_content_list(const std::string &url, BodyItems &result_items) {
        std::string server_response;
        if(download_to_string(fourchan_url + url + "/catalog.json", server_response) != DownloadResult::OK)
            return PluginResult::NET_ERR;

        Json::Value json_root;
        Json::CharReaderBuilder json_builder;
        std::unique_ptr<Json::CharReader> json_reader(json_builder.newCharReader());
        std::string json_errors;
        if(!json_reader->parse(server_response.data(), server_response.data() + server_response.size(), &json_root, &json_errors)) {
            fprintf(stderr, "4chan catalog json error: %s\n", json_errors.c_str());
            return PluginResult::ERR;
        }

        if(json_root.isArray()) {
            for(const Json::Value &page_data : json_root) {
                if(!page_data.isObject())
                    continue;

                const Json::Value &threads = page_data["threads"];
                if(!threads.isArray())
                    continue;

                for(const Json::Value &thread : threads) {
                    if(!thread.isObject())
                        continue;

                    const Json::Value &com = thread["com"];
                    if(!com.isString())
                        continue;

                    const Json::Value &thread_num = thread["no"];
                    if(!thread_num.isNumeric())
                        continue;

                    auto body_item = std::make_unique<BodyItem>(com.asString());
                    body_item->url = std::to_string(thread_num.asInt64());

                    const Json::Value &ext = thread["ext"];
                    const Json::Value &tim = thread["tim"];
                    if(tim.isNumeric() && ext.isString()) {
                        std::string ext_str = ext.asString();
                        if(ext_str == ".png" || ext_str == ".jpg" || ext_str == ".jpeg") {
                        } else {
                            fprintf(stderr, "TODO: Support file extension: %s\n", ext_str.c_str());
                        }
                        // "s" means small, that's the url 4chan uses for thumbnails.
                        // thumbnails always has .jpg extension even if they are gifs or webm.
                        body_item->thumbnail_url = fourchan_image_url + url + "/" + std::to_string(tim.asInt64()) + "s.jpg";
                    }
                    
                    result_items.emplace_back(std::move(body_item));
                }
            }
        }

        return PluginResult::OK;
    }

    PluginResult Fourchan::get_content_details(const std::string &list_url, const std::string &url, BodyItems &result_items) {
        std::string server_response;
        if(download_to_string(fourchan_url + list_url + "/thread/" + url + ".json", server_response) != DownloadResult::OK)
            return PluginResult::NET_ERR;

        Json::Value json_root;
        Json::CharReaderBuilder json_builder;
        std::unique_ptr<Json::CharReader> json_reader(json_builder.newCharReader());
        std::string json_errors;
        if(!json_reader->parse(server_response.data(), server_response.data() + server_response.size(), &json_root, &json_errors)) {
            fprintf(stderr, "4chan thread json error: %s\n", json_errors.c_str());
            return PluginResult::ERR;
        }

        const Json::Value &posts = json_root["posts"];
        if(posts.isArray()) {
            for(const Json::Value &post : posts) {
                if(!post.isObject())
                    continue;

                const Json::Value &com = post["com"];
                if(!com.isString())
                    continue;

                const Json::Value &post_num = post["no"];
                if(!post_num.isNumeric())
                    continue;

                auto body_item = std::make_unique<BodyItem>(com.asString());
                body_item->url = std::to_string(post_num.asInt64());

                const Json::Value &ext = post["ext"];
                const Json::Value &tim = post["tim"];
                if(tim.isNumeric() && ext.isString()) {
                    std::string ext_str = ext.asString();
                    if(ext_str == ".png" || ext_str == ".jpg" || ext_str == ".jpeg") {
                    } else {
                        fprintf(stderr, "TODO: Support file extension: %s\n", ext_str.c_str());
                    }
                    // "s" means small, that's the url 4chan uses for thumbnails.
                    // thumbnails always has .jpg extension even if they are gifs or webm.
                    body_item->thumbnail_url = fourchan_image_url + list_url + "/" + std::to_string(tim.asInt64()) + "s.jpg";
                }
                
                result_items.emplace_back(std::move(body_item));
            }
        }

        return PluginResult::OK;
    }
}