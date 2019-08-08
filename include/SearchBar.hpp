#pragma once

#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Graphics/Font.hpp>
#include <SFML/Graphics/Text.hpp>
#include <SFML/Graphics/RectangleShape.hpp>
#include <functional>

namespace QuickMedia {
    using TextUpdateCallback = std::function<void(const sf::String &text)>;
    using TextSubmitCallback = std::function<void(const sf::String &text)>;

    class SearchBar {
    public:
        SearchBar(sf::Font &font);
        void draw(sf::RenderWindow &window);
        void update();
        void onWindowResize(const sf::Vector2f &window_size);
        void onTextEntered(sf::Uint32 codepoint);
        void clear();

        float getBottom() const;

        TextUpdateCallback onTextUpdateCallback;
        TextSubmitCallback onTextSubmitCallback;
        int text_autosearch_delay;
    private:
        sf::Text text;
        sf::RectangleShape background;
        bool show_placeholder;
        bool updated_search;
        sf::Clock time_since_search_update;
    };
}