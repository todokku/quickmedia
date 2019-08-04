#pragma once

#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Graphics/Texture.hpp>
#include <SFML/Graphics/Sprite.hpp>
#include <SFML/Window/Context.hpp>
#include <thread>
#include <mutex>
#include <atomic>
#include <stdexcept>

class mpv_handle;
class mpv_opengl_cb_context;

namespace QuickMedia {
    class VideoInitializationException : public std::runtime_error {
    public:
        VideoInitializationException(const std::string &errMsg) : std::runtime_error(errMsg) {}
    };
    
    class VideoPlayer {
    public:
        // Throws VideoInitializationException on error
        VideoPlayer(unsigned int width, unsigned int height, const char *file, bool loop = false);
        ~VideoPlayer();
        
        void setPosition(float x, float y);
        bool resize(const sf::Vector2i &size);
        void draw(sf::RenderWindow &window);
        
        // This counter is incremented when mpv wants to redraw content
        std::atomic_int redrawCounter;
    private:
        sf::Context context;
        mpv_handle *mpv;
        mpv_opengl_cb_context *mpvGl;
        std::thread renderThread;
        std::mutex renderMutex;
        sf::Sprite sprite;
        sf::Texture texture;
        sf::Uint8 *textureBuffer;
        bool alive;
        sf::Vector2i video_size;
        sf::Vector2i desired_size;
    };
}
