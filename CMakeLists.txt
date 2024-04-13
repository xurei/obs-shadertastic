# --- Detect if the plugin is build out of tree or not ---
if(CMAKE_PROJECT_NAME STREQUAL "obs-studio")
  set(BUILD_OUT_OF_TREE OFF)
else()
  set(BUILD_OUT_OF_TREE ON)
  cmake_minimum_required(VERSION 3.18)
endif()

if(DEV_MODE EQUAL 1)
  add_compile_definitions(DEV_MODE)
  project(shadertastic VERSION 0.0.8)
else()
  project(shadertastic VERSION 0.0.8)
endif()
set(PROJECT_FULL_NAME "Shadertastic")

set(CMAKE_COMPILE_WARNING_AS_ERROR ON)

# Set new UUIDs when you start to create a new plugin.
set(MACOS_PACKAGE_UUID "90e83546-fa37-453a-9adb-b51149c78df1")
set(MACOS_INSTALLER_UUID "14c0a640-0656-4528-8243-4a2ab2595dd5")

# Base command to generate the so file
add_library(${PROJECT_NAME} MODULE)

# libzip
set(LIBZIP_LIBRARY "" CACHE FILEPATH "Path to libzip library.")
set(LIBZIP_INCLUDE "" CACHE PATH "Path to libzip's includes.")
set(LIBZIP_INCLUDE_CONF "" CACHE PATH "Path to libzip's includes (config file).")

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/src/version.h.in ${CMAKE_CURRENT_SOURCE_DIR}/src/version.h)

file(GLOB sources_HPP
    "src/shader/*.hpp"
    "src/util/*.hpp"
    "src/parameters/*.hpp"
    "src/*.hpp"
)
file(GLOB ovmediapipe_CPP
    "ovmediapipe/src/*"
    "ovmediapipe/src/filtering/*"
)

target_sources(${PROJECT_NAME} PRIVATE
    src/shadertastic.cpp
    src/version.h
    ${sources_HPP}
    ${ovmediapipe_CPP}
)

if(BUILD_OUT_OF_TREE)
    find_package(libobs REQUIRED)
    find_package(obs-frontend-api REQUIRED)
    include(cmake/ObsPluginHelpers.cmake)
    set(OBS_FRONTEND_API_NAME "obs-frontend-api")
else()
    set(OBS_FRONTEND_API_NAME "frontend-api")
endif()

# OpenVINO & OpenCV dependencies
find_package(OpenCV REQUIRED COMPONENTS core highgui imgproc)
find_package(OpenVINO REQUIRED COMPONENTS Runtime)
target_include_directories(${PROJECT_NAME} PRIVATE ovmediapipe/include)

if(OS_WINDOWS)
    message("Windows build")
    find_package(OpenCV REQUIRED PATHS C:/Users/theuser/Path/To/MyOpenCVInstallation/build/install/x64/vc16/lib)
elseif(OS_MACOS)
    # TODO
