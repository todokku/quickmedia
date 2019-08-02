#include "../include/Program.h"
#include "../plugins/Manganelo.hpp"
#include "../plugins/Youtube.hpp"
#include <SFML/Graphics.hpp>
#include <string>
#include <vector>

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

        void select_previous_item() {
            selected_item = std::max(0, selected_item - 1);
        }

        void select_next_item() {
            const int last_item = std::max(0, (int)items.size() - 1);
            selected_item = std::min(last_item, selected_item + 1);
        }

        void reset_selected() {
            selected_item = 0;
        }

        void clear_items() {
            items.clear();
        }

        void draw(sf::RenderWindow &window, sf::Vector2f pos, sf::Vector2f size) {
            const float font_height = title_text.getCharacterSize() + 8.0f;

            sf::RectangleShape image(sf::Vector2f(50, 50));
            image.setFillColor(sf::Color::White);

            sf::RectangleShape item_background;
            item_background.setFillColor(front_color);
            item_background.setOutlineThickness(1.0f);
            item_background.setOutlineColor(sf::Color(63, 65, 67));

            sf::RectangleShape selected_border(sf::Vector2f(5.0f, 50));
            selected_border.setFillColor(sf::Color::Red);

            int i = 0;
            for(const auto &item : items) {
                sf::Vector2f item_pos = pos;
                if(i == selected_item) {
                    selected_border.setPosition(pos);
                    window.draw(selected_border);
                    item_pos.x += selected_border.getSize().x;
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

        sf::Text title_text;
        int selected_item;
        std::vector<std::unique_ptr<BodyItem>> items;
    };
}

static void search(const sf::String &text, QuickMedia::Body *body, QuickMedia::Plugin *plugin) {
    body->clear_items();
    QuickMedia::SearchResult search_result = plugin->search(text, body->items);
    fprintf(stderr, "Search result: %d\n", search_result);
}

static void update_search_suggestions(const sf::String &text, QuickMedia::Body *body, QuickMedia::Plugin *plugin) {
    body->clear_items();
    QuickMedia::SuggestionResult suggestion_result = plugin->update_search_suggestions(text, body->items);
    fprintf(stderr, "Suggestion result: %d\n", suggestion_result);
}

int main() {
    const float padding_horizontal = 10.0f;
    const float padding_vertical = 10.0f;

    sf::RenderWindow window(sf::VideoMode(800, 800), "SFML works!");
    window.setVerticalSyncEnabled(true);

    sf::Font font;
    if(!font.loadFromFile("fonts/Lato-Regular.ttf")) {
        fprintf(stderr, "Failed to load font!\n");
        abort();
    }

    bool show_placeholder = true;
    sf::Color text_placeholder_color(255, 255, 255, 100);
    sf::Text search_text("Search...", font, 18);
    search_text.setFillColor(text_placeholder_color);

    bool resized = true;
    sf::Vector2f window_size(window.getSize().x, window.getSize().y);

    sf::RectangleShape search_background;
    search_background.setFillColor(front_color);
    search_background.setPosition(padding_horizontal, padding_vertical);
    const float search_background_margin_horizontal = 8.0f;
    const float search_background_margin_vertical = 4.0f;

    sf::RectangleShape body_background;
    body_background.setFillColor(front_color);

    QuickMedia::Body body(font);
    QuickMedia::Manganelo manganelo_plugin;
    QuickMedia::Youtube youtube_plugin;
    QuickMedia::Plugin *plugin = &manganelo_plugin;

    sf::Clock time_since_search_update;
    bool updated_search = false;

    while (window.isOpen()) {
        sf::Event event;

        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed)
                window.close();
            else if(event.type == sf::Event::Resized) {
                window_size.x = event.size.width;
                window_size.y = event.size.height;
                sf::FloatRect visible_area(0, 0, window_size.x, window_size.y);
                window.setView(sf::View(visible_area));
                resized = true;
            } else if(event.type == sf::Event::KeyPressed) {
                if(event.key.code == sf::Keyboard::Up) {
                    body.select_previous_item();
                } else if(event.key.code == sf::Keyboard::Down) {
                    body.select_next_item();
                }
            } else if(event.type == sf::Event::TextEntered) {
                if(event.text.unicode == 8 && !show_placeholder) { // Backspace
                    sf::String str = search_text.getString();
                    if(str.getSize() > 0) {
                        str.erase(str.getSize() - 1, 1);
                        search_text.setString(str);
                        if(str.getSize() == 0) {
                            show_placeholder = true;
                            search_text.setString("Search...");
                            search_text.setFillColor(text_placeholder_color);
                        }
                        updated_search = true;
                        time_since_search_update.restart();
                    }
                } else if(event.text.unicode == 13 && !show_placeholder) { // Return
                    body.reset_selected();
                    search(search_text.getString(), &body, plugin);
                    show_placeholder = true;
                    search_text.setString("Search...");
                    search_text.setFillColor(text_placeholder_color);
                } else if(event.text.unicode > 31) { // Non-control character
                    if(show_placeholder) {
                        show_placeholder = false;
                        search_text.setString("");
                        search_text.setFillColor(sf::Color::White);
                    }
                    sf::String str = search_text.getString();
                    str += event.text.unicode;
                    search_text.setString(str);
                    updated_search = true;
                    time_since_search_update.restart();
                }
            }
        }

        if(updated_search && time_since_search_update.getElapsedTime().asMilliseconds() >= 90) {
            updated_search = false;
            sf::String str = search_text.getString();
            if(show_placeholder)
                str.clear();
            update_search_suggestions(str, &body, plugin);
        }

        if(resized) {
            resized = false;

            float font_height = search_text.getCharacterSize() + 8.0f;
            float rect_height = font_height + search_background_margin_vertical * 2.0f;
            search_background.setSize(sf::Vector2f(window_size.x - padding_horizontal * 2.0f, rect_height));
            search_text.setPosition(padding_horizontal + search_background_margin_horizontal, padding_vertical + search_background_margin_vertical);

            float body_padding_horizontal = 50.0f;
            float body_padding_vertical = 50.0f;
            float body_width = window_size.x - body_padding_horizontal * 2.0f;
            if(body_width < 400) {
                body_width = window_size.x;
                body_padding_horizontal = 0.0f;
            }
            body_background.setPosition(body_padding_horizontal, search_background.getPosition().y + search_background.getSize().y + body_padding_vertical);
            body_background.setSize(sf::Vector2f(body_width, window_size.y));
        }

        window.clear(back_color);
        body.draw(window, body_background.getPosition(), body_background.getSize());
        window.draw(search_background);
        window.draw(search_text);
        window.display();
    }

    return 0;
}

