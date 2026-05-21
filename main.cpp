#include "common.h"
#include "controls_manager.h"
#include "slider.h"
#include "src/helpers/global_state.h"
#include "src/helpers/rendering.h"

// Effect headers
#include "src/effects/negative.h"
#include "src/effects/bi_level_color_map.h"
#include "src/effects/additive_factor.h"
#include "src/effects/multiplicative.h"
#include "src/effects/view_placeholder.h"
#include "src/effects/rgb_channels.h"
#include "src/effects/histogram_and_pdf.h"
#include "src/effects/histogram_m_bins.h"
#include "src/effects/gray_level_reduction.h"
#include "src/effects/floyd_steinberg.h"
#include "src/effects/hsv_hue_quantization.h"
#include "src/effects/lab8_statistics.h"
#include "src/effects/lab8_auto_binarize.h"
#include "src/effects/lab8_transforms.h"
#include "src/effects/lab8_histogram_equalization.h"
#include "src/effects/morphology_lab7_combined.h"
#include "src/effects/contour_and_region_fill_lab7.h"
#include "src/effects/boundary_margin_code.h"
#include "src/effects/boundary_reconstruct_from_code.h"
#include "src/effects/selected_object_features.h"
#include "src/effects/filter_objects_by_area_orientation.h"
#include "src/effects/labeling_compare.h"
#include "src/effects/general_filter.h"
#include "src/effects/assignment.h"

