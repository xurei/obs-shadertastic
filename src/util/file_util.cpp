/******************************************************************************
    Copyright (C) 2023 by xurei <xureilab@gmail.com>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/

#if defined(_WIN32)
#include <windows.h>
#else
#include <dirent.h>
#endif

#include <filesystem>
#include <iostream>
#include <obs-module.h>
#include <util/platform.h>
#include <vector>
#include <zip.h>
#include "file_util.h"
#include "../logging_functions.hpp"
namespace fs = std::filesystem;

std::string normalize_path(const std::string &input) {
    std::string result(input);
    #if defined(_WIN32)
        size_t pos = 0;
        while ((pos = result.find('/', pos)) != std::string::npos) {
            result.replace(pos, 1, "\\");
            pos += 1; // Move past the replaced character
        }
    #endif
    return result;
}

char *load_file_zipped_or_local(std::string path) {
    path = normalize_path(path);
    debug("load_file_zipped_or_local %s", path.c_str());

    if (os_file_exists(path.c_str())) {
        return os_quick_read_utf8_file(path.c_str());
    }
    else {
        fs::path fs_path(path);
        std::string zip_path = fs_path.parent_path().string();
        debug("FS PATH: %s", zip_path.c_str());
        if (!ends_with(zip_path, ".shadertastic")) {
            return nullptr;
        }

        int zip_err_code;
        std::string zip_entry = fs_path.filename().string();
        debug("zip_entry: %s", zip_entry.c_str());
        struct zip_stat file_stat{};
        zip_t *zip_archive = zip_open(zip_path.c_str(), ZIP_RDONLY, &zip_err_code);

        if (zip_archive == nullptr) {
            zip_error_t error;
            zip_error_init_with_code(&error, zip_err_code);
            log_error("Cannot open shadertastic archive '%s': %s\n", zip_path.c_str(), zip_error_strerror(&error));
            zip_error_fini(&error);
            return nullptr;
        }

        if (zip_stat(zip_archive, zip_entry.c_str(), 0, &file_stat) != 0) {
            log_error("Cannot open shadertastic file in archive '%s': unable to stat entry file %s : %s\n", zip_path.c_str(), zip_entry.c_str(), zip_error_strerror(zip_get_error(zip_archive)));
            zip_close(zip_archive);
            return nullptr;
        }

        zip_file_t *zipped_file = zip_fopen(zip_archive, zip_entry.c_str(), 0);

        if (zipped_file == nullptr) {
            log_error("Cannot open shadertastic file in archive '%s': unable to open entry file %s\n", zip_path.c_str(), zip_entry.c_str());
            zip_close(zip_archive);
            return nullptr;
        }

        char *file_content = static_cast<char *>(bzalloc(file_stat.size + 1));
        file_content[file_stat.size] = 0; // Normally not needed, but there for safety

        zip_fread(zipped_file, file_content, file_stat.size);
        zip_fclose(zipped_file);
        zip_close(zip_archive);
        return file_content;
    }
}

std::vector<std::string> list_files(const char* folderPath, std::string &extension) {
    std::vector<std::string> files;

    try {
        // Check if the directory exists
        if (fs::exists(folderPath) && fs::is_directory(folderPath)) {
            for (const auto& entry: fs::directory_iterator(folderPath)) {
                std::string path = entry.path().string();
                //std::cout << path.substr(path.length() - extension.length()) << std::endl;

                if (ends_with(path, extension)) {
                    // entry is an fs::directory_entry object representing a file or directory in the directory
                    files.push_back(entry.path().string());
                    //std::cout << entry.path().string() << " FOUND" << std::endl;
                }
            }
        }
        else {
            std::cerr << "The specified path is not a valid directory." << std::endl;
        }
    }
    catch (const std::exception& e) {
        std::cerr << "An error occurred: " << e.what() << std::endl;
    }

    return files;
}

std::vector<std::string> list_directories(const char* folderPath) {
    std::vector<std::string> files;

    #if defined(_WIN32)
        std::string searchPath = folderPath;
        searchPath += "\\*";
        WIN32_FIND_DATAA fileInfo;
        HANDLE searchHandle = FindFirstFileA(searchPath.c_str(), &fileInfo);
        if (searchHandle != INVALID_HANDLE_VALUE) {
            do {
                if (fileInfo.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                    if (strcmp(fileInfo.cFileName, ".") != 0 && strcmp(fileInfo.cFileName, "..") != 0) {
                        files.push_back(fileInfo.cFileName);
                    }
                }
            } while (FindNextFileA(searchHandle, &fileInfo));
            FindClose(searchHandle);
        }
    #else
        DIR* dir = opendir(folderPath);
        if (dir != nullptr) {
            dirent* entry;
            while ((entry = readdir(dir)) != nullptr) {
                if (entry->d_type == DT_DIR && strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
                    files.push_back(entry->d_name);
                }
            }
            closedir(dir);
        }
    #endif

    return files;
}
