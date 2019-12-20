#include "../include/QuickMedia.hpp"
#include "../plugins/Manganelo.hpp"
#include "../plugins/Youtube.hpp"
#include "../plugins/Pornhub.hpp"
#include "../plugins/Fourchan.hpp"
#include "../include/Scale.hpp"
#include "../include/Program.h"
#include "../include/VideoPlayer.hpp"
#include "../include/StringUtils.hpp"
#include "../include/GoogleCaptcha.hpp"
#include <cppcodec/base64_rfc4648.hpp>

#include <SFML/Graphics/RectangleShape.hpp>
#include <SFML/Graphics/Sprite.hpp>
#include <SFML/Graphics/Text.hpp>
#include <SFML/Window/Event.hpp>
#include <json/reader.h>
#include <json/writer.h>
#include <assert.h>
#include <cmath>
#include <string.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <signal.h>

static const sf::Color back_color(30, 32, 34);
static const int DOUBLE_CLICK_TIME = 500;
static const std::string fourchan_google_captcha_api_key = "6Ldp2bsSAAAAAAJ5uyx_lx34lJeEpTLVkP5k04qc";

// Prevent writing to broken pipe from exiting the program
static void sigpipe_handler(int unused) {

}

static int x_error_handler(Display *display, XErrorEvent *event) {
    return 0;
}

static int x_io_error_handler(Display *display) {
    return 0;
}

namespace QuickMedia {
    Program::Program() :
        window(sf::VideoMode(800, 600), "QuickMedia"),
        window_size(800, 600),
        body(nullptr),
        current_plugin(nullptr),
        current_page(Page::SEARCH_SUGGESTION),
        image_index(0)
    {
        window.setVerticalSyncEnabled(true);
        if(!font.loadFromFile("../../../fonts/Lato-Regular.ttf")) {
            fprintf(stderr, "Failed to load font: Lato-Regular.ttf\n");
            abort();
        }
        if(!bold_font.loadFromFile("../../../fonts/Lato-Bold.ttf")) {
            fprintf(stderr, "Failed to load font: Lato-Bold.ttf\n");
            abort();
        }
        body = new Body(this, font, bold_font);

        struct sigaction action;
        action.sa_handler = sigpipe_handler;
        sigemptyset(&action.sa_mask);
        action.sa_flags = 0;
        sigaction(SIGPIPE, &action, NULL);

        XSetErrorHandler(x_error_handler);
        XSetIOErrorHandler(x_io_error_handler);
    }

    Program::~Program() {
        delete body;
        delete current_plugin;
    }

    static SearchResult search_selected_suggestion(Body *input_body, Body *output_body, Plugin *plugin, std::string &selected_title, std::string &selected_url) {
        BodyItem *selected_item = input_body->get_selected();
        if(!selected_item)
            return SearchResult::ERR;

        selected_title = selected_item->title;
        selected_url = selected_item->url;
        output_body->clear_items();
        SearchResult search_result = plugin->search(!selected_url.empty() ? selected_url : selected_title, output_body->items);
        output_body->reset_selected();
        return search_result;
    }

    static void usage() {
        fprintf(stderr, "usage: QuickMedia <plugin> [--tor]\n");
        fprintf(stderr, "OPTIONS:\n");
        fprintf(stderr, "plugin    The plugin to use. Should be either 4chan, manganelo, pornhub or youtube\n");
        fprintf(stderr, "--tor     Use tor. Disabled by default\n");
        fprintf(stderr, "EXAMPLES:\n");
        fprintf(stderr, "QuickMedia manganelo\n");
        fprintf(stderr, "QuickMedia youtube --tor\n");
    }

    static bool is_program_executable_by_name(const char *name) {
        // TODO: Implement for Windows. Windows also uses semicolon instead of colon as a separator
        char *env = getenv("PATH");
        std::unordered_set<std::string> paths;
        string_split(env, ':', [&paths](const char *str, size_t size) {
            paths.insert(std::string(str, size));
            return true;
        });

        for(const std::string &path_str : paths) {
            Path path(path_str);
            path.join(name);
            if(get_file_type(path) == FileType::REGULAR)
                return true;
        }

        return false;
    }

    int Program::run(int argc, char **argv) {
        if(argc < 2) {
            usage();
            return -1;
        }

        current_plugin = nullptr;
        std::string plugin_logo_path;
        bool use_tor = false;

        for(int i = 1; i < argc; ++i) {
            if(!current_plugin) {
                if(strcmp(argv[i], "manganelo") == 0) {
                    current_plugin = new Manganelo();
                    plugin_logo_path = "../../../images/manganelo_logo.png";
                } else if(strcmp(argv[i], "youtube") == 0) {
                    current_plugin = new Youtube();
                    plugin_logo_path = "../../../images/yt_logo_rgb_dark_small.png";
                } else if(strcmp(argv[i], "pornhub") == 0) {
                    current_plugin = new Pornhub();
                    plugin_logo_path = "../../../images/pornhub_logo.png";
                } else if(strcmp(argv[i], "4chan") == 0) {
                    current_plugin = new Fourchan();
                    plugin_logo_path = "../../../images/4chan_logo.png";
                }
            }

            if(strcmp(argv[i], "--tor") == 0) {
                use_tor = true;
            }
        }

        if(!current_plugin) {
            usage();
            return -1;
        }

        if(use_tor && !is_program_executable_by_name("torsocks")) {
            fprintf(stderr, "torsocks needs to be installed (and accessible from PATH environment variable) when using the --tor option\n");
            return -2;
        }

        current_plugin->use_tor = use_tor;

        if(!plugin_logo_path.empty()) {
            if(!plugin_logo.loadFromFile(plugin_logo_path)) {
                fprintf(stderr, "Failed to load plugin logo, path: %s\n", plugin_logo_path.c_str());
                return -2;
            }
            plugin_logo.generateMipmap();
            plugin_logo.setSmooth(true);
        }

        search_bar = std::make_unique<SearchBar>(font, plugin_logo);
        search_bar->text_autosearch_delay = current_plugin->get_search_delay();

        while(window.isOpen()) {
            switch(current_page) {
                case Page::EXIT:
                    window.close();
                    break;
                case Page::SEARCH_SUGGESTION:
                    body->draw_thumbnails = current_plugin->search_suggestions_has_thumbnails();
                    search_suggestion_page();
                    break;
#if 0
                case Page::SEARCH_RESULT:
                    body->draw_thumbnails = current_plugin->search_results_has_thumbnails();
                    search_result_page();
                    break;
#endif
                case Page::VIDEO_CONTENT:
                    body->draw_thumbnails = false;
                    video_content_page();
                    break;
                case Page::EPISODE_LIST:
                    body->draw_thumbnails = false;
                    episode_list_page();
                    break;
                case Page::IMAGES: {
                    body->draw_thumbnails = false;
                    window.setKeyRepeatEnabled(false);
                    window.setFramerateLimit(4);
                    image_page();
                    window.setFramerateLimit(0);
                    window.setKeyRepeatEnabled(true);
                    break;
                }
                case Page::CONTENT_LIST: {
                    body->draw_thumbnails = true;
                    content_list_page();
                    break;
                }
                case Page::CONTENT_DETAILS: {
                    body->draw_thumbnails = true;
                    content_details_page();
                    break;
                }
                case Page::IMAGE_BOARD_THREAD_LIST: {
                    body->draw_thumbnails = true;
                    image_board_thread_list_page();
                    break;
                }
                case Page::IMAGE_BOARD_THREAD: {
                    body->draw_thumbnails = true;
                    image_board_thread_page();
                    break;
                }
                default:
                    fprintf(stderr, "Page not implemented: %d\n", current_page);
                    window.close();
                    break;
            }
        }

        return 0;
    }

    void Program::base_event_handler(sf::Event &event, Page previous_page, bool handle_keypress, bool clear_on_escape, bool handle_searchbar) {
        if (event.type == sf::Event::Closed) {
            current_page = Page::EXIT;
        } else if(event.type == sf::Event::Resized) {
            window_size.x = event.size.width;
            window_size.y = event.size.height;
            sf::FloatRect visible_area(0, 0, window_size.x, window_size.y);
            window.setView(sf::View(visible_area));
        } else if(handle_keypress && event.type == sf::Event::KeyPressed) {
            if(event.key.code == sf::Keyboard::Up) {
                body->select_previous_item();
            } else if(event.key.code == sf::Keyboard::Down) {
                body->select_next_item();
            } else if(event.key.code == sf::Keyboard::Escape) {
                current_page = previous_page;
                if(clear_on_escape) {
                    body->clear_items();
                    body->reset_selected();
                    search_bar->clear();
                }
            }
        } else if(handle_searchbar && event.type == sf::Event::TextEntered) {
            search_bar->onTextEntered(event.text.unicode);
        }
    }

    enum class Urgency {
        LOW,
        NORMAL,
        CRITICAL
    };

    const char* urgency_string(Urgency urgency) {
        switch(urgency) {
            case Urgency::LOW:
                return "low";
            case Urgency::NORMAL:
                return "normal";
            case Urgency::CRITICAL:
                return "critical";
        }
        assert(false);
        return nullptr;
    }

