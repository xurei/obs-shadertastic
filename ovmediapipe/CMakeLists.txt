# Copyright (C) 2022-2023 Intel Corporation
# SPDX-License-Identifier: Apache-2.0
#

file(GLOB SOURCES ./src/*.cpp)
file(GLOB HEADERS ./include/ovmediapipe/*.h  )

# Create named folders for the sources within the .vcproj
# Empty name lists them directly under the .vcproj
source_group("src" FILES ${SOURCES} src/filtering/low_pass_filter.cpp src/filtering/one_euro_filter.cpp)
source_group("include" FILES ${HEADERS} src/filtering/low_pass_filter.h src/filtering/one_euro_filter.h)

add_library(ovmediapipe STATIC ${SOURCES} src/filtering/low_pass_filter.cpp src/filtering/one_euro_filter.cpp ${HEADERS} src/filtering/low_pass_filter.h src/filtering/one_euro_filter.h )
target_include_directories(ovmediapipe PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_include_directories(ovmediapipe PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/filtering")
target_link_libraries(ovmediapipe PRIVATE openvino::runtime ${OpenCV_LIBRARIES} ${ITTNOTIFY_LIB} )
