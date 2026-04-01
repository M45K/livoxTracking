#include "viewer.hpp"
#include <algorithm>

#if defined(WITH_OPENCV)
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#endif

Viewer::Viewer(int width, int height, float meters_per_pixel)
    : width_(width), height_(height), mpp_(meters_per_pixel) {
#if defined(WITH_OPENCV)
    cv::namedWindow("TopView", cv::WINDOW_AUTOSIZE);
#endif
}

bool Viewer::running() const {
    // For debugging, keep running; close with Ctrl+C.
    return true;
}

void Viewer::update_topview(const pcl::PointCloud<pcl::PointXYZI>::ConstPtr& cloud) {
#if defined(WITH_OPENCV)
    cv::Mat img(height_, width_, CV_8UC1, cv::Scalar(0));
    const float half_w = width_ * 0.5f;
    const float half_h = height_ * 0.5f;

    for (const auto& p : cloud->points) {
        int x = static_cast<int>(half_w + p.x / mpp_);
        int y = static_cast<int>(half_h - p.y / mpp_);
        if (x >= 0 && x < width_ && y >= 0 && y < height_) {
            uint8_t& pix = img.at<uint8_t>(y, x);
            uint8_t val = static_cast<uint8_t>(std::min(255.0f, p.intensity * 255.0f));
            uint8_t newv = val == 0 ? 64 : val;
            if (newv > pix) pix = newv;
        }
    }
    cv::applyColorMap(img, img, cv::COLORMAP_JET);
    cv::imshow("TopView", img);
    cv::waitKey(1);
#else
    (void)cloud;
#endif
}
