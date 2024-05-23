// Copyright(C) 2022-2023 Intel Corporation
// SPDX - License - Identifier: Apache - 2.0

// NOTE : this file has been taken from https://github.com/intel/openvino-plugins-for-obs-studio and modified to use ONNX instead
#include "onnxmediapipe/face_mesh.h"
#include "onnxmediapipe/face_detection.h"
#include "onnxmediapipe/face_landmarks.h"
#include <obs-module.h>

#define do_log(level, format, ...) \
    blog(level, "[shadertastic] " format, ##__VA_ARGS__)
#define info(format, ...) do_log(LOG_INFO, format, ##__VA_ARGS__)
#define warn(format, ...) do_log(LOG_WARNING, format, ##__VA_ARGS__)

#ifdef DEV_MODE
    #define debug(format, ...) info("(debug) " #format, ##__VA_ARGS__)
#else
    #define debug(format, ...)
#endif

namespace onnxmediapipe
{
    FaceMesh::FaceMesh(
        std::unique_ptr<Ort::Env> &ort_env
    ) : _facedetection(std::make_shared<FaceDetection>(ort_env)),
        _facelandmarks(std::make_shared<FaceLandmarks>(ort_env)) {}

    bool FaceMesh::Run(const cv::Mat& frameRGB, FaceLandmarksResults& results)
    {
        if (_bNeedsDetection) {
            objects.clear();
            _facedetection->Run(frameRGB, objects);

            if (objects.empty()) {
                return false;
            }

            _tracked_roi = {
                objects[0].center.x * (float) frameRGB.cols,
                objects[0].center.y * (float) frameRGB.rows,
                objects[0].width * (float) frameRGB.cols,
                objects[0].height * (float) frameRGB.rows, objects[0].rotation
            };
        }

        _facelandmarks->Run(frameRGB, _tracked_roi, results);

        _tracked_roi = results.roi;
        _tracked_roi.center_x *= (float)frameRGB.cols;
        _tracked_roi.center_y *= (float)frameRGB.rows;
        _tracked_roi.width *= (float)frameRGB.cols;
        _tracked_roi.height *= (float)frameRGB.rows;

        _bNeedsDetection = (results.face_flag < 0.5f);

        return !_bNeedsDetection;
    }

} //namespace onnxmediapipe
