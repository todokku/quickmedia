#include "../include/DownloadUtils.hpp"
#include "../include/Program.h"
#include <SFML/System/Clock.hpp>

static int accumulate_string(char *data, int size, void *userdata) {
    std::string *str = (std::string*)userdata;
    str->append(data, size);
    return 0;
}

namespace QuickMedia {
    // TODO: Add timeout
    DownloadResult download_to_string(const std::string &url, std::string &result, const std::vector<CommandArg> &additional_args, bool use_tor) {
        sf::Clock timer;
        std::vector<const char*> args;
        if(use_tor)
            args.push_back("torsocks");
        args.insert(args.end(), { "curl", "-f", "-H", "Accept-Language: en-US,en;q=0.5", "--compressed", "-s", "-L" });
        for(const CommandArg &arg : additional_args) {
            args.push_back(arg.option.c_str());
            args.push_back(arg.value.c_str());
        }
        args.push_back("--");
        args.push_back(url.c_str());
        args.push_back(nullptr);
        if(exec_program(args.data(), accumulate_string, &result) != 0)
            return DownloadResult::NET_ERR;
        fprintf(stderr, "Download duration for %s: %d ms\n", url.c_str(), timer.getElapsedTime().asMilliseconds());
        return DownloadResult::OK;
    }

    std::vector<CommandArg> create_command_args_from_form_data(const std::vector<FormData> &form_data) {
        // TODO: This boundary value might need to change, depending on the content. What if the form data contains the boundary value?
        const std::string boundary = "-----------------------------119561554312148213571335532670";
        std::string form_data_str;
        for(const FormData &form_data_item : form_data) {
            form_data_str += boundary;
            form_data_str += "\r\n";
            // TODO: What if the form key contains " or \r\n ?
            form_data_str += "Content-Disposition: form-data; name=\"" + form_data_item.key + "\"";
            form_data_str += "\r\n\r\n";
            // TODO: What is the value contains \r\n ?
            form_data_str += form_data_item.value;
            form_data_str += "\r\n";
        }
        // TODO: Verify if this should only be done also if the form data is empty
        form_data_str += boundary + "--";
        return {
            CommandArg{"-H", "Content-Type: multipart/form-data; boundary=" + boundary},
            CommandArg{"--data-binary", std::move(form_data_str)}
        };
    }
}