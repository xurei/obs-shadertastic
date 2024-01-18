// Copyright(C) 2022-2023 Intel Corporation
// SPDX - License - Identifier: Apache - 2.0
#include <math.h>
#include <stdexcept>
#include <cmath>
#include <iostream>
#include "ovmediapipe/ssd_anchors.h"

namespace ovmediapipe
{
    inline bool fEquals(float x, float y)
    {
        return std::fabs(x - y) <= 0.00001f;
    }

    float CalculateScale(float min_scale, float max_scale, int stride_index, int num_strides) {
        if (num_strides == 1) {
            return (min_scale + max_scale) * 0.5f;
        }
        else {
            return min_scale + (max_scale - min_scale) * 1.0f * static_cast<float>(stride_index) / (static_cast<float>(num_strides) - 1.0f);
        }
    }

    Anchor::Anchor(float x_center, float y_center, float w, float h)
    {
        Anchor::x_center = x_center;
        Anchor::y_center = y_center;
        Anchor::h = h;
        Anchor::w = w;
    }

    bool Anchor::operator==(const Anchor& rhs) const
    {
        return (fEquals(x_center, rhs.x_center))
            && (fEquals(y_center, rhs.y_center))
            && (fEquals(h, rhs.h))
            && (fEquals(w, rhs.w));
    }

    bool Anchor::operator!=(const Anchor& rhs) const
    {
        return !operator==(rhs);
    }

    bool SsdAnchorsCalculator::GenerateAnchors(std::vector<Anchor>& anchors, const SsdAnchorsCalculatorOptions& options) {

        // Verify the options.
        if (!options.feature_map_height.size() && !options.strides.size()) {
            throw std::runtime_error("Both feature map shape and strides are missing. Must provide either one.");
        }
        if (options.feature_map_height.size()) {
            if (options.strides.size()) {
                std::cout << "Found feature map shapes. Strides will be ignored." << std::endl;
            }

            if (options.feature_map_height.size() != (size_t)options.num_layers) {
                throw std::runtime_error("feature_map_height size should match num_layers.");
            }

            if (options.feature_map_height.size() != options.feature_map_width.size()) {
                throw std::runtime_error("feature_map_height size should match feature_map_width size.");
            }
        }
        else {
            if (options.strides.size() != (size_t)options.num_layers) {
                throw std::runtime_error("Strides size should match num_layers.");
            };
        }

        int layer_id = 0;
        while (layer_id < options.num_layers) {
            std::vector<float> anchor_height;
            std::vector<float> anchor_width;
            std::vector<float> aspect_ratios;
            std::vector<float> scales;

            // For same strides, we merge the anchors in the same order.
            int last_same_stride_layer = layer_id;
            while ((size_t)last_same_stride_layer < options.strides.size() &&
                options.strides.at(last_same_stride_layer) ==
                options.strides.at(layer_id)) {
                const float scale =
                    CalculateScale(options.min_scale, options.max_scale,
                        last_same_stride_layer, options.strides.size());
                if (last_same_stride_layer == 0 &&
                    options.reduce_boxes_in_lowest_layer) {
                    // For first layer, it can be specified to use predefined anchors.
                    aspect_ratios.push_back(1.0);
                    aspect_ratios.push_back(2.0);
                    aspect_ratios.push_back(0.5);
                    scales.push_back(0.1f);
                    scales.push_back(scale);
                    scales.push_back(scale);
                }
                else {
                    for (int aspect_ratio_id = 0;
                        (size_t)aspect_ratio_id < options.aspect_ratios.size();
                        ++aspect_ratio_id) {
                        aspect_ratios.push_back(options.aspect_ratios.at(aspect_ratio_id));
                        scales.push_back(scale);
                    }
                    if (options.interpolated_scale_aspect_ratio > 0.0) {
                        const float scale_next =
                            last_same_stride_layer == (int)options.strides.size() - 1
                            ? 1.0f
                            : CalculateScale(options.min_scale, options.max_scale,
                                last_same_stride_layer + 1,
                                options.strides.size());
                        scales.push_back(std::sqrt(scale * scale_next));
                        aspect_ratios.push_back(options.interpolated_scale_aspect_ratio);
                    }
                }
                last_same_stride_layer++;
            }

            for (int i = 0; i < (int)aspect_ratios.size(); ++i) {
                const float ratio_sqrts = std::sqrt(aspect_ratios[i]);
                anchor_height.push_back(scales[i] / ratio_sqrts);
                anchor_width.push_back(scales[i] * ratio_sqrts);
            }

            int feature_map_height = 0;
            int feature_map_width = 0;
            if (options.feature_map_height.size()) {
                feature_map_height = options.feature_map_height.at(layer_id);
                feature_map_width = options.feature_map_width.at(layer_id);
            }
            else {
                const int stride = options.strides.at(layer_id);
                feature_map_height = (int)std::ceil(static_cast<float>(options.input_size_height) / static_cast<float>(stride));
                feature_map_width = (int)std::ceil(static_cast<float>(options.input_size_width) / static_cast<float>(stride));
            }

            for (int y = 0; y < feature_map_height; ++y) {
                for (int x = 0; x < feature_map_width; ++x) {
                    for (int anchor_id = 0; anchor_id < (int)anchor_height.size(); ++anchor_id) {
                        // TODO: Support specifying anchor_offset_x, anchor_offset_y.
                        const float x_center =
                            (x + options.anchor_offset_x) * 1.0f / feature_map_width;
                        const float y_center =
                            (y + options.anchor_offset_y) * 1.0f / feature_map_height;

                        Anchor new_anchor;
                        new_anchor.x_center = x_center;
                        new_anchor.y_center = y_center;

                        if (options.fixed_anchor_size) {
                            new_anchor.w = 1.0f;
                            new_anchor.h = 1.0f;
                        }
                        else {
                            new_anchor.w = anchor_width[anchor_id];
                            new_anchor.h = anchor_height[anchor_id];
                        }
                        anchors.push_back(new_anchor);
                    }
                }
            }
            layer_id = last_same_stride_layer;
        }
        return true;
    }

} //namespace ovmediapipe