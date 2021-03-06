#pragma once

#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Graphics/Font.hpp>
#include <SFML/Graphics/Text.hpp>
#include <SFML/Graphics/RectangleShape.hpp>
#include <SFML/Graphics/Sprite.hpp>
#include <functional>

namespace QuickMedia {
    using TextUpdateCallback = std::function<void(const sf::String &text)>;
    // Return true to consume the search (clear the search field)
    using TextSubmitCallback = std::function<bool(const sf::String &text)>;

    class SearchBar {
    public:
        SearchBar(sf::Font &font, sf::Texture &plugin_logo);
        void draw(sf::RenderWindow &window, bool draw_shadow = true);
        void update();
        void onWindowResize(const sf::Vector2f &window_size);
        void onTextEntered(sf::Uint32 codepoint);
        void clear();
        void append_text(const std::string &text_to_add);
        bool is_cursor_at_start_of_line() const;

        float getBottom() const;
        float getBottomWithoutShadow() const;

        TextUpdateCallback onTextUpdateCallback;
        TextSubmitCallback onTextSubmitCallback;
        int text_autosearch_delay;
    private:
        sf::Text text;
        sf::RectangleShape background;
        sf::RectangleShape background_shadow;
        sf::RectangleShape shade;
        sf::Sprite plugin_logo_sprite;
        bool show_placeholder;
        bool updated_search;
        bool draw_logo;
        bool needs_update;
        sf::Clock time_since_search_update;
    };
}