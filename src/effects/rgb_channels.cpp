#include "rgb_channels.h"

void rgb_channels(const cv::Mat &src, OutputImages &outputs,
                  ControlsManager &controls) {
  cv::Mat bgr;
  if (src.channels() == 1) {
    cv::cvtColor(src, bgr, cv::COLOR_GRAY2BGR);
  } else {
    bgr = src;
  }

  std::vector<cv::Mat> channels;
  cv::split(bgr, channels); // channels[0]=B, [1]=G, [2]=R

  const int mode = controls.getRadio("Channel Mode");
  // mode 0 = Colored (tinted), mode 1 = Grayscale

  if (mode == 1) {
    // Grayscale: output each channel as a single-channel image
    outputs.push_back({"Blue", channels[0]});
    outputs.push_back({"Green", channels[1]});
    outputs.push_back({"Red", channels[2]});
  } else {
    // Colored: tint each channel with its color
    auto makeTinted = [](const cv::Mat &ch, int channelIndex) {
      cv::Mat tinted(ch.rows, ch.cols, CV_8UC3, cv::Scalar(0, 0, 0));
      std::vector<cv::Mat> planes = {cv::Mat::zeros(ch.size(), CV_8UC1),
                                     cv::Mat::zeros(ch.size(), CV_8UC1),
                                     cv::Mat::zeros(ch.size(), CV_8UC1)};
      planes[channelIndex] = ch;
      cv::merge(planes, tinted);
      return tinted;
    };

    outputs.push_back({"Blue", makeTinted(channels[0], 0)});
    outputs.push_back({"Green", makeTinted(channels[1], 1)});
    outputs.push_back({"Red", makeTinted(channels[2], 2)});
  }
}
