#include "../include/QuickMedia.hpp"
#include "../plugins/Manganelo.hpp"
#include "../plugins/Youtube.hpp"
#include "../include/VideoPlayer.hpp"
#include "../include/Program.h"
#include <cppcodec/base64_rfc4648.hpp>

#include <SFML/Graphics/RectangleShape.hpp>
#include <SFML/Graphics/Text.hpp>
#include <SFML/Window/Event.hpp>
#include <json/reader.h>
#include <json/writer.h>
#include <assert.h>
#include <cmath>
#include <string.h>

const sf::Color front_color(43, 45, 47);
const sf::Color back_color(33, 35, 37);

namespace QuickMedia {
    Program::Program() :
        window(sf::VideoMode(800, 600), "QuickMedia"),
        window_size(800, 600),
        body(nullptr),
        current_plugin(nullptr),
        current_page(Page::SEARCH_SUGGESTION),
        image_index(0)
    {
        window.setVerticalSyncEnabled(true);
        if(!font.loadFromFile("fonts/Lato-Regular.ttf")) {
            fprintf(stderr, "Failed to load font!\n");
            abort();
        }
        body = new Body(font);
        search_bar = std::make_unique<SearchBar>(font);
    }

    Program::~Program() {
        delete body;
        delete current_plugin;
    }

    static SearchResult search_selected_suggestion(Body *body, Plugin *plugin, Page &next_page, std::string &selected_title, std::string &selected_url) {
        BodyItem *selected_item = body->get_selected();
        if(!selected_item)
            return SearchResult::ERR;

        selected_title = selected_item->title;
        selected_url = selected_item->url;
        body->clear_items();
        SearchResult search_result = plugin->search(!selected_url.empty() ? selected_url : selected_title, body->items, next_page);
        body->reset_selected();
        return search_result;
    }

    static void update_search_suggestions(const sf::String &text, Body *body, Plugin *plugin) {
        body->clear_items();
        if(text.isEmpty())
            return;

        SuggestionResult suggestion_result = plugin->update_search_suggestions(text, body->items);
        body->clamp_selection();
    }

    static void usage() {
        fprintf(stderr, "usage: QuickMedia <plugin>\n");
        fprintf(stderr, "OPTIONS:\n");
        fprintf(stderr, "plugin    The plugin to use. Should be either manganelo or youtube\n");
    }

    int Program::run(int argc, char **argv) {
        if(argc < 2) {
            usage();
            return -1;
        }

        if(strcmp(argv[1], "manganelo") == 0)
            current_plugin = new Manganelo();
        else if(strcmp(argv[1], "youtube") == 0)
            current_plugin = new Youtube();
        else {
            usage();
            return -1;
        }

        while(window.isOpen()) {
            switch(current_page) {
                case Page::EXIT:
                    window.close();
                    break;
                case Page::SEARCH_SUGGESTION:
                    search_suggestion_page();
                    break;
                case Page::SEARCH_RESULT:
                    search_result_page();
                    break;
                case Page::VIDEO_CONTENT:
                    video_content_page();
                    break;
                case Page::EPISODE_LIST:
                    episode_list_page();
                    break;
                case Page::IMAGES: {
                    window.setKeyRepeatEnabled(false);
                    image_page();
                    window.setKeyRepeatEnabled(true);
                    break;
                }
                default:
                    window.close();
                    break;
            }
        }

        return 0;
    }

    void Program::base_event_handler(sf::Event &event, Page previous_page) {
        if (event.type == sf::Event::Closed) {
            current_page = Page::EXIT;
        } else if(event.type == sf::Event::Resized) {
            window_size.x = event.size.width;
            window_size.y = event.size.height;
            sf::FloatRect visible_area(0, 0, window_size.x, window_size.y);
            window.setView(sf::View(visible_area));
        } else if(event.type == sf::Event::KeyPressed) {
            if(event.key.code == sf::Keyboard::Up) {
                body->select_previous_item();
            } else if(event.key.code == sf::Keyboard::Down) {
                body->select_next_item();
            } else if(event.key.code == sf::Keyboard::Escape) {
                current_page = previous_page;
                body->clear_items();
                body->reset_selected();
                search_bar->clear();
            }
        } else if(event.type == sf::Event::TextEntered) {
            search_bar->onTextEntered(event.text.unicode);
        }
    }

