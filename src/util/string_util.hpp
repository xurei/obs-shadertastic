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

bool ends_with(std::string &input, const std::string suffix) {
    if (suffix.length() > input.length()) {
        return false;
    }
    else {
        return (input.substr(input.length() - suffix.length()) == suffix);
    }
}
