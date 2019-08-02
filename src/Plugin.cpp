#include "../plugins/Plugin.hpp"
#include "../include/Program.h"

static int accumulate_string(char *data, int size, void *userdata) {
    std::string *str = (std::string*)userdata;
    str->append(data, size);
    return 0;
}

namespace QuickMedia {
    DownloadResult Plugin::download_to_string(const std::string &url, std::string &result) {
        const char *args[] = { "curl", "-H", "Accept-Language: en-US,en;q=0.5", "--compressed", "-s", "-L", url.c_str(), nullptr };
        if(exec_program(args, accumulate_string, &result) != 0)
            return DownloadResult::NET_ERR;
        return DownloadResult::OK;
    }
}