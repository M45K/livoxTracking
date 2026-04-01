#pragma once
#include <memory>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>

class Viewer {
public:
    Viewer(int width = 800, int height = 800, float meters_per_pixel = 0.1f);
    bool running() const; // true until window closed (or headless)
    void update_topview(const pcl::PointCloud<pcl::PointXYZI>::ConstPtr& cloud);
private:
    int width_, height_;
    float mpp_;
};
