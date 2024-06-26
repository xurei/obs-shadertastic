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

#include <cstdarg>
#include <string>
#include <obs-module.h>

#include "fdebug.h"
#include "logging_functions.hpp"

#if defined(DEV_MODE) && defined(DEV_LOG_PATH)
    // Opens the debug file for writing
    FILE* fdebug_open(const char* filename) {
        // File pointer for debug file
        static FILE* debugFile = nullptr;

        debugFile = fopen((std::string(DEV_LOG_PATH) + "/" + std::string(filename)).c_str(), "w");
        if (!debugFile) {
            perror("Error opening debug file");
        }

        return debugFile;
    }

    // Closes the debug file
    void fdebug_close(FILE* debugFile) {
        if (debugFile) {
            fclose(debugFile);
            debugFile = nullptr;
        }
    }

    // Appends a line to the debug file
    void fdebug(FILE* debugFile, const char* format, ...) {
        if (debugFile) {
            va_list args;
            va_start(args, format);
            vfprintf(debugFile, format, args);
            va_end(args);
            fprintf(debugFile, "\n");
            fflush(debugFile);
        }
    }
#endif
