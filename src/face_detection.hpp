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

bool bla = false;

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

    s->fd_points_texture = gs_texture_create(468, 2, GS_RG32F, 1, nullptr, GS_DYNAMIC);

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
                    if (!bla)
                    {
                        bla = true;
                        int k = 468 * 2;
                        texpoints[k++] = 0.499977; texpoints[k++] = 0.347466;
                        texpoints[k++] = 0.500026; texpoints[k++] = 0.452513;
                        texpoints[k++] = 0.499974; texpoints[k++] = 0.397628;
                        texpoints[k++] = 0.482113; texpoints[k++] = 0.528021;
                        texpoints[k++] = 0.500151; texpoints[k++] = 0.472844;
                        texpoints[k++] = 0.499910; texpoints[k++] = 0.501747;
                        texpoints[k++] = 0.499523; texpoints[k++] = 0.598938;
                        texpoints[k++] = 0.289712; texpoints[k++] = 0.619236;
                        texpoints[k++] = 0.499955; texpoints[k++] = 0.687602;
                        texpoints[k++] = 0.499987; texpoints[k++] = 0.730081;
                        texpoints[k++] = 0.500023; texpoints[k++] = 0.892950;
                        texpoints[k++] = 0.500023; texpoints[k++] = 0.333766;
                        texpoints[k++] = 0.500016; texpoints[k++] = 0.320776;
                        texpoints[k++] = 0.500023; texpoints[k++] = 0.307652;
                        texpoints[k++] = 0.499977; texpoints[k++] = 0.304722;
                        texpoints[k++] = 0.499977; texpoints[k++] = 0.294066;
                        texpoints[k++] = 0.499977; texpoints[k++] = 0.280615;
                        texpoints[k++] = 0.499977; texpoints[k++] = 0.262981;
                        texpoints[k++] = 0.499968; texpoints[k++] = 0.218629;
                        texpoints[k++] = 0.499816; texpoints[k++] = 0.437019;
                        texpoints[k++] = 0.473773; texpoints[k++] = 0.426090;
                        texpoints[k++] = 0.104907; texpoints[k++] = 0.745859;
                        texpoints[k++] = 0.365930; texpoints[k++] = 0.590424;
                        texpoints[k++] = 0.338758; texpoints[k++] = 0.586975;
                        texpoints[k++] = 0.311120; texpoints[k++] = 0.590540;
                        texpoints[k++] = 0.274658; texpoints[k++] = 0.610869;
                        texpoints[k++] = 0.393362; texpoints[k++] = 0.596294;
                        texpoints[k++] = 0.345234; texpoints[k++] = 0.655989;
                        texpoints[k++] = 0.370094; texpoints[k++] = 0.653924;
                        texpoints[k++] = 0.319322; texpoints[k++] = 0.652735;
                        texpoints[k++] = 0.297903; texpoints[k++] = 0.646409;
                        texpoints[k++] = 0.247792; texpoints[k++] = 0.589190;
                        texpoints[k++] = 0.396889; texpoints[k++] = 0.157245;
                        texpoints[k++] = 0.280098; texpoints[k++] = 0.624400;
                        texpoints[k++] = 0.106310; texpoints[k++] = 0.600044;
                        texpoints[k++] = 0.209925; texpoints[k++] = 0.608647;
                        texpoints[k++] = 0.355808; texpoints[k++] = 0.465594;
                        texpoints[k++] = 0.471751; texpoints[k++] = 0.349596;
                        texpoints[k++] = 0.474155; texpoints[k++] = 0.319808;
                        texpoints[k++] = 0.439785; texpoints[k++] = 0.342771;
                        texpoints[k++] = 0.414617; texpoints[k++] = 0.333459;
                        texpoints[k++] = 0.450374; texpoints[k++] = 0.319139;
                        texpoints[k++] = 0.428771; texpoints[k++] = 0.317309;
                        texpoints[k++] = 0.374971; texpoints[k++] = 0.272195;
                        texpoints[k++] = 0.486717; texpoints[k++] = 0.452371;
                        texpoints[k++] = 0.485301; texpoints[k++] = 0.472605;
                        texpoints[k++] = 0.257765; texpoints[k++] = 0.685510;
                        texpoints[k++] = 0.401223; texpoints[k++] = 0.544828;
                        texpoints[k++] = 0.429819; texpoints[k++] = 0.451385;
                        texpoints[k++] = 0.421352; texpoints[k++] = 0.466259;
                        texpoints[k++] = 0.276896; texpoints[k++] = 0.467943;
                        texpoints[k++] = 0.483370; texpoints[k++] = 0.500413;
                        texpoints[k++] = 0.337212; texpoints[k++] = 0.717117;
                        texpoints[k++] = 0.296392; texpoints[k++] = 0.706757;
                        texpoints[k++] = 0.169295; texpoints[k++] = 0.806186;
                        texpoints[k++] = 0.447580; texpoints[k++] = 0.697390;
                        texpoints[k++] = 0.392390; texpoints[k++] = 0.646112;
                        texpoints[k++] = 0.354490; texpoints[k++] = 0.303216;
                        texpoints[k++] = 0.067305; texpoints[k++] = 0.269895;
                        texpoints[k++] = 0.442739; texpoints[k++] = 0.427174;
                        texpoints[k++] = 0.457098; texpoints[k++] = 0.415208;
                        texpoints[k++] = 0.381974; texpoints[k++] = 0.305289;
                        texpoints[k++] = 0.392389; texpoints[k++] = 0.305797;
                        texpoints[k++] = 0.277076; texpoints[k++] = 0.728068;
                        texpoints[k++] = 0.422552; texpoints[k++] = 0.436767;
                        texpoints[k++] = 0.385919; texpoints[k++] = 0.718636;
                        texpoints[k++] = 0.383103; texpoints[k++] = 0.744160;
                        texpoints[k++] = 0.331431; texpoints[k++] = 0.880286;
                        texpoints[k++] = 0.229924; texpoints[k++] = 0.767997;
                        texpoints[k++] = 0.364501; texpoints[k++] = 0.810886;
                        texpoints[k++] = 0.229622; texpoints[k++] = 0.700459;
                        texpoints[k++] = 0.173287; texpoints[k++] = 0.721252;
                        texpoints[k++] = 0.472879; texpoints[k++] = 0.333802;
                        texpoints[k++] = 0.446828; texpoints[k++] = 0.331473;
                        texpoints[k++] = 0.422762; texpoints[k++] = 0.326110;
                        texpoints[k++] = 0.445308; texpoints[k++] = 0.419934;
                        texpoints[k++] = 0.388103; texpoints[k++] = 0.306039;
                        texpoints[k++] = 0.403039; texpoints[k++] = 0.293460;
                        texpoints[k++] = 0.403629; texpoints[k++] = 0.306047;
                        texpoints[k++] = 0.460042; texpoints[k++] = 0.442861;
                        texpoints[k++] = 0.431158; texpoints[k++] = 0.307634;
                        texpoints[k++] = 0.452182; texpoints[k++] = 0.307634;
                        texpoints[k++] = 0.475387; texpoints[k++] = 0.307634;
                        texpoints[k++] = 0.465828; texpoints[k++] = 0.220810;
                        texpoints[k++] = 0.472329; texpoints[k++] = 0.263774;
                        texpoints[k++] = 0.473087; texpoints[k++] = 0.282143;
                        texpoints[k++] = 0.473122; texpoints[k++] = 0.295374;
                        texpoints[k++] = 0.473033; texpoints[k++] = 0.304722;
                        texpoints[k++] = 0.427942; texpoints[k++] = 0.304722;
                        texpoints[k++] = 0.426479; texpoints[k++] = 0.296460;
                        texpoints[k++] = 0.423162; texpoints[k++] = 0.288154;
                        texpoints[k++] = 0.418309; texpoints[k++] = 0.279937;
                        texpoints[k++] = 0.390095; texpoints[k++] = 0.360427;
                        texpoints[k++] = 0.013954; texpoints[k++] = 0.439966;
                        texpoints[k++] = 0.499914; texpoints[k++] = 0.419853;
                        texpoints[k++] = 0.413200; texpoints[k++] = 0.304600;
                        texpoints[k++] = 0.409626; texpoints[k++] = 0.298177;
                        texpoints[k++] = 0.468080; texpoints[k++] = 0.398465;
                        texpoints[k++] = 0.422729; texpoints[k++] = 0.414015;
                        texpoints[k++] = 0.463080; texpoints[k++] = 0.406216;
                        texpoints[k++] = 0.372120; texpoints[k++] = 0.526586;
                        texpoints[k++] = 0.334562; texpoints[k++] = 0.503927;
                        texpoints[k++] = 0.411671; texpoints[k++] = 0.453035;
                        texpoints[k++] = 0.242176; texpoints[k++] = 0.852324;
                        texpoints[k++] = 0.290777; texpoints[k++] = 0.798554;
                        texpoints[k++] = 0.327338; texpoints[k++] = 0.743473;
                        texpoints[k++] = 0.399510; texpoints[k++] = 0.251079;
                        texpoints[k++] = 0.441728; texpoints[k++] = 0.738324;
                        texpoints[k++] = 0.429765; texpoints[k++] = 0.812166;
                        texpoints[k++] = 0.412198; texpoints[k++] = 0.891099;
                        texpoints[k++] = 0.288955; texpoints[k++] = 0.601048;
                        texpoints[k++] = 0.218937; texpoints[k++] = 0.564589;
                        texpoints[k++] = 0.412782; texpoints[k++] = 0.601030;
                        texpoints[k++] = 0.257135; texpoints[k++] = 0.644560;
                        texpoints[k++] = 0.427685; texpoints[k++] = 0.562039;
                        texpoints[k++] = 0.448340; texpoints[k++] = 0.463064;
                        texpoints[k++] = 0.178560; texpoints[k++] = 0.542446;
                        texpoints[k++] = 0.247308; texpoints[k++] = 0.542806;
                        texpoints[k++] = 0.286267; texpoints[k++] = 0.532325;
                        texpoints[k++] = 0.332828; texpoints[k++] = 0.539288;
                        texpoints[k++] = 0.368756; texpoints[k++] = 0.552793;
                        texpoints[k++] = 0.398964; texpoints[k++] = 0.567345;
                        texpoints[k++] = 0.476410; texpoints[k++] = 0.594194;
                        texpoints[k++] = 0.189241; texpoints[k++] = 0.476076;
                        texpoints[k++] = 0.228962; texpoints[k++] = 0.651049;
                        texpoints[k++] = 0.490726; texpoints[k++] = 0.437599;
                        texpoints[k++] = 0.404670; texpoints[k++] = 0.514867;
                        texpoints[k++] = 0.019469; texpoints[k++] = 0.598436;
                        texpoints[k++] = 0.426243; texpoints[k++] = 0.579569;
                        texpoints[k++] = 0.396993; texpoints[k++] = 0.451203;
                        texpoints[k++] = 0.266470; texpoints[k++] = 0.623023;
                        texpoints[k++] = 0.439121; texpoints[k++] = 0.481042;
                        texpoints[k++] = 0.032314; texpoints[k++] = 0.355643;
                        texpoints[k++] = 0.419054; texpoints[k++] = 0.612845;
                        texpoints[k++] = 0.462783; texpoints[k++] = 0.494253;
                        texpoints[k++] = 0.238979; texpoints[k++] = 0.220255;
                        texpoints[k++] = 0.198221; texpoints[k++] = 0.168062;
                        texpoints[k++] = 0.107550; texpoints[k++] = 0.459245;
                        texpoints[k++] = 0.183610; texpoints[k++] = 0.259743;
                        texpoints[k++] = 0.134410; texpoints[k++] = 0.666317;
                        texpoints[k++] = 0.385764; texpoints[k++] = 0.116846;
                        texpoints[k++] = 0.490967; texpoints[k++] = 0.420622;
                        texpoints[k++] = 0.382385; texpoints[k++] = 0.491427;
                        texpoints[k++] = 0.174399; texpoints[k++] = 0.602329;
                        texpoints[k++] = 0.318785; texpoints[k++] = 0.603765;
                        texpoints[k++] = 0.343364; texpoints[k++] = 0.599403;
                        texpoints[k++] = 0.396100; texpoints[k++] = 0.289783;
                        texpoints[k++] = 0.187885; texpoints[k++] = 0.411462;
                        texpoints[k++] = 0.430987; texpoints[k++] = 0.055935;
                        texpoints[k++] = 0.318993; texpoints[k++] = 0.101715;
                        texpoints[k++] = 0.266248; texpoints[k++] = 0.130299;
                        texpoints[k++] = 0.500023; texpoints[k++] = 0.809424;
                        texpoints[k++] = 0.499977; texpoints[k++] = 0.045547;
                        texpoints[k++] = 0.366170; texpoints[k++] = 0.601178;
                        texpoints[k++] = 0.393207; texpoints[k++] = 0.604463;
                        texpoints[k++] = 0.410373; texpoints[k++] = 0.608920;
                        texpoints[k++] = 0.194993; texpoints[k++] = 0.657898;
                        texpoints[k++] = 0.388665; texpoints[k++] = 0.637716;
                        texpoints[k++] = 0.365962; texpoints[k++] = 0.644029;
                        texpoints[k++] = 0.343364; texpoints[k++] = 0.644643;
                        texpoints[k++] = 0.318785; texpoints[k++] = 0.641660;
                        texpoints[k++] = 0.301415; texpoints[k++] = 0.636844;
                        texpoints[k++] = 0.058133; texpoints[k++] = 0.680924;
                        texpoints[k++] = 0.301415; texpoints[k++] = 0.612551;
                        texpoints[k++] = 0.499988; texpoints[k++] = 0.381566;
                        texpoints[k++] = 0.415838; texpoints[k++] = 0.375804;
                        texpoints[k++] = 0.445682; texpoints[k++] = 0.433923;
                        texpoints[k++] = 0.465844; texpoints[k++] = 0.379359;
                        texpoints[k++] = 0.499923; texpoints[k++] = 0.648476;
                        texpoints[k++] = 0.288719; texpoints[k++] = 0.180054;
                        texpoints[k++] = 0.335279; texpoints[k++] = 0.147180;
                        texpoints[k++] = 0.440512; texpoints[k++] = 0.097581;
                        texpoints[k++] = 0.128294; texpoints[k++] = 0.208059;
                        texpoints[k++] = 0.408772; texpoints[k++] = 0.626106;
                        texpoints[k++] = 0.455607; texpoints[k++] = 0.548199;
                        texpoints[k++] = 0.499877; texpoints[k++] = 0.091010;
                        texpoints[k++] = 0.375437; texpoints[k++] = 0.075808;
                        texpoints[k++] = 0.114210; texpoints[k++] = 0.384978;
                        texpoints[k++] = 0.448662; texpoints[k++] = 0.304722;
                        texpoints[k++] = 0.448020; texpoints[k++] = 0.295368;
                        texpoints[k++] = 0.447112; texpoints[k++] = 0.284192;
                        texpoints[k++] = 0.444832; texpoints[k++] = 0.269206;
                        texpoints[k++] = 0.430012; texpoints[k++] = 0.233191;
                        texpoints[k++] = 0.406787; texpoints[k++] = 0.314327;
                        texpoints[k++] = 0.400738; texpoints[k++] = 0.318931;
                        texpoints[k++] = 0.392400; texpoints[k++] = 0.322297;
                        texpoints[k++] = 0.367856; texpoints[k++] = 0.336081;
                        texpoints[k++] = 0.247923; texpoints[k++] = 0.398667;
                        texpoints[k++] = 0.452770; texpoints[k++] = 0.579150;
                        texpoints[k++] = 0.436392; texpoints[k++] = 0.640113;
                        texpoints[k++] = 0.416164; texpoints[k++] = 0.631286;
                        texpoints[k++] = 0.413386; texpoints[k++] = 0.307634;
                        texpoints[k++] = 0.228018; texpoints[k++] = 0.316428;
                        texpoints[k++] = 0.468268; texpoints[k++] = 0.647329;
                        texpoints[k++] = 0.411362; texpoints[k++] = 0.195673;
                        texpoints[k++] = 0.499989; texpoints[k++] = 0.530175;
                        texpoints[k++] = 0.479154; texpoints[k++] = 0.557346;
                        texpoints[k++] = 0.499974; texpoints[k++] = 0.560363;
                        texpoints[k++] = 0.432112; texpoints[k++] = 0.506411;
                        texpoints[k++] = 0.499886; texpoints[k++] = 0.133083;
                        texpoints[k++] = 0.499913; texpoints[k++] = 0.178271;
                        texpoints[k++] = 0.456549; texpoints[k++] = 0.180799;
                        texpoints[k++] = 0.344549; texpoints[k++] = 0.254561;
                        texpoints[k++] = 0.378909; texpoints[k++] = 0.425990;
                        texpoints[k++] = 0.374293; texpoints[k++] = 0.219815;
                        texpoints[k++] = 0.319688; texpoints[k++] = 0.429262;
                        texpoints[k++] = 0.357155; texpoints[k++] = 0.395730;
                        texpoints[k++] = 0.295284; texpoints[k++] = 0.378419;
                        texpoints[k++] = 0.447750; texpoints[k++] = 0.137523;
                        texpoints[k++] = 0.410986; texpoints[k++] = 0.491277;
                        texpoints[k++] = 0.313951; texpoints[k++] = 0.224692;
                        texpoints[k++] = 0.354128; texpoints[k++] = 0.187447;
                        texpoints[k++] = 0.324548; texpoints[k++] = 0.296007;
                        texpoints[k++] = 0.189096; texpoints[k++] = 0.353700;
                        texpoints[k++] = 0.279777; texpoints[k++] = 0.285342;
                        texpoints[k++] = 0.133823; texpoints[k++] = 0.317299;
                        texpoints[k++] = 0.336768; texpoints[k++] = 0.355267;
                        texpoints[k++] = 0.429884; texpoints[k++] = 0.533478;
                        texpoints[k++] = 0.455528; texpoints[k++] = 0.451377;
                        texpoints[k++] = 0.437114; texpoints[k++] = 0.441104;
                        texpoints[k++] = 0.467288; texpoints[k++] = 0.470075;
                        texpoints[k++] = 0.414712; texpoints[k++] = 0.664780;
                        texpoints[k++] = 0.377046; texpoints[k++] = 0.677222;
                        texpoints[k++] = 0.344108; texpoints[k++] = 0.679849;
                        texpoints[k++] = 0.312876; texpoints[k++] = 0.677668;
                        texpoints[k++] = 0.283526; texpoints[k++] = 0.666810;
                        texpoints[k++] = 0.241246; texpoints[k++] = 0.617214;
                        texpoints[k++] = 0.102986; texpoints[k++] = 0.531237;
                        texpoints[k++] = 0.267612; texpoints[k++] = 0.575440;
                        texpoints[k++] = 0.297879; texpoints[k++] = 0.566824;
                        texpoints[k++] = 0.333434; texpoints[k++] = 0.566122;
                        texpoints[k++] = 0.366427; texpoints[k++] = 0.573884;
                        texpoints[k++] = 0.396012; texpoints[k++] = 0.583304;
                        texpoints[k++] = 0.420121; texpoints[k++] = 0.589772;
                        texpoints[k++] = 0.007561; texpoints[k++] = 0.519223;
                        texpoints[k++] = 0.432949; texpoints[k++] = 0.430482;
                        texpoints[k++] = 0.458639; texpoints[k++] = 0.520911;
                        texpoints[k++] = 0.473466; texpoints[k++] = 0.454256;
                        texpoints[k++] = 0.476088; texpoints[k++] = 0.436170;
                        texpoints[k++] = 0.468472; texpoints[k++] = 0.444943;
                        texpoints[k++] = 0.433991; texpoints[k++] = 0.417638;
                        texpoints[k++] = 0.483518; texpoints[k++] = 0.437016;
                        texpoints[k++] = 0.482483; texpoints[k++] = 0.422151;
                        texpoints[k++] = 0.426450; texpoints[k++] = 0.610201;
                        texpoints[k++] = 0.438999; texpoints[k++] = 0.603505;
                        texpoints[k++] = 0.450067; texpoints[k++] = 0.599566;
                        texpoints[k++] = 0.289712; texpoints[k++] = 0.631747;
                        texpoints[k++] = 0.276670; texpoints[k++] = 0.636627;
                        texpoints[k++] = 0.517862; texpoints[k++] = 0.528052;
                        texpoints[k++] = 0.710288; texpoints[k++] = 0.619236;
                        texpoints[k++] = 0.526227; texpoints[k++] = 0.426090;
                        texpoints[k++] = 0.895093; texpoints[k++] = 0.745859;
                        texpoints[k++] = 0.634070; texpoints[k++] = 0.590424;
                        texpoints[k++] = 0.661242; texpoints[k++] = 0.586975;
                        texpoints[k++] = 0.688880; texpoints[k++] = 0.590540;
                        texpoints[k++] = 0.725342; texpoints[k++] = 0.610869;
                        texpoints[k++] = 0.606630; texpoints[k++] = 0.596295;
                        texpoints[k++] = 0.654766; texpoints[k++] = 0.655989;
                        texpoints[k++] = 0.629906; texpoints[k++] = 0.653924;
                        texpoints[k++] = 0.680678; texpoints[k++] = 0.652735;
                        texpoints[k++] = 0.702097; texpoints[k++] = 0.646409;
                        texpoints[k++] = 0.752212; texpoints[k++] = 0.589195;
                        texpoints[k++] = 0.602918; texpoints[k++] = 0.157137;
                        texpoints[k++] = 0.719902; texpoints[k++] = 0.624400;
                        texpoints[k++] = 0.893693; texpoints[k++] = 0.600040;
                        texpoints[k++] = 0.790082; texpoints[k++] = 0.608646;
                        texpoints[k++] = 0.643998; texpoints[k++] = 0.465512;
                        texpoints[k++] = 0.528249; texpoints[k++] = 0.349596;
                        texpoints[k++] = 0.525850; texpoints[k++] = 0.319809;
                        texpoints[k++] = 0.560215; texpoints[k++] = 0.342771;
                        texpoints[k++] = 0.585384; texpoints[k++] = 0.333459;
                        texpoints[k++] = 0.549626; texpoints[k++] = 0.319139;
                        texpoints[k++] = 0.571228; texpoints[k++] = 0.317308;
                        texpoints[k++] = 0.624852; texpoints[k++] = 0.271901;
                        texpoints[k++] = 0.513050; texpoints[k++] = 0.452718;
                        texpoints[k++] = 0.515097; texpoints[k++] = 0.472748;
                        texpoints[k++] = 0.742247; texpoints[k++] = 0.685493;
                        texpoints[k++] = 0.598631; texpoints[k++] = 0.545021;
                        texpoints[k++] = 0.570338; texpoints[k++] = 0.451425;
                        texpoints[k++] = 0.578632; texpoints[k++] = 0.466377;
                        texpoints[k++] = 0.723087; texpoints[k++] = 0.467946;
                        texpoints[k++] = 0.516446; texpoints[k++] = 0.500361;
                        texpoints[k++] = 0.662801; texpoints[k++] = 0.717082;
                        texpoints[k++] = 0.703624; texpoints[k++] = 0.706729;
                        texpoints[k++] = 0.830705; texpoints[k++] = 0.806186;
                        texpoints[k++] = 0.552386; texpoints[k++] = 0.697432;
                        texpoints[k++] = 0.607610; texpoints[k++] = 0.646112;
                        texpoints[k++] = 0.645429; texpoints[k++] = 0.303293;
                        texpoints[k++] = 0.932695; texpoints[k++] = 0.269895;
                        texpoints[k++] = 0.557261; texpoints[k++] = 0.427174;
                        texpoints[k++] = 0.542902; texpoints[k++] = 0.415208;
                        texpoints[k++] = 0.618026; texpoints[k++] = 0.305289;
                        texpoints[k++] = 0.607591; texpoints[k++] = 0.305797;
                        texpoints[k++] = 0.722943; texpoints[k++] = 0.728037;
                        texpoints[k++] = 0.577414; texpoints[k++] = 0.436833;
                        texpoints[k++] = 0.614083; texpoints[k++] = 0.718613;
                        texpoints[k++] = 0.616907; texpoints[k++] = 0.744114;
                        texpoints[k++] = 0.668509; texpoints[k++] = 0.880086;
                        texpoints[k++] = 0.770092; texpoints[k++] = 0.767979;
                        texpoints[k++] = 0.635536; texpoints[k++] = 0.810751;
                        texpoints[k++] = 0.770391; texpoints[k++] = 0.700444;
                        texpoints[k++] = 0.826722; texpoints[k++] = 0.721245;
                        texpoints[k++] = 0.527121; texpoints[k++] = 0.333802;
                        texpoints[k++] = 0.553172; texpoints[k++] = 0.331473;
                        texpoints[k++] = 0.577238; texpoints[k++] = 0.326110;
                        texpoints[k++] = 0.554692; texpoints[k++] = 0.419934;
                        texpoints[k++] = 0.611897; texpoints[k++] = 0.306039;
                        texpoints[k++] = 0.596961; texpoints[k++] = 0.293460;
                        texpoints[k++] = 0.596371; texpoints[k++] = 0.306047;
                        texpoints[k++] = 0.539958; texpoints[k++] = 0.442861;
                        texpoints[k++] = 0.568842; texpoints[k++] = 0.307634;
                        texpoints[k++] = 0.547818; texpoints[k++] = 0.307634;
                        texpoints[k++] = 0.524613; texpoints[k++] = 0.307634;
                        texpoints[k++] = 0.534090; texpoints[k++] = 0.220859;
                        texpoints[k++] = 0.527671; texpoints[k++] = 0.263774;
                        texpoints[k++] = 0.526913; texpoints[k++] = 0.282143;
                        texpoints[k++] = 0.526878; texpoints[k++] = 0.295374;
                        texpoints[k++] = 0.526967; texpoints[k++] = 0.304722;
                        texpoints[k++] = 0.572058; texpoints[k++] = 0.304722;
                        texpoints[k++] = 0.573521; texpoints[k++] = 0.296460;
                        texpoints[k++] = 0.576838; texpoints[k++] = 0.288154;
                        texpoints[k++] = 0.581691; texpoints[k++] = 0.279937;
                        texpoints[k++] = 0.609945; texpoints[k++] = 0.360090;
                        texpoints[k++] = 0.986046; texpoints[k++] = 0.439966;
                        texpoints[k++] = 0.586800; texpoints[k++] = 0.304600;
                        texpoints[k++] = 0.590372; texpoints[k++] = 0.298177;
                        texpoints[k++] = 0.531915; texpoints[k++] = 0.398463;
                        texpoints[k++] = 0.577268; texpoints[k++] = 0.414065;
                        texpoints[k++] = 0.536915; texpoints[k++] = 0.406214;
                        texpoints[k++] = 0.627543; texpoints[k++] = 0.526648;
                        texpoints[k++] = 0.665586; texpoints[k++] = 0.504049;
                        texpoints[k++] = 0.588354; texpoints[k++] = 0.453138;
                        texpoints[k++] = 0.757824; texpoints[k++] = 0.852324;
                        texpoints[k++] = 0.709250; texpoints[k++] = 0.798492;
                        texpoints[k++] = 0.672684; texpoints[k++] = 0.743419;
                        texpoints[k++] = 0.600409; texpoints[k++] = 0.250995;
                        texpoints[k++] = 0.558266; texpoints[k++] = 0.738328;
                        texpoints[k++] = 0.570304; texpoints[k++] = 0.812129;
                        texpoints[k++] = 0.588166; texpoints[k++] = 0.890956;
                        texpoints[k++] = 0.711045; texpoints[k++] = 0.601048;
                        texpoints[k++] = 0.781070; texpoints[k++] = 0.564595;
                        texpoints[k++] = 0.587247; texpoints[k++] = 0.601068;
                        texpoints[k++] = 0.742870; texpoints[k++] = 0.644554;
                        texpoints[k++] = 0.572156; texpoints[k++] = 0.562348;
                        texpoints[k++] = 0.551868; texpoints[k++] = 0.463430;
                        texpoints[k++] = 0.821442; texpoints[k++] = 0.542444;
                        texpoints[k++] = 0.752702; texpoints[k++] = 0.542818;
                        texpoints[k++] = 0.713757; texpoints[k++] = 0.532373;
                        texpoints[k++] = 0.667113; texpoints[k++] = 0.539327;
                        texpoints[k++] = 0.631101; texpoints[k++] = 0.552846;
                        texpoints[k++] = 0.600862; texpoints[k++] = 0.567527;
                        texpoints[k++] = 0.523481; texpoints[k++] = 0.594373;
                        texpoints[k++] = 0.810748; texpoints[k++] = 0.476074;
                        texpoints[k++] = 0.771046; texpoints[k++] = 0.651041;
                        texpoints[k++] = 0.509127; texpoints[k++] = 0.437282;
                        texpoints[k++] = 0.595293; texpoints[k++] = 0.514976;
                        texpoints[k++] = 0.980531; texpoints[k++] = 0.598436;
                        texpoints[k++] = 0.573500; texpoints[k++] = 0.580000;
                        texpoints[k++] = 0.602995; texpoints[k++] = 0.451312;
                        texpoints[k++] = 0.733530; texpoints[k++] = 0.623023;
                        texpoints[k++] = 0.560611; texpoints[k++] = 0.480983;
                        texpoints[k++] = 0.967686; texpoints[k++] = 0.355643;
                        texpoints[k++] = 0.580985; texpoints[k++] = 0.612840;
                        texpoints[k++] = 0.537728; texpoints[k++] = 0.494615;
                        texpoints[k++] = 0.760966; texpoints[k++] = 0.220247;
                        texpoints[k++] = 0.801779; texpoints[k++] = 0.168062;
                        texpoints[k++] = 0.892441; texpoints[k++] = 0.459239;
                        texpoints[k++] = 0.816351; texpoints[k++] = 0.259740;
                        texpoints[k++] = 0.865595; texpoints[k++] = 0.666313;
                        texpoints[k++] = 0.614074; texpoints[k++] = 0.116754;
                        texpoints[k++] = 0.508953; texpoints[k++] = 0.420562;
                        texpoints[k++] = 0.617942; texpoints[k++] = 0.491684;
                        texpoints[k++] = 0.825608; texpoints[k++] = 0.602325;
                        texpoints[k++] = 0.681215; texpoints[k++] = 0.603765;
                        texpoints[k++] = 0.656636; texpoints[k++] = 0.599403;
                        texpoints[k++] = 0.603900; texpoints[k++] = 0.289783;
                        texpoints[k++] = 0.812086; texpoints[k++] = 0.411461;
                        texpoints[k++] = 0.568013; texpoints[k++] = 0.055435;
                        texpoints[k++] = 0.681008; texpoints[k++] = 0.101715;
                        texpoints[k++] = 0.733752; texpoints[k++] = 0.130299;
                        texpoints[k++] = 0.633830; texpoints[k++] = 0.601178;
                        texpoints[k++] = 0.606793; texpoints[k++] = 0.604463;
                        texpoints[k++] = 0.589660; texpoints[k++] = 0.608938;
                        texpoints[k++] = 0.805016; texpoints[k++] = 0.657892;
                        texpoints[k++] = 0.611335; texpoints[k++] = 0.637716;
                        texpoints[k++] = 0.634038; texpoints[k++] = 0.644029;
                        texpoints[k++] = 0.656636; texpoints[k++] = 0.644643;
                        texpoints[k++] = 0.681215; texpoints[k++] = 0.641660;
                        texpoints[k++] = 0.698585; texpoints[k++] = 0.636844;
                        texpoints[k++] = 0.941867; texpoints[k++] = 0.680924;
                        texpoints[k++] = 0.698585; texpoints[k++] = 0.612551;
                        texpoints[k++] = 0.584177; texpoints[k++] = 0.375893;
                        texpoints[k++] = 0.554318; texpoints[k++] = 0.433923;
                        texpoints[k++] = 0.534154; texpoints[k++] = 0.379360;
                        texpoints[k++] = 0.711218; texpoints[k++] = 0.180025;
                        texpoints[k++] = 0.664630; texpoints[k++] = 0.147129;
                        texpoints[k++] = 0.559100; texpoints[k++] = 0.097368;
                        texpoints[k++] = 0.871706; texpoints[k++] = 0.208059;
                        texpoints[k++] = 0.591234; texpoints[k++] = 0.626106;
                        texpoints[k++] = 0.544341; texpoints[k++] = 0.548416;
                        texpoints[k++] = 0.624563; texpoints[k++] = 0.075808;
                        texpoints[k++] = 0.885770; texpoints[k++] = 0.384971;
                        texpoints[k++] = 0.551338; texpoints[k++] = 0.304722;
                        texpoints[k++] = 0.551980; texpoints[k++] = 0.295368;
                        texpoints[k++] = 0.552888; texpoints[k++] = 0.284192;
                        texpoints[k++] = 0.555168; texpoints[k++] = 0.269206;
                        texpoints[k++] = 0.569944; texpoints[k++] = 0.232965;
                        texpoints[k++] = 0.593203; texpoints[k++] = 0.314324;
                        texpoints[k++] = 0.599262; texpoints[k++] = 0.318931;
                        texpoints[k++] = 0.607600; texpoints[k++] = 0.322297;
                        texpoints[k++] = 0.631938; texpoints[k++] = 0.336500;
                        texpoints[k++] = 0.752033; texpoints[k++] = 0.398685;
                        texpoints[k++] = 0.547226; texpoints[k++] = 0.579605;
                        texpoints[k++] = 0.563544; texpoints[k++] = 0.640172;
                        texpoints[k++] = 0.583841; texpoints[k++] = 0.631286;
                        texpoints[k++] = 0.586614; texpoints[k++] = 0.307634;
                        texpoints[k++] = 0.771915; texpoints[k++] = 0.316422;
                        texpoints[k++] = 0.531597; texpoints[k++] = 0.647517;
                        texpoints[k++] = 0.588371; texpoints[k++] = 0.195559;
                        texpoints[k++] = 0.520797; texpoints[k++] = 0.557435;
                        texpoints[k++] = 0.567985; texpoints[k++] = 0.506521;
                        texpoints[k++] = 0.543283; texpoints[k++] = 0.180745;
                        texpoints[k++] = 0.655317; texpoints[k++] = 0.254485;
                        texpoints[k++] = 0.621009; texpoints[k++] = 0.425982;
                        texpoints[k++] = 0.625560; texpoints[k++] = 0.219688;
                        texpoints[k++] = 0.680198; texpoints[k++] = 0.429281;
                        texpoints[k++] = 0.642764; texpoints[k++] = 0.395662;
                        texpoints[k++] = 0.704663; texpoints[k++] = 0.378470;
                        texpoints[k++] = 0.552012; texpoints[k++] = 0.137408;
                        texpoints[k++] = 0.589072; texpoints[k++] = 0.491363;
                        texpoints[k++] = 0.685945; texpoints[k++] = 0.224643;
                        texpoints[k++] = 0.645735; texpoints[k++] = 0.187360;
                        texpoints[k++] = 0.675343; texpoints[k++] = 0.296022;
                        texpoints[k++] = 0.810858; texpoints[k++] = 0.353695;
                        texpoints[k++] = 0.720122; texpoints[k++] = 0.285333;
                        texpoints[k++] = 0.866152; texpoints[k++] = 0.317295;
                        texpoints[k++] = 0.663187; texpoints[k++] = 0.355403;
                        texpoints[k++] = 0.570082; texpoints[k++] = 0.533674;
                        texpoints[k++] = 0.544562; texpoints[k++] = 0.451624;
                        texpoints[k++] = 0.562759; texpoints[k++] = 0.441215;
                        texpoints[k++] = 0.531987; texpoints[k++] = 0.469860;
                        texpoints[k++] = 0.585271; texpoints[k++] = 0.664823;
                        texpoints[k++] = 0.622953; texpoints[k++] = 0.677221;
                        texpoints[k++] = 0.655896; texpoints[k++] = 0.679837;
                        texpoints[k++] = 0.687132; texpoints[k++] = 0.677654;
                        texpoints[k++] = 0.716482; texpoints[k++] = 0.666799;
                        texpoints[k++] = 0.758757; texpoints[k++] = 0.617213;
                        texpoints[k++] = 0.897013; texpoints[k++] = 0.531231;
                        texpoints[k++] = 0.732392; texpoints[k++] = 0.575453;
                        texpoints[k++] = 0.702114; texpoints[k++] = 0.566837;
                        texpoints[k++] = 0.666525; texpoints[k++] = 0.566134;
                        texpoints[k++] = 0.633505; texpoints[k++] = 0.573912;
                        texpoints[k++] = 0.603876; texpoints[k++] = 0.583413;
                        texpoints[k++] = 0.579658; texpoints[k++] = 0.590055;
                        texpoints[k++] = 0.992440; texpoints[k++] = 0.519223;
                        texpoints[k++] = 0.567192; texpoints[k++] = 0.430580;
                        texpoints[k++] = 0.541366; texpoints[k++] = 0.521101;
                        texpoints[k++] = 0.526564; texpoints[k++] = 0.453882;
                        texpoints[k++] = 0.523913; texpoints[k++] = 0.436170;
                        texpoints[k++] = 0.531529; texpoints[k++] = 0.444943;
                        texpoints[k++] = 0.566036; texpoints[k++] = 0.417671;
                        texpoints[k++] = 0.516311; texpoints[k++] = 0.436946;
                        texpoints[k++] = 0.517472; texpoints[k++] = 0.422123;
                        texpoints[k++] = 0.573595; texpoints[k++] = 0.610193;
                        texpoints[k++] = 0.560698; texpoints[k++] = 0.604668;
                        texpoints[k++] = 0.549756; texpoints[k++] = 0.600249;
                        texpoints[k++] = 0.710288; texpoints[k++] = 0.631747;
                        texpoints[k++] = 0.723330; texpoints[k++] = 0.636627;
                    }
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
