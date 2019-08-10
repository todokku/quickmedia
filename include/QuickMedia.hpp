#pragma once

#include "Body.hpp"
#include "SearchBar.hpp"
#include "Page.hpp"
#include "Storage.hpp"
#include <vector>
#include <memory>
#include <SFML/Graphics/Font.hpp>
#include <SFML/Graphics/RenderWindow.hpp>
#include <json/value.h>
#include <unordered_set>
#include <future>

namespace QuickMedia {
    class Plugin;
    
    class Program {
    public:
        Program();
        ~Program();
        int run(int argc, char **argv);
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
        sf::Texture plugin_logo;
        std::unique_ptr<SearchBar> search_bar;
        Page current_page;
        // TODO: Combine these
        std::string images_url;
        std::string content_title;
        std::string content_url;
        std::string chapter_title;
        int image_index;
        Path content_storage_file;
        Json::Value content_storage_json;
        std::unordered_set<std::string> watched_videos;
        std::future<BodyItems> search_suggestion_future;
    };
}