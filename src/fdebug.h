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

#ifndef FDEBUG_H
#define FDEBUG_H

#include <cstdio>
#include <cstdarg>
#include <string>

#if defined(DEV_MODE) && defined(DEV_LOG_PATH)
    // Opens the debug file for writing
    FILE* fdebug_open(const char* filename);

    // Closes the debug file
    void fdebug_close(FILE* debugFile);

    // Appends a line to the debug file
    void fdebug(FILE* debugFile, const char* format, ...);
#else
    #define fdebug_open(filename) nullptr
    #define fdebug_close(debugFile)
    #define fdebug(debugFile, format, ...)
#endif

#endif // FDEBUG_H