    static void show_notification(const std::string &title, const std::string &description, Urgency urgency = Urgency::NORMAL) {
        const char *args[] = { "notify-send", "-u", urgency_string(urgency), "--", title.c_str(), description.c_str(), nullptr };
        exec_program_async(args, nullptr);
        printf("Notification: title: %s, description: %s\n", title.c_str(), description.c_str());
    }

    static std::string base64_encode(const std::string &data) {
        return cppcodec::base64_rfc4648::encode(data);
    }

    static std::string base64_decode(const std::string &data) {
        return cppcodec::base64_rfc4648::decode<std::string>(data);
    }

    static bool read_file_as_json(const Path &storage_path, Json::Value &result) {
        std::string file_content;
        if(file_get_content(storage_path, file_content) != 0)
            return false;

        Json::CharReaderBuilder json_builder;
        std::unique_ptr<Json::CharReader> json_reader(json_builder.newCharReader());
        std::string json_errors;
        if(!json_reader->parse(file_content.data(), file_content.data() + file_content.size(), &result, &json_errors)) {
            fprintf(stderr, "Failed to read json, error: %s\n", json_errors.c_str());
            return false;
        }

        return true;
    }

    static bool save_manga_progress_json(const Path &path, const Json::Value &json) {
        Json::StreamWriterBuilder json_builder;
        return file_overwrite(path, Json::writeString(json_builder, json)) == 0;
    }

    static bool manga_extract_id_from_url(const std::string &url, std::string &manga_id) {
        bool manganelo_website = false;
        if(url.find("mangakakalot") != std::string::npos || url.find("manganelo") != std::string::npos)
            manganelo_website = true;

        if(manganelo_website) {
            size_t index = url.find("manga/");
            if(index == std::string::npos) {
                std::string err_msg = "Url ";
                err_msg += url;
                err_msg += " doesn't contain manga id";
                show_notification("Manga", err_msg, Urgency::CRITICAL);
                return false;
            }

            manga_id = url.substr(index + 6);
            if(manga_id.size() <= 2) {
                std::string err_msg = "Url ";
                err_msg += url;
                err_msg += " doesn't contain manga id";
                show_notification("Manga", err_msg, Urgency::CRITICAL);
                return false;
            }
            return true;
        } else {
            std::string err_msg = "Unexpected url ";
            err_msg += url;
            err_msg += " is not manganelo or mangakakalot";
            show_notification("Manga", err_msg, Urgency::CRITICAL);
            return false;
        }
    }

    enum class SearchSuggestionTab {
        ALL,
        HISTORY
    };

    void Program::search_suggestion_page() {
        std::string update_search_text;
        bool search_running = false;

        Body history_body(this, font, bold_font);
        const float tab_text_size = 18.0f;
        const float tab_height = tab_text_size + 10.0f;
        sf::Text all_tab_text("All", font, tab_text_size);
        sf::Text history_tab_text("History", font, tab_text_size);

        struct Tab {
            Body *body;
            SearchSuggestionTab tab;
            sf::Text *text;
        };

        std::array<Tab, 2> tabs = { Tab{body, SearchSuggestionTab::ALL, &all_tab_text}, Tab{&history_body, SearchSuggestionTab::HISTORY, &history_tab_text} };
        int selected_tab = 0;

        // TOOD: Make generic, instead of checking for plugin
        if(current_plugin->name == "manganelo") {
            // TODO: Make asynchronous
            for_files_in_dir(get_storage_dir().join("manga"), [&history_body](const std::filesystem::path &filepath) {
                Path fullpath(filepath.c_str());
                Json::Value body;
                if(!read_file_as_json(fullpath, body)) {
                    fprintf(stderr, "Failed to read json file: %s\n", fullpath.data.c_str());
                    return true;
                }

                auto filename = filepath.filename();
                const Json::Value &manga_name = body["name"];
                if(!filename.empty() && manga_name.isString()) {
                    // TODO: Add thumbnail
                    auto body_item = std::make_unique<BodyItem>(manga_name.asString());
                    body_item->url = "https://manganelo.com/manga/" + base64_decode(filename.string());
                    history_body.items.push_back(std::move(body_item));
                }
                return true;
            });
        }

        search_bar->onTextUpdateCallback = [&update_search_text, this, &tabs, &selected_tab](const std::string &text) {
            if(tabs[selected_tab].body == body)
                update_search_text = text;
            else {
                tabs[selected_tab].body->filter_search_fuzzy(text);
                tabs[selected_tab].body->clamp_selection();
            }
        };

        search_bar->onTextSubmitCallback = [this, &tabs, &selected_tab](const std::string &text) -> bool {
            Page next_page = current_plugin->get_page_after_search();
            // TODO: This shouldn't be done if search_selected_suggestion fails
            if(search_selected_suggestion(tabs[selected_tab].body, body, current_plugin, content_title, content_url) != SearchResult::OK)
                return false;

            if(next_page == Page::EPISODE_LIST) {
                if(content_url.empty()) {
                    show_notification("Manga", "Url is missing for manga!", Urgency::CRITICAL);
                    return false;
                }
                
                Path content_storage_dir = get_storage_dir().join("manga");
                if(create_directory_recursive(content_storage_dir) != 0) {
                    show_notification("Storage", "Failed to create directory: " + content_storage_dir.data, Urgency::CRITICAL);
                    return false;
                }

                std::string manga_id;
                if(!manga_extract_id_from_url(content_url, manga_id))
                    return false;

                manga_id_base64 = base64_encode(manga_id);
                content_storage_file = content_storage_dir.join(manga_id_base64);
                content_storage_json.clear();
                content_storage_json["name"] = content_title;
                FileType file_type = get_file_type(content_storage_file);
                if(file_type == FileType::REGULAR)
                    read_file_as_json(content_storage_file, content_storage_json);
            } else if(next_page == Page::VIDEO_CONTENT) {
                watched_videos.clear();
                if(content_url.empty())
                    next_page = Page::SEARCH_RESULT;
                else {
                    page_stack.push(Page::SEARCH_SUGGESTION);
                }
            } else if(next_page == Page::CONTENT_LIST) {
                content_list_url = content_url;
            } else if(next_page == Page::IMAGE_BOARD_THREAD_LIST) {
                image_board_thread_list_url = content_url;
            }
            current_page = next_page;
            return true;
        };

        PluginResult front_page_result = current_plugin->get_front_page(body->items);
        body->clamp_selection();

        sf::Vector2f body_pos;
        sf::Vector2f body_size;
        bool redraw = true;
        sf::Event event;
        
        const sf::Color tab_selected_color(0, 85, 119);
        const sf::Color tab_unselected_color(43, 45, 47);
        sf::RectangleShape tab_spacing_rect(sf::Vector2f(0.0f, 0.0f));
        tab_spacing_rect.setFillColor(tab_unselected_color);
        const float tab_spacer_height = 0.0f;

        sf::RectangleShape tab_drop_shadow;
        tab_drop_shadow.setFillColor(sf::Color(23, 25, 27));

        while (current_page == Page::SEARCH_SUGGESTION) {
            while (window.pollEvent(event)) {
                base_event_handler(event, Page::EXIT, false);
                if(event.type == sf::Event::Resized || event.type == sf::Event::GainedFocus)
                    redraw = true;
                else if(event.type == sf::Event::KeyPressed) {
                    if(event.key.code == sf::Keyboard::Up) {
                        tabs[selected_tab].body->select_previous_item();
                    } else if(event.key.code == sf::Keyboard::Down) {
                        tabs[selected_tab].body->select_next_item();
                    } else if(event.key.code == sf::Keyboard::Escape) {
                        current_page = Page::EXIT;
                    } else if(event.key.code == sf::Keyboard::Left) {
                        selected_tab = std::max(0, selected_tab - 1);
                        search_bar->clear();
                    } else if(event.key.code == sf::Keyboard::Right) {
                        selected_tab = std::min((int)tabs.size() - 1, selected_tab + 1);
                        search_bar->clear();
                    }
                }
            }

            if(redraw) {
                redraw = false;
                search_bar->onWindowResize(window_size);

                float body_padding_horizontal = 50.0f;
                float body_padding_vertical = tab_spacer_height + tab_height + 50.0f;
                float body_width = window_size.x - body_padding_horizontal * 2.0f;
                if(body_width < 400) {
                    body_width = window_size.x;
                    body_padding_horizontal = 0.0f;
                }

                float search_bottom = search_bar->getBottomWithoutShadow();
                body_pos = sf::Vector2f(body_padding_horizontal, search_bottom + body_padding_vertical);
                body_size = sf::Vector2f(body_width, window_size.y - search_bottom - body_padding_vertical);
            }

            search_bar->update();

            if(!update_search_text.empty() && !search_running) {
                search_suggestion_future = std::async(std::launch::async, [this, update_search_text]() {
                    BodyItems result;
                    SuggestionResult suggestion_result = current_plugin->update_search_suggestions(update_search_text, result);
                    return result;
                });
                update_search_text.clear();
                search_running = true;
            }

            if(search_running && search_suggestion_future.valid() && search_suggestion_future.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
                if(update_search_text.empty()) {
                    body->items = search_suggestion_future.get();
                    body->clamp_selection();
                } else {
                    search_suggestion_future.get();
                }
                search_running = false;
            }

            window.clear(back_color);
            {
                tab_spacing_rect.setPosition(0.0f, search_bar->getBottomWithoutShadow());
                tab_spacing_rect.setSize(sf::Vector2f(window_size.x, tab_spacer_height));
                window.draw(tab_spacing_rect);

                tab_drop_shadow.setSize(sf::Vector2f(window_size.x, 5.0f));
                tab_drop_shadow.setPosition(0.0f, std::floor(search_bar->getBottomWithoutShadow() + tab_height));
                window.draw(tab_drop_shadow);

                const float width_per_tab = window_size.x / tabs.size();
                const float tab_y = tab_spacer_height + std::floor(search_bar->getBottomWithoutShadow() + tab_height * 0.5f - (tab_text_size + 5.0f) * 0.5f);
                sf::RectangleShape tab_background(sf::Vector2f(std::floor(width_per_tab), tab_height));
                int i = 0;
                for(Tab &tab : tabs) {
                    if(i == selected_tab)
                        tab_background.setFillColor(tab_selected_color);
                    else
                        tab_background.setFillColor(tab_unselected_color);
                    tab_background.setPosition(std::floor(i * width_per_tab), tab_spacer_height + std::floor(search_bar->getBottomWithoutShadow()));
                    window.draw(tab_background);
                    const float center = (i * width_per_tab) + (width_per_tab * 0.5f);
                    tab.text->setPosition(std::floor(center - tab.text->getLocalBounds().width * 0.5f), tab_y);
                    window.draw(*tab.text);
                    if(i == selected_tab)
                        tab.body->draw(window, body_pos, body_size);
                    ++i;
                }
            }
            search_bar->draw(window, false);
            window.display();
        }
    }

