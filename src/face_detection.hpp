//#include "util/obs_opencv_conversions.hpp"

//#include <ittutils.h>
#include <immintrin.h> // Include the AVX2 header
#define ITT_SCOPED_TASK(TASKNAME) {}

#define FACEDETECTION_WIDTH 1280
#define FACEDETECTION_HEIGHT 720

struct ModelDetails {
    std::string ui_display_name;
    std::string data_path; //path to xml
};

struct face_detection_bounding_box {
    union {
        vec2 point1;
        struct {
            float x1;
            float y1;
        };
    };
    union {
        vec2 point2;
        struct {
            float x2;
            float y2;
        };
    };
};

static const std::list<ModelDetails> supported_face_detection_models = {
    {
        "MediaPipe Face Detection (Full Range)",
        "../openvino-models/mediapipe_face_detection/full-range/FP16/face_detection-full_range-sparse-192x192.xml"
    },
    {
        "MediaPipe Face Detection (Short Range)",
        "../openvino-models/mediapipe_face_detection/short-range/FP16/face_detection_short_range-128x128.xml"
    },
};

static const std::list<ModelDetails> supported_face_landmark_models = {
    {
        "MediaPipe Face Landmark",
        "../openvino-models/mediapipe_face_landmark/without_attention/FP16/face_landmark_without_attention_192x192.xml"
    },
    {
        "MediaPipe Face Landmark (With Attention)",
        "../openvino-models/mediapipe_face_landmark/with_attention/FP16/face_landmark_with_attention_192x192.xml"
    }
};
//----------------------------------------------------------------------------------------------------------------------

vec3* convertVectorToArray(const std::vector<cv::Point3f>& points) {
    // Calculate the total size needed for the array
    size_t totalSize = points.size();  // 3 floats for each Point3f (x, y, z)

    // Allocate memory for the array
    vec3* resultArray = new vec3[totalSize];

    // Copy the data from the vector to the array
    for (size_t i = 0; i < points.size(); ++i) {
        resultArray[i].x = points[i].x;
        resultArray[i].y = points[i].y;
        resultArray[i].z = points[i].z;
    }

    return resultArray;
}
//----------------------------------------------------------------------------------------------------------------------

void face_detection_copy_points(ovmediapipe::FaceLandmarksResults *facelandmark_results, float *points) {
    for (int i=0; i<468; ++i) {
        points[i*2+0] = facelandmark_results->facial_surface[i].x;
        points[i*2+1] = facelandmark_results->facial_surface[i].y;
        //debug("%f %f", points[i*2+0], points[i*2+1]);
        //points[i*3+2] = facelandmark_results->facial_surface[i].z;
    }
}
//----------------------------------------------------------------------------------------------------------------------

face_detection_bounding_box face_detection_get_bounding_box(ovmediapipe::FaceLandmarksResults *facelandmark_results, const unsigned short int *indices, int nb_indices) {
    // Initialize min and max points
    cv::Point2f minPoint(std::numeric_limits<float>::max(), std::numeric_limits<float>::max());
    cv::Point2f maxPoint(std::numeric_limits<float>::min(), std::numeric_limits<float>::min());

    // Find the bounding box
    for (int i=0; i<nb_indices; ++i) {
        //debug("%i", i);
        auto k = facelandmark_results->refined_landmarks[indices[i]];
        minPoint.x = std::min(minPoint.x, k.x);
        minPoint.y = std::min(minPoint.y, k.y);

        maxPoint.x = std::max(maxPoint.x, k.x);
        maxPoint.y = std::max(maxPoint.y, k.y);
    }

    // Finding the bounding box
    face_detection_bounding_box out{
        minPoint.x, minPoint.y,
        maxPoint.x, maxPoint.y
    };
    return out;
}
//----------------------------------------------------------------------------------------------------------------------

static void face_detection_update(face_detection_state *s) {
    //obs_data_t *settings = load_settings();
//    const std::string current_detection_device = obs_data_get_string(settings, "DetectionInferenceDevice");
//    const std::string current_detection_model = obs_data_get_string(settings, "model_select");
//
//    const std::string current_facemesh_device = obs_data_get_string(settings, "FaceMeshInferenceDevice");
//    const std::string current_landmark_model = obs_data_get_string(settings, "landmark_model_select");

    //s->bDebug_mode = obs_data_get_bool(settings, "Debug-Mode");

    std::string model_data_path = "openvino-models/mediapipe_face_detection/full-range/FP16/face_detection-full_range-sparse-192x192.xml";
    std::string landmark_data_path = "openvino-models/mediapipe_face_landmark/with_attention/FP16/face_landmark_with_attention_192x192.xml";

    char* detection_model_file_path = obs_module_file(model_data_path.c_str());
    char* landmark_model_file_path = obs_module_file(landmark_data_path.c_str());

    if (detection_model_file_path && landmark_model_file_path) {
        try {
            auto facemesh = std::make_shared < ovmediapipe::FaceMesh >(detection_model_file_path, "CPU", landmark_model_file_path, "CPU");
            s->facemesh = facemesh;
        }
        catch (const std::exception& error) {
            blog(LOG_INFO, "in detection inference creation, exception: %s", error.what());
        }
    }
    else {
        do_log(LOG_ERROR, "Could not find one of these required models: %s, %s", model_data_path.c_str(), landmark_data_path.c_str());
    }
}
//----------------------------------------------------------------------------------------------------------------------

