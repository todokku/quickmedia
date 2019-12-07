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
#include <stack>

namespace QuickMedia {
    class Plugin;
    class Manganelo;
    
    class Program {
    public:
        Program();
        ~Program();
        int run(int argc, char **argv);

        Plugin* get_current_plugin() { return current_plugin; }
    private:
        void base_event_handler(sf::Event &event, Page previous_page, bool handle_key_press = true, bool clear_on_escape = true);
        void search_suggestion_page();
        void search_result_page();
        void video_content_page();
        void episode_list_page();
        void image_page();
        void content_list_page();
        void content_details_page();
        void image_board_thread_list_page();
        void image_board_thread_page();

        enum class LoadImageResult {
            OK,
            FAILED,
            DOWNLOAD_IN_PROGRESS
        };

        LoadImageResult load_image_by_index(int image_index, sf::Texture &image_texture, sf::String &error_message);
        void download_chapter_images_if_needed(Manganelo *image_plugin);
        void select_episode(BodyItem *item, bool start_from_beginning);

        // Returns Page::EXIT if empty
        Page pop_page_stack();
    private:
        sf::RenderWindow window;
        sf::Vector2f window_size;
        sf::Font font;
        sf::Font bold_font;
        Body *body;
        Plugin *current_plugin;
        sf::Texture plugin_logo;
        std::unique_ptr<SearchBar> search_bar;
        Page current_page;
        std::stack<Page> page_stack;
        // TODO: Combine these
        std::string images_url;
        std::string content_title;
        std::string content_episode;
        std::string content_url;
        std::string content_list_url;
        std::string image_board_thread_list_url;
        std::string chapter_title;
        int image_index;
        Path content_storage_file;
        Path content_cache_dir;
        std::string manga_id_base64;
        Json::Value content_storage_json;
        std::unordered_set<std::string> watched_videos;
        std::future<BodyItems> search_suggestion_future;
        std::future<void> image_download_future;
        std::string downloading_chapter_url;
        bool image_download_cancel;
    };
}