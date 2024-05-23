// Copyright(C) 2022-2023 Intel Corporation
// SPDX - License - Identifier: Apache - 2.0
#pragma once

#include <opencv2/core.hpp>
#include <onnxruntime_cxx_api.h>
#include "onnxmediapipe/common.h"

namespace onnxmediapipe
{
    class FaceDetection;
    class FaceLandmarks;

    class FaceMesh
    {
    public:
        FaceMesh(
            std::unique_ptr<Ort::Env> &ort_env
        );

        // Given a BGR frame, generate landmarks.
        bool Run(const cv::Mat& frameRGB, FaceLandmarksResults& results);

        // TODO make private
        std::shared_ptr < FaceDetection > _facedetection;
        RotatedRect _tracked_roi = {};
        std::vector<DetectedObject> objects;

    private:

        std::shared_ptr < FaceLandmarks > _facelandmarks;

        bool _bNeedsDetection = true;

        //static ov::Core core;
    };

} //namespace ovfacemesh
