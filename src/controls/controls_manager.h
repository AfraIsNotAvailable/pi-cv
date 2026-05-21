#ifndef __CONTROLS_MANAGER_H__
#define __CONTROLS_MANAGER_H__

#include <string>
#include <vector>
#include <functional>

// Describes a single trackbar to be created in the Controls window
struct TrackbarSpec {
    std::string name;
    int defaultValue;
    int maxValue;
    int neutralValue = 0;
    bool enabled = true;
    int minValue = 0;
};

// Describes a group of mutually-exclusive radio buttons.
// At most one option is selected at a time; getRadio() returns its index.
struct RadioGroupSpec {
    std::string name;                  // group label / lookup key
    std::vector<std::string> options;  // option labels
    int defaultIndex = 0;              // initially selected option
};

// Manages the persistent "Controls" window and its trackbars.
// Call activate() with a set of TrackbarSpecs whenever the active
// processing function changes — the window is recreated with the
// correct trackbars. Use get() to read trackbar values by name.
class ControlsManager {
public:
    using UpdateCallback = std::function<void()>;

    explicit ControlsManager(std::string windowName, UpdateCallback onUpdate);
    ~ControlsManager() = default;

    // (Re)create the Controls window with the given trackbar specs
    // and optional radio-group specs.
    void activate(const std::vector<TrackbarSpec>& specs,
                  const std::vector<RadioGroupSpec>& radioSpecs = {});

    // Read the current value of a trackbar by name.
    int get(const std::string& name) const;

    // Read effective value: neutralValue if disabled, otherwise current trackbar value.
    int getEffective(const std::string& name) const;

    // Read the currently selected radio-button index for a group.
    int getRadio(const std::string& groupName) const;

    // Destroy the Controls window (call on exit).
    void destroyControls();

private:
    struct ToggleBinding {
        ControlsManager* self;
        std::size_t index;
    };

    struct RadioBinding {
        ControlsManager* self;
        std::size_t groupIndex;
        std::size_t optionIndex;
    };

    struct NormalizedTrackbar {
        TrackbarSpec spec;
        int range = 0;
        int defaultPos = 0;
    };

    static void onTrackbarChanged(int pos, void* userdata);
    static void onToggleChanged(int state, void* userdata);
    static void onRadioChanged(int state, void* userdata);
    static NormalizedTrackbar normalizeSpec(const TrackbarSpec& rawSpec);
    const TrackbarSpec* findSpec(const std::string& name) const;

    std::string windowName_;
    UpdateCallback updateCallback_;
    std::vector<TrackbarSpec> activeSpecs_;
    std::vector<ToggleBinding> toggleBindings_;
    std::vector<RadioGroupSpec> activeRadioSpecs_;
    std::vector<RadioBinding> radioBindings_;
    std::vector<int> radioSelections_;
    bool windowExists_ = false;
    bool rebuildingControls_ = false;
};

#endif
