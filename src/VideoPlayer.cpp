#include "../include/VideoPlayer.hpp"
#include "../include/Program.h"
#include <string>
#include <json/reader.h>
#include <json/writer.h>
#include <memory>
#include <assert.h>

#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>

const int RETRY_TIME_MS = 1000;
const int MAX_RETRIES_CONNECT = 5;
const int MAX_RETRIES_FIND_WINDOW = 10;
const int READ_TIMEOUT_MS = 200;

namespace QuickMedia {
    VideoPlayer::VideoPlayer(EventCallbackFunc _event_callback, VideoPlayerWindowCreateCallback _window_create_callback) :
        video_process_id(-1),
        ipc_socket(-1),
        connected_to_ipc(false),
        connect_tries(0),
        find_window_tries(0),
        event_callback(_event_callback),
        window_create_callback(_window_create_callback),
        window_handle(0),
        parent_window(0),
        display(nullptr),
        request_id(1),
        expected_request_id(0),
        request_response_data(Json::nullValue),
        response_data_status(ResponseDataStatus::NONE)
    {
        display = XOpenDisplay(NULL);
        if (!display)
            throw std::runtime_error("Failed to open display to X11 server");
    }

    VideoPlayer::~VideoPlayer() {
        if(video_process_id != -1) {
            kill(video_process_id, SIGTERM);
            wait_program(video_process_id);
        }

        if(ipc_socket != -1)
            close(ipc_socket);
        
        if(video_process_id != -1)
            remove(ipc_server_path);

        if(display)
            XCloseDisplay(display);
    }