void face_detection_init(face_detection_state *s) {
    // TODO ça se fait pour chaque filtre, sans partage de resources, c'est débile... A améliorer avant release
    ov::Core core;
    obs_enter_graphics();
    s->facedetection_texrender = gs_texrender_create(GS_BGRA, GS_ZS_NONE);
    s->staging_texture = gs_stagesurface_create(FACEDETECTION_WIDTH, FACEDETECTION_HEIGHT, GS_BGRA);

    s->fd_points_texture = gs_texture_create(468, 1, GS_RG32F, 1, nullptr, GS_DYNAMIC);

    obs_leave_graphics();
    //debug("STAGING TEXTURE = %p", s->staging_texture);
    for (const auto& device : core.get_available_devices()) {
        //don't allow GNA to be a supported device for this plugin.
        if (device == "GNA")
            continue;

        s->ov_available_devices.push_back(device);
        debug("FACE DETECTION %s", device.c_str());
    }

    if (s->ov_available_devices.empty()) {
        blog(LOG_INFO, "No available OpenVINO devices found.");
        return; // NOLINT(*-use-nullptr)
    }

    /** Configure networks **/
    face_detection_update(s);
}
//----------------------------------------------------------------------------------------------------------------------

void face_detection_render(face_detection_state *s, obs_source_t *target_source, effect_shader *main_shader) {
    const enum gs_color_space preferred_spaces[] = {
        GS_CS_SRGB,
        GS_CS_SRGB_16F,
        GS_CS_709_EXTENDED,
    };
    const enum gs_color_space source_space = obs_source_get_color_space(target_source, OBS_COUNTOF(preferred_spaces), preferred_spaces);
    uint32_t cx = obs_source_get_width(target_source); //s->width;
    uint32_t cy = obs_source_get_height(target_source); //s->height;
    //debug("%i %i", cx, cy);

    gs_texrender_reset(s->facedetection_texrender);
    if (gs_texrender_begin_with_color_space(s->facedetection_texrender, FACEDETECTION_WIDTH, FACEDETECTION_HEIGHT, source_space)) {
        gs_blend_state_push();
        gs_blend_function_separate(GS_BLEND_SRCALPHA,
                       GS_BLEND_INVSRCALPHA, GS_BLEND_ONE,
                       GS_BLEND_INVSRCALPHA);

        struct vec4 clear_color{0,0,0,0};
        gs_clear(GS_CLEAR_COLOR, &clear_color, 0.0f, 0);
        gs_ortho(0.0f, (float)cx, 0.0f, (float)cy, -100.0f, 100.0f);

        obs_source_video_render(target_source);

        gs_blend_state_pop();

        gs_texrender_end(s->facedetection_texrender);
        gs_texture_t *tex = gs_texrender_get_texture(s->facedetection_texrender);
        gs_stage_texture(s->staging_texture, tex);

        //debug("%p ----> %p", s->staging_texture, tex);

        uint8_t *data;
        uint32_t linesize;

        if (gs_stagesurface_map(s->staging_texture, &data, &linesize)) {
            //__debug_save_texture_png(s->staging_texture, FACEDETECTION_WIDTH, FACEDETECTION_HEIGHT, "/home/olivier/obs-plugins/obs-shadertastic/lab/debug.png");
            //memcpy(filter->data, data, linesize * FACEDETECTION_HEIGHT);
            //debug("linesize: %i", linesize);
            //debug("test: %i", data[FACEDETECTION_HEIGHT*4]);
//            filter->linesize = linesize;
//            filter->ready = true;

            gs_stagesurface_unmap(s->staging_texture);

            // Convert BGRA to BGR
            uint8_t outputBGR[FACEDETECTION_WIDTH*FACEDETECTION_HEIGHT*3];

//            for (int i = 0; i < facedetection_width * facedetection_height; ++i) {
//                // Calculate the starting index of the current pixel in the input array
//                int inputIndex = i * 4;
//
//                // Calculate the starting index of the current pixel in the output array
//                int outputIndex = i * 3;
//
//                // Copy the Blue, Green, and Red components while skipping Alpha
//                outputBGR[outputIndex + 2] = data[inputIndex + 2]; // Red
//                outputBGR[outputIndex + 1] = data[inputIndex + 1]; // Green
//                outputBGR[outputIndex + 0] = data[inputIndex + 0]; // Blue
//            }

            __m256i b = _mm256_set_epi8(
                0, 0, 0, 0,
                14, 13, 12,
                10, 9, 8,
                6, 5, 4,
                2, 1, 0,

                0, 0, 0, 0,
                14, 13, 12,
                10, 9, 8,
                6, 5, 4,
                2, 1, 0
            ); // Indices for permutation
            for (int i = 0; i < FACEDETECTION_WIDTH * FACEDETECTION_HEIGHT; i += 8) {
                int inputIndex = i * 4;
                int outputIndex = i * 3;

                // Load 16 BGRA pixels (32 bytes) into a AVX2 registers
                __m256i bgra1 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(&data[inputIndex]));

                // Permute the bytes to rearrange BGRA to BGR format
                __m256i bgr1 = _mm256_shuffle_epi8(bgra1, b);
                bgr1 = _mm256_permutevar8x32_epi32(bgr1, _mm256_set_epi32(7, 3, 6, 5, 4, 2, 1, 0));

                // Store the BGR pixels into the output array
                _mm256_storeu_si256(reinterpret_cast<__m256i*>(&outputBGR[outputIndex]), bgr1);
            }
            //stbi_write_bmp("/home/olivier/obs-plugins/obs-shadertastic/lab/facedetection.bmp", FACEDETECTION_WIDTH, FACEDETECTION_HEIGHT, 3, outputBGR);

            auto facemesh = s->facemesh;
            if (!facemesh)
                return;

            // Convert to BGR
            cv::Mat imageBGR(FACEDETECTION_HEIGHT, FACEDETECTION_WIDTH, CV_8UC3, outputBGR);// = convertFrameToBGR(frame, tf);

            bool bDisplayResults = facemesh->Run(imageBGR, s->facelandmark_results);

            if (bDisplayResults)
            {
                {
                    auto bbox = face_detection_get_bounding_box(&s->facelandmark_results, left_iris_refinement_indices, left_iris_refined_region_num_points);
                    try_gs_effect_set_vec2(main_shader->param_fd_leye_1, &bbox.point1);
                    try_gs_effect_set_vec2(main_shader->param_fd_leye_2, &bbox.point2);
                    //debug("Left Eye: %f %f %f %f", bbox.x1, bbox.y1, bbox.x2-bbox.x1, bbox.y2-bbox.y1);
                }
                {
                    auto bbox = face_detection_get_bounding_box(&s->facelandmark_results, right_iris_refinement_indices, right_iris_refined_region_num_points);
                    try_gs_effect_set_vec2(main_shader->param_fd_reye_1, &bbox.point1);
                    try_gs_effect_set_vec2(main_shader->param_fd_reye_2, &bbox.point2);
                    //debug("Right Eye: %f %f %f %f", bbox.x1, bbox.y1, bbox.x2-bbox.x1, bbox.y2-bbox.y1);
                }
                {
                    auto bbox = face_detection_get_bounding_box(&s->facelandmark_results, not_lips_eyes_indices, 310);
                    try_gs_effect_set_vec2(main_shader->param_fd_face_1, &bbox.point1);
                    try_gs_effect_set_vec2(main_shader->param_fd_face_2, &bbox.point2);
                    //debug("Face: %f %f %f %f", bbox.x1, bbox.y1, bbox.x2-bbox.x1, bbox.y2-bbox.y1);
                }
                float points[468*2];
                face_detection_copy_points(&s->facelandmark_results, points);

                float *texpoints;
                uint32_t linesize2 = 0;
	            obs_enter_graphics(); {
                    gs_texture_map(s->fd_points_texture, (uint8_t **)(&texpoints), &linesize2);

//                    for (int i=0; i<468*2; ++i) {
////                        texpoints[i] = (uint16_t)(((float)0xffff) * points[i]);
////                        texpoints[468+i] = (uint16_t)(((float)0xffff) * points[i]);
//                        texpoints[i] = points[i];
//                        //texpoints[468+i] = points[i];
//                    }
                    memcpy(texpoints, points, 468*2 * sizeof(float));

                    gs_texture_unmap(s->fd_points_texture);
	            }
	            obs_leave_graphics();
                try_gs_effect_set_texture(main_shader->param_fd_points_tex, s->fd_points_texture)

//                uint16_t data
//                for (int i=0; i<468; ++i) {
//                    points
//                }
            }
        }
        else {
            debug("cpt2");
        }
    }
    else {
        debug("cpt");
    }
}
//----------------------------------------------------------------------------------------------------------------------

void face_detection_destroy(face_detection_state *s) {
    obs_enter_graphics();
    gs_texrender_destroy(s->facedetection_texrender);
    gs_texture_destroy(s->fd_points_texture);
    if (s->staging_texture) {
        gs_stagesurface_destroy(s->staging_texture);
    }
    obs_leave_graphics();
}
//----------------------------------------------------------------------------------------------------------------------
