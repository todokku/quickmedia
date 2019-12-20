#pragma once

#include <string>
#include <vector>

namespace QuickMedia {
    enum class DownloadResult {
        OK,
        ERR,
        NET_ERR
    };

    struct CommandArg {
        std::string option;
        std::string value;
    };

    struct FormData {
        std::string key;
        std::string value;
    };

    DownloadResult download_to_string(const std::string &url, std::string &result, const std::vector<CommandArg> &additional_args, bool use_tor);
    std::vector<CommandArg> create_command_args_from_form_data(const std::vector<FormData> &form_data);
}