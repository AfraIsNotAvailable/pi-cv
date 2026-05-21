#include "slider.h"
#include "logger.h"

#include "opencv2/opencv.hpp"

Slider::Slider(std::vector<SliderEntry> entries, ControlsManager& controls)
    : entries_(std::move(entries)),
      controls_(controls)
{
}

void Slider::next()
{
    if (entries_.empty()) {
        return;
    }
    selectByIndex((currentIndex_ + 1) % entries_.size());
}

void Slider::previous()
{
    if (entries_.empty()) {
        return;
    }
    const auto previousIndex = (currentIndex_ == 0) ? entries_.size() - 1 : currentIndex_ - 1;
    selectByIndex(previousIndex);
}

void Slider::selectByIndex(std::size_t idx)
{
    if (entries_.empty() || idx >= entries_.size()) {
        return;
    }
    currentIndex_ = idx;
    activateCurrent();
    DEBUG("Select -> {}", entries_[currentIndex_].name);
}

bool Slider::applyPendingSelection()
{
    if (!pendingIndex_.has_value()) {
        return false;
    }

    const std::size_t idx = *pendingIndex_;
    pendingIndex_.reset();

    if (entries_.empty() || idx >= entries_.size() || idx == currentIndex_) {
        return false;
    }

    selectByIndex(idx);
    return true;
}

OutputImages Slider::exec(const cv::Mat& src)
{
    OutputImages outputs;
    if (entries_.empty()) {
        return outputs;
    }

    // Activate controls on first call
    if (!initialised_) {
        activateCurrent();
        initialised_ = true;
    }
    entries_[currentIndex_].process(src, outputs, controls_);
    return outputs;
}

void Slider::reactivateControls()
{
    activateCurrent();
}

const std::string& Slider::currentName() const
{
    static const std::string emptyName;
    if (entries_.empty()) {
        return emptyName;
    }
    return entries_[currentIndex_].name;
}

void Slider::activateCurrent()
{
    if (entries_.empty()) {
        return;
    }

    controls_.activate(entries_[currentIndex_].trackbars,
                        entries_[currentIndex_].radioGroups);
    effectButtonBindings_.clear();
    ensureEffectButtons();
}

void Slider::ensureEffectButtons()
{
    if (entries_.empty()) {
        return;
    }

    // Rebuild every activation: the controls window was destroyed/recreated,
    // so all buttons are gone. Allocate enough room so the stable pointers
    // we hand Qt don't get invalidated mid-loop.
    effectButtonBindings_.reserve(entries_.size());
    effectButtonBindings_.clear();

    bool firstInGroup = true;  // force QT_NEW_BUTTONBAR before the very first entry

    for (std::size_t index = 0; index < entries_.size(); ++index) {
        const auto& entry = entries_[index];

        // When a groupLabel is present, emit a dedicated buttonbar header:
        //   1) a new button row (QT_NEW_BUTTONBAR) visually separates groups,
        //   2) a push button carrying the group label acts as a section heading.
        if (!entry.groupLabel.empty() || firstInGroup) {
            std::string header = entry.groupLabel.empty()
                                     ? std::string("Effects")
                                     : entry.groupLabel;
            cv::createButton("--- " + header + " ---",
                             &Slider::onGroupHeaderPressed,
                             nullptr,
                             cv::QT_PUSH_BUTTON | cv::QT_NEW_BUTTONBAR,
                             0);
            firstInGroup = false;
        }

        // The actual effect selector — one radio per effect.
        effectButtonBindings_.push_back(EffectButtonBinding{this, index});
        int initialState = (index == currentIndex_) ? 1 : 0;
        cv::createButton(entry.name,
                         &Slider::onEffectButtonPressed,
                         &effectButtonBindings_.back(),
                         cv::QT_RADIOBOX,
                         initialState);
    }
}

void Slider::onEffectButtonPressed(int state, void* userdata)
{
    auto* binding = static_cast<EffectButtonBinding*>(userdata);
    if (!binding || !binding->self) {
        return;
    }

    // QT_RADIOBOX fires state=1 for the newly-selected option and state=0
    // for the one being deselected. Only schedule a change for the former.
    if (state == 0) {
        return;
    }

    // Don't self-schedule when the callback fires as a side effect of the
    // radio group being (re)initialised to the current selection.
    if (binding->index == binding->self->currentIndex_) {
        return;
    }

    binding->self->pendingIndex_ = binding->index;
}

void Slider::onGroupHeaderPressed(int /*state*/, void* /*userdata*/)
{
    // Intentional no-op: group-header buttons are decorative labels only.
}

