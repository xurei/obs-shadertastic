std::vector<std::string> list_files(const char* folderPath) {
    std::vector<std::string> files;

    #if defined(_WIN32)
        std::string searchPath = folderPath;
        searchPath += "\\*";
        WIN32_FIND_DATAA fileInfo;
        HANDLE searchHandle = FindFirstFileA(searchPath.c_str(), &fileInfo);
        if (searchHandle != INVALID_HANDLE_VALUE) {
            do {
                files.push_back(fileInfo.cFileName);
            } while (FindNextFileA(searchHandle, &fileInfo));
            FindClose(searchHandle);
        }
    #else
        DIR* dir = opendir(folderPath);
        if (dir != NULL) {
            dirent* entry;
            while ((entry = readdir(dir)) != NULL) {
                if (entry->d_type == DT_REG || entry->d_type == DT_DIR) {
                    files.push_back(entry->d_name);
                }
            }
            closedir(dir);
        }
    #endif

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
        if (dir != NULL) {
            dirent* entry;
            while ((entry = readdir(dir)) != NULL) {
                if (entry->d_type == DT_DIR && strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
                    files.push_back(entry->d_name);
                }
            }
            closedir(dir);
        }
    #endif

    return files;
}
