#include <stdio.h>
#include <string.h>
#include <stdbool.h>

/**
 * Returns the user id from the stream key, or 0 if the stream key is malformed.
 */
int extract_userid_from_twitchkey(const char *stream_key) {
    char *start = (char*)strchr(stream_key, '_') + 1; // Find the position of the first underscore and add 1 to skip it
    char *end = strchr(start, '_'); // Find the position of the second underscore

    // Check that both underscores are present and the number between them is valid
    bool is_valid = (start != NULL && end != NULL);
    if (is_valid) {
        // Create a buffer to hold the extracted number
        char number_buffer[32];
        memset(number_buffer, 0, sizeof(number_buffer));

        // Copy the characters between the underscores to the buffer
        strncpy(number_buffer, start, end - start);

        // Convert the buffer to an integer
        int number = atoi(number_buffer);

        return number;
    }

    return 0;
}
