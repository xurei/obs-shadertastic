// Comparison, not case sensitive.
bool compare_nocase(const std::string& first, const std::string& second) {
    unsigned int i=0;
    while ( (i<first.length()) && (i<second.length()) )
    {
        if (tolower(first[i]) < tolower(second[i])) {
            return true;
        }
        else if (tolower(first[i]) > tolower(second[i])) {
            return false;
        }
        ++i;
    }
    return ( first.length() < second.length() );
}
