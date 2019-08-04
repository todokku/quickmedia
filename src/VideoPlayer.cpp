#include "../include/VideoPlayer.hpp"
#include <SFML/Window/Mouse.hpp>
#include <mpv/client.h>
#include <mpv/opengl_cb.h>
#include <SFML/OpenGL.hpp>
#include <clocale>

namespace QuickMedia {
    static void* getProcAddressMpv(void *funcContext, const char *name) {
        return (void*)sf::Context::getFunction(name);
    }
    
    static void onMpvRedraw(void *rawVideo) {
        VideoPlayer *video_player = (VideoPlayer*)rawVideo;
        ++video_player->redrawCounter;
    }
    
    VideoPlayer::VideoPlayer(unsigned int width, unsigned int height, const char *file, bool loop) : 
        redrawCounter(0),
        onPlaybackEndedCallback(nullptr),
        mpv(nullptr),
        mpvGl(nullptr),
        context(std::make_unique<sf::Context>(sf::ContextSettings(), width, height)),
        textureBuffer(nullptr),
        desired_size(width, height)
    {
        context->setActive(true);
        texture.setSmooth(true);
        
        // mpv_create requires LC_NUMERIC to be set to "C" for some reason, see mpv_create documentation
        std::setlocale(LC_NUMERIC, "C");
        mpv = mpv_create();
        if(!mpv)
            throw VideoInitializationException("Failed to create mpv handle");

        //mpv_set_option_string(mpv, "input-default-bindings", "yes");
        //mpv_set_option_string(mpv, "input-vo-keyboard", "yes");
        mpv_set_option_string(mpv, "cache-secs", "120");
        mpv_set_option_string(mpv, "demuxer-max-bytes", "20M");
        mpv_set_option_string(mpv, "demuxer-max-back-bytes", "10M");
        
        if(mpv_initialize(mpv) < 0)
            throw VideoInitializationException("Failed to initialize mpv");

        mpv_set_option_string(mpv, "vo", "opengl-cb");
        mpv_set_option_string(mpv, "hwdec", "auto");

        if(loop)
            mpv_set_option_string(mpv, "loop", "inf");

        mpvGl = (mpv_opengl_cb_context*)mpv_get_sub_api(mpv, MPV_SUB_API_OPENGL_CB);
        if(!mpvGl)
            throw VideoInitializationException("Failed to initialize mpv opengl render context");
        
        mpv_opengl_cb_set_update_callback(mpvGl, onMpvRedraw, this);
        if(mpv_opengl_cb_init_gl(mpvGl, nullptr, getProcAddressMpv, nullptr) < 0)
            throw VideoInitializationException("Failed to initialize mpv gl callback func");
        
        context->setActive(false);
        
        seekbar.setFillColor(sf::Color::White);
        seekbar_background.setFillColor(sf::Color(0, 0, 0, 150));
        load_file(file);
    }
    
    VideoPlayer::~VideoPlayer() {
        if(mpvGl)
            mpv_opengl_cb_set_update_callback(mpvGl, nullptr, nullptr);
        
        free(textureBuffer);
        mpv_opengl_cb_uninit_gl(mpvGl);
        mpv_detach_destroy(mpv);
    }
    
    void VideoPlayer::setPosition(float x, float y) {
        sprite.setPosition(x, y);
    }

    void VideoPlayer::resize(const sf::Vector2i &size) {
        desired_size = size;
        if(!textureBuffer)
            return;
        float video_ratio = (double)video_size.x / (double)video_size.y;
        float scale_x = 1.0f;
        float scale_y = 1.0f;
        if(video_ratio >= 0.0f) {
            double ratio_x = (double)size.x / (double)video_size.x;
            scale_x = ratio_x;
            scale_y = ratio_x;
            sprite.setPosition(0.0f, size.y * 0.5f - video_size.y * scale_y * 0.5f);
        } else {
            double ratio_y = (double)size.y / (double)video_size.y;
            scale_x = ratio_y;
            scale_y = ratio_y;
            sprite.setPosition(size.x * 0.5f - video_size.x * scale_x * 0.5f, 0.0f);
        }
        sprite.setScale(scale_x, scale_y);
    }
    
