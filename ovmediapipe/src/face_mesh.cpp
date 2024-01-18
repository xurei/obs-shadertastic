// Copyright(C) 2022-2023 Intel Corporation
// SPDX - License - Identifier: Apache - 2.0
#include "ovmediapipe/face_mesh.h"
#include "ovmediapipe/face_detection.h"
#include "ovmediapipe/face_landmarks.h"

namespace ovmediapipe
{
	ov::Core FaceMesh::core = ov::Core();

	FaceMesh::FaceMesh(std::string detection_model_xml_path, std::string detection_device,
		std::string landmarks_model_xml_path, std::string landmarks_device)
		: _facedetection(std::make_shared < FaceDetection >(detection_model_xml_path, detection_device, core)),
		_facelandmarks(std::make_shared < FaceLandmarks >(landmarks_model_xml_path, landmarks_device, core)) {}

	bool FaceMesh::Run(const cv::Mat& frameBGR, FaceLandmarksResults& results)
	{
		if (_bNeedsDetection)
		{
			std::vector<DetectedObject> objects;
			_facedetection->Run(frameBGR, objects);

			if (objects.empty())
				return false;

			_tracked_roi = { objects[0].center.x * frameBGR.cols,
				             objects[0].center.y * frameBGR.rows,
				             objects[0].width * frameBGR.cols,
							 objects[0].height * frameBGR.rows, objects[0].rotation
			               };
		}

		_facelandmarks->Run(frameBGR, _tracked_roi, results);

		_tracked_roi = results.roi;
		_tracked_roi.center_x *= frameBGR.cols;
		_tracked_roi.center_y *= frameBGR.rows;
		_tracked_roi.width *= frameBGR.cols;
		_tracked_roi.height *= frameBGR.rows;

		_bNeedsDetection = (results.face_flag < 0.5f);

		return !_bNeedsDetection;
	}

} //namespace ovmediapipe