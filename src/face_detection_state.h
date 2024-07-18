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

#include <onnxruntime_cxx_api.h>
#include "onnxmediapipe/face_mesh.h"

struct face_detection_state {
    bool created = false;
    gs_texrender_t *facedetection_texrender;
    gs_texture_t *fd_points_texture;
    gs_stagesurf_t *staging_texture = nullptr;
    std::shared_ptr<onnxmediapipe::FaceMesh> facemesh;
    std::vector<std::string> ov_available_devices;
    onnxmediapipe::FaceLandmarksResults facelandmark_results[10];
    onnxmediapipe::FaceLandmarksResults average_results;
    size_t facelandmark_results_counter = 0;
    bool facelandmark_results_display_results = false;
};
