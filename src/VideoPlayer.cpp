#include "../include/VideoPlayer.hpp"
#include <SFML/Window/Mouse.hpp>
#include <mpv/client.h>
#include <mpv/render_gl.h>
#include <SFML/OpenGL.hpp>
#include <clocale>
#include <cmath>

namespace QuickMedia {
    static void* getProcAddressMpv(void *funcContext, const char *name) {
        return (void*)sf::Context::getFunction(name);
    }
    
    static void onMpvRedraw(void *ctx) {
        VideoPlayer *video_player = (VideoPlayer*)ctx;
        video_player->redraw = true;
    }

    static void on_mpv_events(void *ctx) {
        VideoPlayer *video_player = (VideoPlayer*)ctx;
        video_player->event_update = true;
    }

    static void check_error(int status) {
        if(status < 0)
            fprintf(stderr, "mpv api error: %s\n", mpv_error_string(status));
    }

    static void mpv_set_option_bool(mpv_handle *mpv, const char *option, bool value) {
        int int_value = value;
        check_error(mpv_set_option(mpv, option, MPV_FORMAT_FLAG, &int_value));
    }

    static void mpv_set_option_int64(mpv_handle *mpv, const char *option, int64_t value) {
        check_error(mpv_set_option(mpv, option, MPV_FORMAT_INT64, &value));
    }

    class ContextScope {
    public:
        ContextScope(sf::Context *_context) : context(_context) {
            context->setActive(true);
        }

        ~ContextScope() {
            context->setActive(false);
        }

        sf::Context *context;
    };
    
    VideoPlayer::VideoPlayer(unsigned int width, unsigned int height, sf::WindowHandle window_handle, const char *file, bool loop) : 
        redraw(false),
        event_update(false),
        onPlaybackEndedCallback(nullptr),
        mpv(nullptr),
        mpvGl(nullptr),
        context(std::make_unique<sf::Context>(sf::ContextSettings(), width, height)),
        textureBuffer(nullptr),
        desired_size(width, height)
    {
        ContextScope context_scope(context.get());
        texture.setSmooth(true);
        
        // mpv_create requires LC_NUMERIC to be set to "C" for some reason, see mpv_create documentation
        std::setlocale(LC_NUMERIC, "C");
        mpv = mpv_create();
        if(!mpv)
            throw VideoInitializationException("Failed to create mpv handle");

        //check_error(mpv_set_option_string(mpv, "input-default-bindings", "yes"));
        //check_error(mpv_set_option_string(mpv, "input-vo-keyboard", "yes"));
        check_error(mpv_set_option_string(mpv, "cache-secs", "120"));
        check_error(mpv_set_option_string(mpv, "demuxer-max-bytes", "20M"));
        check_error(mpv_set_option_string(mpv, "demuxer-max-back-bytes", "10M"));

        //mpv_set_option_bool(mpv, "osc", true);
        //mpv_set_option_int64(mpv, "wid", window_handle);
        
        if(mpv_initialize(mpv) < 0)
            throw VideoInitializationException("Failed to initialize mpv");

        // TODO: Enabling vo=gpu will make mpv create its own window, or take over the QuickMedia window fully
        // if "wid" option is used. To take advantage of vo=gpu, QuickMedia should create a parent window
        // and make mpv use that and then embed that into the parent QuickMedia window.
        // This will also remove the need for rendering with sfml (no need for texture copy!).
        //check_error(mpv_set_option_string(mpv, "vo", "gpu"));
        //check_error(mpv_set_option_string(mpv, "hwdec", "auto"));

        check_error(mpv_set_option_string(mpv, "terminal", "yes"));
        check_error(mpv_set_option_string(mpv, "msg-level", "all=v"));

        if(loop)
            check_error(mpv_set_option_string(mpv, "loop", "inf"));

        //mpvGl = (mpv_opengl_cb_context*)mpv_get_sub_api(mpv, MPV_SUB_API_OPENGL_CB);
        //if(!mpvGl)
        //    throw VideoInitializationException("Failed to initialize mpv opengl render context");

        mpv_opengl_init_params opengl_init_params;
        opengl_init_params.get_proc_address = getProcAddressMpv;
        opengl_init_params.get_proc_address_ctx = nullptr;
        opengl_init_params.extra_exts = nullptr;
        mpv_render_param params[] = {
            { MPV_RENDER_PARAM_API_TYPE, (void*)MPV_RENDER_API_TYPE_OPENGL },
            { MPV_RENDER_PARAM_OPENGL_INIT_PARAMS, &opengl_init_params },
            { (mpv_render_param_type)0, nullptr }
        };

        if (mpv_render_context_create(&mpvGl, mpv, params) < 0)
            throw VideoInitializationException("failed to initialize mpv GL context");
        
        mpv_render_context_set_update_callback(mpvGl, onMpvRedraw, this);
        mpv_set_wakeup_callback(mpv, on_mpv_events, this);
        
        seekbar.setFillColor(sf::Color::White);
        seekbar_background.setFillColor(sf::Color(0, 0, 0, 150));
        load_file(file);
    }
    
