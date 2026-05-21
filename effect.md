# Effect Architecture

## Overview

An **effect** is a C++ function that takes a source image, optionally reads runtime parameters, and pushes one or more named result images into an output collection. The framework handles rendering, control creation, and effect switching automatically.

Three pieces collaborate:

```
SliderEntry  ──────►  Slider  ──────►  main loop
  (what to run)      (which one)     (render grid)
       │
       ▼
  ControlsManager
  (trackbars + radio buttons)
```

---

## Anatomy of an Effect Function

### Signature A — no controls

```cpp
void my_effect(const cv::Mat& src, OutputImages& outputs);
```

Use when the effect has no adjustable parameters. The framework wraps this automatically into the 3-arg form.

### Signature B — with controls

```cpp
void my_effect(const cv::Mat& src, OutputImages& outputs, ControlsManager& controls);
```

Use when sliders or radio buttons are needed. `controls` is live — every call reflects the current widget state.

### `OutputImages`

```cpp
using OutputImages = std::vector<std::pair<std::string, cv::Mat>>;
```

Push any number of `(label, image)` pairs. They appear in the output grid left-to-right in insertion order, alongside the source panel.

```cpp
outputs.push_back({"Threshold", binary});
outputs.push_back({"Contours",  annotated});
```

Grayscale `cv::Mat`s are auto-converted to BGR at render time — no manual conversion needed for display.

---

## Effect Registration — `SliderEntry`

Every effect is registered as a `SliderEntry` in the `Slider slider({ ... })` initializer inside `main()`.

```cpp
struct SliderEntry {
    std::string name;                       // shown in the effect list
    EffectFn3   process;                    // always stored as 3-arg form
    std::vector<TrackbarSpec>  trackbars;   // sliders for this effect
    std::vector<RadioGroupSpec> radioGroups;// radio button groups
    std::string description;                // shown as tooltip / status text
    std::string groupLabel;                 // non-empty → starts a new section header
};
```

**Constructor forms** (both accepted in the initializer list):

```cpp
// 2-arg effect, no controls
{ "Name", EffectFn2(fn), {/*trackbars*/}, "description" }

// 3-arg effect, with controls
{ "Name", EffectFn3(fn), {/*trackbars*/}, "description", {/*radioGroups*/} }

// first entry of a new lab section
{ "Name", EffectFn3(fn), {}, "description", {}, "--- Lab N --- topic ---" }
```

---

## Controls

### Trackbars — `TrackbarSpec`

```cpp
struct TrackbarSpec {
    std::string name;       // label and lookup key
    int defaultValue;       // initial slider position
    int maxValue;           // upper bound (inclusive)
    int neutralValue = 0;   // value returned when toggled OFF
    bool enabled = true;    // initial ON/OFF state of the checkbox
    int minValue = 0;       // lower bound (inclusive)
};
```

Each spec creates one slider **and** one ON/OFF checkbox in the controls panel.

**Reading inside the effect:**

| Method | Returns |
|---|---|
| `controls.getEffective("name")` | `neutralValue` if OFF, otherwise slider value — **prefer this** |
| `controls.get("name")` | raw slider value always, ignores checkbox |

Example — kernel size that can be disabled:

```cpp
// Registration
{ "Blur Radius", 5, 20, 0, true, 1 }

// Inside effect
int radius = controls.getEffective("Blur Radius");
if (radius > 0) cv::GaussianBlur(src, dst, cv::Size(2*radius+1, 2*radius+1), 0);
else dst = src.clone();
```

### Radio Buttons — `RadioGroupSpec`

```cpp
struct RadioGroupSpec {
    std::string name;                  // group label and lookup key
    std::vector<std::string> options;  // button labels, left to right
    int defaultIndex = 0;              // initially selected (0-based)
};
```

Multiple groups per effect are fine — each is independent.

**Reading inside the effect:**

```cpp
int mode = controls.getRadio("Mode");  // 0-based index of selected option
```

---

## Full Example

```cpp
// ── 1. Effect function ───────────────────────────────────────────────────────
void threshold_demo(const cv::Mat& src, OutputImages& outputs, ControlsManager& controls) {
    int thresh = controls.getEffective("Threshold");
    int method = controls.getRadio("Type");

    cv::Mat binary;
    int type = (method == 0) ? cv::THRESH_BINARY : cv::THRESH_BINARY_INV;
    cv::threshold(src, binary, thresh, 255, type);

    outputs.push_back({"Binary", binary});

    cv::Mat edges;
    cv::Canny(binary, edges, 50, 150);
    outputs.push_back({"Edges", edges});
}

// ── 2. Registration in main() ────────────────────────────────────────────────
Slider slider({
    // ...existing entries...
    { "Threshold Demo",
      EffectFn3(threshold_demo),
      { {"Threshold", 128, 255, 128, true, 0} },
      "Binary threshold + edge detection",
      { {"Type", {"Binary", "Inv Binary"}, 0} } },
}, controls);
```

---

## Controls Panel Layout

```
┌─────────────────────────────────┐
│  --- Lab 3 --- intensity ---    │  ← groupLabel of first entry in section
│  ○ Histogram                    │
│  ● Threshold Demo               │  ← active effect
│  ○ Blur                         │
├─────────────────────────────────┤
│  [✓] Threshold  [====|===] 128  │  ← TrackbarSpec, one per spec
├─────────────────────────────────┤
│  Type:  (●) Binary  (○) Inv     │  ← RadioGroupSpec
└─────────────────────────────────┘
```

The panel is **fully rebuilt** each time the active effect changes (OpenCV Qt limitation). `ControlsManager::activate` is called automatically by `Slider` — do not call it manually.

---

## Click-Driven Effects

For effects that need a pixel coordinate from the source image (e.g., flood-fill, object selection), read the global selection state set by the mouse callback:

```cpp
extern SelectionState g_selectionState;

void my_effect(const cv::Mat& src, OutputImages& outputs, ControlsManager& /*controls*/) {
    if (!g_selectionState.hasClick) { /* show placeholder */ return; }
    cv::Point p = g_selectionState.imagePoint;  // pixel coords in src
    // ...
    g_selectionState.dirty = false;  // clear the "new click" flag after reading
}
```

The mouse callback in `main()` maps window coordinates back to source-image coordinates automatically.

---

## Checklist for Adding an Effect

1. Write the function in `main.cpp` (Signature A or B).
2. Add a `SliderEntry` to the `Slider slider({ ... })` initializer in `main()`.
   - Use `EffectFn2` for Signature A, `EffectFn3` for Signature B.
   - List all `TrackbarSpec`s; list `RadioGroupSpec`s as the 5th argument.
   - Set `groupLabel` on the first entry of a new lab section.
3. Build: `./run.sh build execute`.

No other files need to change.

---

## Key Constraints

- **Never** call `cv::createTrackbar` or `cv::createButton` directly — `ControlsManager` owns all widgets and will clobber manual ones on every effect switch.
- Default image is grayscale (`cv::IMREAD_GRAYSCALE`). Color effects must convert: `cv::cvtColor(src, bgr, cv::COLOR_GRAY2BGR)`.
- `neutralValue` must be a safe "no-op" value for the effect (e.g., `0` for a radius, `128` for a centered offset).
- New `.cpp` files under `src/` are picked up by CMake's glob — do not manually edit `CMakeLists.txt`.
