// Copyright(C) 2022-2023 Intel Corporation
// SPDX - License - Identifier: Apache - 2.0
#include <onnxruntime_c_api.h>
#include "onnxmediapipe/common.h"
#include <opencv2/imgcodecs.hpp>

namespace onnxmediapipe
{
    // pretty prints a shape dimension vector
    std::string printShape(const std::vector<int64_t>& v) {
        std::stringstream ss("");
        for (std::size_t i = 0; i < v.size() - 1; i++) ss << v[i] << "x";
        ss << v[v.size() - 1];
        return ss.str();
    }

    void hwcToChw(cv::InputArray src, cv::OutputArray dst)
    {
//        std::vector<cv::Mat> channels;
//        cv::split(src, channels);
//
//        // Stretch one-channel images to vector
//        int i = 0;
//        for (auto &img : channels) {
//            cv::imwrite("/home/olivier/obs-plugins/obs-shadertastic/plugin/channel_"+std::to_string(i)+".bmp", img);
//            img = img.reshape(1, 1);
//            ++i;
//        }
//
//        // Concatenate three vectors to one
//        cv::hconcat(channels, dst);
        const int src_h = src.rows();
        const int src_w = src.cols();
        const int src_c = src.channels();

        cv::Mat hw_c = src.getMat().reshape(1, src_h * src_w);

        const std::array<int,3> dims = {src_c, src_h, src_w};
        //dst.create(3, &dims[0], CV_MAKETYPE(src.depth(), 1));
        cv::Mat dst_1d = dst.getMat().reshape(1, {src_c, src_h, src_w});

        cv::transpose(hw_c, dst_1d);
    }

    // pretty prints a shape dimension vector
    const char * printElementType(const ONNXTensorElementDataType& type) {
      switch(type) {
          case ONNX_TENSOR_ELEMENT_DATA_TYPE_UNDEFINED: return "TYPE_UNDEFINED";
          case ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT: return "TYPE_FLOAT";
          case ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT8: return "TYPE_UINT8";
          case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT8: return "TYPE_INT8";
          case ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT16: return "TYPE_UINT16";
          case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT16: return "TYPE_INT16";
          case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT32: return "TYPE_INT32";
          case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT64: return "TYPE_INT64";
          case ONNX_TENSOR_ELEMENT_DATA_TYPE_STRING: return "TYPE_STRING";
          case ONNX_TENSOR_ELEMENT_DATA_TYPE_BOOL: return "TYPE_BOOL";
          case ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT16: return "TYPE_FLOAT16";
          case ONNX_TENSOR_ELEMENT_DATA_TYPE_DOUBLE: return "TYPE_DOUBLE";
          case ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT32: return "TYPE_UINT32";
          case ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT64: return "TYPE_UINT64";
          case ONNX_TENSOR_ELEMENT_DATA_TYPE_COMPLEX64:  return "TYPE_COMPLEX64";
          case ONNX_TENSOR_ELEMENT_DATA_TYPE_COMPLEX128: return "TYPE_COMPLEX128";
          case ONNX_TENSOR_ELEMENT_DATA_TYPE_BFLOAT16: return "TYPE_BFLOAT16";
          case ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT8E4M3FN: return "TYPE_FLOAT8E4M3FN";
          case ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT8E4M3FNUZ: return "TYPE_FLOAT8E4M3FNUZ";
          case ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT8E5M2: return "TYPE_FLOAT8E5M2";
          case ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT8E5M2FNUZ: return "TYPE_FLOAT8E5M2FNUZ";
          default: return "TYPE_UNKNOWN";
      }
    }

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

        const float g = 1.0f / (float)rect_width;
        const float h = 1.0f / (float)rect_height;
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
