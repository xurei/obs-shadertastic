// Copyright(C) 2022-2023 Intel Corporation
// SPDX - License - Identifier: Apache - 2.0
#pragma once

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <string>
#include <vector>
#include <onnxruntime_cxx_api.h>
#include "onnxmediapipe/common.h"
#include "onnxmediapipe/ssd_anchors.h"

namespace onnxmediapipe {
    class FaceDetection {

    public:
        explicit FaceDetection(std::unique_ptr<Ort::Env> &ort_env, float bbox_scale = 1.5f);

        void Run(const cv::Mat& frameRGB, std::vector<DetectedObject>& results);

    //TODO make private
    //private:
        void preprocess(const cv::Mat& frameRGB, std::array<float, 16>& transform_matrix);
        void postprocess(const cv::Mat& frameRGB, std::array<float, 16>& transform_matrix, std::vector<DetectedObject>& results);
        void resizeWithAspectRatio(const cv::Mat& inputImage, const cv::Mat &outputImage, cv::Size size);

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

        std::vector<Anchor> anchors;

        int REGRESSORS = 0;
        int CLASSIFICATORS = 1;

        size_t maxProposalCount = 0;

        float face_bbox_scale;
    };
} //ovfacemesh