    VideoPlayer::Error VideoPlayer::launch_video_process(const char *path, sf::WindowHandle _parent_window) {
        parent_window = _parent_window;

        if(!tmpnam(ipc_server_path)) {
            perror("Failed to generate ipc file name");
            return Error::FAIL_TO_GENERATE_IPC_FILENAME;
        }

        const std::string parent_window_str = std::to_string(parent_window);
        const char *args[] = { "mpv", "--keep-open=yes", /*"--keep-open-pause=no",*/ "--input-ipc-server", ipc_server_path,
            "--no-config", "--no-input-default-bindings", "--input-vo-keyboard=no", "--no-input-cursor",
            "--cache-secs=120", "--demuxer-max-bytes=40M", "--demuxer-max-back-bytes=20M",
            "--no-input-terminal",
            "--no-osc",
            "--profile=gpu-hq",
            /*"--vo=gpu", "--hwdec=auto",*/
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

    VideoPlayer::Error VideoPlayer::load_video(const char *path, sf::WindowHandle _parent_window) {
        // This check is to make sure we dont change window that the video belongs to. This is not a usecase we will have so
        // no need to support it for not at least.
        assert(parent_window == 0 || parent_window == _parent_window);
        if(video_process_id == -1)
            return launch_video_process(path, _parent_window);

        Json::Value command_data(Json::arrayValue);
        command_data.append("loadfile");
        command_data.append(path);
        Json::Value command(Json::objectValue);
        command["command"] = command_data;

        Json::StreamWriterBuilder builder;
        builder["commentStyle"] = "None";
        builder["indentation"] = "";
        const std::string cmd_str = Json::writeString(builder, command) + "\n";
        return send_command(cmd_str.c_str(), cmd_str.size());
    }

    static std::vector<Window> get_child_window(Display *display, Window window) {
        std::vector<Window> result;
        Window root_window;
        Window parent_window;
        Window *child_window;
        unsigned int num_children;
        if(XQueryTree(display, window, &root_window, &parent_window, &child_window, &num_children) != 0) {
            for(unsigned int i = 0; i < num_children; i++)
                result.push_back(child_window[i]);
        }
        return result;
    }

    VideoPlayer::Error VideoPlayer::update() {
        if(ipc_socket == -1)
            return Error::INIT_FAILED;

        if(connect_tries == MAX_RETRIES_CONNECT)
            return Error::FAIL_TO_CONNECT_TIMEOUT;

        if(find_window_tries == MAX_RETRIES_FIND_WINDOW)
            return Error::FAIL_TO_FIND_WINDOW;

        if(!connected_to_ipc && retry_timer.getElapsedTime().asMilliseconds() >= RETRY_TIME_MS) {
            retry_timer.restart();
            if(connect(ipc_socket, (struct sockaddr*)&ipc_addr, sizeof(ipc_addr)) == -1) {
                ++connect_tries;
                if(connect_tries == MAX_RETRIES_CONNECT) {
                    fprintf(stderr, "Failed to connect to mpv ipc after %d seconds, last error: %s\n", RETRY_TIME_MS * MAX_RETRIES_CONNECT, strerror(errno));
                    return Error::FAIL_TO_CONNECT_TIMEOUT;
                }
            } else {
                connected_to_ipc = true;
            }
        }

        if(connected_to_ipc && window_handle == 0 && retry_timer.getElapsedTime().asMilliseconds() >= RETRY_TIME_MS) {
            retry_timer.restart();
            std::vector<Window> child_windows = get_child_window(display, parent_window);
            size_t num_children = child_windows.size();
            if(num_children == 0) {
                ++find_window_tries;
                if(find_window_tries == MAX_RETRIES_FIND_WINDOW) {
                    fprintf(stderr, "Failed to find mpv window after %d seconds\n", RETRY_TIME_MS * MAX_RETRIES_FIND_WINDOW);
                    return Error::FAIL_TO_FIND_WINDOW_TIMEOUT;
                }
            } else if(num_children == 1) {
                window_handle = child_windows[0];
                if(window_create_callback)
                    window_create_callback(window_handle);
            } else {
                fprintf(stderr, "Expected window to have one child (the video player) but it has %zu\n", num_children);
                return Error::UNEXPECTED_WINDOW_ERROR;
            }
        }

        if(connected_to_ipc && event_callback) {
            Error err = read_ipc_func();
            if(err != Error::OK)
                return err;
        }

        return Error::OK;
    }

    VideoPlayer::Error VideoPlayer::read_ipc_func() {
        assert(connected_to_ipc);
        Json::Value json_root;
        Json::CharReaderBuilder json_builder;
        std::unique_ptr<Json::CharReader> json_reader(json_builder.newCharReader());
        std::string json_errors;

        char buffer[2048];
        ssize_t bytes_read = read(ipc_socket, buffer, sizeof(buffer));
        if(bytes_read == -1) {
            int err = errno;
            if(err != EAGAIN) {
                fprintf(stderr, "Failed to read from ipc socket, error: %s\n", strerror(err));
                return Error::FAIL_TO_READ;
            }
        } else if(bytes_read > 0) {
            int start = 0;
            for(int i = 0; i < bytes_read; ++i) {
                if(buffer[i] != '\n')
                    continue;

                if(json_reader->parse(buffer + start, buffer + i, &json_root, &json_errors)) {
                    const Json::Value &event = json_root["event"];
                    const Json::Value &request_id_json = json_root["request_id"];
                    if(event.isString()) {
                        if(event_callback)
                            event_callback(event.asCString());
                    }

                    if(expected_request_id != 0 && request_id_json.isNumeric() && request_id_json.asUInt() == expected_request_id) {
                        if(json_root["error"].isNull())
                            response_data_status = ResponseDataStatus::ERROR;
                        else
                            response_data_status = ResponseDataStatus::OK;
                        request_response_data = json_root["data"];
                    }
                } else {
                    fprintf(stderr, "Failed to parse json for ipc: |%.*s|, reason: %s\n", (int)bytes_read, buffer, json_errors.c_str());
                }

                start = i + 1;
            }
        }
        return Error::OK;
    }

    VideoPlayer::Error VideoPlayer::toggle_pause() {
        const char cmd[] = "cycle pause\n";
        return send_command(cmd, sizeof(cmd) - 1);
    }

    VideoPlayer::Error VideoPlayer::get_progress(double *result) {
        Json::Value percent_pos_json;
        Error err = get_property("percent-pos", &percent_pos_json, Json::realValue);
        if(err != Error::OK)
            return err;

        *result = percent_pos_json.asDouble() * 0.01;
        return err;
    }

    VideoPlayer::Error VideoPlayer::get_time_remaining(double *result) {
        Json::Value time_remaining_json;
        Error err = get_property("time-remaining", &time_remaining_json, Json::realValue);
        if(err != Error::OK)
            return err;

        *result = time_remaining_json.asDouble();
        return err;
    }

    VideoPlayer::Error VideoPlayer::set_property(const std::string &property_name, const Json::Value &value) {
        Json::Value command_data(Json::arrayValue);
        command_data.append("set_property");
        command_data.append(property_name);
        command_data.append(value);
        Json::Value command(Json::objectValue);
        command["command"] = command_data;

        Json::StreamWriterBuilder builder;
        builder["commentStyle"] = "None";
        builder["indentation"] = "";
        const std::string cmd_str = Json::writeString(builder, command) + "\n";
        return send_command(cmd_str.c_str(), cmd_str.size());
    }

    VideoPlayer::Error VideoPlayer::get_property(const std::string &property_name, Json::Value *result, Json::ValueType result_type) {
        unsigned int cmd_request_id = request_id;
        ++request_id;
        // Overflow check. 0 is defined as no request, 1 is the first valid one
        if(request_id == 0)
            request_id = 1;

        Json::Value command_data(Json::arrayValue);
        command_data.append("get_property");
        command_data.append(property_name);
        Json::Value command(Json::objectValue);
        command["command"] = command_data;
        command["request_id"] = cmd_request_id;

        Json::StreamWriterBuilder builder;
        builder["commentStyle"] = "None";
        builder["indentation"] = "";
        const std::string cmd_str = Json::writeString(builder, command) + "\n";

        Error err = send_command(cmd_str.c_str(), cmd_str.size());
        if(err != Error::OK)
            return err;

        sf::Clock read_timer;
        expected_request_id = cmd_request_id;
        do {
            err = read_ipc_func();
            if(err != Error::OK)
                goto cleanup;

            if(response_data_status != ResponseDataStatus::NONE)
                break;
        } while(read_timer.getElapsedTime().asMilliseconds() < READ_TIMEOUT_MS);

        if(response_data_status == ResponseDataStatus::OK) {
            if(request_response_data.type() == result_type)
                *result = request_response_data;
            else
                err = Error::READ_INCORRECT_TYPE;
        } else if(response_data_status == ResponseDataStatus::ERROR) {
            err = Error::READ_RESPONSE_ERROR;
            goto cleanup;
        } else {
            err = Error::READ_TIMEOUT;
            goto cleanup;
        }

        cleanup:
        expected_request_id = 0;
        response_data_status = ResponseDataStatus::NONE;
        request_response_data = Json::Value(Json::nullValue);
        return err;
    }

    VideoPlayer::Error VideoPlayer::set_paused(bool paused) {
        return set_property("pause", paused);
    }

    VideoPlayer::Error VideoPlayer::set_progress(double progress) {
        std::string cmd = "{ \"command\": [\"set_property\", \"percent-pos\", ";
        cmd += std::to_string(progress * 100.0) + "] }";
        return send_command(cmd.c_str(), cmd.size());
    }

    VideoPlayer::Error VideoPlayer::is_seekable(bool *result) {
        Json::Value seekable_json;
        Error err = get_property("seekable", &seekable_json, Json::booleanValue);
        if(err != Error::OK)
            return err;

        *result = seekable_json.asBool();
        return err;
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
