// Copyright(C) 2022-2023 Intel Corporation
// SPDX - License - Identifier: Apache - 2.0
#include <opencv2/core.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <openvino/openvino.hpp>
#include "ovmediapipe/pose_landmark.h"
#include "ovmediapipe/landmark_refinement_indices.h"
#include "filtering/one_euro_filter.h"

//#include <ittutils.h>
#define ITT_SCOPED_TASK(TASKNAME) {}

namespace ovmediapipe
{
    class LandmarkSmoother
    {
    public:
        LandmarkSmoother(int _nLandmarksP = 33,
            double frequencyP = 30.,
            double min_cutoffP = 0.05,
            double betaP = 80.,
            double derivative_cutoffP = 1.,
            double min_allowed_object_scaleP = 1e-06,
            bool disable_value_scalingP = false,
            bool normalize_to_frame_sizeP = true
        )
            : _nLandmarks(_nLandmarksP), frequency(frequencyP), min_cutoff(min_cutoffP),
            beta(betaP), derivative_cutoff(derivative_cutoffP), min_allowed_object_scale(min_allowed_object_scaleP),
            disable_value_scaling(disable_value_scalingP), normalize_to_frame_size(normalize_to_frame_sizeP)
        {

        }

        void smooth(std::vector< PoseLandmarkResult::PoseLandmarkKeypoint >& keypoints,
            int frame_width = 0, int frame_height = 0, float roi_width = 0.f, float roi_height = 0.f)
        {
            if (normalize_to_frame_size && ((frame_width <= 0) || (frame_height <= 0)))
            {
                std::cout << "invalid frame width / height" << std::endl;
                return;
            }

            if (keypoints.size() != _nLandmarks)
            {
                std::cout << "Expected number of keypoints " << _nLandmarks << std::endl;
                return;
            }

            if (x_filters.empty())
            {
                for (size_t i = 0; i < _nLandmarks; ++i) {
                    x_filters.push_back(
                        OneEuroFilter(frequency, min_cutoff, beta, derivative_cutoff));
                    y_filters.push_back(
                        OneEuroFilter(frequency, min_cutoff, beta, derivative_cutoff));
                    z_filters.push_back(
                        OneEuroFilter(frequency, min_cutoff, beta, derivative_cutoff));
                }
            }

            // Get value scale as inverse value of the object scale.
            // If value is too small smoothing will be disabled and landmarks will be
            // returned as is
            double value_scale = 1.;
            if (!disable_value_scaling)
            {
                const double object_width = (double)roi_width * frame_width;
                const double object_height = (double)roi_height * frame_height;
                double object_scale = (object_width + object_height) / 2.0;
                if (object_scale < min_allowed_object_scale)
                    return;
                if (object_scale == 0.)
                {
                    std::cout << "object_scale is 0! " << std::endl;
                    return;
                }
                value_scale = 1.0f / object_scale;
            }

            if (normalize_to_frame_size)
            {
                for (auto& lm : keypoints)
                {
                    lm.coord.x *= (float)frame_width;
                    lm.coord.y *= (float)frame_height;
                    lm.coord.z *= (float)frame_width;
                }
            }

            //apply the smooth filter
            for (size_t i = 0; i < _nLandmarks; i++)
            {
                keypoints[i].coord.x = (float)x_filters[i].Apply(value_scale, keypoints[i].coord.x);
                keypoints[i].coord.y = (float)y_filters[i].Apply(value_scale, keypoints[i].coord.y);
                keypoints[i].coord.z = (float)z_filters[i].Apply(value_scale, keypoints[i].coord.z);
            }

            if (normalize_to_frame_size)
            {
                for (auto& lm : keypoints)
                {
                    lm.coord.x /= (float)frame_width;
                    lm.coord.y /= (float)frame_height;
                    lm.coord.z /= (float)frame_width;
                }
            }
        }

        void clear()
        {
            x_filters.clear();
            y_filters.clear();
            z_filters.clear();
        }

    private:

        const size_t _nLandmarks = 33;
        double frequency = 30.;
        double min_cutoff = 0.05;
        double beta = 80.;
        double derivative_cutoff = 1.;
        double min_allowed_object_scale = 1e-06;
        bool disable_value_scaling = false;
        bool normalize_to_frame_size = true;

        std::vector< OneEuroFilter > x_filters;
        std::vector< OneEuroFilter > y_filters;
        std::vector< OneEuroFilter > z_filters;
    };