    static std::string base64_encode(const std::string &data) {
        return cppcodec::base64_rfc4648::encode(data);
    }

    static bool get_manga_storage_json(const Path &storage_path, Json::Value &result) {
        std::string file_content;
        if(file_get_content(storage_path, file_content) != 0)
            return -1;

        Json::CharReaderBuilder json_builder;
        std::unique_ptr<Json::CharReader> json_reader(json_builder.newCharReader());
        std::string json_errors;
        if(json_reader->parse(&file_content.front(), &file_content.back(), &result, &json_errors)) {
            fprintf(stderr, "Failed to read json, error: %s\n", json_errors.c_str());
            return -1;
        }

        return 0;
    }

    static bool save_manga_progress_json(const Path &path, const Json::Value &json) {
        Json::StreamWriterBuilder json_builder;
        return file_overwrite(path, Json::writeString(json_builder, json)) == 0;
    }

    void Program::search_suggestion_page() {
        search_bar->onTextUpdateCallback = [this](const std::string &text) {
            update_search_suggestions(text, body, current_plugin);
        };

        search_bar->onTextSubmitCallback = [this](const std::string &text) {
            Page next_page;
            if(search_selected_suggestion(body, current_plugin, next_page, content_title, content_url) == SearchResult::OK) {
                if(next_page == Page::EPISODE_LIST) {
                    Path content_storage_dir = get_storage_dir().join("manga");
                    if(create_directory_recursive(content_storage_dir) != 0) {
                        // TODO: Show this to the user
                        fprintf(stderr, "Failed to create directory: %s\n", content_storage_dir.data.c_str());
                        return;
                    }

                    content_storage_file = content_storage_dir.join(base64_encode(content_title));
                    content_storage_json.clear();
                    content_storage_json["name"] = content_title;
                    FileType file_type = get_file_type(content_storage_file);
                    if(file_type == FileType::REGULAR)
                        get_manga_storage_json(content_storage_file, content_storage_json);
                }
                current_page = next_page;
            }
        };

        sf::Vector2f body_pos;
        sf::Vector2f body_size;
        bool resized = true;
        sf::Event event;

        while (current_page == Page::SEARCH_SUGGESTION) {
            while (window.pollEvent(event)) {
                base_event_handler(event, Page::EXIT);
                if(event.type == sf::Event::Resized)
                    resized = true;
            }

            if(resized) {
                search_bar->onWindowResize(window_size);

                float body_padding_horizontal = 50.0f;
                float body_padding_vertical = 50.0f;
                float body_width = window_size.x - body_padding_horizontal * 2.0f;
                if(body_width < 400) {
                    body_width = window_size.x;
                    body_padding_horizontal = 0.0f;
                }

                float search_bottom = search_bar->getBottom();
                body_pos = sf::Vector2f(body_padding_horizontal, search_bottom + body_padding_vertical);
                body_size = sf::Vector2f(body_width, window_size.y);
            }

            search_bar->update();

            window.clear(back_color);
            body->draw(window, body_pos, body_size);
            search_bar->draw(window);
            window.display();
        }
    }

