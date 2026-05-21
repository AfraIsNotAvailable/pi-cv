#include "histogram_and_pdf.h"
#include "src/helpers/histogram.h"

void histogram_and_pdf(const cv::Mat &src, OutputImages &outputs,
                       ControlsManager & /*controls*/) {
  cv::Mat gray;
  if (src.channels() == 1)
    gray = src;
  else
    cv::cvtColor(src, gray, cv::COLOR_BGR2GRAY);

  auto hist = computeHistogram(gray, 256);
  auto pdf = computePDF(hist, gray.rows * gray.cols);
  cv::Mat histImg = drawHistogramInt(hist);
  cv::Mat pdfImg = drawHistogramFloat(pdf);

  outputs.push_back({"Histogram", histImg});
  outputs.push_back({"PDF", pdfImg});
}
