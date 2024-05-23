include(FetchContent)

set(CUSTOM_OPENCV_URL
    ""
    CACHE STRING "URL of a downloaded OpenCV static library tarball")

set(CUSTOM_OPENCV_HASH
    ""
    CACHE STRING "Hash of a downloaded OpenCV staitc library tarball")

if(CUSTOM_OPENCV_URL STREQUAL "")
  set(USE_PREDEFINED_OPENCV ON)
else()
  if(CUSTOM_OPENCV_HASH STREQUAL "")
    message(FATAL_ERROR "Both of CUSTOM_OPENCV_URL and CUSTOM_OPENCV_HASH must be present!")
  else()
    set(USE_PREDEFINED_OPENCV OFF)
  endif()
endif()

if(USE_PREDEFINED_OPENCV)
  set(OpenCV_VERSION "v4.9.0-1")
  set(OpenCV_VERSIONCODE "490")
  set(OpenCV_BASEURL "https://github.com/obs-ai/obs-backgroundremoval-dep-opencv/releases/download/${OpenCV_VERSION}")

  if(${CMAKE_BUILD_TYPE} STREQUAL Release OR ${CMAKE_BUILD_TYPE} STREQUAL RelWithDebInfo)
    set(OpenCV_BUILD_TYPE Release)
  else()
    set(OpenCV_BUILD_TYPE Debug)
  endif()

  if(APPLE)
    if(OpenCV_BUILD_TYPE STREQUAL Debug)
      set(OpenCV_URL "${OpenCV_BASEURL}/opencv-macos-${OpenCV_VERSION}-Debug.tar.gz")
      set(OpenCV_HASH SHA256=2930e335a19cc03a3d825e2b76eadd0d5cf08d8baf6537747d43f503dff32454)
    else()
      set(OpenCV_URL "${OpenCV_BASEURL}/opencv-macos-${OpenCV_VERSION}-Release.tar.gz")
      set(OpenCV_HASH SHA256=b0c4fe2370b0bd5aa65c408e875b1ab18508ba31b93083805d7e398a3ecafdac)
    endif()
  elseif(MSVC)
    if(OpenCV_BUILD_TYPE STREQUAL Debug)
      set(OpenCV_URL "${OpenCV_BASEURL}/opencv-windows-${OpenCV_VERSION}-Debug.zip")
      set(OpenCV_HASH SHA256=0a1bbc898dce5f193427586da84d7a34bbb783127957633236344e9ccd61b9ce)
    else()
      set(OpenCV_URL "${OpenCV_BASEURL}/opencv-windows-${OpenCV_VERSION}-Release.zip")
      set(OpenCV_HASH SHA256=56a5e042f490b8390b1c1819b2b48c858f10cd64e613babbf11925a57269c3fa)
    endif()
  else()
    if(OpenCV_BUILD_TYPE STREQUAL Debug)
      set(OpenCV_URL "${OpenCV_BASEURL}/opencv-linux-${OpenCV_VERSION}-Debug.tar.gz")
      set(OpenCV_HASH SHA256=840a7d80b661cff7b7300272a2a2992d539672ececa01836b85f68bd8caf07f5)
    else()
      set(OpenCV_URL "${OpenCV_BASEURL}/opencv-linux-${OpenCV_VERSION}-Release.tar.gz")
      set(OpenCV_HASH SHA256=73652c2155b477b5fd95fcd8ea7ce35d313543ece17bdfa3a2b217a0239d74c6)
    endif()
  endif()
else()
  set(OpenCV_URL "${CUSTOM_OPENCV_URL}")
  set(OpenCV_HASH "${CUSTOM_OPENCV_HASH}")
endif()

set(FETCHCONTENT_QUIET FALSE)
FetchContent_Declare(
  opencv
  URL ${OpenCV_URL}
  URL_HASH ${OpenCV_HASH})
FetchContent_MakeAvailable(opencv)

add_library(OpenCV INTERFACE)
if(MSVC)
  target_link_libraries(
    OpenCV
    INTERFACE ${opencv_SOURCE_DIR}/x64/vc17/staticlib/opencv_imgproc${OpenCV_VERSIONCODE}.lib
              ${opencv_SOURCE_DIR}/x64/vc17/staticlib/opencv_core${OpenCV_VERSIONCODE}.lib
              ${opencv_SOURCE_DIR}/x64/vc17/staticlib/zlib.lib)
  target_include_directories(OpenCV SYSTEM INTERFACE ${opencv_SOURCE_DIR}/include)
else()
  target_link_libraries(
    OpenCV INTERFACE
      ${opencv_SOURCE_DIR}/lib/libopencv_imgproc.a
      ${opencv_SOURCE_DIR}/lib/libopencv_core.a
      ${opencv_SOURCE_DIR}/lib/libopencv_imgcodecs.a
      ${opencv_SOURCE_DIR}/lib/opencv4/3rdparty/libzlib.a)
  target_include_directories(OpenCV SYSTEM INTERFACE ${opencv_SOURCE_DIR}/include/opencv4)
endif()