    void Program::search_result_page() {
        search_bar->onTextUpdateCallback = [this](const std::string &text) {
            body->filter_search_fuzzy(text);
            body->clamp_selection();
        };

        search_bar->onTextSubmitCallback = [this](const std::string &text) {
            BodyItem *selected_item = body->get_selected();
            if(!selected_item)
                return;
            video_url = selected_item->url;
            current_page = Page::VIDEO_CONTENT;
        };

        sf::Vector2f body_pos;
        sf::Vector2f body_size;
        bool resized = true;
        sf::Event event;

        while (current_page == Page::SEARCH_RESULT) {
            while (window.pollEvent(event)) {
                base_event_handler(event, Page::SEARCH_SUGGESTION);
            }

            if(resized) {
                search_bar->onWindowResize(window_size);

                float body_padding_horizontal = 50.0f;
                float body_padding_vertical = 50.0f;
                float body_width = window_size.x - body_padding_horizontal * 2.0f;
                if(body_width < 400) {
                    body_width = window_size.x;
                    body_padding_horizontal = 0.0f;
                }

                float search_bottom = search_bar->getBottom();
                body_pos = sf::Vector2f(body_padding_horizontal, search_bottom + body_padding_vertical);
                body_size = sf::Vector2f(body_width, window_size.y);
            }

            search_bar->update();

            window.clear(back_color);
            body->draw(window, body_pos, body_size);
            search_bar->draw(window);
            window.display();
        }
    }

    void Program::video_content_page() {
        search_bar->onTextUpdateCallback = nullptr;
        search_bar->onTextSubmitCallback = nullptr;

        std::unique_ptr<VideoPlayer> video_player;
        try {
            printf("Play video: %s\n", video_url.c_str());
            video_player = std::make_unique<VideoPlayer>(window_size.x, window_size.y, window.getSystemHandle(), video_url.c_str());
        } catch(VideoInitializationException &e) {
            fprintf(stderr, "Failed to create video player!. TODO: Show this to the user");
        }

        std::vector<std::unique_ptr<BodyItem>> related_media = current_plugin->get_related_media(video_url);

        if(video_player) {
            video_player->onPlaybackEndedCallback = [this, &related_media, &video_player]() {
                if(related_media.empty())
                    return;
                video_url = related_media.front()->url;
                video_player->load_file(video_url);
                related_media = current_plugin->get_related_media(video_url);
            };
        }

        sf::Clock resize_timer;
        sf::Event event;

        while (current_page == Page::VIDEO_CONTENT) {
            while (window.pollEvent(event)) {
                base_event_handler(event, Page::SEARCH_SUGGESTION);
                if(event.type == sf::Event::Resized) {
                    if(video_player)
                        video_player->resize(sf::Vector2i(window_size.x, window_size.y));
                }
            }

            window.clear();
            if(video_player)
                video_player->draw(window);
            window.display();
        }
    }

    enum class TrackMediaType {
        RSS,
        HTML
    };

    const char* track_media_type_string(TrackMediaType media_type) {
        switch(media_type) {
            case TrackMediaType::RSS:
                return "rss";
            case TrackMediaType::HTML:
                return "html";
        }
        assert(false);
        return "";
    }

    static int track_media(TrackMediaType media_type, const std::string &manga_title, const std::string &chapter_title, const std::string &url) {
        const char *args[] = { "automedia.py", "add", track_media_type_string(media_type), url.data(), "--start-after", chapter_title.data(), "--name", manga_title.data(), nullptr };
        return exec_program(args, nullptr, nullptr);
    }

