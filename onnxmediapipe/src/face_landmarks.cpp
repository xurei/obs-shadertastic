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

#include <iostream>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <obs-module.h>
#include "onnxmediapipe/face_landmarks.h"
#include "onnxmediapipe/landmark_refinement_indices.h"

#include "../../src/logging_functions.hpp"
#include "../../src/fdebug.h"

#define UNUSED_PARAMETER(param) (void)param

FILE* faceLandmarksDebugFile;

namespace onnxmediapipe
{
    FaceLandmarks::FaceLandmarks(std::unique_ptr<Ort::Env> &ort_env)
    {
        if (!ortSession) {
            Ort::SessionOptions sessionOptions;
            sessionOptions.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
//            sessionOptions.SetInterOpNumThreads(2);
//            sessionOptions.SetIntraOpNumThreads(2);
//            sessionOptions.DisableMemPattern();
            sessionOptions.DisablePerSessionThreads();
            sessionOptions.SetExecutionMode(ExecutionMode::ORT_PARALLEL);
            //const char *model_data_path = obs_module_file("face_detection_models/face_landmarks_detector.onnx");
            char *model_data_path = obs_module_file("face_detection_models/face_landmark_with_attention_192x192.onnx");
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
                    ortSession = std::make_shared<Ort::Session>(*ort_env, (const ORTCHAR_T*)model_data_path, sessionOptions);
                #endif
                bfree(model_data_path);
                debug("FACE_LANDMARKS Loading model %s", model_data_path);
            }
        }
        if (!ortSession) {
            return;
        }

