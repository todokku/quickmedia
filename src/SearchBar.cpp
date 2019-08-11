#include "../include/SearchBar.hpp"
#include "../include/Scale.hpp"
#include <cmath>

const sf::Color text_placeholder_color(255, 255, 255, 100);
const sf::Color front_color(43, 45, 47);
const float background_margin_horizontal = 8.0f;
const float background_margin_vertical = 4.0f;
const float PADDING_HORIZONTAL = 50.0f;
const float padding_vertical = 20.0f;

namespace QuickMedia {
    SearchBar::SearchBar(sf::Font &font, sf::Texture &plugin_logo) :
        onTextUpdateCallback(nullptr),
        onTextSubmitCallback(nullptr),
        text_autosearch_delay(0),
        text("Search...", font, 18), 
        show_placeholder(true),
        updated_search(false),
        draw_logo(false)
    {
        text.setFillColor(text_placeholder_color);
        background.setFillColor(front_color);
        background_shadow.setFillColor(sf::Color(23, 25, 27));
        //background_shadow.setPosition(background.getPosition() + sf::Vector2f(5.0f, 5.0f));
        shade.setFillColor(sf::Color(0, 85, 119));
        //background.setOutlineThickness(1.0f);
        //background.setOutlineColor(sf::Color(13, 15, 17));
        if(plugin_logo.getNativeHandle() != 0)
            plugin_logo_sprite.setTexture(plugin_logo, true);
    }

    void SearchBar::draw(sf::RenderWindow &window) {
        window.draw(background_shadow);
        window.draw(shade);
        window.draw(background);
        window.draw(text);
        if(draw_logo)
            window.draw(plugin_logo_sprite);
    }

    void SearchBar::update() {
        if(updated_search && time_since_search_update.getElapsedTime().asMilliseconds() >= text_autosearch_delay) {
            time_since_search_update.restart();
            updated_search = false;
            sf::String str = text.getString();
            if(show_placeholder)
                str.clear();
            if(onTextUpdateCallback)
                onTextUpdateCallback(str);
        }
    }

    void SearchBar::onWindowResize(const sf::Vector2f &window_size) {
        draw_logo = plugin_logo_sprite.getTexture() != nullptr;
        float padding_horizontal = PADDING_HORIZONTAL;
        if(window_size.x - padding_horizontal * 2.0f < 400.0f) {
            padding_horizontal = 0.0f;
            draw_logo = false;
        }

        float font_height = text.getCharacterSize() + 8.0f;
        float rect_height = std::floor(font_height + background_margin_vertical * 2.0f);

        float offset_x = padding_horizontal;
        if(draw_logo) {
            auto texture_size = plugin_logo_sprite.getTexture()->getSize();
            sf::Vector2f texture_size_f(texture_size.x, texture_size.y);
            sf::Vector2f new_size = wrap_to_size(texture_size_f, sf::Vector2f(200.0f, rect_height));
            plugin_logo_sprite.setScale(get_ratio(texture_size_f, new_size));
            plugin_logo_sprite.setPosition(25.0f, padding_vertical);
            offset_x = 25.0f + new_size.x + 25.0f;
        }
        const float width = std::floor(window_size.x - offset_x - padding_horizontal);

        background.setSize(sf::Vector2f(width, rect_height));
        shade.setSize(sf::Vector2f(window_size.x, padding_vertical + rect_height + padding_vertical));
        background_shadow.setSize(sf::Vector2f(window_size.x, 5.0f));

        background.setPosition(offset_x, padding_vertical);
        background_shadow.setPosition(0.0f, std::floor(shade.getSize().y));
        text.setPosition(std::floor(offset_x + background_margin_horizontal), std::floor(padding_vertical + background_margin_vertical));
    }

    void SearchBar::onTextEntered(sf::Uint32 codepoint) {
        if(codepoint == 8 && !show_placeholder) { // Backspace
            sf::String str = text.getString();
            if(str.getSize() > 0) {
                str.erase(str.getSize() - 1, 1);
                text.setString(str);
                if(str.getSize() == 0) {
                    show_placeholder = true;
                    text.setString("Search...");
                    text.setFillColor(text_placeholder_color);
                }
                updated_search = true;
                time_since_search_update.restart();
            }
        } else if(codepoint == 13) { // Return
            bool clear_search = true;
            if(onTextSubmitCallback)
                clear_search = onTextSubmitCallback(text.getString());

            if(clear_search && !show_placeholder) {
                show_placeholder = true;
                text.setString("Search...");
                text.setFillColor(text_placeholder_color);
            }
        } else if(codepoint > 31) { // Non-control character
            if(show_placeholder) {
                show_placeholder = false;
                text.setString("");
                text.setFillColor(sf::Color::White);
            }
            sf::String str = text.getString();
            str += codepoint;
            text.setString(str);
            updated_search = true;
            time_since_search_update.restart();
        }
    }

    void SearchBar::clear() {
        show_placeholder = true;
        text.setString("Search...");
        text.setFillColor(text_placeholder_color);
    }

    float SearchBar::getBottom() const {
        return shade.getSize().y + background_shadow.getSize().y;
    }
}