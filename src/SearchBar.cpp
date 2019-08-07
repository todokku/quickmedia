#include "../include/SearchBar.hpp"
#include <cmath>

const sf::Color text_placeholder_color(255, 255, 255, 100);
const sf::Color front_color(43, 45, 47);
const float background_margin_horizontal = 8.0f;
const float background_margin_vertical = 4.0f;
const float padding_horizontal = 10.0f;
const float padding_vertical = 10.0f;

namespace QuickMedia {
    SearchBar::SearchBar(sf::Font &font) :
        onTextUpdateCallback(nullptr),
        onTextSubmitCallback(nullptr),
        text("Search...", font, 18), 
        show_placeholder(true),
        updated_search(false)
    {
        text.setFillColor(text_placeholder_color);
        background.setFillColor(front_color);
        background.setPosition(padding_horizontal, padding_vertical);
        //background.setOutlineThickness(1.0f);
        //background.setOutlineColor(sf::Color(0, 85, 119));
    }

    void SearchBar::draw(sf::RenderWindow &window) {
        window.draw(background);
        window.draw(text);
    }

    void SearchBar::update() {
        if(updated_search && time_since_search_update.getElapsedTime().asMilliseconds() >= 150) {
            updated_search = false;
            sf::String str = text.getString();
            if(show_placeholder)
                str.clear();
            if(onTextUpdateCallback)
                onTextUpdateCallback(str);
        }
    }

    void SearchBar::onWindowResize(const sf::Vector2f &window_size) {
        float font_height = text.getCharacterSize() + 8.0f;
        float rect_height = std::floor(font_height + background_margin_vertical * 2.0f);
        background.setSize(sf::Vector2f(std::floor(window_size.x - padding_horizontal * 2.0f), rect_height));
        text.setPosition(std::floor(padding_horizontal + background_margin_horizontal), std::floor(padding_vertical + background_margin_vertical));
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
            if(onTextSubmitCallback)
                onTextSubmitCallback(text.getString());

            if(!show_placeholder) {
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
        return background.getPosition().y + background.getSize().y;
    }
}