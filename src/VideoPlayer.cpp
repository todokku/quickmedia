#include "../include/VideoPlayer.hpp"
#include "../include/Program.h"
#include <string>
#include <json/reader.h>
#include <assert.h>

#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>

const int RETRY_TIME_MS = 1000;
const int MAX_RETRIES = 5;

namespace QuickMedia {
    VideoPlayer::VideoPlayer(EventCallbackFunc _event_callback) :
        video_process_id(-1),
        ipc_socket(-1),
        connected_to_ipc(false),
        connect_tries(0),
        event_callback(_event_callback),
        alive(true)
    {
        
    }

    VideoPlayer::~VideoPlayer() {
        if(video_process_id != -1)
            kill(video_process_id, SIGTERM);

        if(ipc_socket != -1)
            close(ipc_socket);
        
        if(video_process_id != -1)
            remove(ipc_server_path);

        alive = false;
        if(event_read_thread.joinable())
            event_read_thread.join();
    }

    VideoPlayer::Error VideoPlayer::launch_video_process(const char *path, sf::WindowHandle parent_window) {
        if(!tmpnam(ipc_server_path)) {
            perror("Failed to generate ipc file name");
            return Error::FAIL_TO_GENERATE_IPC_FILENAME;
        }

        const std::string parent_window_str = std::to_string(parent_window);
        const char *args[] = { "mpv", /*"--keep-open=yes", "--keep-open-pause=no",*/ "--input-ipc-server", ipc_server_path,
            "--no-config", "--no-input-default-bindings", "--input-vo-keyboard=no", "--no-input-cursor",
            "--cache-secs=120", "--demuxer-max-bytes=20M", "--demuxer-max-back-bytes=10M",
            "--vo=gpu", "--hwdec=auto",
            "--wid", parent_window_str.c_str(), "--", path, nullptr };
        if(exec_program_async(args, &video_process_id) != 0)
            return Error::FAIL_TO_LAUNCH_PROCESS;

        printf("mpv input ipc server: %s\n", ipc_server_path);

        if((ipc_socket = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
            perror("Failed to create socket for video player");
            return Error::FAIL_TO_CREATE_SOCKET;
        }

        ipc_addr.sun_family = AF_UNIX;
        strcpy(ipc_addr.sun_path, ipc_server_path);

        int flags = fcntl(ipc_socket, F_GETFL, 0);
        fcntl(ipc_socket, F_SETFL, flags | O_NONBLOCK);

        return Error::OK;
    }

    VideoPlayer::Error VideoPlayer::load_video(const char *path, sf::WindowHandle parent_window) {
        if(video_process_id == -1)
            return launch_video_process(path, parent_window);

        std::string cmd = "loadfile ";
        cmd += path;
        return send_command(cmd.c_str(), cmd.size());
    }

    VideoPlayer::Error VideoPlayer::update() {
        if(ipc_socket == -1)
            return Error::INIT_FAILED;

        if(connect_tries == MAX_RETRIES)
            return Error::FAIL_TO_CONNECT_TIMEOUT;

        if(!connected_to_ipc && ipc_connect_retry_timer.getElapsedTime().asMilliseconds() >= RETRY_TIME_MS) {
            if(connect(ipc_socket, (struct sockaddr*)&ipc_addr, sizeof(ipc_addr)) == -1) {
                ++connect_tries;
                if(connect_tries == MAX_RETRIES) {
                    fprintf(stderr, "Failed to connect to mpv ipc after 5 seconds, last error: %s\n", strerror(errno));
                    return Error::FAIL_TO_CONNECT_TIMEOUT;
                }
            } else {
                connected_to_ipc = true;
                if(event_callback)
                    event_read_thread = std::thread(&VideoPlayer::read_ipc_func, this);
            }
        }

        return Error::OK;
    }

    void VideoPlayer::read_ipc_func() {
        assert(connected_to_ipc);
        assert(event_callback);

        Json::Value json_root;
        Json::CharReaderBuilder json_builder;
        std::unique_ptr<Json::CharReader> json_reader(json_builder.newCharReader());
        std::string json_errors;

        char buffer[2048];
        while(alive) {
            ssize_t bytes_read = read(ipc_socket, buffer, sizeof(buffer));
            if(bytes_read == -1) {
                int err = errno;
                if(err == EAGAIN) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(50));
                    continue;
                }

                fprintf(stderr, "Failed to read from ipc socket, error: %s\n", strerror(err));
                break;
            } else if(bytes_read > 0) {
                int start = 0;
                for(int i = 0; i < bytes_read; ++i) {
                    if(buffer[i] != '\n')
                        continue;

                    if(json_reader->parse(buffer + start, buffer + i, &json_root, &json_errors)) {
                        const Json::Value &event = json_root["event"];
                        if(event.isString())
                            event_callback(event.asCString());
                    } else {
                        fprintf(stderr, "Failed to parse json for ipc: |%.*s|, reason: %s\n", (int)bytes_read, buffer, json_errors.c_str());
                    }

                    start = i + 1;
                }
            }
        }
    }

    VideoPlayer::Error VideoPlayer::toggle_pause() {
        const char cmd[] = "cycle pause\n";
        return send_command(cmd, sizeof(cmd) - 1);
    }

    VideoPlayer::Error VideoPlayer::send_command(const char *cmd, size_t size) {
        if(!connected_to_ipc)
            return Error::FAIL_NOT_CONNECTED;

        if(send(ipc_socket, cmd, size, 0) == -1) {
            fprintf(stderr, "Failed to send to ipc socket, error: %s, command: %.*s\n", strerror(errno), (int)size, cmd);
            return Error::FAIL_TO_SEND;
        }

        return Error::OK;
    }
}
