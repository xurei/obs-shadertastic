std::string normalize_path(const std::string input) {
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