    void Program::search_result_page() {
        assert(false);
        #if 0
        search_bar->onTextUpdateCallback = [this](const std::string &text) {
            body->filter_search_fuzzy(text);
            body->clamp_selection();
        };

        search_bar->onTextSubmitCallback = [this](const std::string &text) {
            BodyItem *selected_item = body->get_selected();
            if(!selected_item)
                return;
            video_url = selected_item->url;
            current_page = Page::VIDEO_CONTENT;
        };

        sf::Vector2f body_pos;
        sf::Vector2f body_size;
        bool redraw = true;
        sf::Event event;

        while (current_page == Page::SEARCH_RESULT) {
            while (window.pollEvent(event)) {
                base_event_handler(event, Page::SEARCH_SUGGESTION);
                if(event.type == sf::Event::Resized || event.type == sf::Event::GainedFocus)
                    redraw = true;
            }

            if(redraw) {
                redraw = false;
                search_bar->onWindowResize(window_size);

                float body_padding_horizontal = 50.0f;
                float body_padding_vertical = 50.0f;
                float body_width = window_size.x - body_padding_horizontal * 2.0f;
                if(body_width < 400) {
                    body_width = window_size.x;
                    body_padding_horizontal = 0.0f;
                }

                float search_bottom = search_bar->getBottom();
                body_pos = sf::Vector2f(body_padding_horizontal, search_bottom + body_padding_vertical);
                body_size = sf::Vector2f(body_width, window_size.y - search_bottom - body_padding_vertical);
            }

            search_bar->update();

            window.clear(back_color);
            body->draw(window, body_pos, body_size);
            search_bar->draw(window);
            window.display();
        }
        #endif
    }

    struct XDisplayScope {
        XDisplayScope(Display *_display) : display(_display) {

        }

        ~XDisplayScope() {
            if(display)
                XCloseDisplay(display);
        }

        Display *display;
    };

    enum class WindowFullscreenState {
        UNSET,
        SET,
        TOGGLE
    };

    static bool window_set_fullscreen(Display *display, Window window, WindowFullscreenState state) {
        Atom wm_state_atom = XInternAtom(display, "_NET_WM_STATE", False);
        Atom wm_state_fullscreen_atom = XInternAtom(display, "_NET_WM_STATE_FULLSCREEN", False);
        if(wm_state_atom == False || wm_state_fullscreen_atom == False) {
            fprintf(stderr, "Failed to fullscreen window. The window manager doesn't support fullscreening windows.\n");
            return false;
        }

        XEvent xev;
        xev.type = ClientMessage;
        xev.xclient.window = window;
        xev.xclient.message_type = wm_state_atom;
        xev.xclient.format = 32;
        xev.xclient.data.l[0] = (int)state;
        xev.xclient.data.l[1] = wm_state_fullscreen_atom;
        xev.xclient.data.l[2] = 0;
        xev.xclient.data.l[3] = 1;
        xev.xclient.data.l[4] = 0;

        if(!XSendEvent(display, XDefaultRootWindow(display), 0, SubstructureRedirectMask | SubstructureNotifyMask, &xev)) {
            fprintf(stderr, "Failed to fullscreen window\n");
            return false;
        }

        XFlush(display);
        return true;
    }

    void Program::video_content_page() {
        search_bar->onTextUpdateCallback = nullptr;
        search_bar->onTextSubmitCallback = nullptr;

        Page previous_page = pop_page_stack();

        Display* disp = XOpenDisplay(NULL);
        if (!disp)
            throw std::runtime_error("Failed to open display to X11 server");
        XDisplayScope display_scope(disp);

        std::unique_ptr<sf::RenderWindow> video_player_ui_window;
        auto on_window_create = [disp, &video_player_ui_window](sf::WindowHandle video_player_window) {
            int screen = DefaultScreen(disp);
            Window ui_window = XCreateWindow(disp, RootWindow(disp, screen),
                            0, 0, 1, 1, 0,
                            DefaultDepth(disp, screen),
                            InputOutput,
                            DefaultVisual(disp, screen),
                            0, NULL);

            XReparentWindow(disp, ui_window, video_player_window, 0, 0);
            XMapWindow(disp, ui_window);
            XFlush(disp);

            video_player_ui_window = std::make_unique<sf::RenderWindow>(ui_window);
            video_player_ui_window->setVerticalSyncEnabled(true);
        };

        bool ui_resize = true;
        bool seekable = false;

        std::unique_ptr<VideoPlayer> video_player;

        auto load_video_error_check = [this, &video_player, previous_page]() {
            watched_videos.insert(content_url);
            VideoPlayer::Error err = video_player->load_video(content_url.c_str(), window.getSystemHandle());
            if(err != VideoPlayer::Error::OK) {
                std::string err_msg = "Failed to play url: ";
                err_msg += content_url;
                show_notification("Video player", err_msg.c_str(), Urgency::CRITICAL);
                current_page = previous_page;
            }
        };
        
        video_player = std::make_unique<VideoPlayer>(current_plugin->use_tor, [this, &video_player, &seekable, &load_video_error_check, previous_page](const char *event_name) {
            bool end_of_file = false;
            if(strcmp(event_name, "pause") == 0) {
                double time_remaining = 0.0;
                if(video_player->get_time_remaining(&time_remaining) == VideoPlayer::Error::OK && time_remaining <= 1.0)
                    end_of_file = true;
            } else if(strcmp(event_name, "playback-restart") == 0) {
                video_player->set_paused(false);
                video_player->is_seekable(&seekable);
            }

            if(end_of_file) {
                std::string new_video_url;
                BodyItems related_media = current_plugin->get_related_media(content_url);
                // Find video that hasn't been played before in this video session
                for(auto it = related_media.begin(), end = related_media.end(); it != end; ++it) {
                    if(watched_videos.find((*it)->url) == watched_videos.end()) {
                        new_video_url = (*it)->url;
                        break;
                    }
                }

                // If there are no videos to play, then dont play any...
                if(new_video_url.empty()) {
                    show_notification("Video player", "No more related videos to play");
                    current_page = previous_page;
                    return;
                }

                content_url = std::move(new_video_url);
                load_video_error_check();
            }
        }, on_window_create);
        load_video_error_check();

        auto on_doubleclick = [this, disp]() {
            window_set_fullscreen(disp, window.getSystemHandle(), WindowFullscreenState::TOGGLE);
        };

        sf::Clock ui_hide_timer;
        bool ui_visible = true;
        const int UI_HIDE_TIMEOUT = 4500;

        sf::Clock time_since_last_left_click;
        int left_click_counter;
        sf::Event event;

        sf::RectangleShape rect;
        rect.setFillColor(sf::Color::Red);
        sf::Clock get_progress_timer;
        double progress = 0.0;

        // Clear screen before playing video, to show a black screen instead of being frozen
        // at the previous UI for a moment
        window.clear();
        window.display();

        while (current_page == Page::VIDEO_CONTENT) {
            while (window.pollEvent(event)) {
                base_event_handler(event, previous_page, true, false, false);
                if(event.type == sf::Event::Resized) {
                    if(video_player_ui_window)
                        ui_resize = true;
                } else if(event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::Space) {
                    if(video_player->toggle_pause() != VideoPlayer::Error::OK) {
                        fprintf(stderr, "Failed to toggle pause!\n");
                    }
                } else if(event.type == sf::Event::MouseButtonPressed && event.mouseButton.button == sf::Mouse::Left) {
                    if(time_since_last_left_click.restart().asMilliseconds() <= DOUBLE_CLICK_TIME) {
                        if(++left_click_counter == 2) {
                            on_doubleclick();
                            left_click_counter = 0;
                        }
                    } else {
                        left_click_counter = 1;
                    }
                } else if(event.type == sf::Event::MouseMoved) {
                    ui_hide_timer.restart();
                    if(!ui_visible) {
                        ui_visible = true;
                        video_player_ui_window->setVisible(true);
                        window.setMouseCursorVisible(true);
                    }
                }
            }

            if(video_player_ui_window) {
                while(video_player_ui_window->pollEvent(event)) {
                    if(event.type == sf::Event::Resized) {
                        sf::FloatRect visible_area(0, 0, event.size.width, event.size.height);
                        video_player_ui_window->setView(sf::View(visible_area));
                    }
                }
            }

            VideoPlayer::Error update_err = video_player->update();
            if(update_err == VideoPlayer::Error::FAIL_TO_CONNECT_TIMEOUT) {
                show_notification("Video player", "Failed to connect to mpv ipc after 5 seconds", Urgency::CRITICAL);
                current_page = previous_page;
                return;
            } else if(update_err != VideoPlayer::Error::OK) {
                show_notification("Video player", "Unexpected error while updating", Urgency::CRITICAL);
                current_page = previous_page;
                return;
            }

            // TODO: Show loading video animation
            //window.clear();
            //window.display();

            if(video_player->is_connected() && get_progress_timer.getElapsedTime().asMilliseconds() >= 500) {
                get_progress_timer.restart();
                video_player->get_progress(&progress);
            }

            if(video_player_ui_window) {
                if(!ui_visible) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(50));
                    continue;
                }

                if(ui_hide_timer.getElapsedTime().asMilliseconds() > UI_HIDE_TIMEOUT) {
                    ui_visible = false;
                    video_player_ui_window->setVisible(false);
                    window.setMouseCursorVisible(false);
                }

                const float ui_height = window_size.y * 0.025f;
                if(ui_resize) {
                    ui_resize = false;
                    video_player_ui_window->setSize(sf::Vector2u(window_size.x, ui_height));
                    video_player_ui_window->setPosition(sf::Vector2i(0, window_size.y - ui_height));
                }

                // TODO: Make window transparent, so the ui overlay for the video has transparency
                video_player_ui_window->clear(sf::Color(33, 33, 33));
                rect.setSize(sf::Vector2f(window_size.x * progress, ui_height));
                video_player_ui_window->draw(rect);
                video_player_ui_window->display();

                if(sf::Mouse::isButtonPressed(sf::Mouse::Left)) {
                    auto mouse_pos = sf::Mouse::getPosition(window);
                    if(mouse_pos.y >= window_size.y - ui_height && mouse_pos.y <= window_size.y) {
                        if(seekable)
                            video_player->set_progress((double)mouse_pos.x / (double)window_size.x);
                        else
                            fprintf(stderr, "Video is not seekable!\n"); // TODO: Show this to the user
                    }
                }
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
        }

