#pragma once

#include <SFML/Graphics/Font.hpp>
#include <SFML/Graphics/Text.hpp>
#include <SFML/Graphics/Texture.hpp>
#include <SFML/Graphics/RenderWindow.hpp>
#include <json/value.h>
#include <thread>

namespace QuickMedia {
    class Program;

    class BodyItem {
    public:
        BodyItem(const std::string &_title): title(_title), visible(true) {

        }

        std::string title;
        std::string url;
        std::string thumbnail_url;
        bool visible;
        // Used by image boards for example. The elements are indices to other body items
        std::vector<size_t> replies;
    };

    using BodyItems = std::vector<std::unique_ptr<BodyItem>>;

    class Body {
    public:
        Body(Program *program, sf::Font &font);

        // Select previous item, ignoring invisible items
        void select_previous_item();

        // Select next item, ignoring invisible items
        void select_next_item();
        
        void select_first_item();
        void reset_selected();
        void clear_items();

        BodyItem* get_selected() const;

        void clamp_selection();
        void draw(sf::RenderWindow &window, sf::Vector2f pos, sf::Vector2f size);
        void draw(sf::RenderWindow &window, sf::Vector2f pos, sf::Vector2f size, const Json::Value &content_progress);
        static bool string_find_case_insensitive(const std::string &str, const std::string &substr);

        // TODO: Make this actually fuzzy... Right now it's just a case insensitive string find.
        // TODO: Highlight the part of the text that matches the search.
        // TODO: Ignore dot, whitespace and special characters
        void filter_search_fuzzy(const std::string &text);

        sf::Text title_text;
        sf::Text progress_text;
        int selected_item;
        BodyItems items;
        std::thread thumbnail_load_thread;
        bool draw_thumbnails;
    private:
        struct ThumbnailData {
            std::string url;
            std::shared_ptr<sf::Texture> texture;
        };
        Program *program;
        std::shared_ptr<sf::Texture> load_thumbnail_from_url(const std::string &url);
        std::vector<ThumbnailData> item_thumbnail_textures;
        bool loading_thumbnail;
    };
}