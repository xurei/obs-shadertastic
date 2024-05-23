#include <thread>
#include <immintrin.h> // Include the AVX2 header
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include "onnxmediapipe/landmark_refinement_indices.h"
#include "util/rgba_to_rgb.h"

#define FACEDETECTION_WIDTH 1280
#define FACEDETECTION_HEIGHT 720

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
//----------------------------------------------------------------------------------------------------------------------

// Globals
std::unique_ptr<Ort::Env> ort_env;

face_detection_bounding_box no_bounding_box{
    -1.0f, -1.0f
    -1.0f, -1.0f
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

void face_detection_copy_points(onnxmediapipe::FaceLandmarksResults *facelandmark_results, float *points) {
    for (size_t i=0; i<468; ++i) {
        if (i < facelandmark_results->refined_landmarks.size()) {
            points[i*4+0] = facelandmark_results->refined_landmarks[i].x;
            points[i*4+1] = facelandmark_results->refined_landmarks[i].y;
            points[i*4+2] = facelandmark_results->refined_landmarks[i].z;
        }
        points[i*4+3] = 1.0;
        //debug("%f %f", points[i*2+0], points[i*2+1]);
        //points[i*3+2] = facelandmark_results->refined_landmarks[i].z;
    }
}
//----------------------------------------------------------------------------------------------------------------------

face_detection_bounding_box face_detection_get_bounding_box(onnxmediapipe::FaceLandmarksResults *facelandmark_results, const unsigned short int *indices, int nb_indices) {
    // Initialize min and max points
    cv::Point2f minPoint(std::numeric_limits<float>::max(), std::numeric_limits<float>::max());
    cv::Point2f maxPoint(std::numeric_limits<float>::min(), std::numeric_limits<float>::min());

    // Find the bounding box
    for (int i=0; i<nb_indices; ++i) {
        const size_t landmark_i = indices[i];
        if (landmark_i < facelandmark_results->refined_landmarks.size()) {
            auto k = facelandmark_results->refined_landmarks[landmark_i];
            minPoint.x = std::min(minPoint.x, k.x);
            minPoint.y = std::min(minPoint.y, k.y);

            maxPoint.x = std::max(maxPoint.x, k.x);
            maxPoint.y = std::max(maxPoint.y, k.y);
        }
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
    UNUSED_PARAMETER(s);
    try {
        auto facemesh = std::make_shared <onnxmediapipe::FaceMesh>(ort_env);
        s->facemesh = facemesh;
    }
    catch (const std::exception& error) {
        blog(LOG_INFO, "in detection inference creation, exception: %s", error.what());
    }
}
//----------------------------------------------------------------------------------------------------------------------

void face_detection_init(face_detection_state *s) {
    if (!ort_env) {
        const char *instanceName = "shadertastic-onnx-inference";

        // TODO maybe add a global setting in OBS to allocate the number of threads ?
        Ort::ThreadingOptions ortThreadingOptions;
        ortThreadingOptions.SetGlobalInterOpNumThreads(
            std::min(
                2, // No more than 2 threads
                std::max(1, (int) std::thread::hardware_concurrency() / 4) // A quarter of the threads that can concurrently run on the CPU
            )
        );
        ortThreadingOptions.SetGlobalIntraOpNumThreads(1);

        ort_env.reset(new Ort::Env(ortThreadingOptions, OrtLoggingLevel::ORT_LOGGING_LEVEL_WARNING, instanceName));
    }

    obs_enter_graphics();
    s->facelandmark_results_counter = 0;
    s->facedetection_texrender = gs_texrender_create(GS_BGRA, GS_ZS_NONE);
    s->staging_texture = gs_stagesurface_create(FACEDETECTION_WIDTH, FACEDETECTION_HEIGHT, GS_BGRA);

    s->fd_points_texture = gs_texture_create(468, 2, GS_RGBA32F, 1, nullptr, GS_DYNAMIC);

    // TODO move this in a global. We don't need to update this texture at every filter
    float *texpoints;
    uint32_t linesize2 = 0;
    gs_texture_map(s->fd_points_texture, (uint8_t **)(&texpoints), &linesize2);
    {
        int k = 468 * 4;
        texpoints[k++] = 0.499977f; texpoints[k++] = 0.347466f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.500026f; texpoints[k++] = 0.452513f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.499974f; texpoints[k++] = 0.397628f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.482113f; texpoints[k++] = 0.528021f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.500151f; texpoints[k++] = 0.472844f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.499910f; texpoints[k++] = 0.501747f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.499523f; texpoints[k++] = 0.598938f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.289712f; texpoints[k++] = 0.619236f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.499955f; texpoints[k++] = 0.687602f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.499987f; texpoints[k++] = 0.730081f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.500023f; texpoints[k++] = 0.892950f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.500023f; texpoints[k++] = 0.333766f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.500016f; texpoints[k++] = 0.320776f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.500023f; texpoints[k++] = 0.307652f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.499977f; texpoints[k++] = 0.304722f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.499977f; texpoints[k++] = 0.294066f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.499977f; texpoints[k++] = 0.280615f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.499977f; texpoints[k++] = 0.262981f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.499968f; texpoints[k++] = 0.218629f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.499816f; texpoints[k++] = 0.437019f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.473773f; texpoints[k++] = 0.426090f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.104907f; texpoints[k++] = 0.745859f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.365930f; texpoints[k++] = 0.590424f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.338758f; texpoints[k++] = 0.586975f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.311120f; texpoints[k++] = 0.590540f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.274658f; texpoints[k++] = 0.610869f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.393362f; texpoints[k++] = 0.596294f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.345234f; texpoints[k++] = 0.655989f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.370094f; texpoints[k++] = 0.653924f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.319322f; texpoints[k++] = 0.652735f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.297903f; texpoints[k++] = 0.646409f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.247792f; texpoints[k++] = 0.589190f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.396889f; texpoints[k++] = 0.157245f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.280098f; texpoints[k++] = 0.624400f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.106310f; texpoints[k++] = 0.600044f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.209925f; texpoints[k++] = 0.608647f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.355808f; texpoints[k++] = 0.465594f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.471751f; texpoints[k++] = 0.349596f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.474155f; texpoints[k++] = 0.319808f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.439785f; texpoints[k++] = 0.342771f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.414617f; texpoints[k++] = 0.333459f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.450374f; texpoints[k++] = 0.319139f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.428771f; texpoints[k++] = 0.317309f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.374971f; texpoints[k++] = 0.272195f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.486717f; texpoints[k++] = 0.452371f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.485301f; texpoints[k++] = 0.472605f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.257765f; texpoints[k++] = 0.685510f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.401223f; texpoints[k++] = 0.544828f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.429819f; texpoints[k++] = 0.451385f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.421352f; texpoints[k++] = 0.466259f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.276896f; texpoints[k++] = 0.467943f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.483370f; texpoints[k++] = 0.500413f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.337212f; texpoints[k++] = 0.717117f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.296392f; texpoints[k++] = 0.706757f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.169295f; texpoints[k++] = 0.806186f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.447580f; texpoints[k++] = 0.697390f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.392390f; texpoints[k++] = 0.646112f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.354490f; texpoints[k++] = 0.303216f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.067305f; texpoints[k++] = 0.269895f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.442739f; texpoints[k++] = 0.427174f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.457098f; texpoints[k++] = 0.415208f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.381974f; texpoints[k++] = 0.305289f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.392389f; texpoints[k++] = 0.305797f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.277076f; texpoints[k++] = 0.728068f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.422552f; texpoints[k++] = 0.436767f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.385919f; texpoints[k++] = 0.718636f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.383103f; texpoints[k++] = 0.744160f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.331431f; texpoints[k++] = 0.880286f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.229924f; texpoints[k++] = 0.767997f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.364501f; texpoints[k++] = 0.810886f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.229622f; texpoints[k++] = 0.700459f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.173287f; texpoints[k++] = 0.721252f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.472879f; texpoints[k++] = 0.333802f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.446828f; texpoints[k++] = 0.331473f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.422762f; texpoints[k++] = 0.326110f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.445308f; texpoints[k++] = 0.419934f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.388103f; texpoints[k++] = 0.306039f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.403039f; texpoints[k++] = 0.293460f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.403629f; texpoints[k++] = 0.306047f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.460042f; texpoints[k++] = 0.442861f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.431158f; texpoints[k++] = 0.307634f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.452182f; texpoints[k++] = 0.307634f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.475387f; texpoints[k++] = 0.307634f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.465828f; texpoints[k++] = 0.220810f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.472329f; texpoints[k++] = 0.263774f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.473087f; texpoints[k++] = 0.282143f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.473122f; texpoints[k++] = 0.295374f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.473033f; texpoints[k++] = 0.304722f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.427942f; texpoints[k++] = 0.304722f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.426479f; texpoints[k++] = 0.296460f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.423162f; texpoints[k++] = 0.288154f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.418309f; texpoints[k++] = 0.279937f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.390095f; texpoints[k++] = 0.360427f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.013954f; texpoints[k++] = 0.439966f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.499914f; texpoints[k++] = 0.419853f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.413200f; texpoints[k++] = 0.304600f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.409626f; texpoints[k++] = 0.298177f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.468080f; texpoints[k++] = 0.398465f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.422729f; texpoints[k++] = 0.414015f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.463080f; texpoints[k++] = 0.406216f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.372120f; texpoints[k++] = 0.526586f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.334562f; texpoints[k++] = 0.503927f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.411671f; texpoints[k++] = 0.453035f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.242176f; texpoints[k++] = 0.852324f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.290777f; texpoints[k++] = 0.798554f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.327338f; texpoints[k++] = 0.743473f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.399510f; texpoints[k++] = 0.251079f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.441728f; texpoints[k++] = 0.738324f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.429765f; texpoints[k++] = 0.812166f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.412198f; texpoints[k++] = 0.891099f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.288955f; texpoints[k++] = 0.601048f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.218937f; texpoints[k++] = 0.564589f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.412782f; texpoints[k++] = 0.601030f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.257135f; texpoints[k++] = 0.644560f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.427685f; texpoints[k++] = 0.562039f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.448340f; texpoints[k++] = 0.463064f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.178560f; texpoints[k++] = 0.542446f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.247308f; texpoints[k++] = 0.542806f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.286267f; texpoints[k++] = 0.532325f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.332828f; texpoints[k++] = 0.539288f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.368756f; texpoints[k++] = 0.552793f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.398964f; texpoints[k++] = 0.567345f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.476410f; texpoints[k++] = 0.594194f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.189241f; texpoints[k++] = 0.476076f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.228962f; texpoints[k++] = 0.651049f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.490726f; texpoints[k++] = 0.437599f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.404670f; texpoints[k++] = 0.514867f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.019469f; texpoints[k++] = 0.598436f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.426243f; texpoints[k++] = 0.579569f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.396993f; texpoints[k++] = 0.451203f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.266470f; texpoints[k++] = 0.623023f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.439121f; texpoints[k++] = 0.481042f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.032314f; texpoints[k++] = 0.355643f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.419054f; texpoints[k++] = 0.612845f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.462783f; texpoints[k++] = 0.494253f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.238979f; texpoints[k++] = 0.220255f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.198221f; texpoints[k++] = 0.168062f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.107550f; texpoints[k++] = 0.459245f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.183610f; texpoints[k++] = 0.259743f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.134410f; texpoints[k++] = 0.666317f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.385764f; texpoints[k++] = 0.116846f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.490967f; texpoints[k++] = 0.420622f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.382385f; texpoints[k++] = 0.491427f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.174399f; texpoints[k++] = 0.602329f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.318785f; texpoints[k++] = 0.603765f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.343364f; texpoints[k++] = 0.599403f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.396100f; texpoints[k++] = 0.289783f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.187885f; texpoints[k++] = 0.411462f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.430987f; texpoints[k++] = 0.055935f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.318993f; texpoints[k++] = 0.101715f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.266248f; texpoints[k++] = 0.130299f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.500023f; texpoints[k++] = 0.809424f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.499977f; texpoints[k++] = 0.045547f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.366170f; texpoints[k++] = 0.601178f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.393207f; texpoints[k++] = 0.604463f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.410373f; texpoints[k++] = 0.608920f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.194993f; texpoints[k++] = 0.657898f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.388665f; texpoints[k++] = 0.637716f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.365962f; texpoints[k++] = 0.644029f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.343364f; texpoints[k++] = 0.644643f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.318785f; texpoints[k++] = 0.641660f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.301415f; texpoints[k++] = 0.636844f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.058133f; texpoints[k++] = 0.680924f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.301415f; texpoints[k++] = 0.612551f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.499988f; texpoints[k++] = 0.381566f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.415838f; texpoints[k++] = 0.375804f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.445682f; texpoints[k++] = 0.433923f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.465844f; texpoints[k++] = 0.379359f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.499923f; texpoints[k++] = 0.648476f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.288719f; texpoints[k++] = 0.180054f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.335279f; texpoints[k++] = 0.147180f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.440512f; texpoints[k++] = 0.097581f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.128294f; texpoints[k++] = 0.208059f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.408772f; texpoints[k++] = 0.626106f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.455607f; texpoints[k++] = 0.548199f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.499877f; texpoints[k++] = 0.091010f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.375437f; texpoints[k++] = 0.075808f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.114210f; texpoints[k++] = 0.384978f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.448662f; texpoints[k++] = 0.304722f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.448020f; texpoints[k++] = 0.295368f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.447112f; texpoints[k++] = 0.284192f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.444832f; texpoints[k++] = 0.269206f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.430012f; texpoints[k++] = 0.233191f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.406787f; texpoints[k++] = 0.314327f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.400738f; texpoints[k++] = 0.318931f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.392400f; texpoints[k++] = 0.322297f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.367856f; texpoints[k++] = 0.336081f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.247923f; texpoints[k++] = 0.398667f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.452770f; texpoints[k++] = 0.579150f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.436392f; texpoints[k++] = 0.640113f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.416164f; texpoints[k++] = 0.631286f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.413386f; texpoints[k++] = 0.307634f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.228018f; texpoints[k++] = 0.316428f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.468268f; texpoints[k++] = 0.647329f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.411362f; texpoints[k++] = 0.195673f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.499989f; texpoints[k++] = 0.530175f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.479154f; texpoints[k++] = 0.557346f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.499974f; texpoints[k++] = 0.560363f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.432112f; texpoints[k++] = 0.506411f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.499886f; texpoints[k++] = 0.133083f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.499913f; texpoints[k++] = 0.178271f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.456549f; texpoints[k++] = 0.180799f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.344549f; texpoints[k++] = 0.254561f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.378909f; texpoints[k++] = 0.425990f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.374293f; texpoints[k++] = 0.219815f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.319688f; texpoints[k++] = 0.429262f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.357155f; texpoints[k++] = 0.395730f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.295284f; texpoints[k++] = 0.378419f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.447750f; texpoints[k++] = 0.137523f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.410986f; texpoints[k++] = 0.491277f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.313951f; texpoints[k++] = 0.224692f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.354128f; texpoints[k++] = 0.187447f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.324548f; texpoints[k++] = 0.296007f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.189096f; texpoints[k++] = 0.353700f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.279777f; texpoints[k++] = 0.285342f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.133823f; texpoints[k++] = 0.317299f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.336768f; texpoints[k++] = 0.355267f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.429884f; texpoints[k++] = 0.533478f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.455528f; texpoints[k++] = 0.451377f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.437114f; texpoints[k++] = 0.441104f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.467288f; texpoints[k++] = 0.470075f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.414712f; texpoints[k++] = 0.664780f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.377046f; texpoints[k++] = 0.677222f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.344108f; texpoints[k++] = 0.679849f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.312876f; texpoints[k++] = 0.677668f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.283526f; texpoints[k++] = 0.666810f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.241246f; texpoints[k++] = 0.617214f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.102986f; texpoints[k++] = 0.531237f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.267612f; texpoints[k++] = 0.575440f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.297879f; texpoints[k++] = 0.566824f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.333434f; texpoints[k++] = 0.566122f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.366427f; texpoints[k++] = 0.573884f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.396012f; texpoints[k++] = 0.583304f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.420121f; texpoints[k++] = 0.589772f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.007561f; texpoints[k++] = 0.519223f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.432949f; texpoints[k++] = 0.430482f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.458639f; texpoints[k++] = 0.520911f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.473466f; texpoints[k++] = 0.454256f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.476088f; texpoints[k++] = 0.436170f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.468472f; texpoints[k++] = 0.444943f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.433991f; texpoints[k++] = 0.417638f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.483518f; texpoints[k++] = 0.437016f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.482483f; texpoints[k++] = 0.422151f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.426450f; texpoints[k++] = 0.610201f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.438999f; texpoints[k++] = 0.603505f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.450067f; texpoints[k++] = 0.599566f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.289712f; texpoints[k++] = 0.631747f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.276670f; texpoints[k++] = 0.636627f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.517862f; texpoints[k++] = 0.528052f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.710288f; texpoints[k++] = 0.619236f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.526227f; texpoints[k++] = 0.426090f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.895093f; texpoints[k++] = 0.745859f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.634070f; texpoints[k++] = 0.590424f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.661242f; texpoints[k++] = 0.586975f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.688880f; texpoints[k++] = 0.590540f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.725342f; texpoints[k++] = 0.610869f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.606630f; texpoints[k++] = 0.596295f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.654766f; texpoints[k++] = 0.655989f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.629906f; texpoints[k++] = 0.653924f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.680678f; texpoints[k++] = 0.652735f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.702097f; texpoints[k++] = 0.646409f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.752212f; texpoints[k++] = 0.589195f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.602918f; texpoints[k++] = 0.157137f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.719902f; texpoints[k++] = 0.624400f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.893693f; texpoints[k++] = 0.600040f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.790082f; texpoints[k++] = 0.608646f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.643998f; texpoints[k++] = 0.465512f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.528249f; texpoints[k++] = 0.349596f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.525850f; texpoints[k++] = 0.319809f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.560215f; texpoints[k++] = 0.342771f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.585384f; texpoints[k++] = 0.333459f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.549626f; texpoints[k++] = 0.319139f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.571228f; texpoints[k++] = 0.317308f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.624852f; texpoints[k++] = 0.271901f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.513050f; texpoints[k++] = 0.452718f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.515097f; texpoints[k++] = 0.472748f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.742247f; texpoints[k++] = 0.685493f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.598631f; texpoints[k++] = 0.545021f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.570338f; texpoints[k++] = 0.451425f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.578632f; texpoints[k++] = 0.466377f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.723087f; texpoints[k++] = 0.467946f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.516446f; texpoints[k++] = 0.500361f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.662801f; texpoints[k++] = 0.717082f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.703624f; texpoints[k++] = 0.706729f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.830705f; texpoints[k++] = 0.806186f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.552386f; texpoints[k++] = 0.697432f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.607610f; texpoints[k++] = 0.646112f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.645429f; texpoints[k++] = 0.303293f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.932695f; texpoints[k++] = 0.269895f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.557261f; texpoints[k++] = 0.427174f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.542902f; texpoints[k++] = 0.415208f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.618026f; texpoints[k++] = 0.305289f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.607591f; texpoints[k++] = 0.305797f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.722943f; texpoints[k++] = 0.728037f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.577414f; texpoints[k++] = 0.436833f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.614083f; texpoints[k++] = 0.718613f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.616907f; texpoints[k++] = 0.744114f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.668509f; texpoints[k++] = 0.880086f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.770092f; texpoints[k++] = 0.767979f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.635536f; texpoints[k++] = 0.810751f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.770391f; texpoints[k++] = 0.700444f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.826722f; texpoints[k++] = 0.721245f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.527121f; texpoints[k++] = 0.333802f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.553172f; texpoints[k++] = 0.331473f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.577238f; texpoints[k++] = 0.326110f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.554692f; texpoints[k++] = 0.419934f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.611897f; texpoints[k++] = 0.306039f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.596961f; texpoints[k++] = 0.293460f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.596371f; texpoints[k++] = 0.306047f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.539958f; texpoints[k++] = 0.442861f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.568842f; texpoints[k++] = 0.307634f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.547818f; texpoints[k++] = 0.307634f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.524613f; texpoints[k++] = 0.307634f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.534090f; texpoints[k++] = 0.220859f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.527671f; texpoints[k++] = 0.263774f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.526913f; texpoints[k++] = 0.282143f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.526878f; texpoints[k++] = 0.295374f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.526967f; texpoints[k++] = 0.304722f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.572058f; texpoints[k++] = 0.304722f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.573521f; texpoints[k++] = 0.296460f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.576838f; texpoints[k++] = 0.288154f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.581691f; texpoints[k++] = 0.279937f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.609945f; texpoints[k++] = 0.360090f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.986046f; texpoints[k++] = 0.439966f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.586800f; texpoints[k++] = 0.304600f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.590372f; texpoints[k++] = 0.298177f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.531915f; texpoints[k++] = 0.398463f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.577268f; texpoints[k++] = 0.414065f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.536915f; texpoints[k++] = 0.406214f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.627543f; texpoints[k++] = 0.526648f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.665586f; texpoints[k++] = 0.504049f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.588354f; texpoints[k++] = 0.453138f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.757824f; texpoints[k++] = 0.852324f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.709250f; texpoints[k++] = 0.798492f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.672684f; texpoints[k++] = 0.743419f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.600409f; texpoints[k++] = 0.250995f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.558266f; texpoints[k++] = 0.738328f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.570304f; texpoints[k++] = 0.812129f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.588166f; texpoints[k++] = 0.890956f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.711045f; texpoints[k++] = 0.601048f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.781070f; texpoints[k++] = 0.564595f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.587247f; texpoints[k++] = 0.601068f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.742870f; texpoints[k++] = 0.644554f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.572156f; texpoints[k++] = 0.562348f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.551868f; texpoints[k++] = 0.463430f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.821442f; texpoints[k++] = 0.542444f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.752702f; texpoints[k++] = 0.542818f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.713757f; texpoints[k++] = 0.532373f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.667113f; texpoints[k++] = 0.539327f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.631101f; texpoints[k++] = 0.552846f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.600862f; texpoints[k++] = 0.567527f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.523481f; texpoints[k++] = 0.594373f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.810748f; texpoints[k++] = 0.476074f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.771046f; texpoints[k++] = 0.651041f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.509127f; texpoints[k++] = 0.437282f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.595293f; texpoints[k++] = 0.514976f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.980531f; texpoints[k++] = 0.598436f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.573500f; texpoints[k++] = 0.580000f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.602995f; texpoints[k++] = 0.451312f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.733530f; texpoints[k++] = 0.623023f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.560611f; texpoints[k++] = 0.480983f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.967686f; texpoints[k++] = 0.355643f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.580985f; texpoints[k++] = 0.612840f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.537728f; texpoints[k++] = 0.494615f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.760966f; texpoints[k++] = 0.220247f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.801779f; texpoints[k++] = 0.168062f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.892441f; texpoints[k++] = 0.459239f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.816351f; texpoints[k++] = 0.259740f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.865595f; texpoints[k++] = 0.666313f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.614074f; texpoints[k++] = 0.116754f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.508953f; texpoints[k++] = 0.420562f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.617942f; texpoints[k++] = 0.491684f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.825608f; texpoints[k++] = 0.602325f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.681215f; texpoints[k++] = 0.603765f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.656636f; texpoints[k++] = 0.599403f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.603900f; texpoints[k++] = 0.289783f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.812086f; texpoints[k++] = 0.411461f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.568013f; texpoints[k++] = 0.055435f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.681008f; texpoints[k++] = 0.101715f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.733752f; texpoints[k++] = 0.130299f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.633830f; texpoints[k++] = 0.601178f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.606793f; texpoints[k++] = 0.604463f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.589660f; texpoints[k++] = 0.608938f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.805016f; texpoints[k++] = 0.657892f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.611335f; texpoints[k++] = 0.637716f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.634038f; texpoints[k++] = 0.644029f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.656636f; texpoints[k++] = 0.644643f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.681215f; texpoints[k++] = 0.641660f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.698585f; texpoints[k++] = 0.636844f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.941867f; texpoints[k++] = 0.680924f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.698585f; texpoints[k++] = 0.612551f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.584177f; texpoints[k++] = 0.375893f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.554318f; texpoints[k++] = 0.433923f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.534154f; texpoints[k++] = 0.379360f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.711218f; texpoints[k++] = 0.180025f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.664630f; texpoints[k++] = 0.147129f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.559100f; texpoints[k++] = 0.097368f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.871706f; texpoints[k++] = 0.208059f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.591234f; texpoints[k++] = 0.626106f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.544341f; texpoints[k++] = 0.548416f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.624563f; texpoints[k++] = 0.075808f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.885770f; texpoints[k++] = 0.384971f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.551338f; texpoints[k++] = 0.304722f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.551980f; texpoints[k++] = 0.295368f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.552888f; texpoints[k++] = 0.284192f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.555168f; texpoints[k++] = 0.269206f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.569944f; texpoints[k++] = 0.232965f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.593203f; texpoints[k++] = 0.314324f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.599262f; texpoints[k++] = 0.318931f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.607600f; texpoints[k++] = 0.322297f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.631938f; texpoints[k++] = 0.336500f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.752033f; texpoints[k++] = 0.398685f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.547226f; texpoints[k++] = 0.579605f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.563544f; texpoints[k++] = 0.640172f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.583841f; texpoints[k++] = 0.631286f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.586614f; texpoints[k++] = 0.307634f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.771915f; texpoints[k++] = 0.316422f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.531597f; texpoints[k++] = 0.647517f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.588371f; texpoints[k++] = 0.195559f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.520797f; texpoints[k++] = 0.557435f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.567985f; texpoints[k++] = 0.506521f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.543283f; texpoints[k++] = 0.180745f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.655317f; texpoints[k++] = 0.254485f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.621009f; texpoints[k++] = 0.425982f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.625560f; texpoints[k++] = 0.219688f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.680198f; texpoints[k++] = 0.429281f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.642764f; texpoints[k++] = 0.395662f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.704663f; texpoints[k++] = 0.378470f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.552012f; texpoints[k++] = 0.137408f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.589072f; texpoints[k++] = 0.491363f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.685945f; texpoints[k++] = 0.224643f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.645735f; texpoints[k++] = 0.187360f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.675343f; texpoints[k++] = 0.296022f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.810858f; texpoints[k++] = 0.353695f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.720122f; texpoints[k++] = 0.285333f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.866152f; texpoints[k++] = 0.317295f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.663187f; texpoints[k++] = 0.355403f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.570082f; texpoints[k++] = 0.533674f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.544562f; texpoints[k++] = 0.451624f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.562759f; texpoints[k++] = 0.441215f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.531987f; texpoints[k++] = 0.469860f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.585271f; texpoints[k++] = 0.664823f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.622953f; texpoints[k++] = 0.677221f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.655896f; texpoints[k++] = 0.679837f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.687132f; texpoints[k++] = 0.677654f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.716482f; texpoints[k++] = 0.666799f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.758757f; texpoints[k++] = 0.617213f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.897013f; texpoints[k++] = 0.531231f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.732392f; texpoints[k++] = 0.575453f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.702114f; texpoints[k++] = 0.566837f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.666525f; texpoints[k++] = 0.566134f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.633505f; texpoints[k++] = 0.573912f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.603876f; texpoints[k++] = 0.583413f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.579658f; texpoints[k++] = 0.590055f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.992440f; texpoints[k++] = 0.519223f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.567192f; texpoints[k++] = 0.430580f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.541366f; texpoints[k++] = 0.521101f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.526564f; texpoints[k++] = 0.453882f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.523913f; texpoints[k++] = 0.436170f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.531529f; texpoints[k++] = 0.444943f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.566036f; texpoints[k++] = 0.417671f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.516311f; texpoints[k++] = 0.436946f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.517472f; texpoints[k++] = 0.422123f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.573595f; texpoints[k++] = 0.610193f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.560698f; texpoints[k++] = 0.604668f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.549756f; texpoints[k++] = 0.600249f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.710288f; texpoints[k++] = 0.631747f; k++; texpoints[k++] = 1.0f;
        texpoints[k++] = 0.723330f; texpoints[k++] = 0.636627f; k++; texpoints[k++] = 1.0f;
    }
    gs_texture_unmap(s->fd_points_texture);

    obs_leave_graphics();
    //debug("STAGING TEXTURE = %p", s->staging_texture);

    /** Configure networks **/
    face_detection_update(s);
}
//----------------------------------------------------------------------------------------------------------------------

void face_detection_render(face_detection_state *s, obs_source_t *target_source, effect_shader *main_shader) {
    UNUSED_PARAMETER(main_shader);
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
            gs_stagesurface_unmap(s->staging_texture);

            auto facemesh = s->facemesh;
            if (!facemesh)
                return;

            const size_t NB_ITERATIONS = 2;

            // Convert to BGR
            cv::Mat imageRGBA(FACEDETECTION_HEIGHT, FACEDETECTION_WIDTH, CV_8UC4, data);// = convertFrameToBGR(frame, tf);
            cv::Mat imageRGB = rgbaToRgb(imageRGBA);// = convertFrameToBGR(frame, tf);

            s->facelandmark_results_counter++;
            bool bDisplayResults = facemesh->Run(imageRGB, s->facelandmark_results[s->facelandmark_results_counter%NB_ITERATIONS]);

//            if (s->facelandmark_results_counter >= 4) {
//                s->facelandmark_results_counter = 0;
//            }

            if (s->facelandmark_results_counter <= NB_ITERATIONS || !bDisplayResults) {
                try_gs_effect_set_vec2(main_shader->param_fd_leye_1, &no_bounding_box.point1);
                try_gs_effect_set_vec2(main_shader->param_fd_leye_2, &no_bounding_box.point2);
                try_gs_effect_set_vec2(main_shader->param_fd_reye_1, &no_bounding_box.point1);
                try_gs_effect_set_vec2(main_shader->param_fd_reye_2, &no_bounding_box.point2);
                try_gs_effect_set_vec2(main_shader->param_fd_face_1, &no_bounding_box.point1);
                try_gs_effect_set_vec2(main_shader->param_fd_face_2, &no_bounding_box.point2);

                if (!facemesh->objects.empty())
                {
                    // TODO debug, to remove once the face landmark works
                    face_detection_bounding_box bbox{
                        facemesh->objects[0].center.x - facemesh->objects[0].width * 0.5f,
                        facemesh->objects[0].center.y - facemesh->objects[0].height * 0.5f,
                        facemesh->objects[0].center.x + facemesh->objects[0].width * 0.5f,
                        facemesh->objects[0].center.y + facemesh->objects[0].height * 0.5f
                    };

                    debug("bounding box: %f %f, %f %f", bbox.x1, bbox.y1, bbox.x2, bbox.y2);

                    try_gs_effect_set_vec2(main_shader->param_fd_face_1, &bbox.point1);
                    try_gs_effect_set_vec2(main_shader->param_fd_face_2, &bbox.point2);
                }
            }
            else {
                onnxmediapipe::FaceLandmarksResults average_results;
                average_results.facial_surface.resize(s->facelandmark_results[0].facial_surface.size());
                for (size_t i = 0; i < s->facelandmark_results[0].facial_surface.size(); ++i) {
                    average_results.facial_surface[i].x = 0.0;
                    average_results.facial_surface[i].y = 0.0;
                    average_results.facial_surface[i].z = 0.0;
                    size_t count = 0;
                    for (size_t j = 0; j < NB_ITERATIONS; ++j) {
                        if (i < s->facelandmark_results[j].facial_surface.size()) {
                            average_results.facial_surface[i] += s->facelandmark_results[j].facial_surface[i];
                            ++count;
                        }
                    }
                    average_results.facial_surface[i] /= (float) count;
                }
                average_results.refined_landmarks.resize(s->facelandmark_results[0].refined_landmarks.size());
                for (size_t i = 0; i < s->facelandmark_results[0].refined_landmarks.size(); ++i) {
                    average_results.refined_landmarks[i].x = 0.0;
                    average_results.refined_landmarks[i].y = 0.0;
                    average_results.refined_landmarks[i].z = 0.0;
                    size_t count = 0;
                    for (size_t j = 0; j < NB_ITERATIONS; ++j) {
                        if (i < s->facelandmark_results[j].refined_landmarks.size()) {
                            average_results.refined_landmarks[i] += s->facelandmark_results[j].refined_landmarks[i];
                            ++count;
                        }
                    }
                    average_results.refined_landmarks[i] /= (float) count;
                }

                {
                    auto bbox = face_detection_get_bounding_box(&average_results, left_iris_refinement_indices, left_iris_refined_region_num_points);
                    try_gs_effect_set_vec2(main_shader->param_fd_leye_1, &bbox.point1);
                    try_gs_effect_set_vec2(main_shader->param_fd_leye_2, &bbox.point2);
                    //debug("Left Eye: %f %f %f %f", bbox.x1, bbox.y1, bbox.x2-bbox.x1, bbox.y2-bbox.y1);
                }
                {
                    auto bbox = face_detection_get_bounding_box(&average_results, right_iris_refinement_indices, right_iris_refined_region_num_points);
                    try_gs_effect_set_vec2(main_shader->param_fd_reye_1, &bbox.point1);
                    try_gs_effect_set_vec2(main_shader->param_fd_reye_2, &bbox.point2);
                    //debug("Right Eye: %f %f %f %f", bbox.x1, bbox.y1, bbox.x2-bbox.x1, bbox.y2-bbox.y1);
                }
                {
                    auto bbox = face_detection_get_bounding_box(&average_results, not_lips_eyes_indices, 310);
                    try_gs_effect_set_vec2(main_shader->param_fd_face_1, &bbox.point1);
                    try_gs_effect_set_vec2(main_shader->param_fd_face_2, &bbox.point2);
                    //debug("Face: %f %f %f %f", bbox.x1, bbox.y1, bbox.x2-bbox.x1, bbox.y2-bbox.y1);
                }
                float points[468 * 4];
                face_detection_copy_points(&average_results, points);

                float *texpoints;
                uint32_t linesize2 = 0;
                obs_enter_graphics();
                {
                    gs_texture_map(s->fd_points_texture, (uint8_t **) (&texpoints), &linesize2);

//                    for (int i=0; i<468*2; ++i) {
////                        texpoints[i] = (uint16_t)(((float)0xffff) * points[i]);
////                        texpoints[468+i] = (uint16_t)(((float)0xffff) * points[i]);
//                        texpoints[i] = points[i];
//                        //texpoints[468+i] = points[i];
//                    }
                    memcpy(texpoints, points, 468 * 4 * sizeof(float));
                    gs_texture_unmap(s->fd_points_texture);
                }
                obs_leave_graphics();
                try_gs_effect_set_texture(main_shader->param_fd_points_tex, s->fd_points_texture);
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
