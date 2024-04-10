// Copyright(C) 2022-2023 Intel Corporation
// SPDX - License - Identifier: Apache - 2.0
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <openvino/openvino.hpp>
#include "ovmediapipe/face_landmarks.h"
#include "ovmediapipe/landmark_refinement_indices.h"

#define do_log(level, format, ...) \
    blog(level, "[shadertastic] " format, ##__VA_ARGS__)
#define info(format, ...) do_log(LOG_INFO, format, ##__VA_ARGS__)
#define warn(format, ...) do_log(LOG_WARNING, format, ##__VA_ARGS__)

#ifdef DEV_MODE
    #define debug(format, ...) info("(debug) " #format, ##__VA_ARGS__)
#else
    #define debug(format, ...)
#endif
#include <iostream>

namespace ovmediapipe
{
    FaceLandmarks::FaceLandmarks(std::string model_xml_path, std::string device, ov::Core& core)
    {
        std::shared_ptr<ov::Model> model = core.read_model(model_xml_path);
        logBasicModelInfo(model);

        //prepare inputs / outputs
        {
            if (model->inputs().size() != 1) {
                throw std::logic_error("FaceLandmarks model should have only 1 input");
            }

            inputsNames.push_back(model->input().get_any_name());

            const auto& input = model->input();
            const ov::Shape& inputShape = model->input().get_shape();
            ov::Layout inputLayout = getLayoutFromShape(input.get_shape());

            if (inputShape.size() != 4 || inputShape[ov::layout::channels_idx(inputLayout)] != 3)
                throw std::runtime_error("3-channel 4-dimensional model's input is expected");

            netInputWidth = inputShape[ov::layout::width_idx(inputLayout)];
            netInputHeight = inputShape[ov::layout::height_idx(inputLayout)];

            ov::preprocess::PrePostProcessor ppp(model);

            ppp.input().model().set_layout(inputLayout);

            const ov::OutputVector& outputs = model->outputs();

            ppp.input().tensor().set_element_type(ov::element::u8).set_layout({ "NHWC" });

            if (outputs.size() == 2)
            {
                _bWithAttention = false;

                int i = 0;
                for (auto& out : outputs) {
                    ppp.output(out.get_any_name()).tensor().set_element_type(ov::element::f32);
                    outputsNames.push_back(out.get_any_name());
                    auto outShape = out.get_shape();

                    if ((outShape == std::vector<size_t>{1, 1, 1, 1404}) ||
                        (outShape == std::vector<size_t>{1, 1404, 1, 1}))
                    {
                        facial_surface_tensor_name = out.get_any_name();
                    }
                    else if (outShape == std::vector<size_t>{1, 1, 1, 1})
                    {
                        face_flag_tensor_name = out.get_any_name();
                    }
                    else
                    {
                        throw std::runtime_error("Unexpected FaceLandmarks output tensor size");
                    }

                    i++;
                }

                if (facial_surface_tensor_name.empty())
                    throw std::runtime_error("Expected to find an output tensor with shape {1, 1, 1, 1404} or {1, 1404, 1, 1}, but didn't find one.");

                if (face_flag_tensor_name.empty())
                    throw std::runtime_error("Expected to find an output tensor with shape {1, 1, 1, 1}, but didn't find one.");
            }
            else
            {
                _bWithAttention = true;

                for (auto& out : outputs) {
                    ppp.output(out.get_any_name()).tensor().set_element_type(ov::element::f32);
                    outputsNames.push_back(out.get_any_name());
                }

#define CHECK_OUT_TENSOR_EXISTENCE(out_tensor_name) \
                if (std::find(outputsNames.begin(), outputsNames.end(), out_tensor_name) == outputsNames.end()) \
                    throw std::runtime_error("Face Landmark model was expected to have an output tensor named " + out_tensor_name);

                // For the 'With Attention' model, we know what the expected output tensor names are.
                // So, set them but also double check that there indeed is some output tensor named that.
                facial_surface_tensor_name = "output_mesh_identity";
                CHECK_OUT_TENSOR_EXISTENCE(facial_surface_tensor_name)

                    face_flag_tensor_name = "conv_faceflag";
                CHECK_OUT_TENSOR_EXISTENCE(face_flag_tensor_name)

                    lips_refined_tensor_name = "output_lips";
                CHECK_OUT_TENSOR_EXISTENCE(lips_refined_tensor_name)

                    left_eye_with_eyebrow_tensor_name = "output_left_eye";
                CHECK_OUT_TENSOR_EXISTENCE(left_eye_with_eyebrow_tensor_name)

                    right_eye_with_eyebrow_tensor_name = "output_right_eye";
                CHECK_OUT_TENSOR_EXISTENCE(right_eye_with_eyebrow_tensor_name)

                    left_iris_refined_tensor_name = "output_left_iris";
                CHECK_OUT_TENSOR_EXISTENCE(left_iris_refined_tensor_name)

                    right_iris_refined_tensor_name = "output_right_iris";
                CHECK_OUT_TENSOR_EXISTENCE(right_iris_refined_tensor_name)

#undef CHECK_OUT_TENSOR_EXISTENCE
            }

            model = ppp.build();
        }

        ov::set_batch(model, 1);

        compiledModel = core.compile_model(model, device);
        inferRequest = compiledModel.create_infer_request();
    }

    void FaceLandmarks::Run(const cv::Mat& frameBGR, const RotatedRect& roi, FaceLandmarksResults& results)
    {
        //TODO: sanity checks on roi vs. the cv::Mat. 
        // preprocess (fill the input tensor)
        preprocess(frameBGR, roi);

        // perform inference 
        inferRequest.infer();

        // post-process
        postprocess(frameBGR, roi, results);
    }


    void FaceLandmarks::preprocess(const cv::Mat& frameBGR, const RotatedRect& roi)
    {
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

        cv::Mat dst_points = cv::Mat(4, 2, CV_32F, dst_corners);
        cv::Mat projection_matrix =
            cv::getPerspectiveTransform(src_points, dst_points);

        const ov::Tensor& frameTensor = inferRequest.get_tensor(inputsNames[0]);  // first input should be image
        uint8_t* pTensor = frameTensor.data<uint8_t>();

        //wrap the already-allocated tensor as a cv::Mat
        cv::Mat transformed = cv::Mat((int)dst_height, (int)dst_width, CV_8UC3, pTensor);

        cv::warpPerspective(frameBGR, transformed, projection_matrix,
            cv::Size((int)dst_width, (int)dst_height),
            /*flags=*/cv::INTER_LINEAR,
            /*borderMode=*/cv::BORDER_REPLICATE);
    }

    static inline void fill2d_points_results(const float* raw_tensor, std::vector< cv::Point2f >& v, const int netInputWidth, const int netInputHeight)
    {
        for (size_t i = 0; i < v.size(); i++)
        {
            v[i].x = raw_tensor[i * 2] / (float)netInputWidth;
            v[i].y = raw_tensor[i * 2 + 1] / (float)netInputHeight;
        }
    }

    void FaceLandmarks::postprocess(const cv::Mat& frameBGR, const RotatedRect& roi, FaceLandmarksResults& results)
    {
        results.face_flag = 0.f;
        results.facial_surface.clear();
        results.lips_refined_region.clear();
        results.left_eye_refined_region.clear();
        results.right_eye_refined_region.clear();
        results.left_iris_refined_region.clear();
        results.right_iris_refined_region.clear();


        results.facial_surface.resize(nFacialSurfaceLandmarks);
        const float* facial_surface_tensor_data = inferRequest.get_tensor(facial_surface_tensor_name).data<float>();
        const float* face_flag_data = inferRequest.get_tensor(face_flag_tensor_name).data<float>();

        //double check that the output tensors have the correct size
        {
            size_t facial_surface_tensor_size = inferRequest.get_tensor(facial_surface_tensor_name).get_byte_size();
            if (facial_surface_tensor_size < (nFacialSurfaceLandmarks * 3 * sizeof(float)))
            {
                throw std::logic_error("facial surface tensor is holding a smaller amount of data than expected.");
            }

            size_t face_flag_tensor_size = inferRequest.get_tensor(face_flag_tensor_name).get_byte_size();
            if (face_flag_tensor_size < (sizeof(float)))
            {
                throw std::logic_error("face flag tensor is holding a smaller amount of data than expected.");
            }
        }

        //apply sigmoid activation to produce face flag result 
        results.face_flag = 1.0f / (1.0f + std::exp(-(*face_flag_data)));

        for (int i = 0; i < nFacialSurfaceLandmarks; i++)
        {
            //just set normalized values for now.
            results.facial_surface[i].x = facial_surface_tensor_data[i * 3] / (float)netInputWidth;
            results.facial_surface[i].y = facial_surface_tensor_data[i * 3 + 1] / (float)netInputHeight;
            results.facial_surface[i].z = facial_surface_tensor_data[i * 3 + 2] / (float)netInputWidth;
        }

        if (_bWithAttention)
        {
            const float* lips_refined_region_data = inferRequest.get_tensor(lips_refined_tensor_name).data<float>();
            const float* left_eye_refined_region_data = inferRequest.get_tensor(left_eye_with_eyebrow_tensor_name).data<float>();
            const float* right_eye_refined_region_data = inferRequest.get_tensor(right_eye_with_eyebrow_tensor_name).data<float>();
            const float* left_iris_refined_region_data = inferRequest.get_tensor(left_iris_refined_tensor_name).data<float>();
            const float* right_iris_refined_region_data = inferRequest.get_tensor(right_iris_refined_tensor_name).data<float>();

            //double check that the output tensors have the correct size
            {
                if (inferRequest.get_tensor(lips_refined_tensor_name).get_byte_size() < lips_refined_region_num_points * 2 * sizeof(float))
                    throw std::logic_error(lips_refined_tensor_name + " output tensor is holding a smaller amount of data than expected.");

                if (inferRequest.get_tensor(left_eye_with_eyebrow_tensor_name).get_byte_size() < left_eye_refined_region_num_points * 2 * sizeof(float))
                    throw std::logic_error(left_eye_with_eyebrow_tensor_name + " output tensor is holding a smaller amount of data than expected.");

                if (inferRequest.get_tensor(right_eye_with_eyebrow_tensor_name).get_byte_size() < right_eye_refined_region_num_points * 2 * sizeof(float))
                    throw std::logic_error(right_eye_with_eyebrow_tensor_name + " output tensor is holding a smaller amount of data than expected.");

                if (inferRequest.get_tensor(left_iris_refined_tensor_name).get_byte_size() < left_iris_refined_region_num_points * 2 * sizeof(float))
                    throw std::logic_error(left_iris_refined_tensor_name + " output tensor is holding a smaller amount of data than expected.");

                if (inferRequest.get_tensor(right_iris_refined_tensor_name).get_byte_size() < right_iris_refined_region_num_points * 2 * sizeof(float))
                    throw std::logic_error(right_iris_refined_tensor_name + " output tensor is holding a smaller amount of data than expected.");
            }

            results.lips_refined_region.resize(lips_refined_region_num_points);
            fill2d_points_results(lips_refined_region_data, results.lips_refined_region, netInputWidth, netInputHeight);

            results.left_eye_refined_region.resize(left_eye_refined_region_num_points);
            fill2d_points_results(left_eye_refined_region_data, results.left_eye_refined_region, netInputWidth, netInputHeight);

            results.right_eye_refined_region.resize(right_eye_refined_region_num_points);
            fill2d_points_results(right_eye_refined_region_data, results.right_eye_refined_region, netInputWidth, netInputHeight);

            results.left_iris_refined_region.resize(left_iris_refined_region_num_points);
            fill2d_points_results(left_iris_refined_region_data, results.left_iris_refined_region, netInputWidth, netInputHeight);

            results.right_iris_refined_region.resize(right_iris_refined_region_num_points);
            fill2d_points_results(right_iris_refined_region_data, results.right_iris_refined_region, netInputWidth, netInputHeight);

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
            RotatedRect normalized_rect;
            normalized_rect.center_x = roi.center_x / (float)frameBGR.cols;
            normalized_rect.center_y = roi.center_y / (float)frameBGR.rows;
            normalized_rect.width = roi.width / (float)frameBGR.cols;
            normalized_rect.height = roi.height / (float)frameBGR.rows;
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

        // if we're not using the 'with attention' model, set the 'refined' points to 
        // our normalized / rotated results.
        if (!_bWithAttention)
        {
            results.refined_landmarks = results.facial_surface;
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
            const float x0 = results.refined_landmarks[33].x * (float)frameBGR.cols;
            const float y0 = results.refined_landmarks[33].y * (float)frameBGR.rows;
            const float x1 = results.refined_landmarks[263].x * (float)frameBGR.cols;
            const float y1 = results.refined_landmarks[263].y * (float)frameBGR.rows;

            float target_angle = 0.f;
            results.roi.rotation = NormalizeRadians(target_angle - std::atan2(-(y1 - y0), x1 - x0));

            //final transform
            {
                const float image_width = (float)frameBGR.cols;
                const float image_height = (float)frameBGR.rows;

                float width = results.roi.width;
                float height = results.roi.height;
                const float rotation = results.roi.rotation;

                const float shift_x = 0.f;
                const float shift_y = 0.f;
                const float scale_x = 1.5f;
                const float scale_y = 1.5f;
                const bool square_long = true;
                const bool square_short = false;

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

                if (square_long) {
                    const float long_side =
                        std::max(width * image_width, height * image_height);
                    width = long_side / image_width;
                    height = long_side / image_height;
                }
                else if (square_short) {
                    const float short_side =
                        std::min(width * image_width, height * image_height);
                    width = short_side / image_width;
                    height = short_side / image_height;
                }

                results.roi.width = width * scale_x;
                results.roi.height = height * scale_y;
            }
        }
    }

} //namespace ovmediapipe