    void Program::episode_list_page() {
        search_bar->onTextUpdateCallback = [this](const std::string &text) {
            body->filter_search_fuzzy(text);
            body->clamp_selection();
        };

        search_bar->onTextSubmitCallback = [this](const std::string &text) {
            BodyItem *selected_item = body->get_selected();
            if(!selected_item)
                return;

            images_url = selected_item->url;
            chapter_title = selected_item->title;
            image_index = 0;
            current_page = Page::IMAGES;

            const Json::Value &json_chapters = content_storage_json["chapters"];
            if(json_chapters.isObject()) {
                const Json::Value &json_chapter = json_chapters[chapter_title];
                if(json_chapter.isObject()) {
                    const Json::Value &current = json_chapter["current"];
                    if(current.isNumeric())
                        image_index = current.asInt() - 1;
                }
            }
    
        };

        const Json::Value &json_chapters = content_storage_json["chapters"];

        sf::Vector2f body_pos;
        sf::Vector2f body_size;
        bool resized = true;
        sf::Event event;

        while (current_page == Page::EPISODE_LIST) {
            while (window.pollEvent(event)) {
                base_event_handler(event, Page::SEARCH_SUGGESTION);
                if(event.type == sf::Event::Resized)
                    resized = true;
                else if(event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::T && sf::Keyboard::isKeyPressed(sf::Keyboard::LControl)) {
                    BodyItem *selected_item = body->get_selected();
                    if(selected_item) {
                        if(track_media(TrackMediaType::HTML, content_title, selected_item->title, content_url) != 0) {
                            // TODO: Show this to the user
                            fprintf(stderr, "Failed to track media. Url: %s, title: %s\n", selected_item->url.c_str(), selected_item->title.c_str());
                        }
                    }
                }
            }

            if(resized) {
                search_bar->onWindowResize(window_size);

                float body_padding_horizontal = 50.0f;
                float body_padding_vertical = 50.0f;
                float body_width = window_size.x - body_padding_horizontal * 2.0f;
                if(body_width < 400) {
                    body_width = window_size.x;
                    body_padding_horizontal = 0.0f;
                }

                float search_bottom = search_bar->getBottom();
                body_pos = sf::Vector2f(body_padding_horizontal, search_bottom + body_padding_vertical);
                body_size = sf::Vector2f(body_width, window_size.y);
            }

            search_bar->update();

            window.clear(back_color);
            body->draw(window, body_pos, body_size, json_chapters);
            search_bar->draw(window);
            window.display();
        }
    }

    // TODO: Add option to scale image to window size.
    // TODO: Fix scaling, it'ss a bit broken
    static void clamp_sprite_to_size(sf::Sprite &sprite, const sf::Vector2f &size) {
        auto texture_size = sprite.getTexture()->getSize();
        auto image_size = sf::Vector2f(texture_size.x, texture_size.y);

        double overflow_x  = image_size.x - size.x;
        double overflow_y  = image_size.y - size.y;
        if(overflow_x <= 0.0f && overflow_y <= 0.0f) {
            sprite.setScale(1.0f, 1.0f);
            return;
        }

        auto scale = sprite.getScale();
        float scale_ratio = scale.x / scale.y;

        if(overflow_x * scale_ratio > overflow_y) {
            float overflow_ratio = overflow_x / image_size.x;
            float scale_x = 1.0f - overflow_ratio;
            sprite.setScale(scale_x, scale_x * scale_ratio);
        } else {
            float overflow_ratio = overflow_y / image_size.y;
            float scale_y = 1.0f - overflow_ratio;
            sprite.setScale(scale_y * scale_ratio, scale_y);
        }
    }