    void VideoPlayer::draw(sf::RenderWindow &window) {
        while(true) {
            mpv_event *mpvEvent = mpv_wait_event(mpv, 0.0);
            if(mpvEvent->event_id == MPV_EVENT_NONE)
                break;
            else if(mpvEvent->event_id == MPV_EVENT_SHUTDOWN)
                return;
            else if(mpvEvent->event_id == MPV_EVENT_VIDEO_RECONFIG) {
                int64_t w, h;
                if (mpv_get_property(mpv, "dwidth", MPV_FORMAT_INT64, &w) >= 0 &&
                    mpv_get_property(mpv, "dheight", MPV_FORMAT_INT64, &h) >= 0 &&
                    w > 0 && h > 0 && (w != video_size.x || h != video_size.y))
                {
                    video_size.x = w;
                    video_size.y = h;
                    // TODO: Verify if it's valid to re-create the texture like this,
                    // instead of using deconstructor
                    if(texture.create(w, h)) {
                        void *newTextureBuf = realloc(textureBuffer, w * h * 4);
                        if(newTextureBuf)
                            textureBuffer = (sf::Uint8*)newTextureBuf;
                    }
                    context.reset(new sf::Context(sf::ContextSettings(), w, h));
                    context->setActive(true);
                    glViewport(0, 0, w, h);
                    context->setActive(false);
                    resize(desired_size);
                }
            } else if(mpvEvent->event_id == MPV_EVENT_END_FILE) {
                if(onPlaybackEndedCallback)
                    onPlaybackEndedCallback();
            } else {
                //printf("Mpv event: %s\n", mpv_event_name(mpvEvent->event_id));
            }
        }
        
        if(redrawCounter > 0 && textureBuffer) {
            --redrawCounter;
            auto textureSize = texture.getSize();
            context->setActive(true);
            //mpv_render_context_render(mpvGl, params);
            mpv_opengl_cb_draw(mpvGl, 0, textureSize.x, textureSize.y);
            // TODO: Instead of copying video to cpu buffer and then to texture, copy directly from video buffer to texture buffer
            glReadPixels(0, 0, textureSize.x, textureSize.y, GL_RGBA, GL_UNSIGNED_BYTE, textureBuffer);
            texture.update(textureBuffer);
            sprite.setTexture(texture, true);
            mpv_opengl_cb_report_flip(mpvGl, 0);
            context->setActive(false);
        }
        window.draw(sprite);

        double pos = 0.0;
        mpv_get_property(mpv, "percent-pos", MPV_FORMAT_DOUBLE, &pos);
        pos *= 0.01;

        auto textureSize = sprite.getTextureRect();
        auto scale = sprite.getScale();
        auto video_size = sf::Vector2f(textureSize.width * scale.x, textureSize.height * scale.y);

        const float seekbar_height = video_size.y * 0.025f;
        seekbar.setPosition(sprite.getPosition() + sf::Vector2f(0.0f, video_size.y - seekbar_height));
        seekbar.setSize(sf::Vector2f(video_size.x * pos, seekbar_height));
        window.draw(seekbar);
        seekbar_background.setPosition(seekbar.getPosition() + sf::Vector2f(video_size.x * pos, 0.0f));
        seekbar_background.setSize(sf::Vector2f(video_size.x - video_size.x * pos, seekbar_height));
        window.draw(seekbar_background);

        if(sf::Mouse::isButtonPressed(sf::Mouse::Left)) {
            auto mouse_pos = sf::Mouse::getPosition(window);
            auto seekbar_pos = seekbar.getPosition();
            float diff_x = mouse_pos.x - seekbar_pos.x;
            if(diff_x >= 0.0f && diff_x <= video_size.x && mouse_pos.y >= seekbar_pos.y && mouse_pos.y <= seekbar_pos.y + seekbar_height) {
                double new_pos = ((double)diff_x / video_size.x) * 100.0;
                mpv_set_property(mpv, "percent-pos", MPV_FORMAT_DOUBLE, &new_pos);
            }
        }
    }

    void VideoPlayer::load_file(const std::string &path) {
        const char *cmd[] = { "loadfile", path.c_str(), nullptr };
        mpv_command(mpv, cmd);
    }
}
