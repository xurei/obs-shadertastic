// Copyright(C) 2022-2023 Intel Corporation
// SPDX - License - Identifier: Apache - 2.0
#include "ovmediapipe/face_detection.h"

namespace ovmediapipe
{

    FaceDetection::FaceDetection(std::string model_xml_path, std::string device, ov::Core& core, float bbox_scale)
        : face_bbox_scale(bbox_scale)
    {
        std::shared_ptr<ov::Model> model = core.read_model(model_xml_path);
        logBasicModelInfo(model);

        //prepare inputs / outputs
        {
            if (model->inputs().size() != 1) {
                throw std::logic_error("FaceDetection model topology should have only 1 input");
            }

            const auto& input = model->input();
            const ov::Shape& inputShape = model->input().get_shape();
            ov::Layout inputLayout = getLayoutFromShape(input.get_shape());

            if (inputShape[ov::layout::channels_idx(inputLayout)] != 3) {
                throw std::logic_error("Expected 3-channel input");
            }

            ov::preprocess::PrePostProcessor ppp(model);
            ppp.input().tensor().set_element_type(ov::element::u8).set_layout({ "NHWC" });

            ppp.input().model().set_layout(inputLayout);

            inputsNames.push_back(model->input().get_any_name());
            netInputWidth = inputShape[ov::layout::width_idx(inputLayout)];
            netInputHeight = inputShape[ov::layout::height_idx(inputLayout)];

            const ov::OutputVector& outputs = model->outputs();

            if (outputs.size() != 2)
                throw std::range_error("Expected model with 2 outputs");

            std::map<std::string, ov::Shape> outShapes;
            int i = 0;
            for (auto& out : outputs) {
                ppp.output(out.get_any_name()).tensor().set_element_type(ov::element::f32);
                outputsNames.push_back(out.get_any_name());
                auto outShape = out.get_shape();
                outShapes[out.get_any_name()] = outShape;

                if (outShape[2] > 1)
                {
                    REGRESSORS = i;
                }
                else
                {
                    CLASSIFICATORS = i;
                }

                i++;
            }

            model = ppp.build();

            auto regDims = outShapes[outputsNames[REGRESSORS]];
            auto claDims = outShapes[outputsNames[CLASSIFICATORS]];

            maxProposalCount = static_cast<int>(outShapes[outputsNames[REGRESSORS]][1]);

            classificatorsObjectSize = static_cast<int>(outShapes[outputsNames[CLASSIFICATORS]][2]);
            regressorsObjectSize = static_cast<int>(outShapes[outputsNames[REGRESSORS]][2]);

            //https://github.com/google/mediapipe/blob/master/mediapipe/modules/face_detection/face_detection.pbtxt
            SsdAnchorsCalculatorOptions ssdAnchorsCalculatorOptions;
            ssdAnchorsCalculatorOptions.input_size_height = netInputHeight;
            ssdAnchorsCalculatorOptions.input_size_width = netInputWidth;
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

        ov::set_batch(model, 1);

        compiledModel = core.compile_model(model, device);
        inferRequest = compiledModel.create_infer_request();
    }

    void FaceDetection::Run(const cv::Mat& frameBGR, std::vector<DetectedObject>& results)
    {
        std::array<float, 16> transform_matrix;
        preprocess(frameBGR, transform_matrix);

        // perform inference 
        inferRequest.infer();

        postprocess(frameBGR, transform_matrix, results);
    }

    void FaceDetection::preprocess(const cv::Mat& img, std::array<float, 16>& transform_matrix)
    {
        RotatedRect roi = {/*center_x=*/0.5f * img.cols,
            /*center_y =*/0.5f * img.rows,
            /*width =*/static_cast<float>(img.cols),
            /*height =*/static_cast<float>(img.rows),
            /*rotation =*/0 };

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
        cv::Mat projection_matrix =
            cv::getPerspectiveTransform(src_points, dst_points);


        const ov::Tensor& frameTensor = inferRequest.get_tensor(inputsNames[0]);  // first input should be image
        uint8_t* pTensor = frameTensor.data<uint8_t>();

        //wrap the already-allocated tensor as a cv::Mat
        cv::Mat transformed = cv::Mat((int)dst_height, (int)dst_width, CV_8UC3, pTensor);

        //populate the tensor with the result of warpPerspective.
        cv::warpPerspective(img, transformed, projection_matrix,
            cv::Size((int)dst_width, (int)dst_height),
            cv::INTER_LINEAR,
            cv::BORDER_CONSTANT);
    }

    static void DecodeBoxes(const float* raw_boxes, const std::vector<Anchor>& anchors, std::vector<float>* boxes, int num_boxes_, int num_coords_, int netWidth, int netHeight)
    {
        int box_coord_offset = 0;
        bool reverse_output_order = true;
        const float x_scale = (float)netWidth;
        const float y_scale = (float)netHeight;
        const float h_scale = (float)netWidth;
        const float w_scale = (float)netHeight;
        const bool apply_exponential_on_box_size = false;

        const int num_keypoints = 6;
        const int keypoint_coord_offset = 4;
        const int num_values_per_keypoint = 2;

        for (int i = 0; i < num_boxes_; ++i) {
            const int box_offset = i * num_coords_ + box_coord_offset;
            float y_center = raw_boxes[box_offset];
            float x_center = raw_boxes[box_offset + 1];
            float h = raw_boxes[box_offset + 2];
            float w = raw_boxes[box_offset + 3];

            if (reverse_output_order) {
                x_center = raw_boxes[box_offset];
                y_center = raw_boxes[box_offset + 1];
                w = raw_boxes[box_offset + 2];
                h = raw_boxes[box_offset + 3];
            }

            x_center =
                x_center / x_scale * anchors[i].w + anchors[i].x_center;
            y_center =
                y_center / y_scale * anchors[i].h + anchors[i].y_center;

            if (apply_exponential_on_box_size) {
                h = std::exp(h / h_scale) * anchors[i].h;
                w = std::exp(w / w_scale) * anchors[i].w;
            }
            else {
                h = h / h_scale * anchors[i].h;
                w = w / w_scale * anchors[i].w;
            }

            const float ymin = y_center - h / 2.f;
            const float xmin = x_center - w / 2.f;
            const float ymax = y_center + h / 2.f;
            const float xmax = x_center + w / 2.f;

            (*boxes)[i * num_coords_ + 0] = ymin;
            (*boxes)[i * num_coords_ + 1] = xmin;
            (*boxes)[i * num_coords_ + 2] = ymax;
            (*boxes)[i * num_coords_ + 3] = xmax;

            if (num_keypoints) {
                for (int k = 0; k < num_keypoints; ++k) {
                    const int offset = i * num_coords_ + keypoint_coord_offset +
                        k * num_values_per_keypoint;

                    float keypoint_y = raw_boxes[offset];
                    float keypoint_x = raw_boxes[offset + 1];
                    if (reverse_output_order) {
                        keypoint_x = raw_boxes[offset];
                        keypoint_y = raw_boxes[offset + 1];
                    }

                    (*boxes)[offset] = keypoint_x / x_scale * anchors[i].w +
                        anchors[i].x_center;
                    (*boxes)[offset + 1] =
                        keypoint_y / y_scale * anchors[i].h +
                        anchors[i].y_center;
                }
            }
        }
    }

    static DetectedObject ConvertToDetection(float box_ymin, float box_xmin, float box_ymax, float box_xmax, float score,
        int class_id, bool flip_vertically)
    {
        DetectedObject detection;
        detection.confidence = score;
        detection.labelID = class_id;

        detection.x = box_xmin;
        detection.y = flip_vertically ? 1.f - box_ymax : box_ymin;
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

        std::vector<std::pair<int, float>> indexed_scores;
        indexed_scores.reserve(detections.size());

        for (int index = 0; index < (int)detections.size(); ++index) {
            indexed_scores.push_back(
                std::make_pair(index, detections[index].confidence));
        }
        std::sort(indexed_scores.begin(), indexed_scores.end(), SortBySecond);

        std::vector<std::pair<int, float>> remained_indexed_scores;
        remained_indexed_scores.assign(indexed_scores.begin(),
            indexed_scores.end());

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
                const int num_keypoints = detection.keypoints.size();

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

    static void ConvertToDetections(const float* detection_boxes, const float* detection_scores, const int* detection_classes, std::vector<DetectedObject>& output_detections, int num_boxes_, int num_coords_)
    {
        const int max_results = -1;
        const bool flip_vertically = false;
        const bool has_min_score_thresh = true;
        const float min_score_thresh = 0.5f;

        std::vector<int> box_indices_ = { 0, 1, 2, 3 };

        int num_keypoints = 6;
        int keypoint_coord_offset = 4;
        int num_values_per_keypoint = 2;

        for (int i = 0; i < num_boxes_; ++i) {
            if (max_results > 0 && output_detections.size() == max_results) {
                break;
            }

            if (has_min_score_thresh &&
                detection_scores[i] < min_score_thresh) {
                continue;
            }

            const int box_offset = i * num_coords_;
            DetectedObject detection = ConvertToDetection(
                /*box_ymin=*/detection_boxes[box_offset + box_indices_[0]],
                /*box_xmin=*/detection_boxes[box_offset + box_indices_[1]],
                /*box_ymax=*/detection_boxes[box_offset + box_indices_[2]],
                /*box_xmax=*/detection_boxes[box_offset + box_indices_[3]],
                detection_scores[i], detection_classes[i], flip_vertically);

            if (detection.width < 0 || detection.height < 0 || std::isnan(detection.width) ||
                std::isnan(detection.height)) {
                // Decoded detection boxes could have negative values for width/height due
                // to model prediction. Filter out those boxes since some downstream
                // calculators may assume non-negative values. (b/171391719)
                continue;
            }

            // Add keypoints.
            if (num_keypoints > 0) {
                for (int kp_id = 0; kp_id < num_keypoints *
                    num_values_per_keypoint;
                    kp_id += num_values_per_keypoint) {
                    const int keypoint_index =
                        box_offset + keypoint_coord_offset + kp_id;

                    cv::Point2f keypoint;
                    keypoint.x = detection_boxes[keypoint_index + 0];
                    keypoint.y = flip_vertically
                        ? 1.f - detection_boxes[keypoint_index + 1]
                        : detection_boxes[keypoint_index + 1];

                    detection.keypoints.emplace_back(keypoint);
                }
            }

            output_detections.emplace_back(detection);
        }
    }


    void FaceDetection::postprocess(const cv::Mat& frameBGR, std::array<float, 16>& transform_matrix, std::vector<DetectedObject>& results)
    {
        results.clear();

        const float* raw_boxes = inferRequest.get_tensor(outputsNames[REGRESSORS]).data<float>();
        const float* raw_scores = inferRequest.get_tensor(outputsNames[CLASSIFICATORS]).data<float>();

        int num_boxes_ = maxProposalCount;
        int num_classes_ = 1;
        int num_coords_ = 16;

        //double check that the output tensors have the correct size
        {
            size_t raw_boxes_tensor_size = inferRequest.get_tensor(outputsNames[REGRESSORS]).get_byte_size();
            if (raw_boxes_tensor_size < (num_boxes_ * num_coords_ * sizeof(float)))
            {
                throw std::logic_error("REGRESSORS output tensor is holding a smaller amount of data than expected.");
            }

            size_t raw_scores_tensor_size = inferRequest.get_tensor(outputsNames[CLASSIFICATORS]).get_byte_size();
            if (raw_scores_tensor_size < (num_boxes_ * num_classes_ * sizeof(float)))
            {
                throw std::logic_error("CLASSIFICATORS output tensor is holding a smaller amount of data than expected.");
            }
        }


        std::vector<float> boxes(num_boxes_ * num_coords_);
        DecodeBoxes(raw_boxes, anchors, &boxes, num_boxes_, num_coords_, netInputWidth, netInputHeight);

        float score_clipping_thresh = 100.f;

        std::vector<float> detection_scores(num_boxes_);
        std::vector<int> detection_classes(num_boxes_);

        for (int i = 0; i < num_boxes_; ++i) {
            int class_id = -1;
            float max_score = -std::numeric_limits<float>::max();

            // Find the top score for box i.
            //TODO: Remove this loop -- num_classes_ is 1..
            for (int score_idx = 0; score_idx < num_classes_; ++score_idx) {
                auto score = raw_scores[i * num_classes_ + score_idx];
                score = score < -score_clipping_thresh
                    ? -score_clipping_thresh
                    : score;
                score = score > score_clipping_thresh
                    ? score_clipping_thresh
                    : score;
                score = 1.0f / (1.0f + std::exp(-score));
                if (max_score < score) {
                    max_score = score;
                    class_id = score_idx;
                }
            }

            detection_scores[i] = max_score;
            detection_classes[i] = class_id;

        }

        ConvertToDetections(boxes.data(), detection_scores.data(),
            detection_classes.data(), results, num_boxes_, num_coords_);

        NMS(results);

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
              cv::Point2f{xmin, ymin}, cv::Point2f{xmin + width, ymin},
              cv::Point2f{xmin + width, ymin + height}, cv::Point2f{xmin, ymin + height} };

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

            const float x0 = det.keypoints[start_keypoint_index].x *
                input_image_width;
            const float y0 = det.keypoints[start_keypoint_index].y *
                input_image_height;
            const float x1 = det.keypoints[end_keypoint_index].x *
                input_image_width;
            const float y1 = det.keypoints[end_keypoint_index].y *
                input_image_height;

            det.rotation = NormalizeRadians(target_angle - std::atan2(-(y1 - y0), x1 - x0));
        }

        //transform 
        for (auto& det : results)
        {
            const float image_width = (float)input_image_width;
            const float image_height = (float)input_image_height;

            float width = det.width;
            float height = det.height;
            const float rotation = det.rotation;

            const float shift_x = 0.f;
            const float shift_y = 0.f;
            const float scale_x = face_bbox_scale;
            const float scale_y = face_bbox_scale;
            const bool square_long = true;
            const bool square_short = 0;

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

            det.width = width * scale_x;
            det.height = height * scale_y;

            det.x = det.center.x - (det.width / 2.f);
            det.y = det.center.y - (det.height / 2.f);
        }

    }

} //namespace ovmediapipe