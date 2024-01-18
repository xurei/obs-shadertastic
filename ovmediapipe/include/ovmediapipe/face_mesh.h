// Copyright(C) 2022-2023 Intel Corporation
// SPDX - License - Identifier: Apache - 2.0
#pragma once

#include <openvino/openvino.hpp>
#include <opencv2/core.hpp>
#include "ovmediapipe/common.h"

namespace ovmediapipe
{
    class FaceDetection;
    class FaceLandmarks;

    class FaceMesh
    {
    public:

        FaceMesh(std::string detection_model_xml_path, std::string detection_device,
            std::string landmarks_model_xml_path, std::string landmarks_device);

        // Given a BGR frame, generate landmarks.
        bool Run(const cv::Mat& frameBGR, FaceLandmarksResults& results);

    private:

        std::shared_ptr < FaceDetection > _facedetection;
        std::shared_ptr < FaceLandmarks > _facelandmarks;

        bool _bNeedsDetection = true;

        RotatedRect _tracked_roi = {};

        static ov::Core core;
    };

} //namespace ovfacemesh