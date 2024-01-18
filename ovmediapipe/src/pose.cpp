// Copyright(C) 2022-2023 Intel Corporation
// SPDX - License - Identifier: Apache - 2.0
#include "ovmediapipe/pose.h"
#include "ovmediapipe/pose_detection.h"
#include "ovmediapipe/pose_landmark.h"

namespace ovmediapipe
{
    ov::Core Pose::core = ov::Core();

    Pose::Pose(std::string detection_model_xml_path, std::string detection_device,
        std::string landmark_model_xml_path, std::string landmark_device)
        : _posedetection(std::make_shared < PoseDetection >(detection_model_xml_path, detection_device, core)),
        _poselandmark(std::make_shared < PoseLandmark >(landmark_model_xml_path, landmark_device, core)) {}

    bool Pose::Run(const cv::Mat& frameBGR, PoseLandmarkResult& results, std::shared_ptr<cv::Mat> seg_mask)
    {
        if (_bNeedsDetection)
        {
            std::vector<DetectedObject> objects;
            _posedetection->Run(frameBGR, objects);

            if (objects.empty())
                return false;

            _tracked_roi = { objects[0].center.x * frameBGR.cols,
                             objects[0].center.y * frameBGR.rows,
                             objects[0].width * frameBGR.cols,
                             objects[0].height * frameBGR.rows, objects[0].rotation
            };
        }

        _poselandmark->Run(frameBGR, _tracked_roi, results, seg_mask);

        _tracked_roi = results.roi;
        _tracked_roi.center_x *= frameBGR.cols;
        _tracked_roi.center_y *= frameBGR.rows;
        _tracked_roi.width *= frameBGR.cols;
        _tracked_roi.height *= frameBGR.rows;

        //std::cout << "pose_flag = " << results.pose_flag << std::endl;

        _bNeedsDetection = (results.pose_flag < 0.5f);

        return !_bNeedsDetection;
    }

} //namespace ovmediapipe