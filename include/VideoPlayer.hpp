#pragma once

#include <SFML/Graphics/RenderWindow.hpp>
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
class mpv_opengl_cb_context;

namespace QuickMedia {
    class VideoInitializationException : public std::runtime_error {
    public:
        VideoInitializationException(const std::string &errMsg) : std::runtime_error(errMsg) {}
    };

    using PlaybackEndedCallback = std::function<void()>;
    
    class VideoPlayer {
    public:
        // Throws VideoInitializationException on error
        VideoPlayer(unsigned int width, unsigned int height, const char *file, bool loop = false);
        ~VideoPlayer();
        
        void setPosition(float x, float y);
        void resize(const sf::Vector2i &size);
        void draw(sf::RenderWindow &window);

        void load_file(const std::string &path);
        
        // This counter is incremented when mpv wants to redraw content
        std::atomic_int redrawCounter;
        sf::Context context;
        PlaybackEndedCallback onPlaybackEndedCallback;
    private:
        mpv_handle *mpv;
        mpv_opengl_cb_context *mpvGl;
        sf::Sprite sprite;
        sf::Texture texture;
        sf::Uint8 *textureBuffer;
        sf::Vector2i video_size;
        sf::Vector2i desired_size;
        sf::RectangleShape seekbar;
        sf::RectangleShape seekbar_background;
    };
}
