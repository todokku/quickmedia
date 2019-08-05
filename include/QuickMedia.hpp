#pragma once

#include "SearchBar.hpp"
#include "Page.hpp"
#include <vector>
#include <memory>
#include <SFML/Graphics/Font.hpp>
#include <SFML/Graphics/RenderWindow.hpp>

namespace QuickMedia {
    class Body;
    class Plugin;
    
    class Program {
    public:
        Program();
        ~Program();
        void run();
    private:
        void base_event_handler(sf::Event &event, Page previous_page);
        void search_suggestion_page();
        void search_result_page();
        void video_content_page();
        void episode_list_page();
        void image_page();
    private:
        sf::RenderWindow window;
        sf::Vector2f window_size;
        sf::Font font;
        Body *body;
        Plugin *current_plugin;
        std::unique_ptr<SearchBar> search_bar;
        Page current_page;
        // TODO: Combine these
        std::string video_url;
        std::string images_url;
        int image_index;
    };
}