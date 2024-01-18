// Copyright(C) 2022-2023 Intel Corporation
// SPDX - License - Identifier: Apache - 2.0
# pragma once

#include <vector>

namespace ovmediapipe
{
    struct SsdAnchorsCalculatorOptions {
        // Size of input images
        int32_t input_size_height = 0;                  // no default val
        int32_t input_size_width = 0;                   // no default val

        // Min and max scales for generating anchor boxes on feature maps
        float min_scale = 0;                            // no default val
        float max_scale = 0;                            // no default val

        // The offset for the center of anchors. The value is in the scale of stride
        // E.g. 0.5 meaning 0.5 * |current_stride| in pixels
        float anchor_offset_x = 0.5;                    // default = 0.5
        float anchor_offset_y = 0.5;                    // default = 0.5

        // Number of output feature maps to generate the anchors on
        int num_layers = 0;                             // no default val

        // Sizes of output feature maps to create anchors. Either feature_map size or
        // stride should be provided.
        std::vector<int32_t> feature_map_width = {};
        std::vector<int32_t> feature_map_height = {};
        // Strides of each output feature maps.
        std::vector<int32_t> strides = {};

        // List of different aspect ratio to generate anchors.
        std::vector<float> aspect_ratios = {};

        // A boolean to indicate whether the fixed 3 boxes per location is used in the
        // lowest layer.
        bool reduce_boxes_in_lowest_layer = false;      // default = false

        // An additional anchor is added with this aspect ratio and a scale
        // interpolated between the scale for a layer and the scale for the next layer
        // (1.0 for the last layer). This anchor is not included if this value is 0.
        float interpolated_scale_aspect_ratio = 1.0;    // default = 1.0

        // Whether use fixed width and height (e.g. both 1.0f) for each anchor.
        // This option can be used when the predicted anchor width and height are in
        // pixels.
        bool fixed_anchor_size = false;                 // default = false
    };

    struct Anchor {
        float x_center = 1;
        float y_center = 2;
        float h = 3;
        float w = 4;

        Anchor() = default;
        explicit Anchor(float x_center, float y_center, float w, float h);
        bool operator==(const Anchor& rhs) const;
        bool operator!=(const Anchor& rhs) const;
    };

    class SsdAnchorsCalculator {
    public:
        static bool GenerateAnchors(std::vector<Anchor>& anchors, const SsdAnchorsCalculatorOptions& options);
    };

}