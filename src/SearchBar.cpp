#include "../include/SearchBar.hpp"
#include "../include/Scale.hpp"
#include <cmath>
#include <assert.h>

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
        draw_logo(false),
        needs_update(false)
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

    void SearchBar::draw(sf::RenderWindow &window, bool draw_shadow) {
        if(needs_update) {
            needs_update = false;
            sf::Vector2u window_size = window.getSize();
            onWindowResize(sf::Vector2f(window_size.x, window_size.y));
        }
        if(draw_shadow)
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

        float font_height = text.getLocalBounds().height + 8.0f;
        float rect_height = std::floor(font_height + background_margin_vertical * 2.0f);

        float offset_x = padding_horizontal;
        if(draw_logo) {
            auto texture_size = plugin_logo_sprite.getTexture()->getSize();
            sf::Vector2f texture_size_f(texture_size.x, texture_size.y);
            sf::Vector2f new_size = wrap_to_size(texture_size_f, sf::Vector2f(200.0f, text.getCharacterSize() + 8.0f));
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
                // TODO: When it's possible to move the cursor, then check at cursor position instead of end of the string
                if(str[str.getSize() - 1] == '\n')
                    needs_update = true;
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

            if(clear_search)
                clear();
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
        } else if(codepoint == '\n')
            needs_update = true;
    }

    void SearchBar::clear() {
        if(show_placeholder)
            return;
        show_placeholder = true;
        text.setString("Search...");
        text.setFillColor(text_placeholder_color);
        needs_update = true;
    }

    void SearchBar::append_text(const std::string &text_to_add) {
        if(show_placeholder) {
            show_placeholder = false;
            text.setString("");
            text.setFillColor(sf::Color::White);
        }
        sf::String str = text.getString();
        str += text_to_add;
        text.setString(str);
        updated_search = true;
        time_since_search_update.restart();
        needs_update = true;
    }

    bool SearchBar::is_cursor_at_start_of_line() const {
        // TODO: When it's possible to move the cursor, then check at the cursor position instead of end of the string
        const sf::String &str = text.getString();
        return show_placeholder || str.getSize() == 0 || str[str.getSize() - 1] == '\n';
    }

    float SearchBar::getBottom() const {
        return shade.getSize().y + background_shadow.getSize().y;
    }

    float SearchBar::getBottomWithoutShadow() const {
        return shade.getSize().y;
    }
}