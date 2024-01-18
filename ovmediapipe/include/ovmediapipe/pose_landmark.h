// Copyright(C) 2022-2023 Intel Corporation
// SPDX - License - Identifier: Apache - 2.0
#include <openvino/openvino.hpp>
#include <opencv2/imgproc.hpp>
#include <string>
#include <vector>
#include "ovmediapipe/common.h"

namespace ovmediapipe
{
    class LandmarkSmoother;
    class PoseLandmark
    {

    public:

        PoseLandmark(std::string model_xml_path, std::string device, ov::Core& core);

        void Run(const cv::Mat& frameBGR, const RotatedRect& roi, PoseLandmarkResult& results, std::shared_ptr<cv::Mat> seg_mask = {});

    private:

        void preprocess(const cv::Mat& frameBGR, const RotatedRect& roi, std::array<float, 16>& transform_matrix);
        void postprocess(const cv::Mat& frameBGR, const RotatedRect& roi, std::array<float, 16>& transform_matrix, PoseLandmarkResult& results, std::shared_ptr<cv::Mat> seg_mask);

        size_t netInputHeight = 0;
        size_t netInputWidth = 0;

        std::vector<std::string> inputsNames;
        std::vector<std::string> outputsNames;

        ov::CompiledModel compiledModel;
        ov::InferRequest inferRequest;

        std::string keypoint_tensor_name;
        std::string pose_flag_tensor_name;
        std::string segmentation_mask_tensor_name;
        std::string heatmap_tensor_name;
        std::string world_landmarks_tensor_name;

        std::shared_ptr< LandmarkSmoother > _landmark_smoother;
        std::shared_ptr< LandmarkSmoother > _landmark_aux_smoother;

        bool _bHeatmapIsNCHW = false;

        bool _bPrevSegmaskValid = false;
        cv::Mat _prevSegMask;
    };

} //ovfacemesh