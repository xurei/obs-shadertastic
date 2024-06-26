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
#include "onnxmediapipe/face_mesh.h"
#include "onnxmediapipe/face_detection.h"
#include "onnxmediapipe/face_landmarks.h"
#include "../../src/logging_functions.hpp"

namespace onnxmediapipe
{
    FaceMesh::FaceMesh(
        std::unique_ptr<Ort::Env> &ort_env
    ) : _facedetection(std::make_shared<FaceDetection>(ort_env)),
        _facelandmarks(std::make_shared<FaceLandmarks>(ort_env)) {}

    bool FaceMesh::Run(const cv::Mat& frameRGB, FaceLandmarksResults& results)
    {
        try {
            if (_bNeedsDetection) {
                objects.clear();
                _facedetection->Run(frameRGB, objects);

                if (objects.empty()) {
                    return false;
                }

                _tracked_roi = {
                    objects[0].center.x * (float) frameRGB.cols,
                    objects[0].center.y * (float) frameRGB.rows,
                    objects[0].width * (float) frameRGB.cols,
                    objects[0].height * (float) frameRGB.rows, objects[0].rotation
                };
            }

            _facelandmarks->Run(frameRGB, _tracked_roi, results);

            _tracked_roi = results.roi;
            _tracked_roi.center_x *= (float)frameRGB.cols;
            _tracked_roi.center_y *= (float)frameRGB.rows;
            _tracked_roi.width *= (float)frameRGB.cols;
            _tracked_roi.height *= (float)frameRGB.rows;

            _bNeedsDetection = (results.face_flag < 0.5f);

            return !_bNeedsDetection;
        }
        catch (const Ort::Exception& e) {
            std::cerr << "Caught Ort::Exception: " << e.what() << std::endl;
            return false;
        }
    }

} //namespace onnxmediapipe