    void Program::image_page() {
        search_bar->onTextUpdateCallback = nullptr;
        search_bar->onTextSubmitCallback = nullptr;

        sf::Texture image_texture;
        sf::Sprite image;
        sf::Text error_message("", font, 30);
        error_message.setFillColor(sf::Color::White);

        Manganelo *image_plugin = static_cast<Manganelo*>(current_plugin);
        std::string image_data;

        // TODO: Optimize this somehow. One image alone uses more than 20mb ram! Total ram usage for viewing one image
        // becomes 40mb (private memory, almost 100mb in total!) Unacceptable!
        ImageResult image_result = image_plugin->get_image_by_index(images_url, image_index, image_data);
        if(image_result == ImageResult::OK) {
            if(image_texture.loadFromMemory(image_data.data(), image_data.size())) {
                image_texture.setSmooth(true);
                image.setTexture(image_texture, true);
            } else {
                error_message.setString(std::string("Failed to load image for page ") + std::to_string(image_index + 1));
            }
        } else if(image_result == ImageResult::END) {
            // TODO: Improve this message
            error_message.setString("End of chapter");
        } else {
            // TODO: Convert ImageResult error to a string and show to user
            error_message.setString(std::string("Network error, failed to get image for page ") + std::to_string(image_index + 1));
        }
        image_data.resize(0);

        int num_images = 0;
        image_plugin->get_number_of_images(images_url, num_images);

        Json::Value &json_chapters = content_storage_json["chapters"];
        Json::Value json_chapter;
        int latest_read = image_index + 1;
        if(json_chapters.isObject()) {
            json_chapter = json_chapters[chapter_title];
            if(json_chapter.isObject()) {
                const Json::Value &current = json_chapter["current"];
                if(current.isNumeric())
                    latest_read = std::max(latest_read, current.asInt());
            }
        } else {
            json_chapters = Json::Value(Json::objectValue);
            json_chapter = Json::Value(Json::objectValue);
        }
        json_chapter["current"] = std::min(latest_read, num_images);
        json_chapter["total"] = num_images;
        json_chapters[chapter_title] = json_chapter;
        if(!save_manga_progress_json(content_storage_file, content_storage_json)) {
            // TODO: Show this to the user
            fprintf(stderr, "Failed to save manga progress!\n");
        }

        bool error = !error_message.getString().isEmpty();
        bool resized = true;
        sf::Event event;

        sf::Text chapter_text(std::string("Page ") + std::to_string(image_index + 1) + "/" + std::to_string(num_images), font, 14);
        if(image_index == num_images)
            chapter_text.setString("End");
        chapter_text.setFillColor(sf::Color::White);
        sf::RectangleShape chapter_text_background;
        chapter_text_background.setFillColor(sf::Color(0, 0, 0, 150));

        // TODO: Show to user if a certain page is missing (by checking page name (number) and checking if some are skipped)
        while (current_page == Page::IMAGES) {
            if(window.waitEvent(event)) {
                if (event.type == sf::Event::Closed) {
                    current_page = Page::EXIT;
                } else if(event.type == sf::Event::Resized) {
                    window_size.x = event.size.width;
                    window_size.y = event.size.height;
                    sf::FloatRect visible_area(0, 0, window_size.x, window_size.y);
                    window.setView(sf::View(visible_area));
                    resized = true;
                } else if(event.type == sf::Event::KeyPressed) {
                    if(event.key.code == sf::Keyboard::Up) {
                        if(image_index > 0) {
                            --image_index;
                            return;
                        }
                    } else if(event.key.code == sf::Keyboard::Down) {
                        if(image_index < num_images) {
                            ++image_index;
                            return;
                        }
                    } else if(event.key.code == sf::Keyboard::Escape) {
                        current_page = Page::EPISODE_LIST;
                    }
                }
            }

            const float font_height = chapter_text.getCharacterSize() + 8.0f;
            const float background_height = font_height + 6.0f;

            sf::Vector2f content_size;
            content_size.x = window_size.x;
            content_size.y = window_size.y - background_height;

            if(resized) {
                if(error) {
                    auto bounds = error_message.getLocalBounds();
                    error_message.setPosition(std::floor(content_size.x * 0.5f - bounds.width * 0.5f), std::floor(content_size.y * 0.5f - bounds.height));
                } else {
                    clamp_sprite_to_size(image, content_size);
                    auto texture_size = image.getTexture()->getSize();
                    auto image_scale = image.getScale();
                    auto image_size = sf::Vector2f(texture_size.x, texture_size.y);
                    image_size.x *= image_scale.x;
                    image_size.y *= image_scale.y;
                    image.setPosition(std::floor(content_size.x * 0.5f - image_size.x * 0.5f), std::floor(content_size.y * 0.5f - image_size.y * 0.5f));
                }
            } else {
                continue;
            }

            window.clear(back_color);

            if(error) {
                window.draw(error_message);
            } else {
                window.draw(image);
            }

            chapter_text_background.setSize(sf::Vector2f(window_size.x, background_height));
            chapter_text_background.setPosition(0.0f, std::floor(window_size.y - background_height));
            window.draw(chapter_text_background);

            auto text_bounds = chapter_text.getLocalBounds();
            chapter_text.setPosition(std::floor(window_size.x * 0.5f - text_bounds.width * 0.5f), std::floor(window_size.y - background_height * 0.5f - font_height * 0.5f));
            window.draw(chapter_text);

            window.display();
        }
    }
}