#include "../include/QuickMedia.hpp"
#include "../plugins/Manganelo.hpp"
#include "../plugins/Youtube.hpp"
#include "../include/VideoPlayer.hpp"

#include <SFML/Graphics/RectangleShape.hpp>
#include <SFML/Graphics/Text.hpp>
#include <SFML/Window/Event.hpp>
#include <assert.h>

const sf::Color front_color(43, 45, 47);
const sf::Color back_color(33, 35, 37);

namespace QuickMedia {
    class Body {
    public:
        Body(sf::Font &font) : title_text("", font, 14), selected_item(0) {
            title_text.setFillColor(sf::Color::White);
        }

        void add_item(std::unique_ptr<BodyItem> item) {
            items.push_back(std::move(item));
        }

        // Select previous item, ignoring invisible items
        void select_previous_item() {
            if(items.empty())
                return;

            int num_items = (int)items.size();
            for(int i = 0; i < num_items; ++i) {
                --selected_item;
                if(selected_item < 0)
                    selected_item = num_items - 1;
                if(items[selected_item]->visible)
                    return;
            }
        }

        // Select next item, ignoring invisible items
        void select_next_item() {
            if(items.empty())
                return;

            int num_items = (int)items.size();
            for(int i = 0; i < num_items; ++i) {
                ++selected_item;
                if(selected_item == num_items)
                    selected_item = 0;
                if(items[selected_item]->visible)
                    return;
            }
        }

        void reset_selected() {
            selected_item = 0;
        }

        void clear_items() {
            items.clear();
        }

        BodyItem* get_selected() const {
            if(items.empty() || !items[selected_item]->visible)
                return nullptr;
            return items[selected_item].get();
        }

        void clamp_selection() {
            int num_items = (int)items.size();
            if(items.empty())
                return;

            if(selected_item < 0)
                selected_item = 0;
            else if(selected_item >= num_items)
                selected_item = num_items - 1;

            for(int i = selected_item; i >= 0; --i) {
                if(items[i]->visible) {
                    selected_item = i;
                    return;
                }
            }

            for(int i = selected_item; i < num_items; ++i) {
                if(items[i]->visible) {
                    selected_item = i;
                    return;
                }
            }
        }

        void draw(sf::RenderWindow &window, sf::Vector2f pos, sf::Vector2f size) {
            const float font_height = title_text.getCharacterSize() + 8.0f;
            const float image_height = 50.0f;

            sf::RectangleShape image(sf::Vector2f(50, image_height));
            image.setFillColor(sf::Color::White);

            sf::RectangleShape item_background;
            item_background.setFillColor(front_color);
            item_background.setOutlineThickness(1.0f);
            item_background.setOutlineColor(sf::Color(63, 65, 67));

            sf::RectangleShape selected_border(sf::Vector2f(5.0f, 50));
            selected_border.setFillColor(sf::Color::Red);

            int i = 0;
            for(const auto &item : items) {
                if(!item->visible) {
                    ++i;
                    continue;
                }

                sf::Vector2f item_pos = pos;
                if(i == selected_item) {
                    selected_border.setPosition(pos);
                    window.draw(selected_border);
                    item_pos.x += selected_border.getSize().x;
                    item_background.setFillColor(front_color);
                } else {
                    item_background.setFillColor(sf::Color(38, 40, 42));
                }

                item_background.setPosition(item_pos);
                item_background.setSize(sf::Vector2f(size.x, 50));
                window.draw(item_background);

                image.setPosition(item_pos);
                window.draw(image);

                title_text.setString(item->title);
                title_text.setPosition(item_pos.x + 50 + 10, item_pos.y);
                window.draw(title_text);


                pos.y += 50 + 10;
                ++i;
            }
        }

        static bool string_find_case_insensitive(const std::string &str, const std::string &substr) {
            auto it = std::search(str.begin(), str.end(), substr.begin(), substr.end(),
                [](char c1, char c2) {
                    return std::toupper(c1) == std::toupper(c2);
                });
            return it != str.end();
        }

