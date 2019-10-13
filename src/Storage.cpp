#include "../include/Storage.hpp"
#include "../include/env.hpp"
#include <stdio.h>

#if OS_FAMILY == OS_FAMILY_POSIX
#include <pwd.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#endif

static int makedir(const char *path) {
    return mkdir(path, S_IRWXU);
}

namespace QuickMedia {
    Path get_home_dir()
    {
    #if OS_FAMILY == OS_FAMILY_POSIX
        const char *homeDir = getenv("HOME");
        if(!homeDir)
        {
            passwd *pw = getpwuid(getuid());
            homeDir = pw->pw_dir;
        }
        return homeDir;
    #elif OS_FAMILY == OS_FAMILY_WINDOWS
        BOOL ret;
        HANDLE hToken;
        std::wstring homeDir;
        DWORD homeDirLen = MAX_PATH;
        homeDir.resize(homeDirLen);

        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_READ, &hToken))
            throw std::runtime_error("Failed to open process token");

        if (!GetUserProfileDirectory(hToken, &homeDir[0], &homeDirLen))
        {
            CloseHandle(hToken);
            throw std::runtime_error("Failed to get home directory");
        }

        CloseHandle(hToken);
        homeDir.resize(wcslen(homeDir.c_str()));
        return boost::filesystem::path(homeDir);
    #endif
    }

    Path get_storage_dir() {
        return get_home_dir().join(".config").join("quickmedia");
    }

    Path get_cache_dir() {
        return get_home_dir().join(".cache").join("quickmedia");
    }

    int create_directory_recursive(const Path &path) {
        size_t index = 0;
        while(true) {
            index = path.data.find('/', index);
            
            // Skips first '/', we don't want to try and create the root directory
            if(index == 0) {
                ++index;
                continue;
            }

            std::string path_component = path.data.substr(0, index);
            int err = makedir(path_component.c_str());
            
            if(err == -1 && errno != EEXIST)
                return err;

            if(index == std::string::npos)
                break;
            else
                ++index;
        }
        return 0;
    }

    FileType get_file_type(const Path &path) {
        struct stat file_stat;
        if(stat(path.data.c_str(), &file_stat) == 0)
            return S_ISREG(file_stat.st_mode) ? FileType::REGULAR : FileType::DIRECTORY;
        return FileType::FILE_NOT_FOUND;
    }

    int file_get_content(const Path &path, std::string &result) {
        FILE *file = fopen(path.data.c_str(), "rb");
        if(!file)
            return -errno;
        
        fseek(file, 0, SEEK_END);
        size_t file_size = ftell(file);
        fseek(file, 0, SEEK_SET);

        result.resize(file_size);
        fread(&result[0], 1, file_size, file);

        fclose(file);
        return 0;
    }

    int file_overwrite(const Path &path, const std::string &data) {
        FILE *file = fopen(path.data.c_str(), "wb");
        if(!file)
            return errno;
        
        if(fwrite(data.data(), 1, data.size(), file) != data.size()) {
            fclose(file);
            return -1;
        }

        return fclose(file);
    }

    int create_lock_file(const Path &path) {
        int fd = open(path.data.c_str(), O_CREAT | O_EXCL);
        if(fd == -1)
            return errno;
        return close(fd);
    }
}