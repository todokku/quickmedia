#pragma once

#include <SFML/Graphics/Font.hpp>
#include <SFML/Graphics/Text.hpp>
#include <SFML/Graphics/Texture.hpp>
#include <SFML/Graphics/RenderWindow.hpp>

namespace QuickMedia {
    class BodyItem {
    public:
        BodyItem(const std::string &_title): title(_title), visible(true) {

        }

        std::string title;
        std::string url;
        std::string thumbnail_url;
        bool visible;
    };

    class Body {
    public:
        Body(sf::Font &font);

        // Select previous item, ignoring invisible items
        void select_previous_item();

        // Select next item, ignoring invisible items
        void select_next_item();
        void reset_selected();
        void clear_items();

        BodyItem* get_selected() const;

        void clamp_selection();
        void draw(sf::RenderWindow &window, sf::Vector2f pos, sf::Vector2f size);
        static bool string_find_case_insensitive(const std::string &str, const std::string &substr);

        // TODO: Make this actually fuzzy... Right now it's just a case insensitive string find.
        // TODO: Highlight the part of the text that matches the search
        void filter_search_fuzzy(const std::string &text);

        sf::Text title_text;
        int selected_item;
        std::vector<std::unique_ptr<BodyItem>> items;
        std::vector<std::unique_ptr<sf::Texture>> item_thumbnail_textures;
    };
}