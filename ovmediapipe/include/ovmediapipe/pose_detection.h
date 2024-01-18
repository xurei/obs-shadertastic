// Copyright(C) 2022-2023 Intel Corporation
// SPDX - License - Identifier: Apache - 2.0
#pragma once

#include <opencv2/core.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <openvino/openvino.hpp>
#include <string>
#include <vector>
#include "ovmediapipe/common.h"
#include "ovmediapipe/ssd_anchors.h"

namespace ovmediapipe
{
    class PoseDetection
    {

    public:

        PoseDetection(std::string model_xml_path, std::string device, ov::Core& core);

        void Run(const cv::Mat& frameBGR, std::vector<DetectedObject>& results);

    private:

        void preprocess(const cv::Mat& frameBGR, std::array<float, 16>& transform_matrix);
        void postprocess(const cv::Mat& frameBGR, std::array<float, 16>& transform_matrix, std::vector<DetectedObject>& results);

        std::vector<Anchor> anchors;
        std::vector<std::string> inputsNames;
        std::vector<std::string> outputsNames;

        ov::InferRequest inferRequest;
        ov::CompiledModel compiledModel;

        size_t netInputHeight = 0;
        size_t netInputWidth = 0;

        int CLASSIFICATORS = 0;
        int REGRESSORS = 1;

        size_t maxProposalCount = 0;
        size_t classificatorsObjectSize;
        size_t regressorsObjectSize;

    };

} //ovfacemesh