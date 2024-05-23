#include <cstdio>
#include <cstdarg>

#ifndef FDEBUG_H
#define FDEBUG_H

#ifdef DEV_MODE
    // Opens the debug file for writing
    inline FILE* fdebug_open(const char* filename) {
        // File pointer for debug file
        static FILE* debugFile = nullptr;

        debugFile = fopen(filename, "w");
        if (!debugFile) {
            perror("Error opening debug file");
        }

        return debugFile;
    }

    // Closes the debug file
    inline void fdebug_close(FILE* debugFile) {
        if (debugFile) {
            fclose(debugFile);
            debugFile = nullptr;
        }
    }

    // Appends a line to the debug file
    inline void fdebug(FILE* debugFile, const char* format, ...) {
        if (debugFile) {
            va_list args;
            va_start(args, format);
            vfprintf(debugFile, format, args);
            va_end(args);
            fprintf(debugFile, "\n");
            fflush(debugFile);
        }
    }
#else
    #define fdebug_open(format) nullptr
    #define fdebug_close(format)
    #define fdebug(format, ...)
#endif

#endif // FDEBUG_H
