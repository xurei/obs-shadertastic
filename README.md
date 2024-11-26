# Shadertastic
## User-friendly shader transition and filters for OBS

Create amazing transitions and filters using shaders, with user-friendly configurations.

# Build
1. In-tree build
    - Build OBS Studio: https://obsproject.com/wiki/Install-Instructions
    - Check out this repository to plugins/shadertastic (⚠ not .../obs-shadertastic as git clone may do)
    - Add `add_subdirectory(shadertastic)` to plugins/CMakeLists.txt *twice*, after each `add_subdirectory(rtmp-services)`
    - Rebuild OBS Studio

1. Stand-alone build (Linux only)
    - Verify that you have package with development files for OBS
    - Check out this repository and run `cmake -S . -B build -DBUILD_OUT_OF_TREE=On && cmake --build build`

# Donations
https://ko-fi.com/xurei  
https://github.com/sponsors/xurei

# Special mentions
- Face detection powered by ONNX™, inspired from https://github.com/intel/openvino-plugins-for-obs-studio
- ChatGPT, without which nothing would have been possible (especially the CMake part).
