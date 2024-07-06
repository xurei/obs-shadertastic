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

#ifndef FILE_UTIL_HPP
#define FILE_UTIL_HPP

#include <iostream>
#include <vector>
#include <filesystem>
#include <util/platform.h>
#include <zip.h>
#include "string_util.h"
namespace fs = std::filesystem;

std::string normalize_path(const std::string &input);

char *load_file_zipped_or_local(std::string path);

std::vector<std::string> list_files(const char* folderPath, std::string &extension);

std::vector<std::string> list_directories(const char* folderPath);

#endif //FILE_UTIL_HPP