    VideoPlayer::~VideoPlayer() {
        if(mpvGl) {
            //mpv_render_context_set_update_callback(mpvGl, nullptr, nullptr);
            mpv_render_context_free(mpvGl);
        }

        free(textureBuffer);

        if(mpv) {
            //mpv_set_wakeup_callback(mpv, nullptr, nullptr);
            //mpv_detach_destroy(mpv);
            mpv_terminate_destroy(mpv);
        }
    }
    
    void VideoPlayer::setPosition(float x, float y) {
        sprite.setPosition(x, y);
    }

    // TODO: Fix this, it's incorrect
    static void wrap_sprite_to_size(sf::Sprite &sprite, sf::Vector2i texture_size, const sf::Vector2f &size) {
        auto image_size = sf::Vector2f(texture_size.x, texture_size.y);

        double overflow_x  = image_size.x - size.x;
        double overflow_y  = image_size.y - size.y;

        auto scale = sprite.getScale();
        float scale_ratio = scale.x / scale.y;
        bool reverse = overflow_x < 0.0f && overflow_y < 0.0f;

        if((reverse && overflow_x * scale_ratio < overflow_y) || overflow_x * scale_ratio > overflow_y) {
            float overflow_ratio = overflow_x / image_size.x;
            float scale_x = 1.0f - overflow_ratio;
            sprite.setScale(scale_x, scale_x * scale_ratio);
        } else {
            float overflow_ratio = overflow_y / image_size.y;
            float scale_y = 1.0f - overflow_ratio;
            sprite.setScale(scale_y * scale_ratio, scale_y);
        }
    }

    void VideoPlayer::resize(const sf::Vector2i &size) {
        desired_size = size;
        if(!textureBuffer)
            return;
        wrap_sprite_to_size(sprite, video_size, sf::Vector2f(desired_size.x, desired_size.y));
        auto image_scale = sprite.getScale();
        auto image_size = sf::Vector2f(video_size.x, video_size.y);
        image_size.x *= image_scale.x;
        image_size.y *= image_scale.y;
        sprite.setPosition(std::floor(desired_size.x * 0.5f - image_size.x * 0.5f), std::floor(desired_size.y * 0.5f - image_size.y * 0.5f));
    }

    void VideoPlayer::handle_events() {
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
                    context.reset(new sf::Context(sf::ContextSettings(), w, h));
                    context->setActive(true);
                    // TODO: Verify if it's valid to re-create the texture like this,
                    // instead of using deconstructor
                    if(texture.create(w, h)) {
                        void *newTextureBuf = realloc(textureBuffer, w * h * 4);
                        if(newTextureBuf)
                            textureBuffer = (sf::Uint8*)newTextureBuf;
                    }
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
    }
    
    void VideoPlayer::draw(sf::RenderWindow &window) {
        if(event_update.exchange(false))
            handle_events();
        
        if(textureBuffer && redraw) {
            uint64_t update_flags = mpv_render_context_update(mpvGl);
            if((update_flags & MPV_RENDER_UPDATE_FRAME) && redraw.exchange(false)) {
                auto textureSize = texture.getSize();

                mpv_opengl_fbo opengl_fbo;
                opengl_fbo.fbo = 0;
                opengl_fbo.w = textureSize.x;
                opengl_fbo.h = textureSize.y;
                opengl_fbo.internal_format = 0;
                mpv_render_param params[] = 
                {
                    { MPV_RENDER_PARAM_OPENGL_FBO, &opengl_fbo },
                    { (mpv_render_param_type)0, nullptr }
                };
                
                context->setActive(true);
                mpv_render_context_render(mpvGl, params);
                // TODO: Instead of copying video to cpu buffer and then to texture, copy directly from video buffer to texture buffer
                glReadPixels(0, 0, textureSize.x, textureSize.y, GL_RGBA, GL_UNSIGNED_BYTE, textureBuffer);
                context->setActive(false);
                texture.update(textureBuffer);
                sprite.setTexture(texture, true);
                mpv_render_context_report_swap(mpvGl);
            }
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
