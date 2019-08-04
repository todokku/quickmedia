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
        current_page(Page::SEARCH_SUGGESTION)
    {
        window.setVerticalSyncEnabled(true);
        if(!font.loadFromFile("fonts/Lato-Regular.ttf")) {
            fprintf(stderr, "Failed to load font!\n");
            abort();
        }
        body = new Body(font);
        //current_plugin = new Manganelo();
        current_plugin = new Youtube();
        search_bar = std::make_unique<SearchBar>(font);
    }

    Program::~Program() {
        delete body;
        delete current_plugin;
    }

    static SearchResult search_selected_suggestion(Body *body, Plugin *plugin) {
        BodyItem *selected_item = body->get_selected();
        if(!selected_item)
            return SearchResult::ERR;

        std::string selected_item_title = selected_item->title;
        body->clear_items();
        SearchResult search_result = plugin->search(selected_item_title, body->items);
        body->reset_selected();
        return search_result;
    }

    static void update_search_suggestions(const sf::String &text, Body *body, Plugin *plugin) {
        body->clear_items();
        if(text.isEmpty())
            return;

        body->items.push_back(std::make_unique<BodyItem>(text));
        SuggestionResult suggestion_result = plugin->update_search_suggestions(text, body->items);
        body->clamp_selection();
    }

    void Program::run() {
        while(window.isOpen()) {
            switch(current_page) {
                case Page::EXIT:
                    return;
                case Page::SEARCH_SUGGESTION:
                    search_suggestion_page();
                    break;
                case Page::SEARCH_RESULT:
                    search_result_page();
                    break;
                case Page::VIDEO_CONTENT:
                    video_content_page();
                    break;
                default:
                    return;
            }
        }
    }

    void Program::base_event_handler(sf::Event &event) {

    }

    void Program::search_suggestion_page() {
        search_bar->onTextUpdateCallback = [this](const std::string &text) {
            update_search_suggestions(text, body, current_plugin);
        };

        search_bar->onTextSubmitCallback = [this](const std::string &text) {
            if(search_selected_suggestion(body, current_plugin) == SearchResult::OK)
                current_page = Page::SEARCH_RESULT;
        };

        sf::Vector2f body_pos;
        sf::Vector2f body_size;
        bool resized = true;
        sf::Event event;

        while (window.isOpen() && current_page == Page::SEARCH_SUGGESTION) {
            while (window.pollEvent(event)) {
                if (event.type == sf::Event::Closed) {
                    window.close();
                    current_page = Page::EXIT;
                } else if(event.type == sf::Event::Resized) {
                    window_size.x = event.size.width;
                    window_size.y = event.size.height;
                    sf::FloatRect visible_area(0, 0, window_size.x, window_size.y);
                    window.setView(sf::View(visible_area));
                    resized = true;
                } else if(event.type == sf::Event::KeyPressed) {
                    if(event.key.code == sf::Keyboard::Up) {
                        body->select_previous_item();
                    } else if(event.key.code == sf::Keyboard::Down) {
                        body->select_next_item();
                    } else if(event.key.code == sf::Keyboard::Escape) {
                        current_page = Page::EXIT;
                        window.close();
                    }
                } else if(event.type == sf::Event::TextEntered) {
                    search_bar->onTextEntered(event.text.unicode);
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

        while (window.isOpen() && current_page == Page::SEARCH_RESULT) {
            while (window.pollEvent(event)) {
                if (event.type == sf::Event::Closed) {
                    window.close();
                    current_page = Page::EXIT;
                } else if(event.type == sf::Event::Resized) {
                    window_size.x = event.size.width;
                    window_size.y = event.size.height;
                    sf::FloatRect visible_area(0, 0, window_size.x, window_size.y);
                    window.setView(sf::View(visible_area));
                    resized = true;
                } else if(event.type == sf::Event::KeyPressed) {
                    if(event.key.code == sf::Keyboard::Up) {
                        body->select_previous_item();
                    } else if(event.key.code == sf::Keyboard::Down) {
                        body->select_next_item();
                    } else if(event.key.code == sf::Keyboard::Escape) {
                        current_page = Page::SEARCH_SUGGESTION;
                        body->clear_items();
                        body->reset_selected();
                        search_bar->clear();
                    }
                } else if(event.type == sf::Event::TextEntered) {
                    search_bar->onTextEntered(event.text.unicode);
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
            video_player = std::make_unique<VideoPlayer>(window_size.x, window_size.y, video_url.c_str());
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

        while (window.isOpen() && current_page == Page::VIDEO_CONTENT) {
            while (window.pollEvent(event)) {
                if (event.type == sf::Event::Closed) {
                    window.close();
                    current_page = Page::EXIT;
                } else if(event.type == sf::Event::Resized) {
                    window_size.x = event.size.width;
                    window_size.y = event.size.height;
                    sf::FloatRect visible_area(0, 0, window_size.x, window_size.y);
                    window.setView(sf::View(visible_area));
                    if(video_player)
                        video_player->resize(sf::Vector2i(window_size.x, window_size.y));
                } else if(event.type == sf::Event::KeyPressed) {
                    if(event.key.code == sf::Keyboard::Escape) {
                        current_page = Page::SEARCH_SUGGESTION;
                        body->clear_items();
                        body->reset_selected();
                        search_bar->clear();
                    }
                }
            }

            window.clear();
            if(video_player)
                video_player->draw(window);
            window.display();
        }
    }
}