    PoseLandmark::PoseLandmark(std::string model_xml_path, std::string device, ov::Core& core)
    {
        std::shared_ptr<ov::Model> model = core.read_model(model_xml_path);
        logBasicModelInfo(model);

        //prepare inputs / outputs
        {
            if (model->inputs().size() != 1) {
                throw std::logic_error("PoseLandmark model should have only 1 input");
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

            for (auto& out : outputs) {
                ppp.output(out.get_any_name()).tensor().set_element_type(ov::element::f32);
                outputsNames.push_back(out.get_any_name());

                auto outShape = out.get_shape();
                if (outShape == std::vector<size_t>{1, 195})
                {
                    keypoint_tensor_name = out.get_any_name();
                }
                else if (outShape == std::vector<size_t>{1, 1})
                {
                    pose_flag_tensor_name = out.get_any_name();
                }
                else if ((outShape == std::vector<size_t>{1, 256, 256, 1}) ||
                    (outShape == std::vector<size_t>{1, 1, 256, 256}))
                {
                    segmentation_mask_tensor_name = out.get_any_name();
                }
                else if (outShape == std::vector<size_t>{1, 64, 64, 39})
                {
                    _bHeatmapIsNCHW = false;
                    heatmap_tensor_name = out.get_any_name();
                }
                else if (outShape == std::vector<size_t>{1, 39, 64, 64})
                {
                    _bHeatmapIsNCHW = true;
                    heatmap_tensor_name = out.get_any_name();
                }
                else if (outShape == std::vector<size_t>{1, 117})
                {
                    world_landmarks_tensor_name = out.get_any_name();
                }
            }

            if (keypoint_tensor_name.empty())
                throw std::runtime_error("Did not find expected output tensor of shape {1, 195}");

            if (pose_flag_tensor_name.empty())
                throw std::runtime_error("Did not find expected output tensor of shape {1, 1}");

            if (segmentation_mask_tensor_name.empty())
                throw std::runtime_error("Did not find expected output tensor of shape {1, 256, 256, 1} or {1, 1, 256, 256}");

            if (heatmap_tensor_name.empty())
                throw std::runtime_error("Did not find expected output tensor of shape {1, 64, 64, 39} or {1, 39, 64, 64}");

            if (world_landmarks_tensor_name.empty())
                throw std::runtime_error("Did not find expected output tensor of shape {1, 117}");

            model = ppp.build();
        }

        ov::set_batch(model, 1);

        compiledModel = core.compile_model(model, device);
        inferRequest = compiledModel.create_infer_request();

        _landmark_smoother = std::make_shared< LandmarkSmoother >(33, 30., 0.05, 80., 1., 1e-06, false, true);
        _landmark_aux_smoother = std::make_shared< LandmarkSmoother >(2, 30., 0.1, 10., 1., 1e-06, false, true);
    }

    void PoseLandmark::Run(const cv::Mat& frameBGR, const RotatedRect& roi, PoseLandmarkResult& results, std::shared_ptr<cv::Mat> seg_mask)
    {
        ITT_SCOPED_TASK(PoseLandmark_Run);
        std::array<float, 16> transform_matrix;
        //TODO: sanity checks on roi vs. the cv::Mat.
        // preprocess (fill the input tensor)
        preprocess(frameBGR, roi, transform_matrix);

        // perform inference
        {
            ITT_SCOPED_TASK(PoseLandmark_infer);
            inferRequest.infer();
        }

        // post-process
        postprocess(frameBGR, roi, transform_matrix, results, seg_mask);
    }


    void PoseLandmark::preprocess(const cv::Mat& frameBGR, const RotatedRect& roi, std::array<float, 16>& transform_matrix)
    {
        ITT_SCOPED_TASK(PoseLandmark_preprocess);
        const cv::RotatedRect rotated_rect(cv::Point2f(roi.center_x, roi.center_y),
            cv::Size2f(roi.width, roi.height),
            (float)(roi.rotation * 180.f / M_PI));

        cv::Mat src_points;
        cv::boxPoints(rotated_rect, src_points);

        //calculate transform matrix
        GetRotatedSubRectToRectTransformMatrix(roi, frameBGR.cols, frameBGR.rows, false, &transform_matrix);

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

    static inline void fill2d_points_results(float* raw_tensor, std::vector< cv::Point2f >& v, const int netInputWidth, const int netInputHeight)
    {
        for (size_t i = 0; i < v.size(); i++)
        {
            v[i].x = raw_tensor[i * 2] / (float)netInputWidth;
            v[i].y = raw_tensor[i * 2 + 1] / (float)netInputHeight;
        }
    }

    static inline float sig_activation(float in)
    {
        return 1.0f / (1.0f + std::exp(-(in)));
    }

    void PoseLandmark::postprocess(const cv::Mat& frameBGR, const RotatedRect& roi, std::array<float, 16>& transform_matrix, PoseLandmarkResult& results, std::shared_ptr<cv::Mat> seg_mask)
    {
        ITT_SCOPED_TASK(PoseLandmark_postprocess);
        results.keypoints.clear();
        results.pose_flag = 0.f;
        results.roi = RotatedRect{};

        const int nLandmarks = 39;
        const int heatmap_width = 64;
        const int heatmap_height = 64;
        const int heatmap_channels = 39;

        const float* keypoint_tensor_data = inferRequest.get_tensor(keypoint_tensor_name).data<float>();
        const float* pose_flag_data = inferRequest.get_tensor(pose_flag_tensor_name).data<float>();
        const float* heatmap_tensor_data = inferRequest.get_tensor(heatmap_tensor_name).data<float>();

        //double check that the output tensors have the correct size
        {
            if (inferRequest.get_tensor(keypoint_tensor_name).get_byte_size() < nLandmarks * 5 * sizeof(float))
                throw std::logic_error(keypoint_tensor_name + " output tensor is holding a smaller amount of data than expected.");

            if (inferRequest.get_tensor(pose_flag_tensor_name).get_byte_size() < sizeof(float))
                throw std::logic_error(keypoint_tensor_name + " output tensor is holding a smaller amount of data than expected.");

            if (inferRequest.get_tensor(heatmap_tensor_name).get_byte_size() < heatmap_width * heatmap_height * heatmap_channels * sizeof(float))
                throw std::logic_error(heatmap_tensor_name + " output tensor is holding a smaller amount of data than expected.");
        }

        results.pose_flag = *pose_flag_data;

        if (seg_mask)
        {
            const int seg_mask_width = 256;
            const int seg_mask_height = 256;
            ITT_SCOPED_TASK(PoseLandmark_segmask);
            //apply sigmoid activation to raw segmentation mask.
            cv::Mat activated_256x256_mask = cv::Mat(256, 256, CV_32FC1);
            float* pRawSeg = inferRequest.get_tensor(segmentation_mask_tensor_name).data<float>();

            //double check that the output tensor has the correct size
            if (inferRequest.get_tensor(segmentation_mask_tensor_name).get_byte_size() < (seg_mask_width * seg_mask_height * sizeof(float)))
                throw std::logic_error(segmentation_mask_tensor_name + " output tensor is holding a smaller amount of data than expected.");

            {
                ITT_SCOPED_TASK(PoseLandmark_segmask_sigmoid);
                for (int y = 0; y < seg_mask_height; y++)
                {
                    float* pActivated = (float*)activated_256x256_mask.ptr(y, 0);
                    for (int x = 0; x < seg_mask_width; x++)
                    {
                        pActivated[x] = sig_activation(pRawSeg[x]);
                    }

                    pRawSeg += 256;

                    //if previous frame produced a valid segmentation mask, smooth this one against it.
                    if (_bPrevSegmaskValid)
                    {
                        /*
                         * Assume p := new_mask_value
                         * H(p) := 1 + (p * log(p) + (1-p) * log(1-p)) / log(2)
                         * uncertainty alpha(p) =
                         *   Clamp(1 - (1 - H(p)) * (1 - H(p)), 0, 1) [squaring the uncertainty]
                         *
                         * The following polynomial approximates uncertainty alpha as a function
                         * of (p + 0.5):
                         */
                        const float c1 = 5.68842f;
                        const float c2 = -0.748699f;
                        const float c3 = -57.8051f;
                        const float c4 = 291.309f;
                        const float c5 = -624.717f;
                        const float combine_with_previous_ratio_ = 0.7f;

                        float* pPrevMask = (float*)_prevSegMask.ptr(y, 0);
                        for (int xi = 0; xi < seg_mask_width; xi++)
                        {
                            const float new_mask_value = pActivated[xi];
                            const float prev_mask_value = pPrevMask[xi];
                            const float t = new_mask_value - 0.5f;
                            const float x = t * t;
                            const float uncertainty =
                                1.0f -
                                std::min(1.0f, x * (c1 + x * (c2 + x * (c3 + x * (c4 + x * c5)))));

                            float smoothed_val = new_mask_value + (prev_mask_value - new_mask_value) *
                                (uncertainty * combine_with_previous_ratio_);
                            pActivated[xi] = smoothed_val;
                        }
                    }
                }

                _prevSegMask = activated_256x256_mask;
                _bPrevSegmaskValid = true;
            }

            cv::Mat input_mat(cv::Size(4, 4), CV_32FC1, &transform_matrix[0]);
            cv::Mat inverse_mat = input_mat.inv(cv::DECOMP_LU);

            //first, we'll apply the warp operation to produce a 256x256 image.
            int warp_dst_width = netInputWidth;
            int warp_dst_height = netInputHeight;

            // OpenCV warpAffine works in absolute coordinates, so the transfom (which
            // accepts and produces relative coordinates) should be adjusted to first
            // normalize coordinates and then scale them.
            // clang-format off
            cv::Matx44f normalize_dst_coordinate({
              1.0f / warp_dst_width, 0.0f,               0.0f, 0.0f,
              0.0f,              1.0f / warp_dst_height, 0.0f, 0.0f,
              0.0f,              0.0f,               1.0f, 0.0f,
              0.0f,              0.0f,               0.0f, 1.0f });
            cv::Matx44f scale_src_coordinate({
              1.0f * netInputWidth, 0.0f,                  0.0f, 0.0f,
              0.0f,                 1.0f * netInputHeight, 0.0f, 0.0f,
              0.0f,                 0.0f,                  1.0f, 0.0f,
              0.0f,                 0.0f,                  0.0f, 1.0f });

            // clang-format on
            cv::Matx44f adjust_dst_coordinate;
            cv::Matx44f adjust_src_coordinate;

            adjust_dst_coordinate = normalize_dst_coordinate;
            adjust_src_coordinate = scale_src_coordinate;

            cv::Matx44f transform((float*)inverse_mat.ptr(0, 0));
            cv::Matx44f transform_absolute =
                adjust_src_coordinate * transform * adjust_dst_coordinate;

            cv::Mat cv_affine_transform(2, 3, CV_32F);
            cv_affine_transform.at<float>(0, 0) = transform_absolute.val[0];
            cv_affine_transform.at<float>(0, 1) = transform_absolute.val[1];
            cv_affine_transform.at<float>(0, 2) = transform_absolute.val[3];
            cv_affine_transform.at<float>(1, 0) = transform_absolute.val[4];
            cv_affine_transform.at<float>(1, 1) = transform_absolute.val[5];
            cv_affine_transform.at<float>(1, 2) = transform_absolute.val[7];

            cv::Mat warpedMat;
            cv::warpAffine(activated_256x256_mask, warpedMat, cv_affine_transform,
                cv::Size(warp_dst_width, warp_dst_height),
                /*flags=*/cv::INTER_LINEAR | cv::WARP_INVERSE_MAP,
                0);

            //then, we'll convert it to U8
            warpedMat.convertTo(*seg_mask, CV_8UC1, 255.0);
        }
        else
        {
            _bPrevSegmaskValid = false;
        }

        if (results.pose_flag < 0.5f)
        {
            _bPrevSegmaskValid = false;
        }

        std::vector<PoseLandmarkResult::PoseLandmarkKeypoint> landmarks;
        landmarks.resize(nLandmarks);

        for (int i = 0; i < nLandmarks; i++)
        {
            landmarks[i].coord.x = keypoint_tensor_data[i * 5] / (float)netInputWidth;
            landmarks[i].coord.y = keypoint_tensor_data[i * 5 + 1] / (float)netInputHeight;
            landmarks[i].coord.z = keypoint_tensor_data[i * 5 + 2] / (float)netInputWidth;
            landmarks[i].visibility = sig_activation(keypoint_tensor_data[i * 5 + 3]);
            landmarks[i].presence = sig_activation(keypoint_tensor_data[i * 5 + 4]);
        }

        //refine landmarks from heat map
        {
            ITT_SCOPED_TASK(PoseLandmark_heatmap_refine);
            const int hm_width = heatmap_width;
            const int hm_height = heatmap_height;
            const int hm_channels = heatmap_channels;
            const int kernel_size = 7;
            const int hm_row_size = hm_width * hm_channels;
            const float min_confidence_to_refine = 0.5f;
            const bool refine_presence = false;
            const bool refine_visibility = false;

            for (int lm_index = 0; lm_index < (int)landmarks.size(); lm_index++)
            {
                int center_col = (int)landmarks[lm_index].coord.x * hm_width;
                int center_row = (int)landmarks[lm_index].coord.y * hm_height;

                // Point is outside of the image let's keep it intact.
                if (center_col < 0 || center_col >= hm_width || center_row < 0 ||
                    center_col >= hm_height) {
                    continue;
                }

                int offset = (kernel_size - 1) / 2;
                // Calculate area to iterate over. Note that we decrease the kernel on
                // the edges of the heatmap. Equivalent to zero border.
                int begin_col = std::max(0, center_col - offset);
                int end_col = std::min(hm_width, center_col + offset + 1);
                int begin_row = std::max(0, center_row - offset);
                int end_row = std::min(hm_height, center_row + offset + 1);

                float sum = 0;
                float weighted_col = 0;
                float weighted_row = 0;
                float max_confidence_value = 0;

                // Main loop. Go over kernel and calculate weighted sum of coordinates,
                // sum of weights and max weights.
                for (int row = begin_row; row < end_row; ++row) {
                    for (int col = begin_col; col < end_col; ++col) {

                        int idx;
                        if (_bHeatmapIsNCHW)
                        {
                            // NCHW layout
                            idx = lm_index * hm_width * hm_height + row * hm_height + col;
                        }
                        else
                        {
                            // NHWC layout
                            idx = hm_row_size * row + col * hm_channels + lm_index;
                        }

                        // Right now we hardcode sigmoid activation as it will be wasteful to
                        // calculate sigmoid for each value of heatmap in the model itself.  If
                        // we ever have other activations it should be trivial to expand via
                        // options.
                        float confidence = sig_activation(heatmap_tensor_data[idx]);
                        sum += confidence;
                        max_confidence_value = std::max(max_confidence_value, confidence);
                        weighted_col += col * confidence;
                        weighted_row += row * confidence;
                    }
                }

                if (max_confidence_value >= min_confidence_to_refine && sum > 0) {
                    landmarks[lm_index].coord.x = weighted_col / hm_width / sum;
                    landmarks[lm_index].coord.y = weighted_row / hm_height / sum;
                }

                if (refine_presence && sum > 0) {
                    // We assume confidence in heatmaps describes landmark presence.
                    // If landmark is not confident in heatmaps, probably it is not present.
                    const float presence = landmarks[lm_index].presence;
                    const float new_presence = std::min(presence, max_confidence_value);
                    landmarks[lm_index].presence = new_presence;
                }
                if (refine_visibility && sum > 0) {
                    // We assume confidence in heatmaps describes landmark presence.
                    // As visibility = (not occluded but still present) -> that mean that if
                    // landmark is not present, it is not visible as well.
                    // I.e. visibility confidence cannot be bigger than presence confidence.
                    const float visibility = landmarks[lm_index].visibility;
                    const float new_visibility = std::min(visibility, max_confidence_value);
                    landmarks[lm_index].visibility = new_visibility;
                }
            }
        }

        //out of the 39 landmarks we get from the actual tensor, we end up only giving 33 in results.
        results.keypoints.resize(33);

        //project the points back into the pre-rotated / pre-cropped space
        {
            RotatedRect normalized_rect;
            normalized_rect.center_x = roi.center_x / (float)frameBGR.cols;
            normalized_rect.center_y = roi.center_y / (float)frameBGR.rows;
            normalized_rect.width = roi.width / (float)frameBGR.cols;
            normalized_rect.height = roi.height / (float)frameBGR.rows;
            normalized_rect.rotation = roi.rotation;

            for (size_t i = 0; i < landmarks.size(); i++)
            {
                cv::Point3f p = landmarks[i].coord;
                const float x = p.x - 0.5f;
                const float y = p.y - 0.5f;
                const float angle = normalized_rect.rotation;
                float new_x = std::cos(angle) * x - std::sin(angle) * y;
                float new_y = std::sin(angle) * x + std::cos(angle) * y;

                new_x = new_x * normalized_rect.width + normalized_rect.center_x;
                new_y = new_y * normalized_rect.height + normalized_rect.center_y;
                const float new_z =
                    p.z * normalized_rect.width;  // Scale Z coordinate as X.

                landmarks[i].coord = { new_x, new_y, new_z };

                if (i < 33)
                {
                    results.keypoints[i].coord = landmarks[i].coord;
                    results.keypoints[i].presence = landmarks[i].presence;
                    results.keypoints[i].visibility = landmarks[i].visibility;
                }
            }
        }

        std::vector<PoseLandmarkResult::PoseLandmarkKeypoint> auxiliary_landmarks;
        auxiliary_landmarks.resize(2);

        auxiliary_landmarks[0] = landmarks[33];
        auxiliary_landmarks[1] = landmarks[34];

        {
            const float x_center = auxiliary_landmarks[0].coord.x * (float)frameBGR.cols;
            const float y_center = auxiliary_landmarks[0].coord.y * (float)frameBGR.rows;

            const float x_scale = auxiliary_landmarks[1].coord.x * (float)frameBGR.cols;
            const float y_scale = auxiliary_landmarks[1].coord.y * (float)frameBGR.rows;

            const float box_size =
                std::sqrt((x_scale - x_center) * (x_scale - x_center) +
                    (y_scale - y_center) * (y_scale - y_center)) *
                2.0f;

            float rect_width = box_size / (float)frameBGR.cols;
            float rect_height = box_size / (float)frameBGR.rows;

            _landmark_aux_smoother->smooth(auxiliary_landmarks, frameBGR.cols, frameBGR.rows, rect_width, rect_height);

            if (results.pose_flag < 0.5f)
            {
                _landmark_aux_smoother->clear();
            }
        }

        RotatedRect new_roi;
        //from the auxiliary_landmarks, generate the RotatedRect to return.
        {
            float x_min = std::numeric_limits<float>::max();
            float x_max = std::numeric_limits<float>::min();
            float y_min = std::numeric_limits<float>::max();
            float y_max = std::numeric_limits<float>::min();

            for (auto& p : auxiliary_landmarks)
            {
                x_min = std::min(x_min, p.coord.x);
                x_max = std::max(x_max, p.coord.x);
                y_min = std::min(y_min, p.coord.y);
                y_max = std::max(y_max, p.coord.y);
            }

            const float x_center = auxiliary_landmarks[0].coord.x * frameBGR.cols;
            const float y_center = auxiliary_landmarks[0].coord.y * frameBGR.rows;

            const float x_scale = auxiliary_landmarks[1].coord.x * frameBGR.cols;
            const float y_scale = auxiliary_landmarks[1].coord.y * frameBGR.rows;

            const float box_size = (float)
                std::sqrt((x_scale - x_center) * (x_scale - x_center) +
                    (y_scale - y_center) * (y_scale - y_center)) *
                2.0f;


            new_roi.center_x = x_center / frameBGR.cols;
            new_roi.center_y = y_center / frameBGR.rows;
            new_roi.width = box_size / frameBGR.cols;
            new_roi.height = box_size / frameBGR.rows;

            //calculate rotation from keypoints 33 & 34
            const float x0 = auxiliary_landmarks[0].coord.x * (float)frameBGR.cols;
            const float y0 = auxiliary_landmarks[0].coord.y * (float)frameBGR.rows;
            const float x1 = auxiliary_landmarks[1].coord.x * (float)frameBGR.cols;
            const float y1 = auxiliary_landmarks[1].coord.y * (float)frameBGR.rows;

            float target_angle = (float)(M_PI * 90.f / 180.f);
            new_roi.rotation = NormalizeRadians(target_angle - std::atan2(-(y1 - y0), x1 - x0));

            //final transform
            {
                const float image_width = (float)frameBGR.cols;
                const float image_height = (float)frameBGR.rows;

                float width = new_roi.width;
                float height = new_roi.height;
                const float rotation = new_roi.rotation;

                const float shift_x = 0.f;
                const float shift_y = 0.f;
                const float scale_x = 1.25f;
                const float scale_y = 1.25f;
                const bool square_long = true;
                const bool square_short = 0;

                if (rotation == 0.f)
                {
                    new_roi.center_x = new_roi.center_x + width * shift_x;
                    new_roi.center_y = new_roi.center_y + height * shift_y;
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

                    new_roi.center_x = new_roi.center_x + x_shift;
                    new_roi.center_y = new_roi.center_y + y_shift;
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

                new_roi.width = width * scale_x;
                new_roi.height = height * scale_y;
            }
            results.roi = new_roi;
        }

        //smooth the landmarks
        _landmark_smoother->smooth(results.keypoints, frameBGR.cols, frameBGR.rows, results.roi.width, results.roi.height);

        if (results.pose_flag < 0.5f)
        {
            _landmark_smoother->clear();
        }
    }

} //namespace ovmediapipe
