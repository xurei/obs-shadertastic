// Copyright(C) 2022-2023 Intel Corporation
// SPDX - License - Identifier: Apache - 2.0
#include <openvino/openvino.hpp>
#include <opencv2/imgproc.hpp>
#include <string>
#include <vector>
#include "ovmediapipe/common.h"

namespace ovmediapipe
{
    class FaceLandmarks
    {

    public:

        const int nFacialSurfaceLandmarks = 468;

        FaceLandmarks(std::string model_xml_path, std::string device, ov::Core& core);

        void Run(const cv::Mat& frameBGR, const RotatedRect& roi, FaceLandmarksResults& results);

    private:

        void preprocess(const cv::Mat& frameBGR, const RotatedRect& roi);
        void postprocess(const cv::Mat& frameBGR, const RotatedRect& roi, FaceLandmarksResults& results);

        size_t netInputHeight = 0;
        size_t netInputWidth = 0;

        std::vector<std::string> inputsNames;
        std::vector<std::string> outputsNames;

        ov::CompiledModel compiledModel;
        ov::InferRequest inferRequest;

        bool _bWithAttention = false;

        //these first 2 should exist for both versions
        // of face mesh (with/without attention)
        std::string facial_surface_tensor_name;
        std::string face_flag_tensor_name;

        //only face mesh with attention uses these 5.
        std::string lips_refined_tensor_name;
        std::string left_eye_with_eyebrow_tensor_name;
        std::string right_eye_with_eyebrow_tensor_name;
        std::string left_iris_refined_tensor_name;
        std::string right_iris_refined_tensor_name;
    };

} //ovfacemesh