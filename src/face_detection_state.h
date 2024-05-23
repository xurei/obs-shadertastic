#include <onnxruntime_cxx_api.h>
#include "onnxmediapipe/face_mesh.h"

struct face_detection_state {
    gs_texrender_t *facedetection_texrender;
    gs_texture_t *fd_points_texture;
    gs_stagesurf_t *staging_texture = nullptr;
    std::shared_ptr <onnxmediapipe::FaceMesh> facemesh;
    std::vector<std::string> ov_available_devices;
    onnxmediapipe::FaceLandmarksResults facelandmark_results[10];
    size_t facelandmark_results_counter = 0;
};