        // Prepare inputs / outputs
        {
            if (ortSession->GetInputCount() != 1) {
                throw std::logic_error("FaceDetection model topology should have only 1 input");
            }

            inputCount = ortSession->GetInputCount();
            outputCount = ortSession->GetOutputCount();
            debug("FACE_LANDMARKS Inputs: %lu", inputCount);
            debug("FACE_LANDMARKS Outputs: %lu", outputCount);

            Ort::AllocatorWithDefaultOptions allocator;
            Ort::MemoryInfo memoryInfo = Ort::MemoryInfo::CreateCpu(OrtAllocatorType::OrtArenaAllocator, OrtMemType::OrtMemTypeDefault);

            std::vector<std::vector<int64_t>> inputShapes(inputCount);
            for (size_t i = 0; i < inputCount; ++i) {
                std::string inputName = ortSession->GetInputNameAllocated(i, allocator).get();
                char *inputNameCStr = new char[inputName.length()+1];
                strcpy(inputNameCStr, inputName.c_str());
                inputNames.push_back(inputNameCStr);
                debug("FACE_LANDMARKS Input Name %lu: %s", i, inputNameCStr);
                std::vector<int64_t> inputShape = ortSession->GetInputTypeInfo(i).GetTensorTypeAndShapeInfo().GetShape();
                if (inputShape[0] < 0) {
                    //Negative batch size, aka dynamic. We'll only use one
                    inputShape[0] = 1;
                }
                auto elementType = ortSession->GetInputTypeInfo(i).GetTensorTypeAndShapeInfo().GetElementType();
                inputShapes[i] = inputShape;
                debug("FACE_LANDMARKS Input Shape %lu: %s", i, printShape(inputShape).c_str());
                debug("FACE_LANDMARKS Input Type %lu: %s", i, printElementType(elementType));
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
                char *outputNameCStr = new char[outputName.length()+1];
                strcpy(outputNameCStr, outputName.c_str());
                outputNames.push_back(outputNameCStr);
                debug("FACE_LANDMARKS Output Name %lu: %s", i, outputNameCStr);
                std::vector<int64_t> outputShape = ortSession->GetOutputTypeInfo(i).GetTensorTypeAndShapeInfo().GetShape();
                if (outputShape[0] < 0) {
                    //Negative batch size, aka dynamic. We'll only use one
                    outputShape[0] = 1;
                }
                outputShapes[i] = outputShape;
                debug("FACE_LANDMARKS Output Shape %lu: %s", i, printShape(outputShape).c_str());
                size_t outputTensorSize = vectorProduct(outputShape);
                outputTensorValues.push_back(std::vector<float>(outputTensorSize));

                outputTensors.push_back(Ort::Value::CreateTensor<float>(
                    memoryInfo,
                    outputTensorValues[i].data(), outputTensorSize,
                    outputShape.data(), outputShape.size()
                ));
            }
//            maxProposalCount = static_cast<int>(outputShapes[REGRESSORS][1]);
//            debug("FACE_LANDMARKS maxProposalCount: %lu", maxProposalCount);

            netInputHeight = inputShapes[0][2];
            netInputWidth = inputShapes[0][3];
        }

//        std::shared_ptr<ov::Model> model = core.read_model(model_xml_path);
//        //logBasicModelInfo(model);
//
//        //prepare inputs / outputs
//        {
//            if (model->inputs().size() != 1) {
//                throw std::logic_error("FaceLandmarks model should have only 1 input");
//            }
//
//            inputsNames.push_back(model->input().get_any_name());
//
//            const auto& input = model->input();
//            const ov::Shape& inputShape = model->input().get_shape();
//            ov::Layout inputLayout = getLayoutFromShape(input.get_shape());
//
//            if (inputShape.size() != 4 || inputShape[ov::layout::channels_idx(inputLayout)] != 3)
//                throw std::runtime_error("3-channel 4-dimensional model's input is expected");
//
//            netInputWidth = inputShape[ov::layout::width_idx(inputLayout)];
//            netInputHeight = inputShape[ov::layout::height_idx(inputLayout)];
//
//            ov::preprocess::PrePostProcessor ppp(model);
//
//            ppp.input().model().set_layout(inputLayout);
//
//            const ov::OutputVector& outputs = model->outputs();
//
//            ppp.input().tensor().set_element_type(ov::element::u8).set_layout({ "NHWC" });
//
//            if (outputs.size() == 2)
//            {
//                _bWithAttention = false;
//
//                int i = 0;
//                for (auto& out : outputs) {
//                    ppp.output(out.get_any_name()).tensor().set_element_type(ov::element::f32);
//                    outputsNames.push_back(out.get_any_name());
//                    auto outShape = out.get_shape();
//
//                    if ((outShape == std::vector<size_t>{1, 1, 1, 1404}) ||
//                        (outShape == std::vector<size_t>{1, 1404, 1, 1}))
//                    {
//                        facial_surface_tensor_name = out.get_any_name();
//                    }
//                    else if (outShape == std::vector<size_t>{1, 1, 1, 1})
//                    {
//                        face_flag_tensor_name = out.get_any_name();
//                    }
//                    else
//                    {
//                        throw std::runtime_error("Unexpected FaceLandmarks output tensor size");
//                    }
//
//                    i++;
//                }
//
//                if (facial_surface_tensor_name.empty())
//                    throw std::runtime_error("Expected to find an output tensor with shape {1, 1, 1, 1404} or {1, 1404, 1, 1}, but didn't find one.");
//
//                if (face_flag_tensor_name.empty())
//                    throw std::runtime_error("Expected to find an output tensor with shape {1, 1, 1, 1}, but didn't find one.");
//            }
//            else
//            {
//                _bWithAttention = true;
//
//                for (auto& out : outputs) {
//                    ppp.output(out.get_any_name()).tensor().set_element_type(ov::element::f32);
//                    outputsNames.push_back(out.get_any_name());
//                }
//
//                #define CHECK_OUT_TENSOR_EXISTENCE(out_tensor_name) \
//                if (std::find(outputsNames.begin(), outputsNames.end(), out_tensor_name) == outputsNames.end()) \
//                    throw std::runtime_error("Face Landmark model was expected to have an output tensor named " + out_tensor_name);
//
//                // For the 'With Attention' model, we know what the expected output tensor names are.
//                // So, set them but also double check that there indeed is some output tensor named that.
//                facial_surface_tensor_name = "output_mesh_identity";
//                CHECK_OUT_TENSOR_EXISTENCE(facial_surface_tensor_name)
//
//                    face_flag_tensor_name = "conv_faceflag";
//                CHECK_OUT_TENSOR_EXISTENCE(face_flag_tensor_name)
//
//                    lips_refined_tensor_name = "output_lips";
//                CHECK_OUT_TENSOR_EXISTENCE(lips_refined_tensor_name)
//
//                    left_eye_with_eyebrow_tensor_name = "output_left_eye";
//                CHECK_OUT_TENSOR_EXISTENCE(left_eye_with_eyebrow_tensor_name)
//
//                    right_eye_with_eyebrow_tensor_name = "output_right_eye";
//                CHECK_OUT_TENSOR_EXISTENCE(right_eye_with_eyebrow_tensor_name)
//
//                    left_iris_refined_tensor_name = "output_left_iris";
//                CHECK_OUT_TENSOR_EXISTENCE(left_iris_refined_tensor_name)
//
//                    right_iris_refined_tensor_name = "output_right_iris";
//                CHECK_OUT_TENSOR_EXISTENCE(right_iris_refined_tensor_name)
//                #undef CHECK_OUT_TENSOR_EXISTENCE
//            }
//
//            model = ppp.build();
//        }
//
//        ov::set_batch(model, 1);
//
//        compiledModel = core.compile_model(model, device);
//        inferRequest = compiledModel.create_infer_request();
    }

    void FaceLandmarks::Run(const cv::Mat& frameRGB, const RotatedRect& roi, FaceLandmarksResults& results)
    {
        faceLandmarksDebugFile = fdebug_open("face_landmarks.txt");
        //TODO: sanity checks on roi vs. the cv::Mat.
        // preprocess (fill the input tensor)
        preprocess(frameRGB, roi);

        // perform inference
        /* To run inference, we provide the run options, an array of input names corresponding to the
        inputs in the input tensor, an array of input tensor, number of inputs, an array of output names
        corresponding to the outputs in the output tensor, an array of output tensor, number of outputs. */
        ortSession->Run(Ort::RunOptions{nullptr},
            inputNames.data(), inputTensors.data(), inputCount,
            outputNames.data(), outputTensors.data(), outputCount);

        // post-process
        postprocess(frameRGB, roi, results);
        fdebug_close(faceLandmarksDebugFile);
    }


    void FaceLandmarks::preprocess(const cv::Mat& frameRGB, const RotatedRect& roi)
    {
        const cv::RotatedRect rotated_rect(cv::Point2f(roi.center_x, roi.center_y),
            cv::Size2f(roi.width, roi.height),
            (float)(roi.rotation * 180.f / M_PI));

        cv::Mat src_points;
        cv::boxPoints(rotated_rect, src_points);

        const float dst_width = (float)netInputWidth;
        const float dst_height = (float)netInputHeight;
        /* clang-format off */
        float dst_corners[8] = {
            0.0f,      dst_height,
            0.0f,      0.0f,
            dst_width, 0.0f,
            dst_width, dst_height
        };
        /* clang-format on */

        cv::Mat dst_points = cv::Mat(4, 2, CV_32F, dst_corners);
        cv::Mat projection_matrix = cv::getPerspectiveTransform(src_points, dst_points);

        // Extract ROI from frame
        cv::Mat subFrame = cv::Mat((int)dst_height, (int)dst_width, CV_8UC3);

        cv::warpPerspective(frameRGB, subFrame, projection_matrix,
            cv::Size((int)netInputWidth, (int)netInputHeight),
            /*flags=*/cv::INTER_LINEAR,
            /*borderMode=*/cv::BORDER_REPLICATE);

        // Wrap the already-allocated tensor as a cv::Mat of floats
        cv::Mat floated = cv::Mat((int)netInputHeight, (int)netInputWidth, CV_32FC3);
        subFrame.convertTo(floated, CV_32FC3);
        float* pTensor = inputTensorValues[0].data();
        cv::Mat converted = cv::Mat((int)netInputHeight, (int)netInputWidth, CV_32FC3);

        //hwcToChw
        std::vector<cv::Mat> channels;
        cv::split(floated, channels);
        int i = 0;
        for (auto &img : channels) {
            cv::Mat reshaped = img.reshape(1, 1);
            ++i;
        }

        // Concatenate three vectors to one
        cv::vconcat(channels, converted);

        converted /= 255.0f;
        converted = converted.reshape(3, (int)netInputHeight);
        converted.copyTo(cv::Mat((int)netInputHeight, (int)netInputWidth, CV_32FC3, pTensor));
    }

    static inline void fill2d_points_results(const float* raw_tensor, std::vector< cv::Point2f >& v, const int netInputWidth, const int netInputHeight)
    {
        for (size_t i = 0; i < v.size(); i++)
        {
            v[i].x = raw_tensor[i * 2] / (float)netInputWidth;
            v[i].y = raw_tensor[i * 2 + 1] / (float)netInputHeight;
        }
    }

    void FaceLandmarks::postprocess(const cv::Mat& frameRGB, const RotatedRect& roi, FaceLandmarksResults& results)
    {
//        UNUSED_PARAMETER(frameRGB);
//        UNUSED_PARAMETER(roi);
        results.face_flag = 0.f;
        results.facial_surface.clear();
        results.lips_refined_region.clear();
        results.left_eye_refined_region.clear();
        results.right_eye_refined_region.clear();
        results.left_iris_refined_region.clear();
        results.right_iris_refined_region.clear();

        results.facial_surface.resize(nFacialSurfaceLandmarks);
        const float* facial_surface_tensor_data = outputTensorValues[4].data();
        const float* face_flag_data = outputTensorValues[0].data();

//        //double check that the output tensors have the correct size
//        {
//            size_t facial_surface_tensor_size = inferRequest.get_tensor(facial_surface_tensor_name).get_byte_size();
//            if (facial_surface_tensor_size < (nFacialSurfaceLandmarks * 3 * sizeof(float)))
//            {
//                throw std::logic_error("facial surface tensor is holding a smaller amount of data than expected.");
//            }
//
//            size_t face_flag_tensor_size = inferRequest.get_tensor(face_flag_tensor_name).get_byte_size();
//            if (face_flag_tensor_size < (sizeof(float)))
//            {
//                throw std::logic_error("face flag tensor is holding a smaller amount of data than expected.");
//            }
//        }

        //apply sigmoid activation to produce face flag result
        results.face_flag = 1.0f / (1.0f + std::exp(-(*face_flag_data)));
        fdebug(faceLandmarksDebugFile, "Face Flag: %f", results.face_flag);

        for (int i = 0; i < nFacialSurfaceLandmarks; i++)
        {
            //just set normalized values for now.
            results.facial_surface[i].x = facial_surface_tensor_data[i * 3] / (float)netInputWidth;
            results.facial_surface[i].y = facial_surface_tensor_data[i * 3 + 1] / (float)netInputHeight;
            results.facial_surface[i].z = facial_surface_tensor_data[i * 3 + 2] / (float)netInputWidth;
            fdebug(faceLandmarksDebugFile, "%i %f %f %f", i, results.facial_surface[i].x, results.facial_surface[i].y, results.facial_surface[i].z);
        }

        if (_bWithAttention)
        {
            const float* lips_refined_region_data = outputTensorValues[3].data();
            const float* left_eye_refined_region_data = outputTensorValues[1].data();
            const float* right_eye_refined_region_data = outputTensorValues[5].data();
            const float* left_iris_refined_region_data = outputTensorValues[2].data();
            const float* right_iris_refined_region_data = outputTensorValues[6].data();

//            //double check that the output tensors have the correct size
//            {
//                if (inferRequest.get_tensor(lips_refined_tensor_name).get_byte_size() < lips_refined_region_num_points * 2 * sizeof(float))
//                    throw std::logic_error(lips_refined_tensor_name + " output tensor is holding a smaller amount of data than expected.");
//
//                if (inferRequest.get_tensor(left_eye_with_eyebrow_tensor_name).get_byte_size() < left_eye_refined_region_num_points * 2 * sizeof(float))
//                    throw std::logic_error(left_eye_with_eyebrow_tensor_name + " output tensor is holding a smaller amount of data than expected.");
//
//                if (inferRequest.get_tensor(right_eye_with_eyebrow_tensor_name).get_byte_size() < right_eye_refined_region_num_points * 2 * sizeof(float))
//                    throw std::logic_error(right_eye_with_eyebrow_tensor_name + " output tensor is holding a smaller amount of data than expected.");
//
//                if (inferRequest.get_tensor(left_iris_refined_tensor_name).get_byte_size() < left_iris_refined_region_num_points * 2 * sizeof(float))
//                    throw std::logic_error(left_iris_refined_tensor_name + " output tensor is holding a smaller amount of data than expected.");
//
//                if (inferRequest.get_tensor(right_iris_refined_tensor_name).get_byte_size() < right_iris_refined_region_num_points * 2 * sizeof(float))
//                    throw std::logic_error(right_iris_refined_tensor_name + " output tensor is holding a smaller amount of data than expected.");
//            }

            results.lips_refined_region.resize(lips_refined_region_num_points);
            fill2d_points_results(lips_refined_region_data, results.lips_refined_region, (int)netInputWidth, (int)netInputHeight);

            results.left_eye_refined_region.resize(left_eye_refined_region_num_points);
            fill2d_points_results(left_eye_refined_region_data, results.left_eye_refined_region, (int)netInputWidth, (int)netInputHeight);

            results.right_eye_refined_region.resize(right_eye_refined_region_num_points);
            fill2d_points_results(right_eye_refined_region_data, results.right_eye_refined_region, (int)netInputWidth, (int)netInputHeight);

            results.left_iris_refined_region.resize(left_iris_refined_region_num_points);
            fill2d_points_results(left_iris_refined_region_data, results.left_iris_refined_region, (int)netInputWidth, (int)netInputHeight);

            results.right_iris_refined_region.resize(right_iris_refined_region_num_points);
            fill2d_points_results(right_iris_refined_region_data, results.right_iris_refined_region, (int)netInputWidth, (int)netInputHeight);

            //create a (normalized) refined list of landmarks from the 6 separate lists that we generated.
            results.refined_landmarks.resize(nRefinedLandmarks);

            //initialize the first 468 points to our face surface landmarks
            for (size_t i = 0; i < results.facial_surface.size(); i++)
            {
                results.refined_landmarks[i] = results.facial_surface[i];
            }

            //override x & y for lip points
            for (size_t i = 0; i < lips_refined_region_num_points; i++)
            {
                results.refined_landmarks[lips_refinement_indices[i]].x = results.lips_refined_region[i].x;
                results.refined_landmarks[lips_refinement_indices[i]].y = results.lips_refined_region[i].y;
            }

            //override x & y for left & right_eye points
            for (size_t i = 0; i < left_eye_refined_region_num_points; i++)
            {
                results.refined_landmarks[right_eye_refinement_indices[i]].x = results.right_eye_refined_region[i].x;
                results.refined_landmarks[right_eye_refinement_indices[i]].y = results.right_eye_refined_region[i].y;

                results.refined_landmarks[left_eye_refinement_indices[i]].x = results.left_eye_refined_region[i].x;
                results.refined_landmarks[left_eye_refinement_indices[i]].y = results.left_eye_refined_region[i].y;
            }

            float z_avg_for_left_iris = 0.f;
            for (int i = 0; i < 16; i++)
            {
                z_avg_for_left_iris += results.refined_landmarks[left_iris_z_avg_indices[i]].z;
            }
            z_avg_for_left_iris /= 16.f;

            float z_avg_for_right_iris = 0.f;
            for (int i = 0; i < 16; i++)
            {
                z_avg_for_right_iris += results.refined_landmarks[right_iris_z_avg_indices[i]].z;
            }
            z_avg_for_right_iris /= 16.f;

            //set x & y for left & right iris points
            for (size_t i = 0; i < left_iris_refined_region_num_points; i++)
            {
                results.refined_landmarks[left_iris_refinement_indices[i]].x = results.left_iris_refined_region[i].x;
                results.refined_landmarks[left_iris_refinement_indices[i]].y = results.left_iris_refined_region[i].y;
                results.refined_landmarks[left_iris_refinement_indices[i]].z = z_avg_for_left_iris;
            }

            for (size_t i = 0; i < right_iris_refined_region_num_points; i++)
            {
                results.refined_landmarks[right_iris_refinement_indices[i]].x = results.right_iris_refined_region[i].x;
                results.refined_landmarks[right_iris_refinement_indices[i]].y = results.right_iris_refined_region[i].y;
                results.refined_landmarks[right_iris_refinement_indices[i]].z = z_avg_for_right_iris;
            }

        }

        //project the points back into the pre-rotated / pre-cropped space
        {
            RotatedRect normalized_rect{};
            normalized_rect.center_x = roi.center_x / (float)frameRGB.cols;
            normalized_rect.center_y = roi.center_y / (float)frameRGB.rows;
            normalized_rect.width = roi.width / (float)frameRGB.cols;
            normalized_rect.height = roi.height / (float)frameRGB.rows;
            normalized_rect.rotation = roi.rotation;

            for (size_t i = 0; i < results.facial_surface.size(); i++)
            {
                cv::Point3f p = results.facial_surface[i];
                const float x = p.x - 0.5f;
                const float y = p.y - 0.5f;
                const float angle = normalized_rect.rotation;
                float new_x = std::cos(angle) * x - std::sin(angle) * y;
                float new_y = std::sin(angle) * x + std::cos(angle) * y;

                new_x = new_x * normalized_rect.width + normalized_rect.center_x;
                new_y = new_y * normalized_rect.height + normalized_rect.center_y;
                const float new_z =
                    p.z * normalized_rect.width;  // Scale Z coordinate as X.

                results.facial_surface[i] = { new_x, new_y, new_z };
            }

            for (size_t i = 0; i < results.refined_landmarks.size(); i++)
            {
                cv::Point3f p = results.refined_landmarks[i];
                const float x = p.x - 0.5f;
                const float y = p.y - 0.5f;
                const float angle = normalized_rect.rotation;
                float new_x = std::cos(angle) * x - std::sin(angle) * y;
                float new_y = std::sin(angle) * x + std::cos(angle) * y;

                new_x = new_x * normalized_rect.width + normalized_rect.center_x;
                new_y = new_y * normalized_rect.height + normalized_rect.center_y;
                const float new_z =
                    p.z * normalized_rect.width;  // Scale Z coordinate as X.

                results.refined_landmarks[i] = { new_x, new_y, new_z };
            }

            for (auto& p : results.lips_refined_region)
            {
                const float x = p.x - 0.5f;
                const float y = p.y - 0.5f;
                const float angle = normalized_rect.rotation;
                float new_x = std::cos(angle) * x - std::sin(angle) * y;
                float new_y = std::sin(angle) * x + std::cos(angle) * y;

                new_x = new_x * normalized_rect.width + normalized_rect.center_x;
                new_y = new_y * normalized_rect.height + normalized_rect.center_y;

                p.x = new_x;
                p.y = new_y;
            }

            for (auto& p : results.left_eye_refined_region)
            {
                const float x = p.x - 0.5f;
                const float y = p.y - 0.5f;
                const float angle = normalized_rect.rotation;
                float new_x = std::cos(angle) * x - std::sin(angle) * y;
                float new_y = std::sin(angle) * x + std::cos(angle) * y;

                new_x = new_x * normalized_rect.width + normalized_rect.center_x;
                new_y = new_y * normalized_rect.height + normalized_rect.center_y;

                p.x = new_x;
                p.y = new_y;
            }

            for (auto& p : results.right_eye_refined_region)
            {
                const float x = p.x - 0.5f;
                const float y = p.y - 0.5f;
                const float angle = normalized_rect.rotation;
                float new_x = std::cos(angle) * x - std::sin(angle) * y;
                float new_y = std::sin(angle) * x + std::cos(angle) * y;

                new_x = new_x * normalized_rect.width + normalized_rect.center_x;
                new_y = new_y * normalized_rect.height + normalized_rect.center_y;

                p.x = new_x;
                p.y = new_y;
            }

            for (auto& p : results.left_iris_refined_region)
            {
                const float x = p.x - 0.5f;
                const float y = p.y - 0.5f;
                const float angle = normalized_rect.rotation;
                float new_x = std::cos(angle) * x - std::sin(angle) * y;
                float new_y = std::sin(angle) * x + std::cos(angle) * y;

                new_x = new_x * normalized_rect.width + normalized_rect.center_x;
                new_y = new_y * normalized_rect.height + normalized_rect.center_y;

                p.x = new_x;
                p.y = new_y;
            }

            for (auto& p : results.right_iris_refined_region)
            {
                const float x = p.x - 0.5f;
                const float y = p.y - 0.5f;
                const float angle = normalized_rect.rotation;
                float new_x = std::cos(angle) * x - std::sin(angle) * y;
                float new_y = std::sin(angle) * x + std::cos(angle) * y;

                new_x = new_x * normalized_rect.width + normalized_rect.center_x;
                new_y = new_y * normalized_rect.height + normalized_rect.center_y;

                p.x = new_x;
                p.y = new_y;
            }
        }

        //from the refined landmarks, generated the RotatedRect to return.
        {
            float x_min = std::numeric_limits<float>::max();
            float x_max = std::numeric_limits<float>::min();
            float y_min = std::numeric_limits<float>::max();
            float y_max = std::numeric_limits<float>::min();

            for (auto& p : results.refined_landmarks)
            {
                x_min = std::min(x_min, p.x);
                x_max = std::max(x_max, p.x);
                y_min = std::min(y_min, p.y);
                y_max = std::max(y_max, p.y);
            }

            float bbox_x = x_min;
            float bbox_y = y_min;
            float bbox_width = x_max - x_min;
            float bbox_height = y_max - y_min;

            results.roi.center_x = bbox_x + bbox_width / 2.f;
            results.roi.center_y = bbox_y + bbox_height / 2.f;
            results.roi.width = bbox_width;
            results.roi.height = bbox_height;

            //calculate rotation from keypoints 33 & 263
            const float x0 = results.refined_landmarks[33].x * (float)frameRGB.cols;
            const float y0 = results.refined_landmarks[33].y * (float)frameRGB.rows;
            const float x1 = results.refined_landmarks[263].x * (float)frameRGB.cols;
            const float y1 = results.refined_landmarks[263].y * (float)frameRGB.rows;

            float target_angle = 0.f;
            results.roi.rotation = NormalizeRadians(target_angle - std::atan2(-(y1 - y0), x1 - x0));

            //final transform
            {
                const float image_width = (float)frameRGB.cols;
                const float image_height = (float)frameRGB.rows;

                float width = results.roi.width;
                float height = results.roi.height;
                const float rotation = results.roi.rotation;

                const float shift_x = 0.f;
                const float shift_y = 0.f;
                const float scale_x = 1.5f;
                const float scale_y = 1.5f;

                if (rotation == 0.f)
                {
                    results.roi.center_x = results.roi.center_x + width * shift_x;
                    results.roi.center_y = results.roi.center_y + height * shift_y;
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

                    results.roi.center_x = results.roi.center_x + x_shift;
                    results.roi.center_y = results.roi.center_y + y_shift;
                }

                const float long_side = std::max(width * image_width, height * image_height);
                width = long_side / image_width;
                height = long_side / image_height;

                results.roi.width = width * scale_x;
                results.roi.height = height * scale_y;
            }
        }
    }

} //namespace onnxmediapipe