int main() {
  Logger::init();

  if (!hasQtBackend()) {
    ERROR("OpenCV Qt backend not detected. Rebuild/install OpenCV with "
          "WITH_QT=ON.");
    Logger::destroy();
    return -1;
  }

  cv::namedWindow(MAIN_WINDOW_NAME,
                  cv::WINDOW_NORMAL | cv::WINDOW_GUI_EXPANDED);

  DEBUG("Opening file {}", IMAGE("PI-L4/trasaturi_geom.bmp"));
  cv::Mat img;
  if (!loadImage(IMAGE("PI-L4/trasaturi_geom.bmp"), img)) {
    Logger::destroy();
    return -1;
  }

  DEBUG("image loaded with {} {}", img.rows, img.cols);
  INFO("Controls:");
  INFO("  Left/Right arrows: cycle through processing functions");
  INFO("  O: open file dialog to load new image");
  INFO("  Enter: save all outputs");
  INFO("  Space: refresh windows");
  INFO("  ESC: exit");

  ControlsManager controls(MAIN_WINDOW_NAME, []() { g_needsUpdate = true; });

  Slider slider(
      {{"Bi-Level",
        EffectFn3(bi_level_color_map),
        {{"Threshold", 127, 255, 127, true, 0}},
        "Binary thresholding",
        {},
        "Lab 3 — intensity & colour"},

       {"Negative",
        EffectFn2(negative),
        {},
        "Pixel inversion"},

       {"Additive Factor",
        EffectFn3(additive_factor),
        {{"Additive Factor", 128, 255, 128, true, 0}},
        "Brightness shift"},

       {"Multiplicative",
        EffectFn3(multiplicative),
        {{"Multiplicative", 1, 35, 1, true, 0}},
        "Saturating multiplication"},

       {"View Placeholder",
        EffectFn2(view_placeholder),
        {},
        "Static 4-quadrant reference"},

       {"RGB Channels",
        EffectFn3(rgb_channels),
        {},
        "Split into color-tinted R, G, B channels",
        {{"Channel Mode", {"Colored", "Grayscale"}, 1}}},

       {"Histogram & PDF",
        EffectFn3(histogram_and_pdf),
        {},
        "Compute and display histogram + PDF",
        {},
        "Lab 3 — histogram & quantization"},

       {"Histogram (m bins)",
        EffectFn3(histogram_m_bins),
        {{"Bins (m)", 256, 256, 256, true, 2}},
        "Histogram computed with m<=256 bins"},

       {"Gray Level Reduction",
        EffectFn3(gray_level_reduction),
        {{"Levels (WL)", 4, 128, 4, true, 2}},
        "Uniform quantization into WL gray levels"},

       {"Floyd-Steinberg",
        EffectFn3(floyd_steinberg),
        {{"Levels (WL)", 4, 128, 4, true, 2}},
        "Error-diffused quantization (Floyd–Steinberg)"},

       {"HSV Hue Quantization",
        EffectFn3(hsv_hue_quantization),
        {{"Hue Levels", 6, 128, 6, true, 2}},
        "Reduce hue levels in HSV space",
        {{"S/V Mode", {"Keep Original", "Set to Max"}, 0}}},

       {"Morphology Lab7",
        EffectFn3(morphology_lab7_combined),
        {{"Iterations (N)", 1, 15, 1, true, 1}},
        "N8-only morphology: dilation, erosion, opening, closing",
        {},
        "Lab 7 — morphology & contours"},

       {"Contour + Fill Lab7",
        EffectFn3(contour_and_region_fill_lab7),
        {{"Max Fill Iter", 3000, 50000, 3000, true, 1}},
        "N8-only boundary extraction and iterative region filling"},

       {"Boundary Margin Code",
        EffectFn3(boundary_margin_code),
        {},
        "Trace black component margins and print chain code",
        {{"Neighborhood", {"N4", "N8"}, 1}}},

       {"Boundary Reconstruct",
        EffectFn3(boundary_reconstruct_from_code),
        {},
        "Reconstruct boundary from chain-code file",
        {{"Neighborhood", {"N4", "N8"}, 1}}},

       {"Selected Object Features",
        EffectFn3(selected_object_features),
        {},
        "Click-select object and compute geometric features",
        {},
        "Lab 4 — geometric features"},

       {"Filter Area+Orientation",
        EffectFn3(filter_objects_by_area_orientation),
        {{"TH_area", 1000, 200000, 1000, true, 1},
         {"phi_LOW", 0, 180, 0, true, -90},
         {"phi_HIGH", 180, 180, 180, true, -90}},
        "Keep objects with area < TH_area and phi in [phi_LOW, phi_HIGH]"},

       {"Labeling Compare",
        EffectFn3(labeling_compare),
        {},
        "Compare traversal labeling with two-pass labeling",
        {{"Neighborhood", {"N4", "N8"}, 1}, {"Traversal", {"BFS", "DFS"}, 0}}},

       {"Lab8 Statistics",
        EffectFn3(lab8_statistics),
        {},
        "Histogram, cumulative histogram, PDF, mean, stddev",
        {},
        "Lab 8 — statistical properties"},

       {"Lab8 Auto Binarize",
        EffectFn3(lab8_auto_binarize),
        {{"Epsilon x100", 10, 500, 10, true, 1}},
        "Iterative automatic thresholding (T stops when |dT|<eps)"},

       {"Lab8 Transforms",
        EffectFn3(lab8_transforms),
        {{"Iout Min", 30, 255, 0, true, 0},
         {"Iout Max", 220, 255, 255, true, 0},
         {"Gamma x10", 10, 30, 10, true, 1},
         {"Brightness Offset", 128, 255, 128, true, 0}},
        "Negative, contrast stretch, gamma, brightness + histograms"},

       {"Lab8 Equalize",
        EffectFn3(lab8_histogram_equalization),
        {},
        "Histogram equalization (original + equalized histograms)"},

       {"Spatial Filter",
        EffectFn2(general_filter),
        {},
        "test"},

       {"Contour Similarity",
        EffectFn3(contour_similarity),
        {{"Threshold x100", 80, 100, 0, true, 0}},
        "Label objects, trace extended contours, cross-correlation similarity",
        {},
        "Assignment — contour similarity"}},
      controls);

  int keyCode = -1;
  bool running = true;
  std::string displayedEffectName;
  OutputImages lastOutputs;
  while (running) {
    if (slider.applyPendingSelection()) {
      g_needsUpdate = true;
    }

    const std::string currentEffectName = slider.currentName();
    if (currentEffectName != displayedEffectName) {
      updateMainWindowTitle(currentEffectName);
      displayedEffectName = currentEffectName;
      g_selectionState.dirty = true;
    }

    lastOutputs = slider.exec(img);
    renderGrid(img, lastOutputs);
    cv::setMouseCallback(MAIN_WINDOW_NAME, mainWindowMouseCallback, nullptr);

    keyCode = WaitKey(30);

    if (cv::getWindowProperty(MAIN_WINDOW_NAME, cv::WND_PROP_VISIBLE) < 1) {
      running = false;
      break;
    }

    if (keyCode == -1)
      continue;

    KEY operation = resolvedKey(keyCode);

    switch (operation) {
    case KEY::LEFT_ARROW:
      slider.previous();
      g_needsUpdate = true;
      break;
    case KEY::RIGHT_ARROW:
      slider.next();
      g_needsUpdate = true;
      break;
    case KEY::ENTER:
      saveAllOutputs(lastOutputs);
      break;
    case KEY::SPACE:
      slider.reactivateControls();
      g_needsUpdate = true;
      break;
    case KEY::ESC:
      running = false;
      break;
    default:
      if (keyCode >= '1' && keyCode <= '9') {
        slider.selectByIndex(static_cast<std::size_t>(keyCode - '1'));
        g_needsUpdate = true;
      } else if (keyCode == 'o' || keyCode == 'O') {
        std::string newPath = FileUtils::openFileDialog();
        if (!newPath.empty()) {
          loadImage(newPath, img);
          slider.reactivateControls();
        }
      }
      break;
    }
  }

  controls.destroyControls();
  Logger::destroy();
  return 0;
}
