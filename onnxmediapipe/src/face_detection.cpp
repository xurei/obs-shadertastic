/******************************************************************************
    Copyright (C) 2023 by xurei <xureilab@gmail.com>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/

// NOTE : this file has been taken from https://github.com/intel/openvino-plugins-for-obs-studio and modified to use ONNX instead

#include <memory>
#include "onnxmediapipe/face_detection.h"
#include <obs-module.h>
#include <cstdint>
#include <opencv2/imgcodecs.hpp>

#include "../../src/logging_functions.hpp"

#include "../../src/fdebug.h"

FILE* faceDetectionDebugFile;

namespace onnxmediapipe
{
    FaceDetection::FaceDetection(std::unique_ptr<Ort::Env> &ort_env, float bbox_scale)
    : face_bbox_scale(bbox_scale) {
        if (!ortSession) {
            Ort::SessionOptions sessionOptions;
            sessionOptions.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
//            sessionOptions.SetInterOpNumThreads(2);
//            sessionOptions.SetIntraOpNumThreads(2);
//            sessionOptions.DisableMemPattern();
            sessionOptions.DisablePerSessionThreads();
            sessionOptions.SetExecutionMode(ExecutionMode::ORT_SEQUENTIAL);
            char *model_data_path = obs_module_file("face_detection_models/face_detection_full_range.onnx");
            //const char *model_data_path = obs_module_file("face_detection_models/blazeface.onnx");

            if (model_data_path) {
                #if defined(_WIN32)
                    std::string model_data_path_ = std::string(model_data_path);
                    std::wstring model_data_path__ = std::wstring(model_data_path_.begin(), model_data_path_.end());
                    info("MODEL DATA PATH: %s", model_data_path_.c_str());
                    info("MODEL DATA PATH: %s", model_data_path_.c_str());
                    info("MODEL DATA PATH: %s", model_data_path_.c_str());
                    ortSession = std::make_shared<Ort::Session>(*ort_env, (const ORTCHAR_T*)(model_data_path__.c_str()), sessionOptions);
                #else
                    ortSession = std::make_shared<Ort::Session>(*ort_env, model_data_path, sessionOptions);
                #endif
                bfree(model_data_path);
                debug("FACE_DETECTION Loading model %s", model_data_path);
            }
        }
        if (!ortSession) {
            return;
        }

        // Prepare inputs / outputs
        {
            inputCount = ortSession->GetInputCount();
            outputCount = ortSession->GetOutputCount();
            debug("FACE_DETECTION Inputs: %lu", inputCount);
            debug("FACE_DETECTION Outputs: %lu", outputCount);
            if (inputCount != 1) {
                throw std::logic_error("FaceDetection model topology should have only 1 input");
            }

            Ort::AllocatorWithDefaultOptions allocator;
            Ort::MemoryInfo memoryInfo = Ort::MemoryInfo::CreateCpu(OrtAllocatorType::OrtArenaAllocator, OrtMemType::OrtMemTypeDefault);

            std::vector<std::vector<int64_t>> inputShapes(inputCount);
            for (size_t i = 0; i < inputCount; ++i) {
                std::string inputName = ortSession->GetInputNameAllocated(i, allocator).get();
                char *inputNameCStr = new char[inputName.length()];
                strcpy(inputNameCStr, inputName.c_str());
                inputNames.push_back(inputNameCStr);
                debug("FACE_DETECTION Input Name %lu: %s", i, inputNameCStr);
                std::vector<int64_t> inputShape = ortSession->GetInputTypeInfo(i).GetTensorTypeAndShapeInfo().GetShape();
                auto elementType = ortSession->GetInputTypeInfo(i).GetTensorTypeAndShapeInfo().GetElementType();
                inputShapes[i] = inputShape;
                debug("FACE_DETECTION Input Shape %lu: %s", i, printShape(inputShape).c_str());
                debug("FACE_DETECTION Input Type %lu: %s", i, printElementType(elementType));
                int64_t inputTensorSize = vectorProduct(inputShape);
                inputTensorValues.push_back(std::vector<float>((size_t)inputTensorSize));

                inputTensors.push_back(Ort::Value::CreateTensor<float>(
                    memoryInfo,
                    inputTensorValues[i].data(), (size_t)inputTensorSize,
                    inputShape.data(), inputShape.size()
                ));
            }

            std::vector<std::vector<int64_t>> outputShapes(outputCount);
            for (size_t i = 0; i < outputCount; ++i) {
                std::string outputName = ortSession->GetOutputNameAllocated(i, allocator).get();
                char *outputNameCStr = new char[outputName.length()];
                strcpy(outputNameCStr, outputName.c_str());
                outputNames.push_back(outputNameCStr);
                debug("FACE_DETECTION Output Name %lu: %s", i, outputNameCStr);
                std::vector<int64_t> outputShape = ortSession->GetOutputTypeInfo(i).GetTensorTypeAndShapeInfo().GetShape();
                outputShapes[i] = outputShape;
                debug("FACE_DETECTION Output Shape %lu: %s", i, printShape(outputShape).c_str());
                size_t outputTensorSize = vectorProduct(outputShape);
                outputTensorValues.push_back(std::vector<float>(outputTensorSize));

                outputTensors.push_back(Ort::Value::CreateTensor<float>(
                    memoryInfo,
                    outputTensorValues[i].data(), outputTensorSize,
                    outputShape.data(), outputShape.size()
                ));
            }
            maxProposalCount = static_cast<int>(outputShapes[REGRESSORS][1]);
            debug("FACE_DETECTION maxProposalCount: %lu", maxProposalCount);

            netInputHeight = inputShapes[0][1];
            netInputWidth = inputShapes[0][2];
        }

        // Anchors generation
        //https://github.com/google/mediapipe/blob/master/mediapipe/modules/face_detection/face_detection.pbtxt
        {
            SsdAnchorsCalculatorOptions ssdAnchorsCalculatorOptions;
            ssdAnchorsCalculatorOptions.input_size_height = (int32_t) netInputHeight;
            ssdAnchorsCalculatorOptions.input_size_width = (int32_t) netInputWidth;
            ssdAnchorsCalculatorOptions.min_scale = 0.1484375;
            ssdAnchorsCalculatorOptions.max_scale = 0.75;
            ssdAnchorsCalculatorOptions.anchor_offset_x = 0.5;
            ssdAnchorsCalculatorOptions.anchor_offset_y = 0.5;
            ssdAnchorsCalculatorOptions.aspect_ratios = { 1.0 };
            ssdAnchorsCalculatorOptions.fixed_anchor_size = true;

            //192x192 implies 'full range' face detection.
            if ((netInputHeight == 192) && (netInputWidth == 192))
            {
                //https://github.com/google/mediapipe/blob/master/mediapipe/modules/face_detection/face_detection_full_range.pbtxt
                ssdAnchorsCalculatorOptions.num_layers = 1;
                ssdAnchorsCalculatorOptions.strides = { 4 };
                ssdAnchorsCalculatorOptions.interpolated_scale_aspect_ratio = 0.0;
            }
            else
            {
                //https://github.com/google/mediapipe/blob/master/mediapipe/modules/face_detection/face_detection_short_range.pbtxt
                ssdAnchorsCalculatorOptions.num_layers = 4;
                ssdAnchorsCalculatorOptions.strides = { 8, 16, 16, 16 };
                ssdAnchorsCalculatorOptions.interpolated_scale_aspect_ratio = 1.0;
            }

            anchors.clear();
            SsdAnchorsCalculator::GenerateAnchors(anchors, ssdAnchorsCalculatorOptions);
        }
        debug("FACE_DETECTION initialization done !");
    }

    void FaceDetection::Run(const cv::Mat& frameRGB, std::vector<DetectedObject>& results)
    {
        if (!ortSession) {
            return;
        }
        faceDetectionDebugFile = fdebug_open("face_detection.txt");

        std::array<float, 16> transform_matrix;  // NOLINT(*-pro-type-member-init)
        preprocess(frameRGB, transform_matrix);

        // perform inference
        /* To run inference, we provide the run options, an array of input names corresponding to the
        inputs in the input tensor, an array of input tensor, number of inputs, an array of output names
        corresponding to the outputs in the output tensor, an array of output tensor, number of outputs. */
        ortSession->Run(Ort::RunOptions{nullptr},
            inputNames.data(), inputTensors.data(), inputCount,
            outputNames.data(), outputTensors.data(), outputCount);

        postprocess(frameRGB, transform_matrix, results);
        fdebug_close(faceDetectionDebugFile);
    }

    void FaceDetection::preprocess(const cv::Mat& img, std::array<float, 16>& transform_matrix)
    {
        RotatedRect roi = {/*center_x=*/0.5f * (float)img.cols,
            /*center_y =*/0.5f * (float)img.rows,
            /*width =*/static_cast<float>(img.cols),
            /*height =*/static_cast<float>(img.rows),
            /*rotation =*/0
        };

        //pad the roi
        {
            const float tensor_aspect_ratio =
                static_cast<float>(netInputHeight) / static_cast<float>(netInputWidth);

            const float roi_aspect_ratio = (float)img.rows / (float)img.cols;
            float new_width;
            float new_height;
            if (tensor_aspect_ratio > roi_aspect_ratio) {
                new_width = (float)img.cols;
                new_height = (float)img.cols * tensor_aspect_ratio;
            }
            else {
                new_width = (float)img.rows / tensor_aspect_ratio;
                new_height = (float)img.rows;
            }

            roi.width = new_width;
            roi.height = new_height;
        }

        GetRotatedSubRectToRectTransformMatrix(roi, img.cols, img.rows, false, &transform_matrix);

        const cv::RotatedRect rotated_rect(cv::Point2f(roi.center_x, roi.center_y),
            cv::Size2f(roi.width, roi.height),
            (float)(roi.rotation * 180.f / M_PI));

        cv::Mat src_points;
        cv::boxPoints(rotated_rect, src_points);

        const float dst_width = (float)netInputWidth;
        const float dst_height = (float)netInputHeight;

        /* clang-format off */
        float dst_corners[8] = { 0.0f,      dst_height,
                                 0.0f,      0.0f,
                                 dst_width, 0.0f,
                                 dst_width, dst_height };
        /* clang-format on */

        cv::Mat dst_points = cv::Mat(4, 2, CV_32F, dst_corners);
        cv::Mat projection_matrix = cv::getPerspectiveTransform(src_points, dst_points);

        //get the input tensor as an array
        //const ov::Tensor& frameTensor = inferRequest.get_tensor(inputsNames[0]);  // first input should be image

                //frameTensor.data<uint8_t>();

        //Scale down image
        cv::Mat scaledDown((int)netInputWidth, (int)netInputHeight, CV_8UC3, cv::Scalar(0, 0, 0));
        resizeWithAspectRatio(img, scaledDown, cv::Size((int)netInputWidth, (int)netInputHeight));

        // Wrap the already-allocated tensor as a cv::Mat of floats
        float* pTensor = inputTensorValues[0].data();
        cv::Mat transformed = cv::Mat((int)netInputHeight, (int)netInputWidth, CV_32FC3, pTensor);
        scaledDown.convertTo(transformed, CV_32FC3);
        transformed /= 127.5f;
        transformed -= 1.0f;
    }

    // Function to resize an image while preserving aspect ratio and adding black bands
    void FaceDetection::resizeWithAspectRatio(const cv::Mat& inputImage, const cv::Mat &outputImage, cv::Size size) {
        // Get the dimensions of the input image
        int inputWidth = inputImage.cols;
        int inputHeight = inputImage.rows;

        // Calculate the aspect ratios of the input and target sizes
        double inputAspectRatio = static_cast<double>(inputWidth) / inputHeight;
        double targetAspectRatio = static_cast<double>(size.width) / size.height;

        // Initialize the output image
        //cv::Mat outputImage(newHeight, newWidth, inputImage.type(), cv::Scalar(0, 0, 0));

        // Calculate the scale factor to fit the image into the target size while preserving aspect ratio
        double scaleFactor = (targetAspectRatio > inputAspectRatio) ?
                             static_cast<double>(size.height) / inputHeight :
                             static_cast<double>(size.width) / inputWidth;

        // Calculate the resized dimensions
        int resizedWidth = static_cast<int>(inputWidth * scaleFactor);
        int resizedHeight = static_cast<int>(inputHeight * scaleFactor);

        // Calculate the position to paste the resized image (centering)
        int startX = (size.width - resizedWidth) / 2;
        int startY = (size.height - resizedHeight) / 2;

        // Resize the input image while preserving aspect ratio
        cv::resize(inputImage, outputImage(cv::Rect(startX, startY, resizedWidth, resizedHeight)), cv::Size(resizedWidth, resizedHeight));
    }

    static void DecodeBoxes(const float* raw_boxes, const std::vector<Anchor>& anchors, std::vector<float>* boxes, size_t num_boxes_, size_t num_coords_, int netWidth, int netHeight)
    {
//        debug("DecodeBoxes");
        int box_coord_offset = 0;
        const float x_scale = (float)netWidth;
        const float y_scale = (float)netHeight;
        const float h_scale = (float)netWidth;
        const float w_scale = (float)netHeight;

        const size_t num_keypoints = 6;
        const int keypoint_coord_offset = 4;
        const int num_values_per_keypoint = 2;

        for (size_t i = 0; i < num_boxes_; ++i) {
//            debug("DecodeBoxes %lu", i);
            const int box_offset = (int)(i * num_coords_ + box_coord_offset);
//            debug("DecodeBoxes %lu b", i);

            float x_center = raw_boxes[box_offset];
            float y_center = raw_boxes[box_offset + 1];
            float w = raw_boxes[box_offset + 2];
            float h = raw_boxes[box_offset + 3];

            x_center = x_center / x_scale * anchors[i].w + anchors[i].x_center;
            y_center = y_center / y_scale * anchors[i].h + anchors[i].y_center;

            h = h / h_scale * anchors[i].h;
            w = w / w_scale * anchors[i].w;

//            debug("DecodeBoxes %lu d", i);

            const float ymin = y_center - h / 2.f;
            const float xmin = x_center - w / 2.f;
            const float ymax = y_center + h / 2.f;
            const float xmax = x_center + w / 2.f;

//            debug("DecodeBoxes %lu e", i);

            (*boxes)[i * num_coords_ + 0] = ymin;
            (*boxes)[i * num_coords_ + 1] = xmin;
            (*boxes)[i * num_coords_ + 2] = ymax;
            (*boxes)[i * num_coords_ + 3] = xmax;

//            debug("DecodeBoxes %lu f", i);

            for (size_t k = 0; k < num_keypoints; ++k) {
//                debug("DecodeBoxes %lu %lu", i, k);
                const int offset = (int)(i * num_coords_ + keypoint_coord_offset + k * num_values_per_keypoint);

                float keypoint_x = raw_boxes[offset];
                float keypoint_y = raw_boxes[offset + 1];

                (*boxes)[offset    ] = keypoint_x / x_scale * anchors[i].w + anchors[i].x_center;
                (*boxes)[offset + 1] = keypoint_y / y_scale * anchors[i].h + anchors[i].y_center;
            }
        }
    }

    static DetectedObject ConvertToDetection(float box_ymin, float box_xmin, float box_ymax, float box_xmax, float score, int class_id)
    {
        DetectedObject detection;
        detection.confidence = score;
        detection.labelID = class_id;

        detection.x = box_xmin;
        detection.y = box_ymin;
        detection.width = box_xmax - box_xmin;
        detection.height = box_ymax - box_ymin;
        return detection;
    }

    static bool SortBySecond(const std::pair<int, float>& indexed_score_0,
        const std::pair<int, float>& indexed_score_1) {
        return (indexed_score_0.second > indexed_score_1.second);
    }

    static void NMS(std::vector<DetectedObject>& detections)
    {
        float min_suppression_threshold = 0.3f;
        fdebug(faceDetectionDebugFile, "NMS %lu", detections.size());

        std::vector<std::pair<int, float>> indexed_scores;
        indexed_scores.reserve(detections.size());

        for (size_t index = 0; index < detections.size(); ++index) {
            fdebug(faceDetectionDebugFile, "%lu confidence: %f", index, detections[index].confidence);
            indexed_scores.push_back(std::make_pair((int)index, detections[index].confidence));
        }
        std::sort(indexed_scores.begin(), indexed_scores.end(), SortBySecond);

        std::vector<std::pair<int, float>> remained_indexed_scores;
        remained_indexed_scores.assign(indexed_scores.begin(), indexed_scores.end());

        std::vector<std::pair<int, float>> remained;
        std::vector<std::pair<int, float>> candidates;

        std::vector<DetectedObject> output_detections;
        while (!remained_indexed_scores.empty()) {
            const size_t original_indexed_scores_size = remained_indexed_scores.size();
            const auto& detection = detections[remained_indexed_scores[0].first];

            remained.clear();
            candidates.clear();
            // This includes the first box.
            for (const auto& indexed_score : remained_indexed_scores) {
                float similarity = OverlapSimilarity(detections[indexed_score.first], detection);
                if (similarity > min_suppression_threshold) {
                    candidates.push_back(indexed_score);
                }
                else {
                    remained.push_back(indexed_score);
                }
            }

            auto weighted_detection = detection;
            if (!candidates.empty()) {
                const int num_keypoints = (int) detection.keypoints.size();

                std::vector<float> keypoints(num_keypoints * 2);
                float w_xmin = 0.0f;
                float w_ymin = 0.0f;
                float w_xmax = 0.0f;
                float w_ymax = 0.0f;
                float total_score = 0.0f;
                for (const auto& candidate : candidates) {
                    total_score += candidate.second;

                    const auto& bbox = detections[candidate.first];
                    w_xmin += bbox.x * candidate.second;
                    w_ymin += bbox.y * candidate.second;
                    w_xmax += (bbox.x + bbox.width) * candidate.second;
                    w_ymax += (bbox.y + bbox.height) * candidate.second;

                    for (int i = 0; i < num_keypoints; ++i) {
                        keypoints[i * 2] +=
                            bbox.keypoints[i].x * candidate.second;
                        keypoints[i * 2 + 1] +=
                            bbox.keypoints[i].y * candidate.second;
                    }
                }

                weighted_detection.x = w_xmin / total_score;
                weighted_detection.y = w_ymin / total_score;
                weighted_detection.width = (w_xmax / total_score) - weighted_detection.x;
                weighted_detection.height = (w_ymax / total_score) - weighted_detection.y;

                for (int i = 0; i < num_keypoints; ++i) {
                    weighted_detection.keypoints[i].x = keypoints[i * 2] / total_score;
                    weighted_detection.keypoints[i].y = keypoints[i * 2 + 1] / total_score;
                }
            }

            output_detections.push_back(weighted_detection);
            // Breaks the loop if the size of indexed scores doesn't change after an iteration.
            if (original_indexed_scores_size == remained.size()) {
                break;
            }
            else {
                remained_indexed_scores = std::move(remained);
            }
        }

        detections = output_detections;
    }

    static void ConvertToDetections(
        const float* detection_boxes, const float* detection_scores, const int* detection_classes,
        std::vector<DetectedObject>& output_detections, size_t num_boxes_, size_t num_coords_) {
        fdebug(faceDetectionDebugFile, "ConvertToDetections %lu %lu", num_boxes_, num_coords_);
        const float min_score_thresh = 0.4f;

        std::vector<int> box_indices_ = { 0, 1, 2, 3 };

        int num_keypoints = 6;
        int keypoint_coord_offset = 4;
        int num_values_per_keypoint = 2;

        for (size_t i = 0; i < num_boxes_; ++i) {
            if (detection_scores[i] < min_score_thresh) {
                //fdebug(faceDetectionDebugFile, "score is too low for index %lu: %f", i, detection_scores[i]);
                continue;
            }

            const size_t box_offset = i * num_coords_;
            DetectedObject detection = ConvertToDetection(
                /*box_ymin=*/detection_boxes[box_offset + box_indices_[0]],
                /*box_xmin=*/detection_boxes[box_offset + box_indices_[1]],
                /*box_ymax=*/detection_boxes[box_offset + box_indices_[2]],
                /*box_xmax=*/detection_boxes[box_offset + box_indices_[3]],
                detection_scores[i], detection_classes[i]
            );

            if (detection.width < 0 || detection.height < 0 || std::isnan(detection.width) ||
                std::isnan(detection.height)) {
                // Decoded detection boxes could have negative values for width/height due
                // to model prediction. Filter out those boxes since some downstream
                // calculators may assume non-negative values. (b/171391719)
                continue;
            }

            // Add keypoints.
            for (int kp_id = 0; kp_id < num_keypoints *
                num_values_per_keypoint;
                kp_id += num_values_per_keypoint) {
                const size_t keypoint_index = box_offset + keypoint_coord_offset + kp_id;

                cv::Point2f keypoint;
                keypoint.x = detection_boxes[keypoint_index + 0];
                keypoint.y = detection_boxes[keypoint_index + 1];

                detection.keypoints.emplace_back(keypoint);
            }

            output_detections.emplace_back(detection);
        }
    }

    void FaceDetection::postprocess(const cv::Mat& frameBGR, std::array<float, 16>& transform_matrix, std::vector<DetectedObject>& results)
    {
        results.clear();

        fdebug(faceDetectionDebugFile, "postprocess");

        const float* raw_boxes = outputTensorValues[REGRESSORS].data();
        const float* raw_scores = outputTensorValues[CLASSIFICATORS].data();

        size_t num_boxes_ = maxProposalCount;
        int num_coords_ = 16;

//        //double check that the output tensors have the correct size
//        {
//            size_t raw_boxes_tensor_size = inferRequest.get_tensor(outputsNames[REGRESSORS]).get_byte_size();
//            if (raw_boxes_tensor_size < (num_boxes_ * num_coords_ * sizeof(float)))
//            {
//                throw std::logic_error("REGRESSORS output tensor is holding a smaller amount of data than expected.");
//            }
//
//            size_t raw_scores_tensor_size = inferRequest.get_tensor(outputsNames[CLASSIFICATORS]).get_byte_size();
//            if (raw_scores_tensor_size < (num_boxes_ * num_classes_ * sizeof(float)))
//            {
//                throw std::logic_error("CLASSIFICATORS output tensor is holding a smaller amount of data than expected.");
//            }
//        }


        std::vector<float> boxes(num_boxes_ * num_coords_);
        DecodeBoxes(raw_boxes, anchors, &boxes, num_boxes_, num_coords_, (int)netInputWidth, (int)netInputHeight);

        float score_clipping_thresh = 100.f;

        std::vector<float> detection_scores(num_boxes_);
        std::vector<int> detection_classes(num_boxes_);

        for (size_t i = 0; i < num_boxes_; ++i) {
            // Set the top score for box i
            auto score = raw_scores[i];
            score = score < -score_clipping_thresh
                ? -score_clipping_thresh
                : score;
            score = score > score_clipping_thresh
                ? score_clipping_thresh
                : score;
            score = 1.0f / (1.0f + std::exp(-score));
            detection_scores[i] = score;
            detection_classes[i] = 0;

//            fdebug(faceDetectionDebugFile, "score_idxB %lu: %f -> %f %f %f %f %f", i, raw_scores[i], score,
//                  raw_boxes[i*num_coords_+0],
//                  raw_boxes[i*num_coords_+1],
//                  raw_boxes[i*num_coords_+2],
//                  raw_boxes[i*num_coords_+3]
//            );
            //fdebug(faceDetectionDebugFile, "score_idxB %lu: %f %f %f %f %f", i, score, raw_boxes[i][0], anchors[i].y_center, anchors[i].w, anchors[i].h);
        }

        ConvertToDetections(boxes.data(), detection_scores.data(),
            detection_classes.data(), results, num_boxes_, num_coords_);

        NMS(results);
        fdebug(faceDetectionDebugFile, "results: %lu", results.size());
        DetectedObject bestDetectedObject;

        for (size_t i = 0; i < results.size(); i++)
        {
            //project keypoints
            for (size_t k = 0; k < results[i].keypoints.size(); ++k) {
                float x = results[i].keypoints[k].x;
                float y = results[i].keypoints[k].y;

                x = x * transform_matrix[0] + y * transform_matrix[1] + transform_matrix[3];
                y = x * transform_matrix[4] + y * transform_matrix[5] + transform_matrix[7];

                results[i].keypoints[k].x = x;
                results[i].keypoints[k].y = y;
            }

            //project bounding box
            const float xmin = results[i].x;
            const float ymin = results[i].y;
            const float width = results[i].width;
            const float height = results[i].height;

            // a) Define and project box points.
            std::array<cv::Point2f, 4> box_coordinates = {
              cv::Point2f{xmin, ymin},
              cv::Point2f{xmin + width, ymin},
              cv::Point2f{xmin + width, ymin + height},
              cv::Point2f{xmin, ymin + height}
            };

            for (auto& p : box_coordinates)
            {
                float x = p.x;
                float y = p.y;
                x = x * transform_matrix[0] + y * transform_matrix[1] + transform_matrix[3];
                y = x * transform_matrix[4] + y * transform_matrix[5] + transform_matrix[7];
                p.x = x;
                p.y = y;
            }

            // b) Find new left top and right bottom points for a box which encompases
            //    non-projected (rotated) box.
            constexpr float kFloatMax = std::numeric_limits<float>::max();
            constexpr float kFloatMin = std::numeric_limits<float>::lowest();
            cv::Point2f left_top = { kFloatMax, kFloatMax };
            cv::Point2f right_bottom = { kFloatMin, kFloatMin };

            std::for_each(box_coordinates.begin(), box_coordinates.end(),
                [&left_top, &right_bottom](const cv::Point2f& p) {
                    left_top.x = std::min(left_top.x, p.x);
                    left_top.y = std::min(left_top.y, p.y);
                    right_bottom.x = std::max(right_bottom.x, p.x);
                    right_bottom.y = std::max(right_bottom.y, p.y);
                });

            results[i].x = left_top.x;
            results[i].y = left_top.y;
            results[i].width = right_bottom.x - left_top.x;
            results[i].height = right_bottom.y - left_top.y;
        }

        int input_image_width = frameBGR.cols;
        int input_image_height = frameBGR.rows;
        float target_angle = 0.f;

        int start_keypoint_index = 0;
        int end_keypoint_index = 1;

        //to rects
        for (auto& det : results)
        {
            det.center = { det.x + det.width / 2, det.y + det.height / 2 };
            det.bCenterValid = true;

            const float x0 = det.keypoints[start_keypoint_index].x * (float)input_image_width;
            const float y0 = det.keypoints[start_keypoint_index].y * (float)input_image_height;
            const float x1 = det.keypoints[end_keypoint_index].x * (float)input_image_width;
            const float y1 = det.keypoints[end_keypoint_index].y * (float)input_image_height;

            det.rotation = NormalizeRadians(target_angle - std::atan2(-(y1 - y0), x1 - x0));
        }

        //transform
        for (size_t i = 0; i < results.size(); ++i) {
            auto &det = results[i];
            const float image_width = (float)input_image_width;
            const float image_height = (float)input_image_height;

            float width = det.width;
            float height = det.height;
            const float rotation = det.rotation;

            const float shift_x = 0.f;
            const float shift_y = 0.f;
            const float scale_x = face_bbox_scale;
            const float scale_y = face_bbox_scale;

            if (rotation == 0.f)
            {
                det.center = { det.center.x + width * shift_x,  det.center.y + height * shift_y };
            }
            else
            {
                const float x_shift =
                    (image_width * width * shift_x * std::cos(rotation) -
                        image_height * height * shift_y * std::sin(rotation)) /
                    image_width;
                const float y_shift =
                    (image_width * width * shift_x * std::sin(rotation) +
                        image_height * height * shift_y * std::cos(rotation)) /
                    image_height;

                det.center = { det.center.x + x_shift,  det.center.y + y_shift };
            }

            const float long_side = std::max(width * image_width, height * image_height);
            width = long_side / image_width;
            height = long_side / image_height;

            det.width = width * scale_x;
            det.height = height * scale_y;

            det.x = det.center.x - (det.width / 2.f);
            det.y = det.center.y - (det.height / 2.f);

            fdebug(faceDetectionDebugFile, "result %lu: %f %f %f %f (confidence %i %)", i, det.x, det.y, det.width, det.height, (int)(det.confidence*100.0f));
        }

    }

} //namespace onnxmediapipe
