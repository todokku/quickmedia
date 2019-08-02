#pragma once

#include "Plugin.hpp"

namespace QuickMedia {
    class Youtube : public Plugin {
    public:
        SearchResult search(const std::string &text, std::vector<std::unique_ptr<BodyItem>> &result_items) override;
    };
}