/******************************************************************************
    Copyright (C) 2013-2014 by Hugh Bailey <obs.jim@gmail.com>

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
