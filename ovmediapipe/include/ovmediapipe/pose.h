// Copyright(C) 2022-2023 Intel Corporation
// SPDX - License - Identifier: Apache - 2.0
#pragma once

#include <openvino/openvino.hpp>
#include <opencv2/core.hpp>
#include "ovmediapipe/common.h"

//todo: our namespace should change to ovmediapipe or something.
namespace ovmediapipe
{
    class PoseDetection;
    class PoseLandmark;

    class Pose
    {
    public:

        Pose(std::string detection_model_xml_path, std::string detection_device,
            std::string landmark_model_xml_path, std::string landmark_device);

        // Given a BGR frame, generate landmarks.
        bool Run(const cv::Mat& frameBGR, PoseLandmarkResult& results, std::shared_ptr<cv::Mat> seg_mask = {});

    private:

        std::shared_ptr < PoseDetection > _posedetection;
        std::shared_ptr < PoseLandmark > _poselandmark;

        bool _bNeedsDetection = true;

        RotatedRect _tracked_roi = {};

        //todo: move someplace outside this class. If someone uses both Pose and FaceMesh
        // pipelines, we want to use a common core object.
        static ov::Core core;
    };

} //namespace ovfacemesh