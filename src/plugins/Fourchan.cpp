#include "../../plugins/Fourchan.hpp"
#include <json/reader.h>
#include <string.h>
#include "../../include/DataView.hpp"
#include <tidy.h>
#include <tidybuffio.h>

// API documentation: https://github.com/4chan/4chan-API

static const std::string fourchan_url = "https://a.4cdn.org/";
static const std::string fourchan_image_url = "https://i.4cdn.org/";

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
                    auto body_item = std::make_unique<BodyItem>("/" + board_id.asString() + "/ " + board_title.asString());
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

    struct CommentPiece {
        enum class Type {
            TEXT,
            QUOTE, // >
            QUOTELINK, // >>POSTNO,
            LINE_CONTINUE
        };

        DataView text; // Set when type is TEXT, QUOTE or QUOTELINK
        int64_t quote_postnumber; // Set when type is QUOTELINK
        Type type;
    };

    static TidyAttr get_attribute_by_name(TidyNode node, const char *name) {
        for(TidyAttr attr = tidyAttrFirst(node); attr; attr = tidyAttrNext(attr)) {
            const char *attr_name = tidyAttrName(attr);
            if(attr_name && strcmp(name, attr_name) == 0)
                return attr;
        }
        return nullptr;
    }

    static const char* get_attribute_value(TidyNode node, const char *name) {
        TidyAttr attr = get_attribute_by_name(node, name);
        if(attr)
            return tidyAttrValue(attr);
        return nullptr;
    }

    using CommentPieceCallback = std::function<void(const CommentPiece&)>;
    static void extract_comment_pieces(TidyDoc doc, TidyNode node, CommentPieceCallback callback) {
        for(TidyNode child = tidyGetChild(node); child; child = tidyGetNext(child)) {
            const char *node_name = tidyNodeGetName(child);
            if(node_name && strcmp(node_name, "wbr") == 0) {
                CommentPiece comment_piece;
                comment_piece.type = CommentPiece::Type::LINE_CONTINUE;
                comment_piece.text = { (char*)"", 0 };
                callback(comment_piece);
                continue;
            }
            TidyNodeType node_type = tidyNodeGetType(child);
            if(node_type == TidyNode_Start && node_name) {
                TidyNode text_node = tidyGetChild(child);
                //fprintf(stderr, "Child node name: %s, child text type: %d\n", node_name, tidyNodeGetType(text_node));
                if(tidyNodeGetType(text_node) == TidyNode_Text) {
                    TidyBuffer tidy_buffer;
                    tidyBufInit(&tidy_buffer);
                    if(tidyNodeGetText(doc, text_node, &tidy_buffer)) {
                        CommentPiece comment_piece;
                        comment_piece.type = CommentPiece::Type::TEXT;
                        comment_piece.text = { (char*)tidy_buffer.bp, tidy_buffer.size };
                        if(strcmp(node_name, "span") == 0) {
                            const char *span_class = get_attribute_value(child, "class");
                            //fprintf(stderr, "span class: %s\n", span_class);
                            if(span_class && strcmp(span_class, "quote") == 0)
                                comment_piece.type = CommentPiece::Type::QUOTE;
                        } else if(strcmp(node_name, "a") == 0) {
                            const char *a_class = get_attribute_value(child, "class");
                            const char *a_href = get_attribute_value(child, "href");
                            //fprintf(stderr, "a class: %s, href: %s\n", a_class, a_href);
                            if(a_class && a_href && strcmp(a_class, "quotelink") == 0 && strncmp(a_href, "#p", 2) == 0) {
                                comment_piece.type = CommentPiece::Type::QUOTELINK;
                                comment_piece.quote_postnumber = strtoll(a_href + 2, nullptr, 10);
                            }
                        }
                        callback(comment_piece);
                    }
                    tidyBufFree(&tidy_buffer);
                }
            } else if(node_type == TidyNode_Text) {
                TidyBuffer tidy_buffer;
                tidyBufInit(&tidy_buffer);
                if(tidyNodeGetText(doc, child, &tidy_buffer)) {
                    CommentPiece comment_piece;
                    comment_piece.type = CommentPiece::Type::TEXT;
                    comment_piece.text = { (char*)tidy_buffer.bp, tidy_buffer.size };
                    callback(comment_piece);
                }
                tidyBufFree(&tidy_buffer);
            }
        }
    }

    static void extract_comment_pieces(const char *html_source, size_t size, CommentPieceCallback callback) {
        TidyDoc doc = tidyCreate();
        for(int i = 0; i < N_TIDY_OPTIONS; ++i)
            tidyOptSetBool(doc, (TidyOptionId)i, no);
        if(tidyParseString(doc, html_source) < 0) {
            CommentPiece comment_piece;
            comment_piece.type = CommentPiece::Type::TEXT;
            // Warning: Cast from const char* to char* ...
            comment_piece.text = { (char*)html_source, size };
            callback(comment_piece);
        } else {
            extract_comment_pieces(doc, tidyGetBody(doc), std::move(callback));
        }
        tidyRelease(doc);
    }

    PluginResult Fourchan::get_threads(const std::string &url, BodyItems &result_items) {
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

                    const Json::Value &sub = thread["sub"];
                    const char *sub_begin = "";
                    const char *sub_end = sub_begin;
                    sub.getString(&sub_begin, &sub_end);

                    const Json::Value &com = thread["com"];
                    const char *comment_begin = "";
                    const char *comment_end = comment_begin;
                    com.getString(&comment_begin, &comment_end);

                    const Json::Value &thread_num = thread["no"];
                    if(!thread_num.isNumeric())
                        continue;

                    std::string comment_text;
                    extract_comment_pieces(sub_begin, sub_end - sub_begin,
                        [&comment_text](const CommentPiece &cp) {
                            switch(cp.type) {
                                case CommentPiece::Type::TEXT:
                                    comment_text.append(cp.text.data, cp.text.size);
                                    break;
                                case CommentPiece::Type::QUOTE:
                                    comment_text += '>';
                                    comment_text.append(cp.text.data, cp.text.size);
                                    //comment_text += '\n';
                                    break;
                                case CommentPiece::Type::QUOTELINK: {
                                    comment_text.append(cp.text.data, cp.text.size);
                                    break;
                                }
                                case CommentPiece::Type::LINE_CONTINUE: {
                                    if(!comment_text.empty() && comment_text.back() == '\n') {
                                        comment_text.pop_back();
                                    }
                                    break;
                                }
                            }
                        }
                    );
                    extract_comment_pieces(comment_begin, comment_end - comment_begin,
                        [&comment_text](const CommentPiece &cp) {
                            switch(cp.type) {
                                case CommentPiece::Type::TEXT:
                                    comment_text.append(cp.text.data, cp.text.size);
                                    break;
                                case CommentPiece::Type::QUOTE:
                                    comment_text += '>';
                                    comment_text.append(cp.text.data, cp.text.size);
                                    //comment_text += '\n';
                                    break;
                                case CommentPiece::Type::QUOTELINK: {
                                    comment_text.append(cp.text.data, cp.text.size);
                                    break;
                                }
                                case CommentPiece::Type::LINE_CONTINUE: {
                                    if(!comment_text.empty() && comment_text.back() == '\n') {
                                        comment_text.pop_back();
                                    }
                                    break;
                                }
                            }
                        }
                    );
                    if(!comment_text.empty() && comment_text.back() == '\n')
                        comment_text.back() = ' ';
                    html_unescape_sequences(comment_text);
                    // TODO: Do the same when wrapping is implemented
                    int num_lines = 0;
                    for(size_t i = 0; i < comment_text.size(); ++i) {
                        if(comment_text[i] == '\n') {
                            ++num_lines;
                            if(num_lines == 6) {
                                comment_text = comment_text.substr(0, i) + " (...)";
                                break;
                            }
                        }
                    }
                    auto body_item = std::make_unique<BodyItem>(std::move(comment_text));
                    body_item->url = std::to_string(thread_num.asInt64());

                    const Json::Value &ext = thread["ext"];
                    const Json::Value &tim = thread["tim"];
                    if(tim.isNumeric() && ext.isString()) {
                        std::string ext_str = ext.asString();
                        if(ext_str == ".png" || ext_str == ".jpg" || ext_str == ".jpeg" || ext_str == ".webm" || ext_str == ".mp4" || ext_str == ".gif") {
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

    PluginResult Fourchan::get_thread_comments(const std::string &list_url, const std::string &url, BodyItems &result_items) {
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

        std::unordered_map<int64_t, size_t> comment_by_postno;

        const Json::Value &posts = json_root["posts"];
        if(posts.isArray()) {
            for(const Json::Value &post : posts) {
                if(!post.isObject())
                    continue;

                const Json::Value &post_num = post["no"];
                if(!post_num.isNumeric())
                    continue;
                
                int64_t post_num_int = post_num.asInt64();
                comment_by_postno[post_num_int] = result_items.size();
                result_items.push_back(std::make_unique<BodyItem>(""));
                result_items.back()->post_number = std::to_string(post_num_int);
            }
        }

        size_t body_item_index = 0;
        if(posts.isArray()) {
            for(const Json::Value &post : posts) {
                if(!post.isObject())
                    continue;

                const Json::Value &sub = post["sub"];
                const char *sub_begin = "";
                const char *sub_end = sub_begin;
                sub.getString(&sub_begin, &sub_end);

                const Json::Value &com = post["com"];
                const char *comment_begin = "";
                const char *comment_end = comment_begin;
                com.getString(&comment_begin, &comment_end);

                const Json::Value &post_num = post["no"];
                if(!post_num.isNumeric())
                    continue;

                const Json::Value &author = post["name"];
                std::string author_str = "Anonymous";
                if(author.isString())
                    author_str = author.asString();

                std::string comment_text;
                extract_comment_pieces(sub_begin, sub_end - sub_begin,
                    [&comment_text](const CommentPiece &cp) {
                        switch(cp.type) {
                            case CommentPiece::Type::TEXT:
                                comment_text.append(cp.text.data, cp.text.size);
                                break;
                            case CommentPiece::Type::QUOTE:
                                comment_text += '>';
                                comment_text.append(cp.text.data, cp.text.size);
                                //comment_text += '\n';
                                break;
                            case CommentPiece::Type::QUOTELINK: {
                                comment_text.append(cp.text.data, cp.text.size);
                                break;
                            }
                            case CommentPiece::Type::LINE_CONTINUE: {
                                if(!comment_text.empty() && comment_text.back() == '\n') {
                                    comment_text.pop_back();
                                }
                                break;
                            }
                        }
                    }
                );
                if(!comment_text.empty())
                    comment_text += '\n';
                extract_comment_pieces(comment_begin, comment_end - comment_begin,
                    [&comment_text, &comment_by_postno, &result_items, body_item_index](const CommentPiece &cp) {
                        switch(cp.type) {
                            case CommentPiece::Type::TEXT:
                                comment_text.append(cp.text.data, cp.text.size);
                                break;
                            case CommentPiece::Type::QUOTE:
                                comment_text += '>';
                                comment_text.append(cp.text.data, cp.text.size);
                                //comment_text += '\n';
                                break;
                            case CommentPiece::Type::QUOTELINK: {
                                comment_text.append(cp.text.data, cp.text.size);
                                auto it = comment_by_postno.find(cp.quote_postnumber);
                                if(it == comment_by_postno.end()) {
                                    // TODO: Link this quote to a 4chan archive that still has the quoted comment (if available)
                                    comment_text += "(dead)";
                                } else {
                                    result_items[it->second]->replies.push_back(body_item_index);
                                }
                                break;
                            }
                            case CommentPiece::Type::LINE_CONTINUE: {
                                if(!comment_text.empty() && comment_text.back() == '\n') {
                                    comment_text.pop_back();
                                }
                                break;
                            }
                        }
                    }
                );
                if(!comment_text.empty() && comment_text.back() == '\n')
                    comment_text.back() = ' ';
                html_unescape_sequences(comment_text);
                BodyItem *body_item = result_items[body_item_index].get();
                body_item->set_title(std::move(comment_text));
                body_item->author = std::move(author_str);

                const Json::Value &ext = post["ext"];
                const Json::Value &tim = post["tim"];
                if(tim.isNumeric() && ext.isString()) {
                    std::string ext_str = ext.asString();
                    if(ext_str == ".png" || ext_str == ".jpg" || ext_str == ".jpeg" || ext_str == ".webm" || ext_str == ".mp4" || ext_str == ".gif") {
                    } else {
                        fprintf(stderr, "TODO: Support file extension: %s\n", ext_str.c_str());
                    }
                    // "s" means small, that's the url 4chan uses for thumbnails.
                    // thumbnails always has .jpg extension even if they are gifs or webm.
                    std::string tim_str = std::to_string(tim.asInt64());
                    body_item->thumbnail_url = fourchan_image_url + list_url + "/" + tim_str + "s.jpg";
                    body_item->attached_content_url = fourchan_image_url + list_url + "/" + tim_str + ext_str;
                }
                
                ++body_item_index;
            }
        }

        return PluginResult::OK;
    }

    PostResult Fourchan::post_comment(const std::string &board, const std::string &thread, const std::string &captcha_id, const std::string &comment) {
        std::string url = "https://sys.4chan.org/" + board + "/post";
        std::vector<FormData> form_data = {
            FormData{"resto", thread},
            FormData{"com", comment},
            FormData{"mode", "regist"},
            FormData{"g-recaptcha-response", captcha_id}
        };

        std::vector<CommandArg> additional_args = {
            CommandArg{"-H", "Referer: https://boards.4chan.org/"},
            CommandArg{"-H", "Content-Type: multipart/form-data; boundary=---------------------------119561554312148213571335532670"},
            CommandArg{"-H", "Origin: https://boards.4chan.org"}
        };
        std::vector<CommandArg> form_data_args = create_command_args_from_form_data(form_data);
        additional_args.insert(additional_args.end(), form_data_args.begin(), form_data_args.end());

        std::string response;
        if(download_to_string(url, response, additional_args, use_tor) != DownloadResult::OK)
            return PostResult::ERR;
        
        if(response.find("successful") != std::string::npos)
            return PostResult::OK;
        if(response.find("banned") != std::string::npos)
            return PostResult::BANNED;
        if(response.find("try again") != std::string::npos || response.find("No valid captcha") != std::string::npos)
            return PostResult::TRY_AGAIN;
        return PostResult::ERR;
    }
}