elseif(OS_LINUX)
    message("Linux build")
    message("${OPENVINO_LIBRARY}")
    file(COPY ${OPENVINO_LIBRARY} DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/lib" FOLLOW_SYMLINK_CHAIN)
    file(COPY "/usr/lib/libopenvino_ir_frontend.so.2330" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/lib" FOLLOW_SYMLINK_CHAIN)
    file(COPY "/usr/lib/openvino-2023.3.0/" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/lib" FOLLOW_SYMLINK_CHAIN)
    file(COPY "/usr/lib/x86_64-linux-gnu/libopencv_imgproc.so.4.5d" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/lib" FOLLOW_SYMLINK_CHAIN)
    file(COPY "/usr/lib/x86_64-linux-gnu/libopencv_core.so.4.5d" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/lib" FOLLOW_SYMLINK_CHAIN)
endif()

if(OS_WINDOWS)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /arch:avx2 /O2 /openmp")
    get_filename_component(ISS_FILES_DIR "${CMAKE_BINARY_DIR}\\..\\package" ABSOLUTE)
    file(TO_NATIVE_PATH "${ISS_FILES_DIR}" ISS_FILES_DIR)
    get_filename_component(ISS_PACKAGE_DIR "${CMAKE_PACKAGE_PREFIX}\\.." ABSOLUTE)
    file(TO_NATIVE_PATH "${ISS_PACKAGE_DIR}" ISS_PACKAGE_DIR)
    get_filename_component(ISS_SOURCE_DIR "${PROJECT_SOURCE_DIR}" ABSOLUTE)
    file(TO_NATIVE_PATH "${ISS_SOURCE_DIR}" ISS_SOURCE_DIR)
    configure_file("installer.iss.in"
        "${PROJECT_BINARY_DIR}/installer.iss"
    )
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/resource.rc.in ${CMAKE_CURRENT_SOURCE_DIR}/${PROJECT_NAME}.rc)

    include_directories(AFTER ${LIBZIP_INCLUDE})
    include_directories(AFTER ${LIBZIP_INCLUDE_CONF})
    target_link_libraries(${PROJECT_NAME}
         PRIVATE ${LIBZIP_LIBRARY}
    )
    set(ZLIB_USE_STATIC_LIBS ON)
    find_package(ZLIB REQUIRED)
    target_link_libraries(${PROJECT_NAME} PRIVATE ${ZLIB_LIBRARIES})
    include_directories(${ZLIB_INCLUDE_DIRS})

    target_sources(${PROJECT_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/${PROJECT_NAME}.rc)
elseif(OS_MACOS)
    set(MACOS_BUNDLEID "com.xureilab.${PROJECT_NAME}")
    set(MACOSX_PLUGIN_GUI_IDENTIFIER "${MACOS_BUNDLEID}")
    set(MACOSX_PLUGIN_BUNDLE_VERSION "${PROJECT_VERSION}")
    set(MACOSX_PLUGIN_SHORT_VERSION_STRING "1")
    configure_file(cmake/bundle/macos/installer-macos.pkgproj.in ${CMAKE_BINARY_DIR}/installer-macos.generated.pkgproj)
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall)
elseif(OS_LINUX)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mavx2 -O3 -fopenmp")
    include_directories(AFTER ${LIBZIP_INCLUDE})
    include_directories(AFTER ${LIBZIP_INCLUDE_CONF})
    target_link_libraries(${PROJECT_NAME}
        PRIVATE ${LIBZIP_LIBRARY}
    )
    find_package(ZLIB REQUIRED)
    target_link_libraries(${PROJECT_NAME} PRIVATE ${ZLIB_LIBRARIES})
    include_directories(${ZLIB_INCLUDE_DIRS})

    # Link the OpenVINO runtime library
    target_link_libraries(${PROJECT_NAME}
        PRIVATE ${OpenCV_LIBRARIES}
        PRIVATE openvino::runtime
    )

    # Allow external libs to be loaded from the build directory with RPATH
    # Set the rpath to the directory containing the executable or shared object file
    #set(CMAKE_SKIP_BUILD_RPATH  FALSE)
    #set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
    #set(CMAKE_INSTALL_RPATH "$ORIGIN/lib")
    #set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
    set_target_properties(${PROJECT_NAME} PROPERTIES INSTALL_RPATH "$ORIGIN/lib")
    set_target_properties(${PROJECT_NAME} PROPERTIES SKIP_RPATH FALSE)
    set_target_properties(${PROJECT_NAME} PROPERTIES BUILD_WITH_INSTALL_RPATH TRUE)
endif()

# lines if you want add Qt UI in your plugin
find_qt(COMPONENTS Core Widgets Gui)
set_target_properties(${PROJECT_NAME} PROPERTIES AUTOMOC ON AUTOUIC ON AUTORCC ON)

target_link_libraries(${PROJECT_NAME}
    PRIVATE OBS::${OBS_FRONTEND_API_NAME}
    PRIVATE Qt::Widgets
    PRIVATE OBS::libobs
)

### THESE LINES MIGHT BREAK THE WIN32 BUILD. TO BE TESTED AND CONFIRMED OK
if(WIN32)
    # Enable Multicore Builds and disable FH4 (to not depend on VCRUNTIME140_1.DLL when building with VS2019)
    if (MSVC)
        add_definitions(/MP /d2FH4- /std:c++latest)
    endif()
endif()

if(BUILD_OUT_OF_TREE)
    if(NOT LIB_OUT_DIR)
        set(LIB_OUT_DIR "/lib/obs-plugins")
    endif()
    if(NOT DATA_OUT_DIR)
        set(DATA_OUT_DIR "/share/obs/obs-plugins/${PROJECT_NAME}")
    endif()
    set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "")
    install(TARGETS ${PROJECT_NAME}
        LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}/${LIB_OUT_DIR})
    install(DIRECTORY data/locale
        DESTINATION ${CMAKE_INSTALL_PREFIX}/${DATA_OUT_DIR})
else()
    target_include_directories(${PROJECT_NAME} PRIVATE "${CMAKE_SOURCE_DIR}/UI/obs-frontend-api")
    set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER "plugins/xurei")
endif()
setup_plugin_target(${PROJECT_NAME})
