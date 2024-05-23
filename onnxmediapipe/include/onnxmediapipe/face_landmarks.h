// Copyright(C) 2022-2023 Intel Corporation
// SPDX - License - Identifier: Apache - 2.0
#include <opencv2/imgproc.hpp>
#include <string>
#include <vector>
#include <onnxruntime_cxx_api.h>
#include "onnxmediapipe/common.h"

namespace onnxmediapipe
{
    class FaceLandmarks {

    public:
        const int nFacialSurfaceLandmarks = 468;

        explicit FaceLandmarks(std::unique_ptr<Ort::Env> &ort_env);

        void Run(const cv::Mat& frameRGB, const RotatedRect& roi, FaceLandmarksResults& results);

    private:
        void preprocess(const cv::Mat& frameRGB, const RotatedRect& roi);
        void postprocess(const cv::Mat& frameRGB, const RotatedRect& roi, FaceLandmarksResults& results);

        std::shared_ptr<Ort::Session> ortSession;
        size_t netInputHeight = 0;
        size_t netInputWidth = 0;

        size_t inputCount;
        size_t outputCount;
        std::vector<const char *> inputNames;
        std::vector<const char *> outputNames;

        std::vector<std::vector<float>> inputTensorValues;
        std::vector<std::vector<float>> outputTensorValues;
        std::vector<Ort::Value> inputTensors;
        std::vector<Ort::Value> outputTensors;

//        ov::CompiledModel compiledModel;
//        ov::InferRequest inferRequest;

        bool _bWithAttention = true;

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