        video_player_ui_window.reset();
        window_set_fullscreen(disp, window.getSystemHandle(), WindowFullscreenState::UNSET);
        window.setMouseCursorVisible(true);
    }

    enum class TrackMediaType {
        RSS,
        HTML
    };

    const char* track_media_type_string(TrackMediaType media_type) {
        switch(media_type) {
            case TrackMediaType::RSS:
                return "rss";
            case TrackMediaType::HTML:
                return "html";
        }
        assert(false);
        return "";
    }

    static int track_media(TrackMediaType media_type, const std::string &manga_title, const std::string &chapter_title, const std::string &url) {
        const char *args[] = { "automedia.py", "add", track_media_type_string(media_type), url.data(), "--start-after", chapter_title.data(), "--name", manga_title.data(), nullptr };
        return exec_program(args, nullptr, nullptr);
    }

    void Program::select_episode(BodyItem *item, bool start_from_beginning) {
        images_url = item->url;
        chapter_title = item->title;
        image_index = 0;
        current_page = Page::IMAGES;
        if(start_from_beginning)
            return;

        const Json::Value &json_chapters = content_storage_json["chapters"];
        if(json_chapters.isObject()) {
            const Json::Value &json_chapter = json_chapters[chapter_title];
            if(json_chapter.isObject()) {
                const Json::Value &current = json_chapter["current"];
                if(current.isNumeric())
                    image_index = current.asInt() - 1;
            }
        }
    }

    Page Program::pop_page_stack() {
        if(!page_stack.empty()) {
            Page previous_page = page_stack.top();
            page_stack.pop();
            return previous_page;
        }
        return Page::EXIT;
    }

    void Program::episode_list_page() {
        search_bar->onTextUpdateCallback = [this](const std::string &text) {
            body->filter_search_fuzzy(text);
            body->select_first_item();
        };

        search_bar->onTextSubmitCallback = [this](const std::string &text) -> bool {
            BodyItem *selected_item = body->get_selected();
            if(!selected_item)
                return false;

            select_episode(selected_item, false);
            return true;
        };

        const Json::Value &json_chapters = content_storage_json["chapters"];

        sf::Vector2f body_pos;
        sf::Vector2f body_size;
        bool redraw = true;
        sf::Event event;

        while (current_page == Page::EPISODE_LIST) {
            while (window.pollEvent(event)) {
                base_event_handler(event, Page::SEARCH_SUGGESTION);
                if(event.type == sf::Event::Resized || event.type == sf::Event::GainedFocus)
                    redraw = true;
                else if(event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::T && sf::Keyboard::isKeyPressed(sf::Keyboard::LControl)) {
                    BodyItem *selected_item = body->get_selected();
                    if(selected_item) {
                        if(track_media(TrackMediaType::HTML, content_title, selected_item->title, content_url) == 0) {
                            show_notification("Media tracker", "You are now tracking \"" + content_title + "\" after \"" + selected_item->title + "\"");
                        } else {
                            show_notification("Media tracker", "Failed to track media \"" + content_title + "\", chapter: \"" + selected_item->title + "\"", Urgency::CRITICAL);
                        }
                    }
                }
            }

            // TODO: This code is duplicated in many places. Handle it in one place.
            if(redraw) {
                redraw = false;
                search_bar->onWindowResize(window_size);

                float body_padding_horizontal = 50.0f;
                float body_padding_vertical = 50.0f;
                float body_width = window_size.x - body_padding_horizontal * 2.0f;
                if(body_width < 400) {
                    body_width = window_size.x;
                    body_padding_horizontal = 0.0f;
                }

                float search_bottom = search_bar->getBottom();
                body_pos = sf::Vector2f(body_padding_horizontal, search_bottom + body_padding_vertical);
                body_size = sf::Vector2f(body_width, window_size.y - search_bottom - body_padding_vertical);
            }

            search_bar->update();

            window.clear(back_color);
            body->draw(window, body_pos, body_size, json_chapters);
            search_bar->draw(window);
            window.display();
        }
    }

    // TODO: Optimize this somehow. One image alone uses more than 20mb ram! Total ram usage for viewing one image
    // becomes 40mb (private memory, almost 100mb in total!) Unacceptable!
    Program::LoadImageResult Program::load_image_by_index(int image_index, sf::Texture &image_texture, sf::String &error_message) {
        Path image_path = content_cache_dir;
        image_path.join(std::to_string(image_index + 1));

        Path image_finished_path(image_path.data + ".finished");
        if(get_file_type(image_finished_path) != FileType::FILE_NOT_FOUND && get_file_type(image_path) == FileType::REGULAR) {
            std::string image_data;
            if(file_get_content(image_path, image_data) == 0) {
                if(image_texture.loadFromMemory(image_data.data(), image_data.size())) {
                    image_texture.setSmooth(true);
                    //image_texture.generateMipmap();
                    return LoadImageResult::OK;
                } else {
                    error_message = std::string("Failed to load image for page ") + std::to_string(image_index + 1);
                    return LoadImageResult::FAILED;
                }
            } else {
                show_notification("Manganelo", "Failed to load image for page " + std::to_string(image_index + 1) + ". Image filepath: " + image_path.data, Urgency::CRITICAL);
                error_message = std::string("Failed to load image for page ") + std::to_string(image_index + 1);
                return LoadImageResult::FAILED;
            }
        } else {
            error_message = "Downloading page " + std::to_string(image_index + 1) + "...";
            return LoadImageResult::DOWNLOAD_IN_PROGRESS;
        }
    }

    void Program::download_chapter_images_if_needed(Manganelo *image_plugin) {
        if(downloading_chapter_url == images_url)
            return;

        downloading_chapter_url = images_url;
        if(image_download_future.valid()) {
            image_download_cancel = true;
            image_download_future.get();
            image_download_cancel = false;
        }

        std::string chapter_url = images_url;
        Path content_cache_dir_ = content_cache_dir;
        image_download_future = std::async(std::launch::async, [chapter_url, image_plugin, content_cache_dir_, this]() {
            // TODO: Download images in parallel
            int page = 1;
            image_plugin->for_each_page_in_chapter(chapter_url, [content_cache_dir_, &page, this](const std::string &url) {
                if(image_download_cancel)
                    return false;
                #if 0
                size_t last_index = url.find_last_of('/');
                if(last_index == std::string::npos || (int)url.size() - (int)last_index + 1 <= 0) {
                    show_notification("Manganelo", "Image url is in incorrect format, missing '/': " + url, Urgency::CRITICAL);
                    return false;
                }

                std::string image_filename = url.substr(last_index + 1);
                Path image_filepath = content_cache_dir_;
                image_filepath.join(image_filename);
                #endif
                Path image_filepath = content_cache_dir_;
                image_filepath.join(std::to_string(page++));

                Path lockfile_path(image_filepath.data + ".finished");
                if(get_file_type(lockfile_path) != FileType::FILE_NOT_FOUND) 
                    return true;

                std::string image_content;
                if(download_to_string(url, image_content, {}, current_plugin->use_tor) != DownloadResult::OK) {
                    show_notification("Manganelo", "Failed to download image: " + url, Urgency::CRITICAL);
                    return false;
                }

                if(file_overwrite(image_filepath, image_content) != 0) {
                    show_notification("Storage", "Failed to save image to file: " + image_filepath.data, Urgency::CRITICAL);
                    return false;
                }

                if(create_lock_file(lockfile_path) != 0) {
                    show_notification("Storage", "Failed to save image finished state to file: " + lockfile_path.data, Urgency::CRITICAL);
                    return false;
                }
                return true;
            });
        });
    }

    void Program::image_page() {
        search_bar->onTextUpdateCallback = nullptr;
        search_bar->onTextSubmitCallback = nullptr;

        sf::Texture image_texture;
        sf::Sprite image;
        sf::Text error_message("", font, 30);
        error_message.setFillColor(sf::Color::White);

        assert(current_plugin->name == "manganelo");
        Manganelo *image_plugin = static_cast<Manganelo*>(current_plugin);
        std::string image_data;
        bool download_in_progress = false;

        content_cache_dir = get_cache_dir().join("manga").join(manga_id_base64).join(base64_encode(chapter_title));
        if(create_directory_recursive(content_cache_dir) != 0) {
            show_notification("Storage", "Failed to create directory: " + content_cache_dir.data, Urgency::CRITICAL);
            current_page = Page::EPISODE_LIST;
            return;
        }
        download_chapter_images_if_needed(image_plugin);

        int num_images = 0;
        image_plugin->get_number_of_images(images_url, num_images);
        image_index = std::min(image_index, num_images);

        if(image_index < num_images) {
            sf::String error_msg;
            LoadImageResult load_image_result = load_image_by_index(image_index, image_texture, error_msg);
            if(load_image_result == LoadImageResult::OK)
                image.setTexture(image_texture, true);
            else if(load_image_result == LoadImageResult::DOWNLOAD_IN_PROGRESS)
                download_in_progress = true;
            error_message.setString(error_msg);
        } else if(image_index == num_images) {
            error_message.setString("End of " + chapter_title);
        }

        Json::Value &json_chapters = content_storage_json["chapters"];
        Json::Value json_chapter;
        int latest_read = image_index + 1;
        if(json_chapters.isObject()) {
            json_chapter = json_chapters[chapter_title];
            if(json_chapter.isObject()) {
                const Json::Value &current = json_chapter["current"];
                if(current.isNumeric())
                    latest_read = std::max(latest_read, current.asInt());
            } else {
                json_chapter = Json::Value(Json::objectValue);
            }
        } else {
            json_chapters = Json::Value(Json::objectValue);
            json_chapter = Json::Value(Json::objectValue);
        }
        json_chapter["current"] = std::min(latest_read, num_images);
        json_chapter["total"] = num_images;
        json_chapters[chapter_title] = json_chapter;
        if(!save_manga_progress_json(content_storage_file, content_storage_json)) {
            show_notification("Manga progress", "Failed to save manga progress", Urgency::CRITICAL);
        }

        bool error = !error_message.getString().isEmpty();
        bool redraw = true;
        sf::Event event;

        sf::Text chapter_text(chapter_title + " | Page " + std::to_string(image_index + 1) + "/" + std::to_string(num_images), font, 14);
        if(image_index == num_images)
            chapter_text.setString(chapter_title + " | End");
        chapter_text.setFillColor(sf::Color::White);
        sf::RectangleShape chapter_text_background;
        chapter_text_background.setFillColor(sf::Color(0, 0, 0, 150));

        sf::Vector2u texture_size;
        sf::Vector2f texture_size_f;
        if(!error) {
            texture_size = image.getTexture()->getSize();
            texture_size_f = sf::Vector2f(texture_size.x, texture_size.y);
        }

        sf::Clock check_downloaded_timer;
        const sf::Int32 check_downloaded_timeout_ms = 500;

        // TODO: Show to user if a certain page is missing (by checking page name (number) and checking if some are skipped)
        while (current_page == Page::IMAGES) {
            while(window.pollEvent(event)) {
                if (event.type == sf::Event::Closed) {
                    current_page = Page::EXIT;
                } else if(event.type == sf::Event::Resized) {
                    window_size.x = event.size.width;
                    window_size.y = event.size.height;
                    sf::FloatRect visible_area(0, 0, window_size.x, window_size.y);
                    window.setView(sf::View(visible_area));
                    redraw = true;
                } else if(event.type == sf::Event::GainedFocus) {
                    redraw = true;
                } else if(event.type == sf::Event::KeyPressed) {
                    if(event.key.code == sf::Keyboard::Up) {
                        if(image_index > 0) {
                            --image_index;
                            return;
                        } else if(image_index == 0 && body->selected_item < (int)body->items.size() - 1) {
                            // TODO: Make this work if the list is sorted differently than from newest to oldest.
                            body->selected_item++;
                            select_episode(body->items[body->selected_item].get(), true);
                            image_index = 99999; // Start at the page that shows we are at the end of the chapter
                            return;
                        }
                    } else if(event.key.code == sf::Keyboard::Down) {
                        if(image_index < num_images) {
                            ++image_index;
                            return;
                        } else if(image_index == num_images && body->selected_item > 0) {
                            // TODO: Make this work if the list is sorted differently than from newest to oldest.
                            body->selected_item--;
                            select_episode(body->items[body->selected_item].get(), true);
                            return;
                        }
                    } else if(event.key.code == sf::Keyboard::Escape) {
                        current_page = Page::EPISODE_LIST;
                    }
                }
            }

            if(download_in_progress && check_downloaded_timer.getElapsedTime().asMilliseconds() >= check_downloaded_timeout_ms) {
                sf::String error_msg;
                LoadImageResult load_image_result = load_image_by_index(image_index, image_texture, error_msg);
                if(load_image_result == LoadImageResult::OK) {
                    image.setTexture(image_texture, true);
                    download_in_progress = false;
                    error = false;
                    texture_size = image.getTexture()->getSize();
                    texture_size_f = sf::Vector2f(texture_size.x, texture_size.y);
                } else if(load_image_result == LoadImageResult::FAILED) {
                    download_in_progress = false;
                }
                error_message.setString(error_msg);
                redraw = true;
                check_downloaded_timer.restart();
            }

            const float font_height = chapter_text.getCharacterSize() + 8.0f;
            const float background_height = font_height + 6.0f;

            sf::Vector2f content_size;
            content_size.x = window_size.x;
            content_size.y = window_size.y - background_height;

            if(redraw) {
                redraw = false;
                if(error) {
                    auto bounds = error_message.getLocalBounds();
                    error_message.setPosition(std::floor(content_size.x * 0.5f - bounds.width * 0.5f), std::floor(content_size.y * 0.5f - bounds.height));
                } else {
                    auto image_scale = get_ratio(texture_size_f, clamp_to_size(texture_size_f, content_size));
                    image.setScale(image_scale);

                    auto image_size = texture_size_f;
                    image_size.x *= image_scale.x;
                    image_size.y *= image_scale.y;
                    image.setPosition(std::floor(content_size.x * 0.5f - image_size.x * 0.5f), std::floor(content_size.y * 0.5f - image_size.y * 0.5f));
                }

                window.clear(back_color);

                if(error) {
                    window.draw(error_message);
                } else {
                    window.draw(image);
                }

                chapter_text_background.setSize(sf::Vector2f(window_size.x, background_height));
                chapter_text_background.setPosition(0.0f, std::floor(window_size.y - background_height));
                window.draw(chapter_text_background);

                auto text_bounds = chapter_text.getLocalBounds();
                chapter_text.setPosition(std::floor(window_size.x * 0.5f - text_bounds.width * 0.5f), std::floor(window_size.y - background_height * 0.5f - font_height * 0.5f));
                window.draw(chapter_text);
            }

            window.display();
        }
    }

    void Program::content_list_page() {
        if(current_plugin->get_content_list(content_list_url, body->items) != PluginResult::OK) {
            show_notification("Content list", "Failed to get content list for url: " + content_list_url, Urgency::CRITICAL);
            current_page = Page::SEARCH_SUGGESTION;
            return;
        }

        search_bar->onTextUpdateCallback = [this](const std::string &text) {
            body->filter_search_fuzzy(text);
            body->select_first_item();
        };

        search_bar->onTextSubmitCallback = [this](const std::string &text) -> bool {
            BodyItem *selected_item = body->get_selected();
            if(!selected_item)
                return false;

            content_episode = selected_item->title;
            content_url = selected_item->url;
            current_page = Page::CONTENT_DETAILS;
            body->clear_items();
            return true;
        };

        sf::Vector2f body_pos;
        sf::Vector2f body_size;
        bool redraw = true;
        sf::Event event;

        while (current_page == Page::CONTENT_LIST) {
            while (window.pollEvent(event)) {
                base_event_handler(event, Page::SEARCH_SUGGESTION);
                if(event.type == sf::Event::Resized || event.type == sf::Event::GainedFocus)
                    redraw = true;
            }

            // TODO: This code is duplicated in many places. Handle it in one place.
            if(redraw) {
                redraw = false;
                search_bar->onWindowResize(window_size);

                float body_padding_horizontal = 50.0f;
                float body_padding_vertical = 50.0f;
                float body_width = window_size.x - body_padding_horizontal * 2.0f;
                if(body_width < 400) {
                    body_width = window_size.x;
                    body_padding_horizontal = 0.0f;
                }

                float search_bottom = search_bar->getBottom();
                body_pos = sf::Vector2f(body_padding_horizontal, search_bottom + body_padding_vertical);
                body_size = sf::Vector2f(body_width, window_size.y - search_bottom - body_padding_vertical);
            }

            search_bar->update();

            window.clear(back_color);
            body->draw(window, body_pos, body_size);
            search_bar->draw(window);
            window.display();
        }
    }

    void Program::content_details_page() {
        if(current_plugin->get_content_details(content_list_url, content_url, body->items) != PluginResult::OK) {
            show_notification("Content details", "Failed to get content details for url: " + content_url, Urgency::CRITICAL);
            // TODO: This will return to an empty content list.
            // Each page should have its own @Body so we can return to the last page and still have the data loaded
            // however the cached images should be cleared.
            current_page = Page::CONTENT_LIST;
            return;
        }

        // Instead of using search bar to searching, use it for commenting.
        // TODO: Have an option for the search bar to be multi-line.
        search_bar->onTextUpdateCallback = nullptr;

        search_bar->onTextSubmitCallback = [this](const std::string &text) -> bool {
            if(text.empty())
                return false;
            
            return true;
        };

        sf::Vector2f body_pos;
        sf::Vector2f body_size;
        bool redraw = true;
        sf::Event event;

        while (current_page == Page::CONTENT_DETAILS) {
            while (window.pollEvent(event)) {
                base_event_handler(event, Page::CONTENT_LIST);
                if(event.type == sf::Event::Resized || event.type == sf::Event::GainedFocus)
                    redraw = true;
            }

            // TODO: This code is duplicated in many places. Handle it in one place.
            if(redraw) {
                redraw = false;
                search_bar->onWindowResize(window_size);

                float body_padding_horizontal = 50.0f;
                float body_padding_vertical = 50.0f;
                float body_width = window_size.x - body_padding_horizontal * 2.0f;
                if(body_width < 400) {
                    body_width = window_size.x;
                    body_padding_horizontal = 0.0f;
                }

                float search_bottom = search_bar->getBottom();
                body_pos = sf::Vector2f(body_padding_horizontal, search_bottom + body_padding_vertical);
                body_size = sf::Vector2f(body_width, window_size.y - search_bottom - body_padding_vertical);
            }

            search_bar->update();

            window.clear(back_color);
            body->draw(window, body_pos, body_size);
            search_bar->draw(window);
            window.display();
        }
    }

    void Program::image_board_thread_list_page() {
        assert(current_plugin->is_image_board());
        ImageBoard *image_board = static_cast<ImageBoard*>(current_plugin);
        if(image_board->get_threads(image_board_thread_list_url, body->items) != PluginResult::OK) {
            show_notification("Content list", "Failed to get threads for url: " + image_board_thread_list_url, Urgency::CRITICAL);
            current_page = Page::SEARCH_SUGGESTION;
            return;
        }

        search_bar->onTextUpdateCallback = [this](const std::string &text) {
            body->filter_search_fuzzy(text);
            body->select_first_item();
        };

        search_bar->onTextSubmitCallback = [this](const std::string &text) -> bool {
            BodyItem *selected_item = body->get_selected();
            if(!selected_item)
                return false;

            content_episode = selected_item->title;
            content_url = selected_item->url;
            current_page = Page::IMAGE_BOARD_THREAD;
            body->clear_items();
            return true;
        };

        sf::Vector2f body_pos;
        sf::Vector2f body_size;
        bool redraw = true;
        sf::Event event;

        while (current_page == Page::IMAGE_BOARD_THREAD_LIST) {
            while (window.pollEvent(event)) {
                base_event_handler(event, Page::SEARCH_SUGGESTION);
                if(event.type == sf::Event::Resized || event.type == sf::Event::GainedFocus)
                    redraw = true;
            }

            // TODO: This code is duplicated in many places. Handle it in one place.
            if(redraw) {
                redraw = false;
                search_bar->onWindowResize(window_size);

                float body_padding_horizontal = 50.0f;
                float body_padding_vertical = 50.0f;
                float body_width = window_size.x - body_padding_horizontal * 2.0f;
                if(body_width < 400) {
                    body_width = window_size.x;
                    body_padding_horizontal = 0.0f;
                }

                float search_bottom = search_bar->getBottom();
                body_pos = sf::Vector2f(body_padding_horizontal, search_bottom + body_padding_vertical);
                body_size = sf::Vector2f(body_width, window_size.y - search_bottom - body_padding_vertical);
            }

            search_bar->update();

            window.clear(back_color);
            body->draw(window, body_pos, body_size);
            search_bar->draw(window);
            window.display();
        }
    }

    static bool is_url_video(const std::string &url) {
        return string_ends_with(url, ".webm") || string_ends_with(url, ".mp4") || string_ends_with(url, ".gif");
    }

    void Program::image_board_thread_page() {
        assert(current_plugin->is_image_board());
        // TODO: Support image board other than 4chan. To make this work, the captcha code needs to be changed
        // to work with other captcha than google captcha
        assert(current_plugin->name == "4chan");
        ImageBoard *image_board = static_cast<ImageBoard*>(current_plugin);
        if(image_board->get_thread_comments(image_board_thread_list_url, content_url, body->items) != PluginResult::OK) {
            show_notification("Content details", "Failed to get content details for url: " + content_url, Urgency::CRITICAL);
            // TODO: This will return to an empty content list.
            // Each page should have its own @Body so we can return to the last page and still have the data loaded
            // however the cached images should be cleared.
            current_page = Page::IMAGE_BOARD_THREAD_LIST;
            return;
        }

        const std::string &board = image_board_thread_list_url;
        const std::string &thread = content_url;

        // TODO: Instead of using stage here, use different pages for each stage
        enum class NavigationStage {
            VIEWING_COMMENTS,
            REPLYING,
            SOLVING_POST_CAPTCHA,
            POSTING_SOLUTION,
            POSTING_COMMENT,
            VIEWING_ATTACHED_IMAGE
        };

        NavigationStage navigation_stage = NavigationStage::VIEWING_COMMENTS;
        std::future<bool> captcha_request_future;
        std::future<bool> captcha_post_solution_future;
        std::future<bool> post_comment_future;
        std::future<bool> load_image_future;
        sf::Texture captcha_texture;
        sf::Sprite captcha_sprite;
        std::mutex captcha_image_mutex;

        auto attached_image_texture = std::make_unique<sf::Texture>();
        sf::Sprite attached_image_sprite;
        std::mutex attachment_load_mutex;

        GoogleCaptchaChallengeInfo challenge_info;
        sf::Text challenge_description_text("", font, 24);
        challenge_description_text.setFillColor(sf::Color::White);
        const size_t captcha_num_columns = 3;
        const size_t captcha_num_rows = 3;
        std::array<bool, captcha_num_columns * captcha_num_rows> selected_captcha_images;
        for(size_t i = 0; i < selected_captcha_images.size(); ++i) {
            selected_captcha_images[i] = false;
        }
        sf::RectangleShape captcha_selection_rect;
        captcha_selection_rect.setOutlineThickness(5.0f);
        captcha_selection_rect.setOutlineColor(sf::Color(0, 85, 119));
        // TODO: Draw only the outline instead of a transparent rectangle
        captcha_selection_rect.setFillColor(sf::Color::Transparent);

        // Valid for 2 minutes after solving a captcha
        std::string captcha_post_id;
        sf::Clock captcha_solved_time;
        std::string comment_to_post;

        // TODO: Show a white image with "Loading..." text while the captcha image is downloading

        // TODO: Make this work with other sites than 4chan
        auto request_google_captcha_image = [this, &captcha_texture, &captcha_image_mutex, &navigation_stage, &captcha_sprite, &challenge_description_text](GoogleCaptchaChallengeInfo &challenge_info) {
            std::string payload_image_data;
            DownloadResult download_image_result = download_to_string(challenge_info.payload_url, payload_image_data, {}, current_plugin->use_tor);
            if(download_image_result == DownloadResult::OK) {
                std::lock_guard<std::mutex> lock(captcha_image_mutex);
                if(captcha_texture.loadFromMemory(payload_image_data.data(), payload_image_data.size())) {
                    captcha_texture.setSmooth(true);
                    captcha_sprite.setTexture(captcha_texture, true);
                    challenge_description_text.setString(challenge_info.description);
                } else {
                    show_notification("Google captcha", "Failed to load downloaded captcha image", Urgency::CRITICAL);
                    navigation_stage = NavigationStage::VIEWING_COMMENTS;
                }
            } else {
                show_notification("Google captcha", "Failed to download captcha image", Urgency::CRITICAL);
                navigation_stage = NavigationStage::VIEWING_COMMENTS;
            }
        };

        auto request_new_google_captcha_challenge = [this, &selected_captcha_images, &navigation_stage, &captcha_request_future, &request_google_captcha_image, &challenge_info]() {
            fprintf(stderr, "Solving captcha!\n");
            navigation_stage = NavigationStage::SOLVING_POST_CAPTCHA;
            for(size_t i = 0; i < selected_captcha_images.size(); ++i) {
                selected_captcha_images[i] = false;
            }
            const std::string referer = "https://boards.4chan.org/";
            captcha_request_future = google_captcha_request_challenge(fourchan_google_captcha_api_key, referer,
                [&navigation_stage, &request_google_captcha_image, &challenge_info](std::optional<GoogleCaptchaChallengeInfo> new_challenge_info) {
                    if(navigation_stage != NavigationStage::SOLVING_POST_CAPTCHA)
                        return;

                    if(new_challenge_info) {
                        challenge_info = new_challenge_info.value();
                        request_google_captcha_image(challenge_info);
                    } else {
                        show_notification("Google captcha", "Failed to get captcha challenge", Urgency::CRITICAL);
                        navigation_stage = NavigationStage::VIEWING_COMMENTS;
                    }
                }, current_plugin->use_tor);
        };

        auto post_comment = [this, &navigation_stage, &image_board, &board, &thread, &captcha_post_id, &comment_to_post, &request_new_google_captcha_challenge]() {
            navigation_stage = NavigationStage::POSTING_COMMENT;
            PostResult post_result = image_board->post_comment(board, thread, captcha_post_id, comment_to_post);
            if(post_result == PostResult::OK) {
                show_notification(current_plugin->name, "Comment posted!");
                navigation_stage = NavigationStage::VIEWING_COMMENTS;
                // TODO: Append posted comment to the thread so the user can see their posted comment.
                // TODO: Asynchronously update the thread periodically to show new comments.
            } else if(post_result == PostResult::TRY_AGAIN) {
                show_notification(current_plugin->name, "Error while posting, did the captcha expire? Please try again");
                // TODO: Check if the response contains a new captcha instead of requesting a new one manually
                request_new_google_captcha_challenge();
            } else if(post_result == PostResult::BANNED) {
                show_notification(current_plugin->name, "Failed to post comment because you are banned", Urgency::CRITICAL);
                navigation_stage = NavigationStage::VIEWING_COMMENTS;
            } else if(post_result == PostResult::ERR) {
                show_notification(current_plugin->name, "Failed to post comment. Is " + current_plugin->name + " down or is your internet down?", Urgency::CRITICAL);
                navigation_stage = NavigationStage::VIEWING_COMMENTS;
            } else {
                assert(false && "Unhandled post result");
                show_notification(current_plugin->name, "Failed to post comment. Unknown error", Urgency::CRITICAL);
                navigation_stage = NavigationStage::VIEWING_COMMENTS;
            }
        };

        // Instead of using search bar to searching, use it for commenting.
        // TODO: Have an option for the search bar to be multi-line.
        search_bar->onTextUpdateCallback = nullptr;
        search_bar->onTextSubmitCallback = [&post_comment_future, &navigation_stage, &request_new_google_captcha_challenge, &comment_to_post, &captcha_post_id, &captcha_solved_time, &post_comment](const std::string &text) -> bool {
            if(text.empty())
                return false;
            
            assert(navigation_stage == NavigationStage::REPLYING);
            comment_to_post = text;
            if(!captcha_post_id.empty() && captcha_solved_time.getElapsedTime().asSeconds() < 120) {
                post_comment_future = std::async(std::launch::async, [&post_comment]() -> bool {
                    post_comment();
                    return true;
                });
            } else {
                request_new_google_captcha_challenge();
            }
            return true;
        };

        sf::Vector2f body_pos;
        sf::Vector2f body_size;
        bool redraw = true;
        sf::Event event;

        std::stack<int> comment_navigation_stack;

        while (current_page == Page::IMAGE_BOARD_THREAD) {
            while (window.pollEvent(event)) {
                if (event.type == sf::Event::Closed) {
                    current_page = Page::EXIT;
                } else if(event.type == sf::Event::Resized) {
                    window_size.x = event.size.width;
                    window_size.y = event.size.height;
                    sf::FloatRect visible_area(0, 0, window_size.x, window_size.y);
                    window.setView(sf::View(visible_area));
                }

                if(event.type == sf::Event::Resized || event.type == sf::Event::GainedFocus)
                    redraw = true;
                else if(event.type == sf::Event::KeyPressed && navigation_stage == NavigationStage::VIEWING_COMMENTS) {
                    if(event.key.code == sf::Keyboard::Up) {
                        body->select_previous_item();
                    } else if(event.key.code == sf::Keyboard::Down) {
                        body->select_next_item();
                    } else if(event.key.code == sf::Keyboard::Escape) {
                        current_page = Page::IMAGE_BOARD_THREAD_LIST;
                        body->clear_items();
                        body->reset_selected();
                        search_bar->clear();
                    } else if(event.key.code == sf::Keyboard::P) {
                        // TODO: Make this work when thumbnail is preview for a video/gif instead of a static image
                        BodyItem *selected_item = body->get_selected();
                        if(selected_item && !selected_item->attached_content_url.empty()) {
                            if(is_url_video(selected_item->attached_content_url)) { 
                                page_stack.push(Page::IMAGE_BOARD_THREAD);
                                current_page = Page::VIDEO_CONTENT;
                                std::string prev_content_url = content_url;
                                content_url = selected_item->attached_content_url;
                                video_content_page();
                                content_url = std::move(prev_content_url);
                            } else {
                                navigation_stage = NavigationStage::VIEWING_ATTACHED_IMAGE;
                                load_image_future = std::async(std::launch::async, [this, &image_board, &attached_image_texture, &attached_image_sprite, &attachment_load_mutex]() -> bool {
                                    BodyItem *selected_item = body->get_selected();
                                    if(!selected_item || selected_item->attached_content_url.empty()) {
                                        return false;
                                    }
                                    std::string image_data;
                                    if(download_to_string(selected_item->attached_content_url, image_data, {}, image_board->use_tor) != DownloadResult::OK) {
                                        show_notification(image_board->name, "Failed to download image: " + selected_item->attached_content_url, Urgency::CRITICAL);
                                        return false;
                                    }

                                    std::lock_guard<std::mutex> lock(attachment_load_mutex);
                                    if(!attached_image_texture->loadFromMemory(image_data.data(), image_data.size())) {
                                        show_notification(image_board->name, "Failed to load image downloaded from url: " + selected_item->attached_content_url, Urgency::CRITICAL);
                                        return false;
                                    }
                                    attached_image_texture->setSmooth(true);
                                    attached_image_sprite.setTexture(*attached_image_texture, true);
                                    return true;
                                });
                            }
                        }
                    }

                    BodyItem *selected_item = body->get_selected();
                    if(event.key.code == sf::Keyboard::Enter && selected_item && (comment_navigation_stack.empty() || body->selected_item != comment_navigation_stack.top()) && !selected_item->replies.empty()) {
                        for(auto &body_item : body->items) {
                            body_item->visible = false;
                        }
                        selected_item->visible = true;
                        for(size_t reply_index : selected_item->replies) {
                            body->items[reply_index]->visible = true;
                        }
                        comment_navigation_stack.push(body->selected_item);
                    } else if(event.key.code == sf::Keyboard::BackSpace && !comment_navigation_stack.empty()) {
                        size_t previous_selected = 0;
                        if(!comment_navigation_stack.empty()) {
                            previous_selected = comment_navigation_stack.top();
                        }
                        comment_navigation_stack.pop();
                        if(comment_navigation_stack.empty()) {
                            for(auto &body_item : body->items) {
                                body_item->visible = true;
                            }
                            body->selected_item = previous_selected;
                        } else {
                            for(auto &body_item : body->items) {
                                body_item->visible = false;
                            }
                            body->selected_item = previous_selected;
                            selected_item = body->items[comment_navigation_stack.top()].get();
                            selected_item->visible = true;
                            for(size_t reply_index : selected_item->replies) {
                                body->items[reply_index]->visible = true;
                            }
                        }
                    } else if(event.key.code == sf::Keyboard::C && sf::Keyboard::isKeyPressed(sf::Keyboard::LControl) && selected_item) {
                        navigation_stage = NavigationStage::REPLYING;
                    } else if(event.key.code == sf::Keyboard::R && selected_item) {
                        std::string text_to_add = ">>" + selected_item->post_number;
                        if(search_bar->is_cursor_at_start_of_line())
                            text_to_add += '\n';
                        search_bar->append_text(text_to_add);
                    }
                } else if(event.type == sf::Event::TextEntered && navigation_stage == NavigationStage::REPLYING) {
                    search_bar->onTextEntered(event.text.unicode);
                }

                if(event.type == sf::Event::KeyPressed && navigation_stage == NavigationStage::REPLYING) {
                    if(event.key.code == sf::Keyboard::Escape) {
                        //search_bar->clear();
                        navigation_stage = NavigationStage::VIEWING_COMMENTS;
                    }
                }

                if(event.type == sf::Event::KeyPressed && navigation_stage == NavigationStage::SOLVING_POST_CAPTCHA) {
                    int num = -1;
                    if(event.key.code >= sf::Keyboard::Num1 && event.key.code <= sf::Keyboard::Num9) {
                        num = event.key.code - sf::Keyboard::Num1;
                    } else if(event.key.code >= sf::Keyboard::Numpad1 && event.key.code <= sf::Keyboard::Numpad9) {
                        num = event.key.code - sf::Keyboard::Numpad1;
                    }

                    constexpr int select_map[9] = { 6, 7, 8, 3, 4, 5, 0, 1, 2 };
                    if(num != -1) {
                        int index = select_map[num];
                        selected_captcha_images[index] = !selected_captcha_images[index];
                    }

                    if(event.key.code == sf::Keyboard::Escape) {
                        navigation_stage = NavigationStage::VIEWING_COMMENTS;
                    } else if(event.key.code == sf::Keyboard::Enter) {
                        navigation_stage = NavigationStage::POSTING_SOLUTION;
                        captcha_post_solution_future = google_captcha_post_solution(fourchan_google_captcha_api_key, challenge_info.id, selected_captcha_images,
                            [&navigation_stage, &captcha_post_id, &captcha_solved_time, &selected_captcha_images, &challenge_info, &request_google_captcha_image, &post_comment](std::optional<std::string> new_captcha_post_id, std::optional<GoogleCaptchaChallengeInfo> new_challenge_info) {
                                if(navigation_stage != NavigationStage::POSTING_SOLUTION)
                                    return;

                                if(new_captcha_post_id) {
                                    captcha_post_id = new_captcha_post_id.value();
                                    captcha_solved_time.restart();
                                    post_comment();
                                } else if(new_challenge_info) {
                                    show_notification("Google captcha", "Failed to solve captcha, please try again");
                                    challenge_info = new_challenge_info.value();
                                    navigation_stage = NavigationStage::SOLVING_POST_CAPTCHA;
                                    for(size_t i = 0; i < selected_captcha_images.size(); ++i) {
                                        selected_captcha_images[i] = false;
                                    }
                                    request_google_captcha_image(challenge_info);
                                }
                            }, current_plugin->use_tor);
                    }
                }

                if(event.type == sf::Event::KeyPressed && (navigation_stage == NavigationStage::POSTING_SOLUTION || navigation_stage == NavigationStage::POSTING_COMMENT)) {
                    if(event.key.code == sf::Keyboard::Escape) {
                        navigation_stage = NavigationStage::VIEWING_COMMENTS;
                    }
                }

                if(event.type == sf::Event::KeyPressed && navigation_stage == NavigationStage::VIEWING_ATTACHED_IMAGE) {
                    if(event.key.code == sf::Keyboard::Escape || event.key.code == sf::Keyboard::BackSpace) {
                        navigation_stage = NavigationStage::VIEWING_COMMENTS;
                        attached_image_texture.reset(new sf::Texture());
                    }
                }
            }

            // TODO: This code is duplicated in many places. Handle it in one place.
            if(redraw) {
                redraw = false;
                search_bar->onWindowResize(window_size);

                float body_padding_horizontal = 50.0f;
                float body_padding_vertical = 50.0f;
                float body_width = window_size.x - body_padding_horizontal * 2.0f;
                if(body_width < 400) {
                    body_width = window_size.x;
                    body_padding_horizontal = 0.0f;
                }

                float search_bottom = search_bar->getBottom();
                body_pos = sf::Vector2f(body_padding_horizontal, search_bottom + body_padding_vertical);
                body_size = sf::Vector2f(body_width, window_size.y - search_bottom - body_padding_vertical);
            }

            //search_bar->update();

            window.clear(back_color);
            if(navigation_stage == NavigationStage::SOLVING_POST_CAPTCHA) {
                std::lock_guard<std::mutex> lock(captcha_image_mutex);
                if(captcha_texture.getNativeHandle() != 0) {
                    const float challenge_description_height = challenge_description_text.getCharacterSize() + 10.0f;
                    sf::Vector2f content_size = window_size;
                    content_size.y -= challenge_description_height;

                    sf::Vector2u captcha_texture_size = captcha_texture.getSize();
                    sf::Vector2f captcha_texture_size_f(captcha_texture_size.x, captcha_texture_size.y);
                    auto image_scale = get_ratio(captcha_texture_size_f, clamp_to_size(captcha_texture_size_f, content_size));
                    captcha_sprite.setScale(image_scale);

                    auto image_size = captcha_texture_size_f;
                    image_size.x *= image_scale.x;
                    image_size.y *= image_scale.y;
                    captcha_sprite.setPosition(std::floor(content_size.x * 0.5f - image_size.x * 0.5f), std::floor(challenge_description_height + content_size.y * 0.5f - image_size.y * 0.5f));
                    window.draw(captcha_sprite);

                    challenge_description_text.setPosition(captcha_sprite.getPosition() + sf::Vector2f(image_size.x * 0.5f, 0.0f) - sf::Vector2f(challenge_description_text.getLocalBounds().width * 0.5f, challenge_description_height));
                    window.draw(challenge_description_text);

                    for(size_t column = 0; column < captcha_num_columns; ++column) {
                        for(size_t row = 0; row < captcha_num_rows; ++row) {
                            if(selected_captcha_images[column + captcha_num_columns * row]) {
                                captcha_selection_rect.setPosition(captcha_sprite.getPosition() + sf::Vector2f(image_size.x / captcha_num_columns * column, image_size.y / captcha_num_rows * row));
                                captcha_selection_rect.setSize(sf::Vector2f(image_size.x / captcha_num_columns, image_size.y / captcha_num_rows));
                                window.draw(captcha_selection_rect);
                            }
                        }
                    }
                }
            } else if(navigation_stage == NavigationStage::POSTING_SOLUTION) {
                // TODO: Show "Posting..." when posting solution
            } else if(navigation_stage == NavigationStage::POSTING_COMMENT) {
                // TODO: Show "Posting..." when posting comment
            } else if(navigation_stage == NavigationStage::VIEWING_ATTACHED_IMAGE) {
                // TODO: Show a white image with the text "Downloading..." while the image is downloading and loading
                std::lock_guard<std::mutex> lock(attachment_load_mutex);
                if(attached_image_texture->getNativeHandle() != 0) {
                    auto content_size = window_size;
                    sf::Vector2u texture_size = attached_image_texture->getSize();
                    sf::Vector2f texture_size_f(texture_size.x, texture_size.y);
                    auto image_scale = get_ratio(texture_size_f, clamp_to_size(texture_size_f, content_size));
                    attached_image_sprite.setScale(image_scale);

                    auto image_size = texture_size_f;
                    image_size.x *= image_scale.x;
                    image_size.y *= image_scale.y;
                    attached_image_sprite.setPosition(std::floor(content_size.x * 0.5f - image_size.x * 0.5f), std::floor(content_size.y * 0.5f - image_size.y * 0.5f));
                    window.draw(attached_image_sprite);
                } else {
                    sf::RectangleShape rect(sf::Vector2f(640.0f, 480.0f));
                    rect.setFillColor(sf::Color::White);
                    auto content_size = window_size;
                    auto rect_size = clamp_to_size(rect.getSize(), content_size);
                    rect.setSize(rect_size);
                    rect.setPosition(std::floor(content_size.x * 0.5f - rect_size.x * 0.5f), std::floor(content_size.y * 0.5f - rect_size.y * 0.5f));
                    window.draw(rect);
                }
            } else if(navigation_stage == NavigationStage::REPLYING) {
                body->draw(window, body_pos, body_size);
                search_bar->draw(window);
            } else if(navigation_stage == NavigationStage::VIEWING_COMMENTS) {
                body->draw(window, body_pos, body_size);
                search_bar->draw(window);
            }
            window.display();
        }

        // TODO: Instead of waiting for them, kill them somehow
        if(captcha_request_future.valid())
            captcha_request_future.get();
        if(captcha_post_solution_future.valid())
            captcha_post_solution_future.get();
        if(post_comment_future.valid())
            post_comment_future.get();
        if(load_image_future.valid())
            load_image_future.get();

        // Clear post that is still being written.
        // TODO: A multiline text edit widget should be cleared instead of the search bar.
        // TODO: This post should be saved for the thread. Each thread should have its own text edit widget,
        // so you dont have to retype a post that was in the middle of being posted when returning.
        search_bar->clear();
    }
}