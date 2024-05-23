#include <opencv2/core.hpp>

cv::Mat rgbaToRgb(const cv::Mat& rgbaImage) {
    // Split the RGBA image into individual channels
    std::vector<cv::Mat> channels;
    cv::split(rgbaImage, channels);

    // Extract the RGB channels (excluding the alpha channel)
    cv::Mat rgbImage;
    cv::merge(std::vector<cv::Mat>{channels[0], channels[1], channels[2]}, rgbImage);

    return rgbImage;
}
