#ifndef __SLIDER_H__
#define __SLIDER_H__

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "controls_manager.h"
#include "output_images.h"

// ============================================================================
// Effect function signatures
// ============================================================================

// Full signature: receives source, output collection, and controls
using EffectFn3 = std::function<void(const cv::Mat&, OutputImages&, ControlsManager&)>;

// Simplified signature: no controls needed
using EffectFn2 = std::function<void(const cv::Mat&, OutputImages&)>;

// One entry in the slider carousel: a named processing function
// plus the trackbar specs it needs.
struct SliderEntry {
    std::string name;
    EffectFn3 process;
    std::vector<TrackbarSpec> trackbars;
    std::vector<RadioGroupSpec> radioGroups;
    std::string description;
    std::string groupLabel;

    // Construct with full 3-arg effect function (src, outputs, controls)
    SliderEntry(std::string name, EffectFn3 fn,
                std::vector<TrackbarSpec> trackbars, std::string description,
                std::vector<RadioGroupSpec> radioGroups = {},
                std::string groupLabel = {})
        : name(std::move(name)),
          process(std::move(fn)),
          trackbars(std::move(trackbars)),
          radioGroups(std::move(radioGroups)),
          description(std::move(description)),
          groupLabel(std::move(groupLabel)) {}

    // Construct with 2-arg effect function (src, outputs) — controls ignored
    SliderEntry(std::string name, EffectFn2 fn,
                std::vector<TrackbarSpec> trackbars, std::string description,
                std::vector<RadioGroupSpec> radioGroups = {},
                std::string groupLabel = {})
        : name(std::move(name)),
          process([f = std::move(fn)](const cv::Mat& src, OutputImages& out, ControlsManager&) {
              f(src, out);
          }),
          trackbars(std::move(trackbars)),
          radioGroups(std::move(radioGroups)),
          description(std::move(description)),
          groupLabel(std::move(groupLabel)) {}
};

class Slider {
  std::vector<SliderEntry> entries_;
  ControlsManager& controls_;

  struct EffectButtonBinding {
      Slider* self;
      std::size_t index;
  };

  std::size_t currentIndex_ = 0;
  bool initialised_ = false;
  std::vector<EffectButtonBinding> effectButtonBindings_;
  std::optional<std::size_t> pendingIndex_;

  void activateCurrent();
  void ensureEffectButtons();

  static void onEffectButtonPressed(int state, void* userdata);
  static void onGroupHeaderPressed(int state, void* userdata);

public:
  Slider(std::vector<SliderEntry> entries, ControlsManager& controls);
  ~Slider() = default;

  void next();
  void previous();
  void selectByIndex(std::size_t idx);
  bool applyPendingSelection();

  // Execute the current effect, returning named output images.
  OutputImages exec(const cv::Mat& src);

  // Re-activate controls for the current entry (useful after
  // destroyAllWindows or image reload).
  void reactivateControls();

  const std::string& currentName() const;
};

#endif