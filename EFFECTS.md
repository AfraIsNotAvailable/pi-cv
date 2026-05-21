# How to Write New Effects

This guide covers everything you need to add a new image-processing effect to the project.

## Quick Overview

1. Write a processing function in `main.cpp`
2. Register it in the `Slider` constructor (also in `main.cpp`)
3. Build and run

No other files need to be modified.

---

## Step 1 — Write the Processing Function

Every effect is a plain C++ function. There are **two signatures** you can choose from:

### Simple effect (no controls)

Use this when your effect has no adjustable parameters.

```cpp
void my_effect(const cv::Mat& src, OutputImages& outputs) {
    cv::Mat dst;
    // ... process src into dst ...
    outputs.push_back({"Result Name", dst});
}
```

### Effect with controls (trackbars / radio buttons)

Use this when you need sliders or radio buttons.

```cpp
void my_effect(const cv::Mat& src, OutputImages& outputs, ControlsManager& controls) {
    int param = controls.getEffective("My Param");
    int mode  = controls.getRadio("My Mode");

    cv::Mat dst;
    // ... process src using param and mode ...
    outputs.push_back({"Result Name", dst});
}
```

### What is `OutputImages`?

```cpp
using OutputImages = std::vector<std::pair<std::string, cv::Mat>>;
```

It's a list of `(name, image)` pairs. Push as many as you want — they all appear in the grid alongside the source image. The name is drawn as a label above each panel.

---

## Step 2 — Register in the Slider

Find the `Slider` constructor call in `main()` and add a new `SliderEntry`:

### Minimal (no controls)

```cpp
{ "My Effect",
  EffectFn2(my_effect),
  {} /* no trackbars */,
  "Short description" },
```

### With trackbars

```cpp
{ "My Effect",
  EffectFn3(my_effect),
  { {"My Param", 128, 255, 128, true, 0} },
  "Short description" },
```

### With trackbars and radio buttons

```cpp
{ "My Effect",
  EffectFn3(my_effect),
  { {"My Param", 128, 255, 128, true, 0} },      // trackbars
  "Short description",
  { {"My Mode", {"Option A", "Option B"}, 0} } }, // radio groups
```

---

## TrackbarSpec Reference

```cpp
struct TrackbarSpec {
    std::string name;       // trackbar label and lookup key
    int defaultValue;       // initial slider position
    int maxValue;           // upper bound
    int neutralValue = 0;   // value used when toggled OFF
    bool enabled = true;    // initial ON/OFF state
    int minValue = 0;       // lower bound
};
```

**Reading values in your effect:**

| Method | Behaviour |
|---|---|
| `controls.get("name")` | Always returns the raw trackbar value |
| `controls.getEffective("name")` | Returns `neutralValue` when the parameter is toggled OFF, otherwise the trackbar value |

Use `getEffective()` unless you have a reason to ignore the ON/OFF toggle.

---

## RadioGroupSpec Reference

```cpp
struct RadioGroupSpec {
    std::string name;                   // group label / lookup key
    std::vector<std::string> options;   // option labels shown as radio buttons
    int defaultIndex = 0;               // initially selected option (0-based)
};
```

**Reading the selection:**

```cpp
int choice = controls.getRadio("group name");
// Returns the 0-based index of the currently selected radio button.
```

Multiple radio groups per effect are supported — each group is independent.

---

## Complete Example

Here is a full, copy-paste-ready example that creates a brightness-adjustment effect with a trackbar and a radio group:

```cpp
void brightness(const cv::Mat& src, OutputImages& outputs, ControlsManager& controls) {
    int shift  = controls.getEffective("Shift") - 128; // centre at 0
    int method = controls.getRadio("Method");           // 0 = Add, 1 = Multiply

    cv::Mat dst;
    if (method == 0) {
        dst = src + cv::Scalar(shift);
    } else {
        float factor = 1.0f + shift / 128.0f;
        src.convertTo(dst, -1, factor, 0);
    }

    outputs.push_back({"Adjusted", dst});
}
```

Registration:

```cpp
{ "Brightness",
  EffectFn3(brightness),
  { {"Shift", 128, 255, 128, true, 0} },
  "Adjust brightness via addition or multiplication",
  { {"Method", {"Add", "Multiply"}, 0} } },
```

---

## Tips

