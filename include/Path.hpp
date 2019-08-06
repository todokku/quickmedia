#pragma once

#include <string>

namespace QuickMedia {
    class Path {
    public:
        Path() = default;
        ~Path() = default;
        Path(const Path &other) = default;
        Path& operator=(const Path &other) = default;
        Path(const char *path) : data(path) {}
        Path(const std::string &path) : data(path) {}
        Path(Path &&other) {
            data = std::move(other.data);
        }

        Path& join(const Path &other) {
            data += "/";
            data += other.data;
            return *this;
        }

        std::string data;
    };
}