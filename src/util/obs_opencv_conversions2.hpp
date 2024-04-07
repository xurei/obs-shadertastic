// Copyright(C) 2022-2023 Intel Corporation
// SPDX - License - Identifier: Apache - 2.0
#pragma once

#include <obs-module.h>
#include <opencv2/imgproc.hpp>

//attempt to convert an obs_source_frame to an OpenCV BGR Mat.
// Returns true if this function was successful, false on failure.
static bool obsframe_to_bgrmat(struct obs_source_frame* frame,
    cv::Mat& imageBGR)
{
    if (!frame)
        return false;

    if (frame->format == VIDEO_FORMAT_I420)
    {
        //this opencv conversion only works if the 3 planes are contigous in memory.
        //plane 0 and plane1
        uint8_t* pY = frame->data[0];
        uint8_t* pU = pY + frame->width * frame->height;
        uint8_t* pV = pU + (frame->width * frame->height / 4);
        if (pU == frame->data[1] && pV == frame->data[2])
        {
            cv::Mat mat_src = cv::Mat(frame->height * 3 / 2, frame->width, CV_8UC1, pY);
            cv::cvtColor(mat_src, imageBGR, cv::COLOR_YUV2BGR_I420);
            return true;
        }
    }
    else if (frame->format == VIDEO_FORMAT_NV12)
    {
        //this opencv conversion only works if the 3 planes are contigous in memory.
        uint8_t* pY = frame->data[0];
        uint8_t* pUV = pY + frame->width * frame->height;
        if (pUV == frame->data[1])
        {
            cv::Mat mat_src = cv::Mat(frame->height * 3 / 2, frame->width, CV_8UC1, pY);
            cv::cvtColor(mat_src, imageBGR, cv::COLOR_YUV2BGR_NV12);
            return true;
        }
    }

    return false;
}

//attempt to convert an OpenCV BGT Mat to an (already created) obs_source_frame.
// Returns true if this function was successful, false on failure.
static bool bgrmat_to_obsframe(cv::Mat& imageBGR,
    struct obs_source_frame* frame)
{
    if (!frame)
        return false;

    if (frame->format == VIDEO_FORMAT_I420)
    {
        //this opencv conversion only works if the 3 planes are contiguous in memory.
        //plane 0 and plane1
        uint8_t* pY = frame->data[0];
        uint8_t* pU = pY + frame->width * frame->height;
        uint8_t* pV = pU + (frame->width * frame->height / 4);
        if (pU == frame->data[1] && pV == frame->data[2])
        {
            cv::Mat mat_dst = cv::Mat(frame->height * 3 / 2, frame->width, CV_8UC1, pY);
            cv::cvtColor(imageBGR, mat_dst, cv::COLOR_BGR2YUV_I420);
            return true;
        }
    }
    else if (frame->format == VIDEO_FORMAT_NV12)
    {
        //this opencv conversion only works if the 3 planes are contigous in memory.
        uint8_t* pY = frame->data[0];
        uint8_t* pUV = pY + frame->width * frame->height;
        if (pUV == frame->data[1])
        {
            cv::Mat mat_i420 = cv::Mat(frame->height * 3 / 2, frame->width, CV_8UC1, pY);
            cv::cvtColor(imageBGR, mat_i420, cv::COLOR_BGR2YUV_I420);

            //For NV12, we need to interleave the planes. First, copy the I420 U/V output
            // someplace temporary. The imageBGR buffer will do, as it's not long for this world
            // anyway -- and would save us having to allocate a new buffer.
            std::memcpy(imageBGR.ptr(0), pUV, (frame->width * frame->height)/2);

            ////now, interleave the U/V planes
            uint8_t* pU_i420 = imageBGR.ptr(0);
            uint8_t* pV_i420 = pU_i420 + (frame->width * frame->height) / 4;

            for (unsigned int p = 0; p < (frame->width * frame->height) / 4; p++)
            {
                pUV[p * 2] = pU_i420[p];
                pUV[p * 2 + 1] = pV_i420[p];
            }

            return true;
        }
    }

    return false;
}
