#ifndef __OUTPUT_IMAGES_H__
#define __OUTPUT_IMAGES_H__

#include "opencv2/opencv.hpp"
#include <string>
#include <utility>
#include <vector>

// An ordered collection of named output images.
// The order determines the display layout in the grid view.
using OutputImages = std::vector<std::pair<std::string, cv::Mat>>;

#endif // __OUTPUT_IMAGES_H__