- **Grayscale vs colour**: The default image load uses `cv::IMREAD_GRAYSCALE`. If your effect needs colour, convert with `cv::cvtColor(src, bgr, cv::COLOR_GRAY2BGR)` or load the image differently.
- **Multiple outputs**: Push multiple entries to `outputs` — e.g., one per channel. They all show in the grid.
- **Clamping**: Use `std::clamp(value, 0, 255)` to avoid overflow when doing arithmetic on pixel values.
- **File dialog**: Press `O` at runtime to load a different image via the file dialog.
- **Saving**: Press `Enter` to save all current output images to `assets/export/`.
- **Effect selection**: Use Left/Right arrows, number keys `1-9`, or the Qt push buttons in the control bar.

---

## L3-specific Effects

The repository now includes several lab‑level effects which may serve as templates:

* **Histogram & PDF** – computes the 256‑bin histogram of a grayscale image and its normalized probability density function. No controls.
* **Histogram (m bins)** – same as above but uses a user‑adjustable number of bins `m` (slider 2..256).
* **Gray Level Reduction** – quantizes an image to a user‑specified number of gray levels (`WL` slider). Also shows the resulting histogram.
* **Floyd-Steinberg** – applies error‑diffusion quantization with the Floyd–Steinberg kernel; the number of output levels is adjustable and the histogram of the result is shown.
* **HSV Hue Quantization** – reduces the number of discrete hue values in the HSV colour space. A radio button lets you either keep the original saturation/value or force them to maximum, aiding visualization.

The implementation of these effects illustrates use of helper functions (`computeHistogram`, `drawHistogramInt`, etc.) placed in `main.cpp`.

## L4 / Geometric-Feature Effects

The repository also includes object-level geometric analysis effects for labeled/binary images:

* **Selected Object Features** – click an object in the **Source** panel to select it. The effect auto-detects labeled images (`CV_32S` / `CV_16U`) or computes connected components from binary/grayscale input. It prints to standard output:
    * area
    * center of mass
    * elongation axis orientation (principal axis from central moments)
    * perimeter
    * thinness ratio ($4\pi A / P^2$)
    * aspect ratio (major/minor axis estimate)

    It also displays:
    * contour points + centroid + elongation axis on a source clone
    * object projections (X, Y, major-axis, minor-axis) on a separate source clone

* **Filter Area+Orientation** – keeps only objects that satisfy both conditions:
    * `area < TH_area`
    * `phi_LOW < phi < phi_HIGH` (degrees in `[-90, 90]`)

    Controls:
    * `TH_area`
    * `phi_LOW`
    * `phi_HIGH`

## L8 / Statistical Properties

Effects added for Lab 8 (statistical properties of intensity images). All operate on grayscale input; full walk-through in [docs/lab8_explained.md](docs/lab8_explained.md).

* **Lab8 Statistics** — histogram (with μ / σ overlaid), cumulative histogram, PDF. Terminal logs `μ` and `σ`.
* **Lab8 Auto Binarize** — iterative automatic thresholding. `Epsilon x100` controls the termination error `ε = slider / 100`.
* **Lab8 Transforms** — negative, contrast stretch (`Iout Min`, `Iout Max`), gamma correction (`Gamma x10 / 10`), brightness offset (`Brightness Offset − 128`), with histograms for all four plus the original.
* **Lab8 Equalize** — histogram equalization (`I_out = 255 · FDPC(I_in)`) with μ / σ on both histograms.

Helpers introduced in `main.cpp`: `computeCumulativeHistogram`, `computeMeanStdDev`, `drawCumulativeHistogram`, `drawHistogramIntWithOverlay`.

## Controls panel layout

The control panel on the right of the main window is organized as:

1. **Effect list (top)** — one radio button per effect, grouped by lab with a heading line such as `--- Lab 3 --- intensity & colour ---`. A filled circle marks the active effect. Click any radio to switch.
2. **Trackbars + ON/OFF checkboxes (middle)** — one per `TrackbarSpec` of the active effect; each checkbox toggles the slider against its `neutralValue`.
3. **Radio groups (bottom)** — one per `RadioGroupSpec` of the active effect.

Section 1 is rebuilt every time the active effect changes (because OpenCV's Qt HighGUI destroys and recreates the whole panel); sections 2 and 3 come from `ControlsManager::activate` called during that rebuild. Arrow keys, `1..9`, and `O` still work alongside the radios.

To add a new section (for example, a Lab 9 group), set `groupLabel` on the first `SliderEntry` of that section inside the `Slider slider({ ... })` initializer. Subsequent entries that leave `groupLabel` empty belong to the same group.

    Output includes both the filtered objects and a colorized filtered-label view.
