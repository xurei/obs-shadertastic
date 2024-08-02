# --- Detect if the plugin is build out of tree or not ---
if(CMAKE_PROJECT_NAME STREQUAL "obs-studio")
  set(BUILD_OUT_OF_TREE OFF)
else()
  set(BUILD_OUT_OF_TREE ON)
  cmake_minimum_required(VERSION 3.18)
endif()

#Commit hash of the project, used by the CI. Set to 'localbuild' otherwise
if (NOT DEFINED SHADERTASTIC_COMMIT)
    set(SHADERTASTIC_COMMIT "localbuild")
endif()

project(shadertastic VERSION 0.0.9)
set(PROJECT_FULL_NAME "Shadertastic")

#cmake_policy(SET CMP0135 NEW)

set(CMAKE_COMPILE_WARNING_AS_ERROR ON)

# Set new UUIDs when you start to create a new plugin.
set(MACOS_PACKAGE_UUID "90e83546-fa37-453a-9adb-b51149c78df1")
set(MACOS_INSTALLER_UUID "14c0a640-0656-4528-8243-4a2ab2595dd5")

# Base command to generate the so file
add_library(${PROJECT_NAME} MODULE)

if(DEV_MODE EQUAL 1)
  add_compile_definitions(DEV_MODE)
  if(DEV_LOG_PATH)
      set(DEV_LOG_PATH "" CACHE STRING "Value of DEV_LOG_PATH")
      target_compile_definitions(${PROJECT_NAME} PRIVATE DEV_LOG_PATH="${DEV_LOG_PATH}")
  endif()
endif()

# libzip
set(LIBZIP_LIBRARY "" CACHE FILEPATH "Path to libzip library.")
set(LIBZIP_INCLUDE "" CACHE PATH "Path to libzip's includes.")
set(LIBZIP_INCLUDE_CONF "" CACHE PATH "Path to libzip's includes (config file).")

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/src/version.h.in ${CMAKE_CURRENT_SOURCE_DIR}/src/version.h)

file(GLOB sources_CPP
    "src/*.cpp"
    "src/shader/*.cpp"
    "src/util/*.cpp"
)
file(GLOB sources_HPP
    "src/shader/*.hpp"
    "src/util/*.hpp"
    "src/parameters/*.hpp"
    "src/*.hpp"
)
# TODO Remove this
file(GLOB onnxmediapipe_CPP
    "onnxmediapipe/src/*"
    "onnxmediapipe/src/filtering/*"
)

target_sources(${PROJECT_NAME} PRIVATE
    src/version.h
    ${sources_CPP}
    ${onnxmediapipe_CPP}
)

if(BUILD_OUT_OF_TREE)
    find_package(libobs REQUIRED)
    find_package(obs-frontend-api REQUIRED)
    include(cmake/ObsPluginHelpers.cmake)
    set(OBS_FRONTEND_API_NAME "obs-frontend-api")
else()
    set(OBS_FRONTEND_API_NAME "frontend-api")
endif()

# ONNXRuntime & OpenCV dependencies
# Disable GPU (for now)
target_compile_definitions(${PROJECT_NAME} PRIVATE DISABLE_ONNXRUNTIME_GPU)
message("Fetching ONNX Runtime...")
include(cmake/FetchOnnxruntime.cmake)
message("Fetching OpenCV...")
include(cmake/FetchOpenCV.cmake)
target_link_libraries(${PROJECT_NAME} PRIVATE OpenCV)

#if(OS_WINDOWS)
#	set (OpenCV_DIR "C:\\tools\\opencv\\build")
#endif()
target_include_directories(${PROJECT_NAME} PRIVATE onnxmediapipe/include)
#
if(OS_WINDOWS)
#    message("Windows build")
elseif(OS_MACOS)
    # TODO
elseif(OS_LINUX)
#    message("Linux build")
    file(COPY "${CMAKE_BINARY_DIR}/_deps/onnxruntime-src/lib/libonnxruntime.so.${Onnxruntime_VERSION}" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/lib" FOLLOW_SYMLINK_CHAIN)
endif()

if(OS_WINDOWS)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /arch:avx2 /O2 /openmp")
    # Disable treating warnings as errors for your project
    string(REPLACE "/WX " "/WX- " CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
    string(REPLACE "/WX " "/WX- " CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")

    # Ensure that the changes apply to all configurations
    foreach(config ${CMAKE_CONFIGURATION_TYPES})
        string(REPLACE "/WX " "/WX- " CMAKE_C_FLAGS_${config} "${CMAKE_C_FLAGS_${config}}")
        string(REPLACE "/WX " "/WX- " CMAKE_CXX_FLAGS_${config} "${CMAKE_CXX_FLAGS_${config}}")
    endforeach()

    add_link_options(
        "LINKER:/WX-"
    )
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
