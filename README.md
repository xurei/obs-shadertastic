# Shadertastic
## User-friendly shader transition and filters for OBS

Create amazing transitions and filters using shaders, with user-friendly configurations.

# Build
1. In-tree build
    - Build OBS Studio: https://obsproject.com/wiki/Install-Instructions
    - Check out this repository to plugins/shadertastic
    - Add `add_subdirectory(shadertastic)` to plugins/CMakeLists.txt
    - Rebuild OBS Studio

1. Stand-alone build (Linux only)
    - Verify that you have package with development files for OBS
    - Check out this repository and run `cmake -S . -B build -DBUILD_OUT_OF_TREE=On && cmake --build build`

# Donations
https://ko-fi.com/xurei  
https://github.com/sponsors/xurei
