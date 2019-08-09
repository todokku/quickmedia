#pragma once

#include <SFML/Window/WindowHandle.hpp>
#include <SFML/System/Clock.hpp>
#include <stdio.h>
#include <functional>
#include <json/value.h>

#include <sys/un.h>
#include <X11/Xlib.h>

namespace QuickMedia {
    using EventCallbackFunc = std::function<void(const char *event_name)>;
    using VideoPlayerWindowCreateCallback = std::function<void(sf::WindowHandle window)>;

    // Currently this video player launches mpv and embeds it into the QuickMedia window
    class VideoPlayer {
    public:
        enum class Error {
            OK,
            FAIL_TO_LAUNCH_PROCESS,
            FAIL_TO_CREATE_SOCKET,
            FAIL_TO_GENERATE_IPC_FILENAME,
            FAIL_TO_CONNECT_TIMEOUT,
            FAIL_NOT_CONNECTED,
            FAIL_TO_SEND,
            FAIL_TO_FIND_WINDOW,
            FAIL_TO_FIND_WINDOW_TIMEOUT,
            UNEXPECTED_WINDOW_ERROR,
            FAIL_TO_READ,
            READ_TIMEOUT,
            READ_INCORRECT_TYPE,
            INIT_FAILED
        };

        // @event_callback is called from another thread
        VideoPlayer(EventCallbackFunc event_callback, VideoPlayerWindowCreateCallback window_create_callback);
        ~VideoPlayer();
        VideoPlayer(const VideoPlayer&) = delete;
        VideoPlayer& operator=(const VideoPlayer&) = delete;

        // @path can also be an url if youtube-dl is installed and accessible to mpv
        Error load_video(const char *path, sf::WindowHandle parent_window);
        // Should be called every update frame
        Error update();

        Error toggle_pause();

        // Progress is in range [0..1]
        Error get_progress(double *result);
        // Progress is in range [0..1]
        Error set_progress(double progress);
    private:
        Error send_command(const char *cmd, size_t size);
        Error launch_video_process(const char *path, sf::WindowHandle parent_window);
        VideoPlayer::Error read_ipc_func();
    private:
        pid_t video_process_id;
        int ipc_socket;
        bool connected_to_ipc;
        sf::Clock retry_timer;
        int connect_tries;
        int find_window_tries;
        struct sockaddr_un ipc_addr;
        char ipc_server_path[L_tmpnam];
        EventCallbackFunc event_callback;
        VideoPlayerWindowCreateCallback window_create_callback;
        sf::WindowHandle window_handle;
        sf::WindowHandle parent_window;
        Display *display;
        unsigned int request_id;
        unsigned int expected_request_id;
        Json::Value request_response_data;
    };
}
