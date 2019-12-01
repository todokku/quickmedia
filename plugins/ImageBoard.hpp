#pragma once

#include "Plugin.hpp"

namespace QuickMedia {
    enum class PostResult {
        OK,
        TRY_AGAIN,
        BANNED,
        ERR
    };

    class ImageBoard : public Plugin {
    public:
        ImageBoard(const std::string &name) : Plugin(name) {}
        virtual ~ImageBoard() = default;

        bool is_image_board() override { return true; }

        virtual PluginResult get_threads(const std::string &url, BodyItems &result_items) = 0;
        virtual PluginResult get_thread_comments(const std::string &list_url, const std::string &url, BodyItems &result_items) = 0;
        virtual PostResult post_comment(const std::string &board, const std::string &thread, const std::string &captcha_id, const std::string &comment) = 0;
    };
}