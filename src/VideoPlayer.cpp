#include "../include/VideoPlayer.hpp"
#include <mpv/client.h>
#include <mpv/opengl_cb.h>
#include <clocale>

#include <SFML/Config.hpp>

#if defined(SFML_SYSTEM_WINDOWS)
    #ifdef _MSC_VER
        #include <windows.h>
    #endif
    #include <GL/gl.h>
    #include <GL/glx.h>
#elif defined(SFML_SYSTEM_LINUX) || defined(SFML_SYSTEM_FREEBSD)
    #if defined(SFML_OPENGL_ES)
        #include <GLES/gl.h>
        #include <GLES/glext.h>
    #else
        #include <GL/gl.h>
    #endif
    #include <GL/glx.h>
    #define glGetProcAddress glXGetProcAddress
#elif defined(SFML_SYSTEM_MACOS)
    #include <OpenGL/gl.h>
#elif defined (SFML_SYSTEM_IOS)
    #include <OpenGLES/ES1/gl.h>
    #include <OpenGLES/ES1/glext.h>
#elif defined (SFML_SYSTEM_ANDROID)
    #include <GLES/gl.h>
    #include <GLES/glext.h>
    // We're not using OpenGL ES 2+ yet, but we can use the sRGB extension
    #include <GLES2/gl2ext.h>
#endif

using namespace std;

namespace QuickMedia {
    void* getProcAddressMpv(void *funcContext, const char *name) {
        return (void*)glGetProcAddress((const GLubyte*)name);
    }
    
    void onMpvRedraw(void *rawVideo) {
        VideoPlayer *video = (VideoPlayer*)rawVideo;
        ++video->redrawCounter;
    }
    
    VideoPlayer::VideoPlayer(unsigned int width, unsigned int height, const char *file, bool loop) : 
        redrawCounter(0),
        context(sf::ContextSettings(), width, height),
        mpv(nullptr),
        mpvGl(nullptr),
        textureBuffer((sf::Uint8*)malloc(width * height * 4)), // 4 = red, green, blue and alpha
        alive(true),
        video_size(width, height),
        desired_size(width, height)
    {
        if(!textureBuffer)
            throw VideoInitializationException("Failed to allocate memory for video");

        context.setActive(true);
        
        if(!texture.create(width, height))
            throw VideoInitializationException("Failed to create texture for video");
        texture.setSmooth(true);
        
        // mpv_create requires LC_NUMERIC to be set to "C" for some reason, see mpv_create documentation
        std::setlocale(LC_NUMERIC, "C");
        mpv = mpv_create();
        if(!mpv)
            throw VideoInitializationException("Failed to create mpv handle");
        
        if(mpv_initialize(mpv) < 0)
            throw VideoInitializationException("Failed to initialize mpv");

        mpv_set_option_string(mpv, "input-default-bindings", "yes");
        // Enable keyboard input on the X11 window
        mpv_set_option_string(mpv, "input-vo-keyboard", "yes");
        
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
        
        renderThread = thread([this]() {
            context.setActive(true);
            while(alive) {
                while(true) {
                    mpv_event *mpvEvent = mpv_wait_event(mpv, 0.010);
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
                            {
                                lock_guard<mutex> lock(renderMutex);
                                video_size.x = w;
                                video_size.y = h;
                                context.setActive(true);
                                if(texture.create(w, h)) {
                                    void *newTextureBuf = realloc(textureBuffer, w * h * 4);
                                    if(newTextureBuf)
                                        textureBuffer = (sf::Uint8*)newTextureBuf;
                                }
                            }
                            resize(desired_size);
                        }
                    }
                }
                
                if(redrawCounter > 0) {
                    --redrawCounter;
                    context.setActive(true);
                    lock_guard<mutex> lock(renderMutex);
                    auto textureSize = texture.getSize();
                    //mpv_render_context_render(mpvGl, params);
                    mpv_opengl_cb_draw(mpvGl, 0, textureSize.x, textureSize.y);
                    // TODO: Instead of copying video to cpu buffer and then to texture, copy directly from video buffer to texture buffer
                    glReadPixels(0, 0, textureSize.x, textureSize.y, GL_RGBA, GL_UNSIGNED_BYTE, textureBuffer);
                    texture.update(textureBuffer);
                    sprite.setTexture(texture, true);
                    mpv_opengl_cb_report_flip(mpvGl, 0);
                }
            }
        });
        
        const char *cmd[] = { "loadfile", file, nullptr };
        mpv_command(mpv, cmd);
        context.setActive(false);
    }
    
    VideoPlayer::~VideoPlayer() {
        alive = false;
        renderThread.join();
        
        lock_guard<mutex> lock(renderMutex);
        context.setActive(true);
        if(mpvGl)
            mpv_opengl_cb_set_update_callback(mpvGl, nullptr, nullptr);
        
        free(textureBuffer);
        mpv_opengl_cb_uninit_gl(mpvGl);
        mpv_detach_destroy(mpv);
    }
    
    void VideoPlayer::setPosition(float x, float y) {
        sprite.setPosition(x, y);
    }

    bool VideoPlayer::resize(const sf::Vector2i &size) {
        lock_guard<mutex> lock(renderMutex);
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
        desired_size = size;
        #if 0
        void *newTextureBuf = realloc(textureBuffer, size.x * size.y * 4);
        if(!newTextureBuf)
            return false;
        textureBuffer = (sf::Uint8*)newTextureBuf;
        if(!texture.create(size.x, size.y))
            return false;
        return true;
        #endif
        return true;
    }
    
    void VideoPlayer::draw(sf::RenderWindow &window) {
        lock_guard<mutex> lock(renderMutex);
        window.draw(sprite);
    }
}
