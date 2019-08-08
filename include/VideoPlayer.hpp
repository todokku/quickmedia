#pragma once

#include <SFML/Window/WindowHandle.hpp>
#include <SFML/System/Clock.hpp>
#include <stdio.h>
#include <functional>
#include <thread>

#include <sys/un.h>

namespace QuickMedia {
    using EventCallbackFunc = std::function<void(const char *event_name)>;

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
            INIT_FAILED
        };

        // @event_callback is called from another thread
        VideoPlayer(EventCallbackFunc event_callback);
        ~VideoPlayer();
        VideoPlayer(const VideoPlayer&) = delete;
        VideoPlayer& operator=(const VideoPlayer&) = delete;

        // @path can also be an url if youtube-dl is installed and accessible to mpv
        Error load_video(const char *path, sf::WindowHandle parent_window);
        // Should be called every update frame
        Error update();

        Error toggle_pause();
    private:
        Error send_command(const char *cmd, size_t size);
        Error launch_video_process(const char *path, sf::WindowHandle parent_window);
        void read_ipc_func();
    private:
        pid_t video_process_id;
        int ipc_socket;
        bool connected_to_ipc;
        sf::Clock ipc_connect_retry_timer;
        int connect_tries;
        struct sockaddr_un ipc_addr;
        char ipc_server_path[L_tmpnam];
        EventCallbackFunc event_callback;
        std::thread event_read_thread;
        bool alive;
    };
}