        // TODO: Make this actually fuzzy... Right now it's just a case insensitive string find.
        // TODO: Highlight the part of the text that matches the search
        void filter_search_fuzzy(const std::string &text) {
            if(text.empty()) {
                for(auto &item : items) {
                    item->visible = true;
                }
                return;
            }

            for(auto &item : items) {
                item->visible = string_find_case_insensitive(item->title, text);
            }
        }

        sf::Text title_text;
        int selected_item;
        std::vector<std::unique_ptr<BodyItem>> items;
    };

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
        current_plugin = new Manganelo();
        //current_plugin = new Youtube();
        search_bar = std::make_unique<SearchBar>(font);
    }

    Program::~Program() {
        delete body;
        delete current_plugin;
    }

    static SearchResult search_selected_suggestion(Body *body, Plugin *plugin, Page &next_page) {
        BodyItem *selected_item = body->get_selected();
        if(!selected_item)
            return SearchResult::ERR;

        std::string selected_item_title = selected_item->title;
        std::string selected_item_url = selected_item->url;
        body->clear_items();
        SearchResult search_result = plugin->search(!selected_item_url.empty() ? selected_item_url : selected_item_title, body->items, next_page);
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

    void Program::run() {
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
                case Page::IMAGES:
                    image_page();
                    break;
                default:
                    return;
            }
        }
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

    void Program::search_suggestion_page() {
        search_bar->onTextUpdateCallback = [this](const std::string &text) {
            update_search_suggestions(text, body, current_plugin);
        };

        search_bar->onTextSubmitCallback = [this](const std::string &text) {
            Page next_page;
            if(search_selected_suggestion(body, current_plugin, next_page) == SearchResult::OK)
                current_page = next_page;
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
            image_index = 0;
            current_page = Page::IMAGES;
        };

        sf::Vector2f body_pos;
        sf::Vector2f body_size;
        bool resized = true;
        sf::Event event;

        while (current_page == Page::EPISODE_LIST) {
            while (window.pollEvent(event)) {
                base_event_handler(event, Page::SEARCH_SUGGESTION);
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
                error_message.setString(std::string("Failed to load image for page ") + std::to_string(image_index));
            }
        } else if(image_result == ImageResult::END) {
            // TODO: Better error message, with chapter name
            error_message.setString("End of chapter");
        } else {
            // TODO: Convert ImageResult error to a string and show to user
            error_message.setString(std::string("Network error, failed to get image for page ") + std::to_string(image_index));
        }
        image_data.resize(0);

        bool error = !error_message.getString().isEmpty();
        bool resized = true;
        sf::Event event;

        // TODO: Show current page / number of pages.
        // TODO: Show to user if a certain page is missing (by checking page name (number) and checking if some is skipped)
        while (current_page == Page::IMAGES) {
            while (window.pollEvent(event)) {
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
                        if(!error) {
                            ++image_index;
                            return;
                        }
                    } else if(event.key.code == sf::Keyboard::Escape) {
                        current_page = Page::EPISODE_LIST;
                    }
                }
            }

            if(resized) {
                if(error) {
                    auto bounds = error_message.getLocalBounds();
                    error_message.setPosition(window_size.x * 0.5f - bounds.width * 0.5f, window_size.y * 0.5f - bounds.height);
                } else {
                    auto texture_size = image.getTexture()->getSize();
                    auto image_scale = image.getScale();
                    auto image_size = sf::Vector2f(texture_size.x, texture_size.y);
                    image_size.x *= image_scale.x;
                    image_size.y *= image_scale.y;

                    image.setPosition(window_size.x * 0.5f - image_size.x * 0.5f, window_size.y * 0.5f - image_size.y * 0.5f);
                }
            }

            window.clear(back_color);
            if(error) {
                window.draw(error_message);
            } else {
                window.draw(image);
            }
            window.display();
        }
    }
}