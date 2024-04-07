#include "ovmediapipe/face_mesh.h"
#include "ovmediapipe/landmark_refinement_indices.h"
#include <media-io/video-scaler.h>

struct face_detection_state {
    gs_texrender_t *facedetection_texrender;
    gs_stagesurf_t *staging_texture = NULL;
    std::shared_ptr <ovmediapipe::FaceMesh> facemesh;
    std::vector<std::string> ov_available_devices;
    ovmediapipe::FaceLandmarksResults facelandmark_results;

//
//    // Use the media-io converter to both scale and convert the colorspace
//    video_scaler_t* scalerToBGR = nullptr;
//    video_scaler_t* scalerFromBGR = nullptr;
//
//    bool bDebug_mode = false;
//
//    std::string detectionModelSelection;
//    std::string deviceDetectionInference;
//
//
//    //face landmark
//    std::string landmarkModelSelection;
//    std::string deviceFaceMeshInference;
};
