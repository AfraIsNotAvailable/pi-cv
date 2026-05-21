#include "controls_manager.h"
#include "logger.h"
#include "opencv2/opencv.hpp"

#include <algorithm>

ControlsManager::ControlsManager(std::string windowName, UpdateCallback onUpdate)
    : windowName_(std::move(windowName)),
      updateCallback_(std::move(onUpdate))
{
}

void ControlsManager::activate(const std::vector<TrackbarSpec>& specs,
                               const std::vector<RadioGroupSpec>& radioSpecs) {
    // Destroy the old Controls state so we start fresh.
    if (windowExists_) {
        cv::destroyWindow(windowName_);
        windowExists_ = false;
    }

    activeSpecs_ = specs;
    toggleBindings_.clear();
    toggleBindings_.reserve(activeSpecs_.size());

    activeRadioSpecs_ = radioSpecs;
    radioBindings_.clear();
    radioSelections_.clear();
    radioSelections_.resize(activeRadioSpecs_.size());

    cv::namedWindow(windowName_, cv::WINDOW_NORMAL | cv::WINDOW_GUI_EXPANDED);
    windowExists_ = true;

    rebuildingControls_ = true;

    // Create trackbars + ON/OFF checkboxes
    for (std::size_t index = 0; index < activeSpecs_.size(); ++index) {
        auto normalized = normalizeSpec(activeSpecs_[index]);
        activeSpecs_[index] = normalized.spec;
        auto& spec = activeSpecs_[index];

        cv::createTrackbar(spec.name, windowName_, nullptr, normalized.range,
                           &ControlsManager::onTrackbarChanged, this);
        cv::setTrackbarPos(spec.name, windowName_, normalized.defaultPos);

        toggleBindings_.push_back(ToggleBinding{this, index});
        cv::createButton(spec.name + " ON/OFF",
                         &ControlsManager::onToggleChanged,
                         &toggleBindings_.back(),
                         cv::QT_CHECKBOX,
                         spec.enabled);
    }

    // Create radio-button groups
    for (std::size_t gi = 0; gi < activeRadioSpecs_.size(); ++gi) {
        const auto& group = activeRadioSpecs_[gi];
        int defaultIdx = std::clamp(group.defaultIndex, 0,
                                    std::max(0, static_cast<int>(group.options.size()) - 1));
        radioSelections_[gi] = defaultIdx;

        for (std::size_t oi = 0; oi < group.options.size(); ++oi) {
            radioBindings_.push_back(RadioBinding{this, gi, oi});
            cv::createButton(group.options[oi],
                             &ControlsManager::onRadioChanged,
                             &radioBindings_.back(),
                             cv::QT_RADIOBOX,
                             static_cast<int>(oi) == defaultIdx);
        }
    }

    rebuildingControls_ = false;
}

int ControlsManager::get(const std::string& name) const {
    if (!windowExists_) return 0;

    const auto* spec = findSpec(name);
    if (!spec) return 0;

    int valueRange = spec->maxValue - spec->minValue;

    const int pos = std::clamp(cv::getTrackbarPos(name, windowName_), 0, std::max(0, valueRange));
    return spec->minValue + pos;
}

int ControlsManager::getEffective(const std::string& name) const {
    const auto* spec = findSpec(name);
    if (!spec) return 0;
    if (!spec->enabled) {
        return spec->neutralValue;
    }
    return get(name);
}

void ControlsManager::destroyControls() {
    if (windowExists_) {
        cv::destroyWindow(windowName_);
        windowExists_ = false;
    }
}

void ControlsManager::onTrackbarChanged(int /*pos*/, void* userdata) {
    auto* self = static_cast<ControlsManager*>(userdata);
    if (self && self->updateCallback_) {
        self->updateCallback_();
    }
}

void ControlsManager::onToggleChanged(int state, void* userdata) {
    auto* binding = static_cast<ToggleBinding*>(userdata);
    if (!binding || !binding->self) return;

    auto* self = binding->self;
    if (self->rebuildingControls_) return;
    if (binding->index >= self->activeSpecs_.size()) return;

    self->activeSpecs_[binding->index].enabled = (state != 0);
    if (self->updateCallback_) {
        self->updateCallback_();
    }
}

void ControlsManager::onRadioChanged(int state, void* userdata) {
    auto* binding = static_cast<RadioBinding*>(userdata);
    if (!binding || !binding->self) return;

    auto* self = binding->self;
    if (self->rebuildingControls_) return;
    if (binding->groupIndex >= self->activeRadioSpecs_.size()) return;

    // Qt RADIOBOX fires state=1 for the newly selected option
    if (state != 0) {
        self->radioSelections_[binding->groupIndex] = static_cast<int>(binding->optionIndex);
        if (self->updateCallback_) {
            self->updateCallback_();
        }
    }
}

int ControlsManager::getRadio(const std::string& groupName) const {
    for (std::size_t i = 0; i < activeRadioSpecs_.size(); ++i) {
        if (activeRadioSpecs_[i].name == groupName) {
            return radioSelections_[i];
        }
    }
    return 0;
}

const TrackbarSpec* ControlsManager::findSpec(const std::string& name) const {
    for (const auto& spec : activeSpecs_) {
        if (spec.name == name) {
            return &spec;
        }
    }
    return nullptr;
}

ControlsManager::NormalizedTrackbar ControlsManager::normalizeSpec(const TrackbarSpec& rawSpec) {
    NormalizedTrackbar normalized;
    normalized.spec = rawSpec;

    if (normalized.spec.minValue > normalized.spec.maxValue) {
        std::swap(normalized.spec.minValue, normalized.spec.maxValue);
        WARN("Trackbar '{}' had minValue > maxValue. Values were swapped.", normalized.spec.name);
    }

    normalized.spec.defaultValue = std::clamp(
        normalized.spec.defaultValue,
        normalized.spec.minValue,
        normalized.spec.maxValue
    );

    normalized.spec.neutralValue = std::clamp(
        normalized.spec.neutralValue,
        normalized.spec.minValue,
        normalized.spec.maxValue
    );

    normalized.range = std::max(0, normalized.spec.maxValue - normalized.spec.minValue);
    normalized.defaultPos = std::clamp(
        normalized.spec.defaultValue - normalized.spec.minValue,
        0,
        normalized.range
    );

    return normalized;
}
