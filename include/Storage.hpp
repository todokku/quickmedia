#pragma once

#include "Path.hpp"

namespace QuickMedia {
    enum class FileType {
        FILE_NOT_FOUND,
        REGULAR,
        DIRECTORY
    };

    Path get_home_dir();
    Path get_storage_dir();
    Path get_cache_dir();
    int create_directory_recursive(const Path &path);
    FileType get_file_type(const Path &path);
    int file_get_content(const Path &path, std::string &result);
    int file_overwrite(const Path &path, const std::string &data);
    int create_lock_file(const Path &path);
}