// Copyright(C) 2022-2023 Intel Corporation
// SPDX - License - Identifier: Apache - 2.0
#include "ovmediapipe/common.h"

namespace ovmediapipe
{
    void GetRotatedSubRectToRectTransformMatrix(const RotatedRect& sub_rect,
        int rect_width, int rect_height,
        bool flip_horizontaly,
        std::array<float, 16>* matrix_ptr) {
        std::array<float, 16>& matrix = *matrix_ptr;
        // The resulting matrix is multiplication of below commented out matrices:
        //   post_scale_matrix
        //     * translate_matrix
        //     * rotate_matrix
        //     * flip_matrix
        //     * scale_matrix
        //     * initial_translate_matrix

        // Matrix to convert X,Y to [-0.5, 0.5] range "initial_translate_matrix"
        // { 1.0f,  0.0f, 0.0f, -0.5f}
        // { 0.0f,  1.0f, 0.0f, -0.5f}
        // { 0.0f,  0.0f, 1.0f,  0.0f}
        // { 0.0f,  0.0f, 0.0f,  1.0f}

        const float a = sub_rect.width;
        const float b = sub_rect.height;
        // Matrix to scale X,Y,Z to sub rect "scale_matrix"
        // Z has the same scale as X.
        // {   a, 0.0f, 0.0f, 0.0f}
        // {0.0f,    b, 0.0f, 0.0f}
        // {0.0f, 0.0f,    a, 0.0f}
        // {0.0f, 0.0f, 0.0f, 1.0f}

        const float flip = flip_horizontaly ? -1.f : 1.f;
        // Matrix for optional horizontal flip around middle of output image.
        // { fl  , 0.0f, 0.0f, 0.0f}
        // { 0.0f, 1.0f, 0.0f, 0.0f}
        // { 0.0f, 0.0f, 1.0f, 0.0f}
        // { 0.0f, 0.0f, 0.0f, 1.0f}

        const float c = std::cos(sub_rect.rotation);
        const float d = std::sin(sub_rect.rotation);
        // Matrix to do rotation around Z axis "rotate_matrix"
        // {    c,   -d, 0.0f, 0.0f}
        // {    d,    c, 0.0f, 0.0f}
        // { 0.0f, 0.0f, 1.0f, 0.0f}
        // { 0.0f, 0.0f, 0.0f, 1.0f}

        const float e = sub_rect.center_x;
        const float f = sub_rect.center_y;
        // Matrix to do X,Y translation of sub rect within parent rect
        // "translate_matrix"
        // {1.0f, 0.0f, 0.0f, e   }
        // {0.0f, 1.0f, 0.0f, f   }
        // {0.0f, 0.0f, 1.0f, 0.0f}
        // {0.0f, 0.0f, 0.0f, 1.0f}

        const float g = 1.0f / rect_width;
        const float h = 1.0f / rect_height;
        // Matrix to scale X,Y,Z to [0.0, 1.0] range "post_scale_matrix"
        // {g,    0.0f, 0.0f, 0.0f}
        // {0.0f, h,    0.0f, 0.0f}
        // {0.0f, 0.0f,    g, 0.0f}
        // {0.0f, 0.0f, 0.0f, 1.0f}

        // row 1
        matrix[0] = a * c * flip * g;
        matrix[1] = -b * d * g;
        matrix[2] = 0.0f;
        matrix[3] = (-0.5f * a * c * flip + 0.5f * b * d + e) * g;

        // row 2
        matrix[4] = a * d * flip * h;
        matrix[5] = b * c * h;
        matrix[6] = 0.0f;
        matrix[7] = (-0.5f * b * c - 0.5f * a * d * flip + f) * h;

        // row 3
        matrix[8] = 0.0f;
        matrix[9] = 0.0f;
        matrix[10] = a * g;
        matrix[11] = 0.0f;

        // row 4
        matrix[12] = 0.0f;
        matrix[13] = 0.0f;
        matrix[14] = 0.0f;
        matrix[15] = 1.0f;
    }
}