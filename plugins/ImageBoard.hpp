#pragma once

#include "Plugin.hpp"

namespace QuickMedia {
    class ImageBoard : public Plugin {
    public:
        ImageBoard(const std::string &name) : Plugin(name) {}
        virtual ~ImageBoard() = default;

        bool is_image_board() override { return true; }

        virtual PluginResult get_threads(const std::string &url, BodyItems &result_items) = 0;
        virtual PluginResult get_thread_comments(const std::string &list_url, const std::string &url, BodyItems &result_items) = 0;
    };
}