#pragma once

#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Window/Event.hpp>
#include <SFML/Graphics/Texture.hpp>
#include <SFML/Graphics/Sprite.hpp>
#include <SFML/Graphics/RectangleShape.hpp>
#include <SFML/Window/Context.hpp>
#include <thread>
#include <mutex>
#include <atomic>
#include <stdexcept>
#include <functional>

class mpv_handle;
class mpv_render_context;

namespace QuickMedia {
    class VideoInitializationException : public std::runtime_error {
    public:
        VideoInitializationException(const std::string &errMsg) : std::runtime_error(errMsg) {}
    };

    using PlaybackEndedCallback = std::function<void()>;
    
    class VideoPlayer {
    public:
        // Throws VideoInitializationException on error
        VideoPlayer(sf::RenderWindow *window, unsigned int width, unsigned int height, const char *file, bool loop = false);
        ~VideoPlayer();

        VideoPlayer(const VideoPlayer&) = delete;
        VideoPlayer& operator=(const VideoPlayer&) = delete;
        
        void handle_event(sf::Event &event);
        void setPosition(float x, float y);
        void resize(const sf::Vector2f &size);
        void draw(sf::RenderWindow &window);

        // @path can also be an url if youtube-dl is installed
        void load_file(const std::string &path);
        
        // This is updated when mpv wants to render
        std::atomic_bool redraw;
        std::atomic_bool event_update;
        // Important: Do not destroy the video player in this callback
        PlaybackEndedCallback onPlaybackEndedCallback;
    private:
        void handle_mpv_events();
    private:
        mpv_handle *mpv;
        mpv_render_context *mpvGl;
        std::unique_ptr<sf::Context> context;
        sf::Sprite sprite;
        sf::Texture texture;
        sf::Uint8 *textureBuffer;
        sf::Vector2i video_size;
        sf::Vector2f desired_size;
        sf::RectangleShape seekbar;
        sf::RectangleShape seekbar_background;
        sf::Clock cursor_last_active_timer;
    };
}
